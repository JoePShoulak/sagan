"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { spawnSync } = require("node:child_process");
const { runTests } = require("@vscode/test-electron");

function verifyPackagedDemo(extensionDevelopmentPath, compilerPath) {
  const demoPath = path.join(extensionDevelopmentPath, "examples", "demo.sagan");
  const result = spawnSync(compilerPath, ["--diagnostics-json", demoPath], {
    encoding: "utf8"
  });
  if (result.status !== 0) {
    throw new Error(`Bundled demo failed compiler validation:\n${result.stdout}${result.stderr}`);
  }

  const report = JSON.parse(result.stdout);
  if (report.state !== "complete" || !Array.isArray(report.diagnostics) || report.diagnostics.length !== 0) {
    throw new Error(`Bundled demo is not diagnostic-free: ${result.stdout}`);
  }
}

async function main() {
  const extensionDevelopmentPath = path.resolve(__dirname, "..", "..");
  const repositoryRoot = path.resolve(extensionDevelopmentPath, "..", "..");
  const serverName = process.platform === "win32" ? "sagan-lsp.exe" : "sagan-lsp";
  const compilerName = process.platform === "win32" ? "sagan.exe" : "sagan";
  const serverPath = process.env.SAGAN_LSP_PATH || path.join(repositoryRoot, "bin", serverName);
  const compilerPath = process.env.SAGAN_COMPILER_PATH || path.join(repositoryRoot, "bin", compilerName);
  if (!fs.existsSync(serverPath)) {
    throw new Error(`Build the language server first or set SAGAN_LSP_PATH: ${serverPath}`);
  }
  if (!fs.existsSync(compilerPath)) {
    throw new Error(`Build the compiler first or set SAGAN_COMPILER_PATH: ${compilerPath}`);
  }

  verifyPackagedDemo(extensionDevelopmentPath, compilerPath);

  await runTests({
    extensionDevelopmentPath,
    extensionTestsPath: path.join(__dirname, "suite.js"),
    launchArgs: [path.join(__dirname, "workspace"), "--disable-extensions"],
    extensionTestsEnv: { SAGAN_LSP_PATH: serverPath },
    version: process.env.VSCODE_TEST_VERSION || "stable"
  });
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
