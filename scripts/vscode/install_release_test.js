"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const { spawnSync } = require("node:child_process");
const {
  downloadAndUnzipVSCode,
  resolveCliArgsFromVSCodeExecutablePath
} = require("../../editors/vscode-sagan/node_modules/@vscode/test-electron");

async function main() {
  const [vsixArgument, expectedVersion] = process.argv.slice(2);
  assert(vsixArgument && expectedVersion,
    "usage: node install_release_test.js VSIX EXPECTED_VERSION");
  const repositoryRoot = path.resolve(__dirname, "..", "..");
  const vsix = path.resolve(vsixArgument);
  const smokeRoot = path.join(repositoryRoot, "build", "vsix-install-smoke");
  const extensionsDir = path.join(smokeRoot, "extensions");
  const userDataDir = path.join(smokeRoot, "user-data");
  fs.rmSync(smokeRoot, { recursive: true, force: true });
  fs.mkdirSync(extensionsDir, { recursive: true });
  fs.mkdirSync(userDataDir, { recursive: true });

  const executable = await downloadAndUnzipVSCode(process.env.VSCODE_TEST_VERSION || "stable");
  const [cli, ...baseArguments] = resolveCliArgsFromVSCodeExecutablePath(
    executable, { reuseMachineInstall: true });
  const common = [...baseArguments, "--extensions-dir", extensionsDir, "--user-data-dir", userDataDir];
  const command = process.platform === "win32" ? `"${cli}"` : cli;
  const options = {
    encoding: "utf8",
    shell: process.platform === "win32",
    windowsHide: true
  };
  const install = spawnSync(command, [...common, "--install-extension", vsix, "--force"], {
    ...options
  });
  assert.equal(install.status, 0,
    `VSIX installation failed: ${install.error || ""}\n${install.stdout || ""}${install.stderr || ""}`);

  const listed = spawnSync(command, [...common, "--list-extensions", "--show-versions"], options);
  assert.equal(listed.status, 0,
    `VSIX listing failed: ${listed.error || ""}\n${listed.stdout || ""}${listed.stderr || ""}`);
  const expected = `sagan-language.sagan-language@${expectedVersion}`;
  assert(listed.stdout.split(/\r?\n/).includes(expected),
    `isolated VS Code did not report ${expected}:\n${listed.stdout}`);
  console.log(`Isolated VSIX installation verified: ${expected}`);
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
