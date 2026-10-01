"use strict";

const vscode = require("vscode");

function activeDocument() {
  const document = vscode.window.activeTextEditor?.document;
  return document?.languageId === "sagan" ? document : undefined;
}

function operationParameters(document, kind, scope = "document", profile = "debug") {
  const parameters = {
    kind,
    scope,
    profile,
    textDocument: { uri: document.uri.toString(), version: document.version }
  };
  if (scope === "project") {
    const folder = vscode.workspace.getWorkspaceFolder(document.uri);
    if (folder) parameters.projectUri = folder.uri.toString();
  }
  return parameters;
}

function renderResult(output, result) {
  output.appendLine(`[${result.kind}] ${result.state}`);
  if (result.stdout) output.append(result.stdout);
  if (result.stderr) output.append(result.stderr);
  for (const issue of result.diagnostics || []) {
    output.appendLine(`${issue.severity || "error"} ${issue.code || ""}: ${issue.message}`);
  }
  if (result.executable) output.appendLine(`Artifact: ${result.executable}`);
  if (result.exitStatus !== null && result.exitStatus !== undefined) {
    output.appendLine(`Exit status: ${result.exitStatus}`);
  }
}

async function runOperation(client, output, kind, options = {}) {
  const document = options.document || activeDocument();
  if (!document) throw new Error("Open a Sagan document before running this command.");
  const scope = options.scope || "document";
  return vscode.window.withProgress({
    location: vscode.ProgressLocation.Notification,
    title: `Sagan ${kind}`,
    cancellable: true
  }, async (_progress, token) => {
    const result = await client.sendRequest("sagan/operation",
      operationParameters(document, kind, scope, options.profile), token);
    renderResult(output, result);
    if (result.stdout || result.stderr || result.state !== "completed") output.show(true);
    if (result.state === "failed") throw new Error(`Sagan ${kind} failed.`);
    return result;
  });
}

function registerOperations(context, client, output, capabilities) {
  if (!capabilities.operationTransport) return;
  const register = (name, kind) => context.subscriptions.push(vscode.commands.registerCommand(name, async () => {
    try { await runOperation(client, output, kind); }
    catch (error) {
      if (error?.name !== "CancellationError") void vscode.window.showErrorMessage(error.message);
    }
  }));
  register("sagan.check", "check");
  if (capabilities.nativeBuild) register("sagan.build", "build");
  if (capabilities.nativeRun) register("sagan.run", "run");

  const provider = vscode.tasks.registerTaskProvider("sagan", {
    provideTasks() {
      const tasks = [];
      for (const kind of ["check", ...(capabilities.nativeBuild ? ["build"] : []),
        ...(capabilities.nativeRun ? ["run"] : [])]) {
        for (const scope of ["document", "project"]) {
          const definition = { type: "sagan", operation: kind, scope };
          tasks.push(new vscode.Task(definition, vscode.TaskScope.Workspace,
            `${kind} ${scope}`, "sagan", new vscode.CustomExecution(async () => {
              const write = new vscode.EventEmitter();
              const close = new vscode.EventEmitter();
              const cancellation = new vscode.CancellationTokenSource();
              return {
                onDidWrite: write.event,
                onDidClose: close.event,
                open: async () => {
                  try {
                    const document = activeDocument();
                    if (!document) throw new Error("Open a Sagan document first.");
                    const result = await client.sendRequest("sagan/operation",
                      operationParameters(document, kind, scope), cancellation.token);
                    if (result.stdout) write.fire(result.stdout.replace(/\n/g, "\r\n"));
                    if (result.stderr) write.fire(result.stderr.replace(/\n/g, "\r\n"));
                    write.fire(`\r\nSagan ${kind}: ${result.state}\r\n`);
                    close.fire(result.state === "completed" ? 0 : 1);
                  } catch (error) {
                    write.fire(`\r\n${error.message}\r\n`);
                    close.fire(1);
                  }
                },
                close: () => cancellation.cancel()
              };
            })));
        }
      }
      return tasks;
    },
    resolveTask: () => undefined
  });
  context.subscriptions.push(provider);
}

module.exports = { operationParameters, registerOperations, renderResult, runOperation };
