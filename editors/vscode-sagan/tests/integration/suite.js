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

  let entryRenameRefused = false;
  try {
    const entryRename = await vscode.commands.executeCommand(
      "vscode.executeDocumentRenameProvider", document.uri, new vscode.Position(1, 5), "start");
    entryRenameRefused = entryRename === undefined;
  } catch (error) {
    entryRenameRefused = /No result/.test(String(error));
  }
  assert(entryRenameRefused, "entry-point rename should be refused until workspace rename is proven");
  console.log("Sagan Extension Development Host integration test passed.");
}

module.exports = { run };
