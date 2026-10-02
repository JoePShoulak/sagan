"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const bundle = fs.readFileSync(path.resolve(__dirname, "..", "dist", "extension.js"), "utf8");
const extensionRoot = path.resolve(__dirname, "..");
const manifest = JSON.parse(fs.readFileSync(path.join(extensionRoot, "package.json"), "utf8"));
const language = manifest.contributes.languages.find((candidate) => candidate.id === "sagan");
const manifestLanguage = manifest.contributes.languages.find((candidate) => candidate.id === "sagan-manifest");
assert(bundle.includes("Sagan Language Server"), "bundle omitted the extension entry point");
assert(bundle.includes("vscode-languageclient"), "bundle omitted the standard VS Code language client");
assert(bundle.length > 100_000, "bundle is unexpectedly small and may be missing runtime dependencies");
assert(language && language.icon, "Sagan language contribution omitted its default file icon");
assert(manifestLanguage && manifestLanguage.filenames.includes("sagan.toml"),
  "Sagan manifest language contribution omitted sagan.toml");
for (const variant of ["light", "dark"]) {
  assert(fs.existsSync(path.resolve(extensionRoot, language.icon[variant])),
    `Sagan ${variant} file icon does not exist`);
  assert(fs.existsSync(path.resolve(extensionRoot, manifestLanguage.icon[variant])),
    `Sagan manifest ${variant} file icon does not exist`);
}
console.log("Sagan extension bundle integrity test passed.");
