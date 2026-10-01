"use strict";

const assert = require("node:assert/strict");
const Module = require("node:module");

function collection() {
  const values = new Map();
  return {
    add: (item) => values.set(item.id, item),
    delete: (id) => values.delete(id),
    forEach: (action) => values.forEach(action),
    get: (id) => values.get(id),
    get size() { return values.size; }
  };
}

const uri = { toString: () => "file:///workspace/orbit.sagan" };
const document = { languageId: "sagan", uri, version: 3 };
const events = [];
let profileHandler;
const controller = {
  items: collection(),
  createTestItem: (id, label, itemUri) => ({ id, label, uri: itemUri, children: collection() }),
  createRunProfile: (_name, _kind, handler) => {
    profileHandler = handler;
    return { dispose: () => {} };
  },
  createTestRun: () => ({
    enqueued: (item) => events.push(["enqueued", item.id]),
    started: (item) => events.push(["started", item.id]),
    passed: (item) => events.push(["passed", item.id]),
    failed: (item) => events.push(["failed", item.id]),
    errored: (item) => events.push(["errored", item.id]),
    skipped: (item) => events.push(["skipped", item.id]),
    appendOutput: () => {},
    end: () => events.push(["end"])
  }),
  dispose: () => {}
};
const vscode = {
  Range: class Range { constructor(...points) { this.points = points; } },
  TestMessage: class TestMessage { constructor(message) { this.message = message; } },
  TestRunProfileKind: { Run: 1 },
  Uri: { parse: () => uri },
  tests: { createTestController: () => controller },
  workspace: {
    asRelativePath: () => "orbit.sagan",
    findFiles: async () => [uri],
    openTextDocument: async () => document,
    onDidOpenTextDocument: () => ({ dispose: () => {} }),
    onDidSaveTextDocument: () => ({ dispose: () => {} })
  }
};
const originalLoad = Module._load;
Module._load = function load(request, parent, isMain) {
  if (request === "vscode") return vscode;
  return originalLoad.call(this, request, parent, isMain);
};

async function main() {
  const requests = [];
  const client = { sendRequest: async (method, parameters) => {
    requests.push([method, parameters]);
    if (method === "sagan/tests/discover") return { tests: [{
      id: "local::orbit", name: "orbit", uri: uri.toString(), version: 3,
      range: { start: { line: 0, character: 5 }, end: { line: 0, character: 12 } }
    }] };
    return { tests: [{ id: "local::orbit", state: "passed", durationMilliseconds: 4 }] };
  } };
  const { registerTesting } = require("../src/testing");
  const context = { subscriptions: [] };
  registerTesting(context, client, { appendLine: () => {} },
    { testDocumentDiscovery: true, testDocumentRun: true });
  await new Promise((resolve) => setImmediate(resolve));
  assert.equal(requests[0][0], "sagan/tests/discover");
  assert.equal(controller.items.size, 1);
  assert.equal(typeof profileHandler, "function");
  await profileHandler({}, { isCancellationRequested: false });
  assert(requests.some(([method]) => method === "sagan/tests/run"));
  assert(events.some(([kind, id]) => kind === "passed" && id === "local::orbit"));
  console.log("All Sagan Test Explorer client tests passed.");
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
}).finally(() => { Module._load = originalLoad; });
