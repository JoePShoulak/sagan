"use strict";

const assert = require("node:assert/strict");
const Module = require("node:module");

const commands = new Map();
const outputLines = [];
const configuration = new Map([
  ["compiler.path", ""],
  ["server.path", ""],
  ["server.trace", false]
]);
const output = {
  appendLine: (line) => outputLines.push(line),
  clear: () => { outputLines.length = 0; },
  dispose: () => {},
  show: () => {}
};
const vscode = {
  commands: {
    registerCommand: (name, action) => {
      commands.set(name, action);
      return { dispose: () => commands.delete(name) };
    }
  },
  window: {
    createOutputChannel: () => output,
    showErrorMessage: () => {},
    showInformationMessage: () => {},
    showWarningMessage: () => {}
  },
  workspace: {
    getConfiguration: () => ({ get: (name, fallback) => configuration.get(name) ?? fallback }),
    workspaceFolders: []
  }
};

class FakeLanguageClient {
  static instances = [];

  constructor(id, name, serverOptions, clientOptions) {
    Object.assign(this, { id, name, serverOptions, clientOptions, started: false, stopped: false });
    FakeLanguageClient.instances.push(this);
  }

  async start() { this.started = true; }
  async stop() { this.stopped = true; }
}

const originalLoad = Module._load;
Module._load = function load(request, parent, isMain) {
  if (request === "vscode") return vscode;
  if (request === "vscode-languageclient/node") {
    return { LanguageClient: FakeLanguageClient, RevealOutputChannelOn: { Error: 1 } };
  }
  return originalLoad.call(this, request, parent, isMain);
};

async function main() {
  const extension = require("../src/extension");
  const context = { subscriptions: [] };
  await extension.activate(context);

  assert.equal(FakeLanguageClient.instances.length, 1);
  const client = FakeLanguageClient.instances[0];
  assert.equal(client.serverOptions.command, "sagan-lsp");
  assert.equal(client.started, true);
  assert.deepEqual(client.clientOptions.documentSelector, [
    { scheme: "file", language: "sagan" },
    { scheme: "untitled", language: "sagan" }
  ]);
  assert(commands.has("sagan.showToolingStatus"));
  assert(commands.has("sagan.restartLanguageServer"));
  assert(outputLines.some((line) => line.includes("language server started")));

  await extension.deactivate();
  assert.equal(client.stopped, true);
  console.log("All Sagan extension lifecycle tests passed.");
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
}).finally(() => {
  Module._load = originalLoad;
});
