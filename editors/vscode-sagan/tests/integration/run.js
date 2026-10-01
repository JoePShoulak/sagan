"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { runTests } = require("@vscode/test-electron");

async function main() {
  const extensionDevelopmentPath = path.resolve(__dirname, "..", "..");
  const repositoryRoot = path.resolve(extensionDevelopmentPath, "..", "..");
  const serverName = process.platform === "win32" ? "sagan-lsp.exe" : "sagan-lsp";
  const serverPath = process.env.SAGAN_LSP_PATH || path.join(repositoryRoot, "bin", serverName);
  if (!fs.existsSync(serverPath)) {
    throw new Error(`Build the language server first or set SAGAN_LSP_PATH: ${serverPath}`);
  }

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
