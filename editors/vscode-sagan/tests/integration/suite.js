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

  const workspaceSymbols = await vscode.commands.executeCommand(
    "vscode.executeWorkspaceSymbolProvider", "main");
  assert(workspaceSymbols && workspaceSymbols.some((symbol) => symbol.name === "main"),
    "workspace symbol provider did not return main");

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
  console.log("Sagan Extension Development Host integration test passed.");
}

module.exports = { run };
