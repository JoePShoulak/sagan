"use strict";

const vscode = require("vscode");
const { LanguageClient, RevealOutputChannelOn } = require("vscode-languageclient/node");
const {
  capabilityNames, discoverCompiler, executableCandidates, selectServerExecutable, serverExecutableCandidates
} = require("./capabilities");
const { registerOperations } = require("./operations");
const { registerTesting } = require("./testing");

let client;
let clientContext;
let clientOutput;
let clientFeatureDisposables = [];

function enabledCapabilities(capabilities) {
  return capabilityNames.filter((name) => capabilities[name]);
}

function workspacePaths() {
  return (vscode.workspace.workspaceFolders || []).map((folder) => folder.uri.fsPath);
}

function serverForWorkspace() {
  const configuration = vscode.workspace.getConfiguration("sagan");
  return selectServerExecutable(serverExecutableCandidates(
    configuration.get("server.path", ""), configuration.get("compiler.path", ""), workspacePaths()));
}

async function inspectTooling(output) {
  output.clear();
  output.appendLine("Sagan tooling discovery");
  const configuration = vscode.workspace.getConfiguration("sagan");
  const configuredCompiler = configuration.get("compiler.path", "");
  const result = await discoverCompiler(executableCandidates(configuredCompiler, workspacePaths()));

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
  output.appendLine(`Language server: ${serverForWorkspace() || "not found"}`);
  output.show(true);
  void vscode.window.showInformationMessage(`Sagan tooling found (${available.length} capabilities).`);
}

async function stopLanguageServer() {
  for (const disposable of clientFeatureDisposables.splice(0)) disposable.dispose();
  if (!client) return;
  const stopping = client;
  client = undefined;
  await stopping.stop();
}

async function startLanguageServer(output) {
  await stopLanguageServer();
  const server = serverForWorkspace();
  if (!server) {
    output.appendLine("No Sagan language server was found. Configure sagan.server.path.");
    return false;
  }

  const traceServer = vscode.workspace.getConfiguration("sagan").get("server.trace", false);
  const serverOptions = {
    command: server,
    args: [],
    options: {
      env: { ...process.env, ...(traceServer ? { SAGAN_LSP_LOG: "stderr" } : {}) },
      windowsHide: true
    }
  };
  const clientOptions = {
    documentSelector: [
      { scheme: "file", language: "sagan" },
      { scheme: "untitled", language: "sagan" }
    ],
    synchronize: { configurationSection: "sagan" },
    outputChannel: output,
    revealOutputChannelOn: RevealOutputChannelOn.Error
  };

  client = new LanguageClient("sagan", "Sagan Language Server", serverOptions, clientOptions);
  output.appendLine(`Starting Sagan language server: ${server}`);
  await client.start();
  const compilerContract = client.initializeResult?.experimental?.compiler ||
    client.initializeResult?.capabilities?.experimental?.compiler;
  const compilerCapabilities = compilerContract?.capabilities || {};
  const featureContext = { subscriptions: clientFeatureDisposables };
  registerOperations(featureContext, client, output, compilerCapabilities);
  registerTesting(featureContext, client, output, compilerCapabilities);
  const enabled = enabledCapabilities(compilerCapabilities);
  output.appendLine(`Compiler capabilities: ${enabled.join(", ") || "none"}`);
  output.appendLine("Sagan language server started; VS Code registered its advertised capabilities.");
  return true;
}

async function restartLanguageServer() {
  if (!clientContext || !clientOutput) return;
  try {
    const started = await startLanguageServer(clientOutput);
    if (started) void vscode.window.showInformationMessage("Sagan language server restarted.");
    else void vscode.window.showWarningMessage("No Sagan language server was found. Configure sagan.server.path.");
  } catch (error) {
    clientOutput.appendLine(`Language server restart failed: ${error.message}`);
    clientOutput.show(true);
    void vscode.window.showErrorMessage(`Sagan language server failed to start: ${error.message}`);
  }
}

async function activate(context) {
  const output = vscode.window.createOutputChannel("Sagan");
  clientContext = context;
  clientOutput = output;
  context.subscriptions.push(output);
  context.subscriptions.push(vscode.commands.registerCommand("sagan.showToolingStatus", () => inspectTooling(output)));
  context.subscriptions.push(vscode.commands.registerCommand("sagan.restartLanguageServer", restartLanguageServer));
  context.subscriptions.push(vscode.workspace.onDidChangeConfiguration((event) => {
    if (event.affectsConfiguration("sagan.server.path") ||
        event.affectsConfiguration("sagan.compiler.path") ||
        event.affectsConfiguration("sagan.server.trace")) {
      void restartLanguageServer();
    }
  }));
  context.subscriptions.push({ dispose: () => { void stopLanguageServer(); } });
  output.appendLine("Sagan extension activated. Language features follow server capability negotiation.");
  try {
    await startLanguageServer(output);
  } catch (error) {
    output.appendLine(`Language server startup failed: ${error.message}`);
    output.show(true);
    void vscode.window.showErrorMessage(`Sagan language server failed to start: ${error.message}`);
  }
}

async function deactivate() {
  await stopLanguageServer();
}

module.exports = { activate, deactivate, enabledCapabilities, inspectTooling, restartLanguageServer,
  serverForWorkspace, startLanguageServer, stopLanguageServer };
