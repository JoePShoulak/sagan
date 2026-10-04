"use strict";

const assert = require("node:assert/strict");
const vscode = require("vscode");

async function waitFor(probe, description, timeoutMs = 15000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const value = await probe();
    if (value) return value;
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw new Error(`Timed out waiting for ${description}`);
}

async function run() {
  const serverPath = process.env.SAGAN_LSP_PATH;
  assert(serverPath, "SAGAN_LSP_PATH was not passed to the Extension Development Host");
  await vscode.workspace.getConfiguration("sagan").update(
    "server.path", serverPath, vscode.ConfigurationTarget.Global);

  const extension = vscode.extensions.getExtension("sagan-language.sagan-language");
  assert(extension, "Sagan extension was not registered");
  await extension.activate();

  const document = await vscode.workspace.openTextDocument(
    vscode.Uri.joinPath(vscode.workspace.workspaceFolders[0].uri, "main.sagan"));
  assert.equal(document.languageId, "sagan");
  await vscode.window.showTextDocument(document);

  const registeredCommands = await vscode.commands.getCommands(true);
  assert(registeredCommands.includes("sagan.check"),
    "check command was not registered from the advertised operation transport");
  assert(registeredCommands.includes("sagan.build") && registeredCommands.includes("sagan.run"),
    "native build/run commands were not registered from advertised capabilities");
  await vscode.commands.executeCommand("sagan.check");

  const callPosition = new vscode.Position(2, 15);
  const hovers = await waitFor(
    () => vscode.commands.executeCommand("vscode.executeHoverProvider", document.uri, callPosition)
      .then((items) => items && items.length ? items : undefined),
    "Sagan hover results");
  assert(hovers.length > 0);

  const definitions = await vscode.commands.executeCommand(
    "vscode.executeDefinitionProvider", document.uri, callPosition);
  assert(definitions && definitions.length > 0, "definition provider returned no locations");

  const references = await vscode.commands.executeCommand(
    "vscode.executeReferenceProvider", document.uri, callPosition);
  assert(references && references.length >= 2,
    "reference provider did not include the function declaration and call");

  const highlights = await vscode.commands.executeCommand(
    "vscode.executeDocumentHighlights", document.uri, callPosition);
  assert(highlights && highlights.length >= 2,
    "document highlights did not include the function declaration and call");

  const completions = await vscode.commands.executeCommand(
    "vscode.executeCompletionItemProvider", document.uri, new vscode.Position(3, 8));
  assert(completions && completions.items.length > 0, "completion provider returned no items");

  const signatures = await vscode.commands.executeCommand(
    "vscode.executeSignatureHelpProvider", document.uri, new vscode.Position(2, 18), "(");
  assert(signatures && signatures.signatures.length > 0, "signature provider returned no signatures");

  const symbols = await vscode.commands.executeCommand(
    "vscode.executeDocumentSymbolProvider", document.uri);
  assert(symbols && symbols.length > 0, "document symbol provider returned no symbols");

  const workspaceSymbols = await waitFor(
    () => vscode.commands.executeCommand("vscode.executeWorkspaceSymbolProvider", "🚀")
      .then((items) => items && items.some((symbol) => symbol.name === "🚀") ? items : undefined),
    "Sagan workspace symbols");
  assert(workspaceSymbols && workspaceSymbols.some((symbol) => symbol.name === "🚀"),
    "workspace symbol provider did not return the declared function");

  const foldingRanges = await vscode.commands.executeCommand(
    "vscode.executeFoldingRangeProvider", document.uri);
  assert(foldingRanges && foldingRanges.length > 0, "folding provider returned no ranges");

  const selectionRanges = await vscode.commands.executeCommand(
    "vscode.executeSelectionRangeProvider", document.uri, [callPosition]);
  assert(selectionRanges && selectionRanges.length === 1,
    "selection range provider did not return the requested position");

  const semanticTokens = await vscode.commands.executeCommand(
    "vscode.provideDocumentSemanticTokens", document.uri);
  assert(semanticTokens && semanticTokens.data && semanticTokens.data.length > 0,
    "semantic token provider returned no tokens");

  const edits = await vscode.commands.executeCommand(
    "vscode.executeFormatDocumentProvider", document.uri, { tabSize: 2, insertSpaces: true });
  assert(Array.isArray(edits) && edits.length > 0, "format provider did not return the expected indentation edit");

  const originalText = document.getText();
  const invalidEdit = new vscode.WorkspaceEdit();
  invalidEdit.insert(document.uri, document.positionAt(originalText.length), "\nlet broken = missing\n");
  assert(await vscode.workspace.applyEdit(invalidEdit), "could not introduce the diagnostic fixture");
  await waitFor(() => vscode.languages.getDiagnostics(document.uri).length > 0 || undefined,
    "a published diagnostic");

  const repairEdit = new vscode.WorkspaceEdit();
  repairEdit.replace(document.uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)),
    originalText);
  assert(await vscode.workspace.applyEdit(repairEdit), "could not repair the diagnostic fixture");
  await waitFor(() => vscode.languages.getDiagnostics(document.uri).length === 0 || undefined,
    "diagnostic clearing");

  const rename = await vscode.commands.executeCommand(
    "vscode.executeDocumentRenameProvider", document.uri, callPosition, "launch");
  assert(rename instanceof vscode.WorkspaceEdit, "rename provider did not return a workspace edit");
  const renameEdits = rename.entries().flatMap(([, editsForDocument]) => editsForDocument);
  assert(renameEdits.length >= 2 && renameEdits.every((edit) => edit.newText === "launch"),
    "rename did not cover the function declaration and call");

  const localRename = await vscode.commands.executeCommand(
    "vscode.executeDocumentRenameProvider", document.uri, new vscode.Position(3, 8), "altitude");
  assert(localRename instanceof vscode.WorkspaceEdit, "local-variable rename did not return a workspace edit");
  const localRenameEdits = localRename.entries().flatMap(([, editsForDocument]) => editsForDocument);
  assert(localRenameEdits.length >= 2 && localRenameEdits.every((edit) => edit.newText === "altitude"),
    "local-variable rename did not cover the declaration and reference");

  const declarationRename = await vscode.commands.executeCommand(
    "vscode.executeDocumentRenameProvider", document.uri, new vscode.Position(2, 8), "height");
  assert(declarationRename instanceof vscode.WorkspaceEdit,
    "local-variable rename from its declaration did not return a workspace edit");
  const declarationRenameEdits = declarationRename.entries().flatMap(([, editsForDocument]) => editsForDocument);
  assert(declarationRenameEdits.length >= 2 && declarationRenameEdits.every((edit) => edit.newText === "height"),
    "local-variable rename from its declaration did not cover the declaration and reference");
  const beforeAppliedRename = document.getText();
  assert(await vscode.workspace.applyEdit(declarationRename),
    "local-variable declaration rename could not be applied atomically");
  assert(document.getText().includes("let height =") && document.getText().includes("print(height)"),
    "applied local-variable rename did not update every occurrence");
  await vscode.commands.executeCommand("undo");
  await waitFor(() => document.getText() === beforeAppliedRename || undefined,
    "single-operation rename undo");

  const memberMain = await vscode.workspace.openTextDocument(vscode.Uri.joinPath(
    vscode.workspace.workspaceFolders[0].uri, "member_rename", "main.sagan"));
  const memberOwner = await vscode.workspace.openTextDocument(vscode.Uri.joinPath(
    vscode.workspace.workspaceFolders[0].uri, "member_rename", "vehicle.sagan"));
  await vscode.window.showTextDocument(memberMain);
  const memberPosition = new vscode.Position(4, memberMain.lineAt(4).text.indexOf("sample") + 1);
  const memberDefinitions = await waitFor(
    () => vscode.commands.executeCommand("vscode.executeDefinitionProvider", memberMain.uri, memberPosition)
      .then((items) => items && items.length ? items : undefined),
    "cross-file public-member definition");
  assert(memberDefinitions.some((location) => location.uri.toString() === memberOwner.uri.toString()),
    "cross-file public-member definition did not target its declaring module");
  const memberReferences = await vscode.commands.executeCommand(
    "vscode.executeReferenceProvider", memberMain.uri, memberPosition);
  assert(memberReferences && memberReferences.length === 3,
    "cross-file public-member references did not include the declaration and both calls");
  const memberMainBeforeRename = memberMain.getText();
  const memberOwnerBeforeRename = memberOwner.getText();
  const memberRename = await vscode.commands.executeCommand(
    "vscode.executeDocumentRenameProvider", memberMain.uri, memberPosition, "measure");
  assert(memberRename instanceof vscode.WorkspaceEdit,
    "cross-file public-member rename did not return a workspace edit");
  const memberRenameEntries = memberRename.entries();
  assert.equal(memberRenameEntries.length, 2,
    "cross-file public-member rename did not cover both module documents");
  assert.equal(memberRenameEntries.flatMap(([, editsForDocument]) => editsForDocument).length, 3,
    "cross-file public-member rename did not cover the declaration and both calls");
  assert(await vscode.workspace.applyEdit(memberRename),
    "cross-file public-member rename could not be applied atomically");
  assert(memberMain.getText().includes("probe.measure()") &&
         memberMain.getText().includes("sensor.measure()") &&
         memberOwner.getText().includes("fun measure()"),
    "applied cross-file public-member rename missed an identity-resolved occurrence");
  await vscode.commands.executeCommand("undo");
  await waitFor(() => memberMain.getText() === memberMainBeforeRename &&
                      memberOwner.getText() === memberOwnerBeforeRename || undefined,
    "single-operation cross-file rename undo");

  const overlayEdit = new vscode.WorkspaceEdit();
  overlayEdit.insert(memberMain.uri, memberMain.positionAt(memberMain.getText().length),
    "\nfun inspect_again(probe: Probe): Int => probe.sample()\n");
  assert(await vscode.workspace.applyEdit(overlayEdit),
    "could not add the unsaved rename occurrence");
  const overlayOffset = memberMain.getText().lastIndexOf("sample");
  const overlayRename = await vscode.commands.executeCommand(
    "vscode.executeDocumentRenameProvider", memberMain.uri,
    memberMain.positionAt(overlayOffset + 1), "measure");
  assert(overlayRename instanceof vscode.WorkspaceEdit,
    "rename did not resolve the unsaved public-member occurrence");
  assert.equal(overlayRename.entries().flatMap(([, editsForDocument]) => editsForDocument).length, 4,
    "rename omitted an unsaved public-member occurrence from the atomic workspace edit");
  await vscode.commands.executeCommand("undo");
  await waitFor(() => memberMain.getText() === memberMainBeforeRename || undefined,
    "unsaved rename fixture undo");

  const mainRename = await vscode.commands.executeCommand(
    "vscode.executeDocumentRenameProvider", document.uri, new vscode.Position(1, 5), "start");
  assert(mainRename instanceof vscode.WorkspaceEdit,
    "ordinary function named main did not return a rename edit");
  const mainRenameEdits = mainRename.entries().flatMap(([, editsForDocument]) => editsForDocument);
  assert(mainRenameEdits.length === 1 && mainRenameEdits[0].newText === "start",
    "ordinary function named main did not rename its declaration exactly once");

  const privateMembers = await vscode.workspace.openTextDocument(vscode.Uri.joinPath(
    vscode.workspace.workspaceFolders[0].uri, "private_members.sagan"));
  await vscode.window.showTextDocument(privateMembers);
  const privateMethodLine = privateMembers.lineAt(12).text;
  const privateMethodPosition = new vscode.Position(12, privateMethodLine.indexOf("advance!") + 1);
  const privateMethodRename = await vscode.commands.executeCommand(
    "vscode.executeDocumentRenameProvider", privateMembers.uri, privateMethodPosition, "step!");
  assert(privateMethodRename instanceof vscode.WorkspaceEdit,
    "private mutating method rename did not return a workspace edit");
  const privateMethodEdits = privateMethodRename.entries().flatMap(([, editsForDocument]) => editsForDocument);
  assert(privateMethodEdits.length === 2 && privateMethodEdits.every((edit) => edit.newText === "step!"),
    "private mutating method rename did not preserve the ! spelling across declaration and call");

  const privateFieldLine = privateMembers.lineAt(4).text;
  const privateFieldPosition = new vscode.Position(4, privateFieldLine.indexOf("value") + 1);
  const privateFieldRename = await vscode.commands.executeCommand(
    "vscode.executeDocumentRenameProvider", privateMembers.uri, privateFieldPosition, "count");
  assert(privateFieldRename instanceof vscode.WorkspaceEdit,
    "private field rename did not return a workspace edit");
  const privateFieldEdits = privateFieldRename.entries().flatMap(([, editsForDocument]) => editsForDocument);
  assert(privateFieldEdits.length === 4 && privateFieldEdits.every((edit) => edit.newText === "count"),
    "private field rename did not cover its declaration and every self reference");

  const aliasMain = await vscode.workspace.openTextDocument(vscode.Uri.joinPath(
    vscode.workspace.workspaceFolders[0].uri, "alias_rename", "main.sagan"));
  const aliasOwner = await vscode.workspace.openTextDocument(vscode.Uri.joinPath(
    vscode.workspace.workspaceFolders[0].uri, "alias_rename", "guidance.sagan"));
  await vscode.window.showTextDocument(aliasMain);
  const importLine = aliasMain.lineAt(2).text;
  const publicNamePosition = new vscode.Position(2, importLine.indexOf("course") + 1);
  const aliasMainBeforeRename = aliasMain.getText();
  const aliasOwnerBeforeRename = aliasOwner.getText();
  const aliasRename = await vscode.commands.executeCommand(
    "vscode.executeDocumentRenameProvider", aliasMain.uri, publicNamePosition, "route");
  assert(aliasRename instanceof vscode.WorkspaceEdit,
    "exported public-name rename did not return a workspace edit from its import");
  assert.equal(aliasRename.entries().flatMap(([, editsForDocument]) => editsForDocument).length, 2,
    "exported public-name rename did not cover its export and import spellings exactly once");
  assert(await vscode.workspace.applyEdit(aliasRename),
    "exported public-name rename could not be applied atomically");
  assert(aliasMain.getText().includes("import route from guidance as calculate_course") &&
         aliasMain.getText().includes("calculate_course(21)") &&
         aliasOwner.getText().includes("export calculate as route"),
    "exported public-name rename changed an independent alias or missed a public spelling");
  await vscode.commands.executeCommand("undo");
  await waitFor(() => aliasMain.getText() === aliasMainBeforeRename &&
                      aliasOwner.getText() === aliasOwnerBeforeRename || undefined,
    "single-operation exported-name rename undo");

  let collisionRefused = false;
  try {
    const collision = await vscode.commands.executeCommand(
      "vscode.executeDocumentRenameProvider", aliasMain.uri, publicNamePosition, "trajectory");
    collisionRefused = collision === undefined;
  } catch (error) {
    collisionRefused = String(error).includes("Proposed public name is already exported by this module");
  }
  assert(collisionRefused,
    "exported-name rename did not surface the compiler-owned collision explanation");

  const manifest = await vscode.workspace.openTextDocument(vscode.Uri.joinPath(
    vscode.workspace.workspaceFolders[0].uri, "manifest_project", "sagan.toml"));
  assert.equal(manifest.languageId, "sagan-manifest");
  await vscode.window.showTextDocument(manifest);

  const manifestHover = await waitFor(
    () => vscode.commands.executeCommand(
      "vscode.executeHoverProvider", manifest.uri, new vscode.Position(7, 1))
      .then((items) => items && items.length ? items : undefined),
    "Sagan manifest hover results");
  assert(manifestHover.length > 0, "manifest hover provider returned no results");

  const manifestSymbols = await vscode.commands.executeCommand(
    "vscode.executeDocumentSymbolProvider", manifest.uri);
  assert(manifestSymbols && manifestSymbols.some((symbol) => symbol.name === "package") &&
         manifestSymbols.some((symbol) => symbol.name === "application"),
    "manifest symbols omitted a declared section");

  const entryDefinitions = await vscode.commands.executeCommand(
    "vscode.executeDefinitionProvider", manifest.uri, new vscode.Position(4, 10));
  assert(entryDefinitions && entryDefinitions.some((location) => location.uri.fsPath.endsWith("entry.sagan")),
    "manifest entry navigation did not resolve entry.sagan");

  const manifestOriginal = manifest.getText();
  const incompleteManifest = "[package]\nna";
  const incompleteEdit = new vscode.WorkspaceEdit();
  incompleteEdit.replace(manifest.uri,
    new vscode.Range(manifest.positionAt(0), manifest.positionAt(manifestOriginal.length)), incompleteManifest);
  assert(await vscode.workspace.applyEdit(incompleteEdit), "could not create the manifest completion fixture");
  const manifestCompletions = await waitFor(
    () => vscode.commands.executeCommand(
      "vscode.executeCompletionItemProvider", manifest.uri, new vscode.Position(1, 2))
      .then((items) => items && items.items.some((item) => item.label === "name") ? items : undefined),
    "Sagan manifest completion results");
  assert(manifestCompletions.items.some((item) => item.label === "name"),
    "manifest completion omitted the package name key");

  const invalidManifest = manifestOriginal.replace("[application]", "mystery = \"value\"\n[application]");
  const invalidManifestEdit = new vscode.WorkspaceEdit();
  invalidManifestEdit.replace(manifest.uri,
    new vscode.Range(manifest.positionAt(0), manifest.positionAt(manifest.getText().length)), invalidManifest);
  assert(await vscode.workspace.applyEdit(invalidManifestEdit), "could not create the manifest diagnostic fixture");
  await waitFor(() => vscode.languages.getDiagnostics(manifest.uri)
    .some((diagnostic) => diagnostic.message.includes("mystery")) || undefined,
  "a published manifest diagnostic");

  const restoreManifest = new vscode.WorkspaceEdit();
  restoreManifest.replace(manifest.uri,
    new vscode.Range(manifest.positionAt(0), manifest.positionAt(manifest.getText().length)), manifestOriginal);
  assert(await vscode.workspace.applyEdit(restoreManifest), "could not repair the manifest fixture");
  await waitFor(() => vscode.languages.getDiagnostics(manifest.uri).length === 0 || undefined,
    "manifest diagnostic clearing");

  const manifestFormattingFixture = manifestOriginal.replace("name =", "name    =");
  const formattingEdit = new vscode.WorkspaceEdit();
  formattingEdit.replace(manifest.uri,
    new vscode.Range(manifest.positionAt(0), manifest.positionAt(manifest.getText().length)),
    manifestFormattingFixture);
  assert(await vscode.workspace.applyEdit(formattingEdit), "could not create the manifest formatting fixture");
  const manifestFormatting = await vscode.commands.executeCommand(
    "vscode.executeFormatDocumentProvider", manifest.uri, { tabSize: 2, insertSpaces: true });
  assert(Array.isArray(manifestFormatting) && manifestFormatting.length > 0,
    "manifest formatting did not normalize key spacing");
  console.log("Sagan Extension Development Host integration test passed.");
}

module.exports = { run };
