"use strict";

const vscode = require("vscode");
const { capabilityNames, discoverCompiler, executableCandidates } = require("./capabilities");

function enabledCapabilities(capabilities) {
  return capabilityNames.filter((name) => capabilities[name]);
}

async function inspectTooling(output) {
  output.clear();
  output.appendLine("Sagan tooling discovery");

  const configuredPath = vscode.workspace.getConfiguration("sagan").get("compiler.path", "");
  const workspaceFolders = (vscode.workspace.workspaceFolders || []).map((folder) => folder.uri.fsPath);
  const candidates = executableCandidates(configuredPath, workspaceFolders);
  const result = await discoverCompiler(candidates);

  for (const failure of result.failures) output.appendLine(`Skipped ${failure}`);
  if (!result.executable) {
    output.appendLine("No compatible Sagan compiler was found.");
    output.show(true);
    void vscode.window.showWarningMessage("No compatible Sagan compiler was found. Configure sagan.compiler.path.");
    return;
  }

  const available = enabledCapabilities(result.capabilities.capabilities);
  output.appendLine(`Compiler: ${result.executable}`);
  output.appendLine(`Schema: ${result.capabilities.schema}`);
  output.appendLine(`Available capabilities: ${available.join(", ") || "none"}`);
  if (!result.capabilities.capabilities.languageServer) {
    output.appendLine("Language server: unavailable; semantic providers remain disabled.");
  }
  output.show(true);
  void vscode.window.showInformationMessage(`Sagan tooling found (${available.length} capabilities).`);
}

function activate(context) {
  const output = vscode.window.createOutputChannel("Sagan");
  context.subscriptions.push(output);
  context.subscriptions.push(vscode.commands.registerCommand("sagan.showToolingStatus", () => inspectTooling(output)));
  output.appendLine("Sagan extension activated. Semantic providers are registered only after server capability negotiation.");
}

function deactivate() {}

module.exports = { activate, deactivate, enabledCapabilities, inspectTooling };
