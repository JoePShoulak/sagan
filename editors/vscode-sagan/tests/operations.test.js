"use strict";

const assert = require("node:assert/strict");
const Module = require("node:module");

const document = {
  languageId: "sagan",
  version: 7,
  uri: { toString: () => "file:///workspace/main.sagan" }
};
const lines = [];
const vscode = {
  ProgressLocation: { Notification: 15 },
  window: {
    activeTextEditor: { document },
    withProgress: async (_options, action) => action({}, { isCancellationRequested: false }),
  },
  workspace: {
    getWorkspaceFolder: () => ({ uri: { toString: () => "file:///workspace" } })
  }
};

const originalLoad = Module._load;
Module._load = function load(request, parent, isMain) {
  if (request === "vscode") return vscode;
  return originalLoad.call(this, request, parent, isMain);
};

async function main() {
  const { operationParameters, renderResult, runOperation } = require("../src/operations");
  assert.deepEqual(operationParameters(document, "build", "project", "optimized"), {
    kind: "build",
    scope: "project",
    profile: "optimized",
    textDocument: { uri: "file:///workspace/main.sagan", version: 7 },
    projectUri: "file:///workspace"
  });

  const output = {
    append: (text) => lines.push(text),
    appendLine: (text) => lines.push(`${text}\n`),
    show: () => {}
  };
  const requests = [];
  const client = {
    sendRequest: async (method, parameters, token) => {
      requests.push({ method, parameters, token });
      return { kind: "check", state: "completed", stdout: "ok\n", stderr: "",
        diagnostics: [], exitStatus: 0 };
    }
  };
  await runOperation(client, output, "check");
  assert.equal(requests[0].method, "sagan/operation");
  assert.equal(requests[0].parameters.textDocument.version, 7);
  assert(lines.join("").includes("[check] completed"));
  assert(lines.join("").includes("Exit status: 0"));

  lines.length = 0;
  renderResult(output, { kind: "build", state: "failed", diagnostics: [
    { severity: "error", code: "SAG-TEST", message: "broken" }
  ] });
  assert(lines.join("").includes("error SAG-TEST: broken"));
  console.log("All Sagan operation client tests passed.");
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
}).finally(() => { Module._load = originalLoad; });
