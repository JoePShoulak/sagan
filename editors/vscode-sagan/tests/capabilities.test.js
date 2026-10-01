"use strict";

const assert = require("node:assert/strict");
const path = require("node:path");
const {
  capabilityNames,
  discoverCompiler,
  executableOnPath,
  executableCandidates,
  parseCapabilities,
  selectServerExecutable,
  serverExecutableCandidates,
  schema
} = require("../src/capabilities");

function response(overrides = {}) {
  return JSON.stringify({
    schema,
    positionEncodings: ["utf-16", "utf-8-bytes"],
    capabilities: Object.fromEntries(capabilityNames.map((name) => [name, overrides[name] ?? false]))
  });
}

async function main() {
  const parsed = parseCapabilities(response({ recovery: true, semanticIndex: true }));
  assert.equal(parsed.capabilities.recovery, true);
  assert.equal(parsed.capabilities.languageServer, false);

  assert.throws(() => parseCapabilities("not json"), /invalid capability JSON/);
  assert.throws(() => parseCapabilities(JSON.stringify({ schema: "future/2" })), /Unsupported/);
  assert.throws(
    () => parseCapabilities(JSON.stringify({ schema, positionEncodings: ["utf-8"], capabilities: {} })),
    /UTF-16/
  );

  const candidates = executableCandidates("./custom/sagan", [path.resolve("workspace")], "linux");
  assert.equal(candidates[0], path.resolve("./custom/sagan"));
  assert.equal(candidates[1], path.join(path.resolve("workspace"), "bin", "sagan"));
  assert.equal(candidates.at(-1), "sagan");

  const servers = serverExecutableCandidates("./custom/sagan-lsp", "./compiler/sagan", [path.resolve("workspace")], "linux");
  assert.equal(servers[0], path.resolve("./custom/sagan-lsp"));
  assert.equal(servers[1], path.join(path.dirname(path.resolve("./compiler/sagan")), "sagan-lsp"));
  assert.equal(servers[2], path.join(path.resolve("workspace"), "bin", "sagan-lsp"));
  assert.equal(servers.at(-1), "sagan-lsp");
  assert.equal(selectServerExecutable([path.resolve("missing-sagan-lsp")], "", "linux"), undefined);
  assert.equal(selectServerExecutable([process.execPath]), process.execPath);
  assert.equal(executableOnPath(path.basename(process.execPath), path.dirname(process.execPath)), process.execPath);

  const attempts = [];
  const discovered = await discoverCompiler(["missing", "working"], async (candidate) => {
    attempts.push(candidate);
    if (candidate === "missing") throw new Error("unavailable");
    return parseCapabilities(response({ structuredDiagnostics: true }));
  });
  assert.deepEqual(attempts, ["missing", "working"]);
  assert.equal(discovered.executable, "working");
  assert.equal(discovered.capabilities.capabilities.structuredDiagnostics, true);
  assert.equal(discovered.failures.length, 1);

  console.log("All Sagan capability discovery tests passed.");
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
