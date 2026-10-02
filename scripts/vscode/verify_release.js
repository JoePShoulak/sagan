"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const [manifestPath, expectedVersion] = process.argv.slice(2);
assert(manifestPath && expectedVersion, "usage: node verify_release.js MANIFEST EXPECTED_VERSION");

const extensionRoot = path.dirname(path.resolve(manifestPath));
const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
assert.equal(manifest.version, expectedVersion, "VSIX manifest version does not match its filename");
assert.equal(manifest.main, "./dist/extension.js", "VSIX manifest has an unexpected entry point");

for (const relative of [
  "dist/extension.js",
  "language-configuration.json",
  "syntaxes/sagan.tmLanguage.json",
  "images/sagan-logo.png",
  "examples/demo.sagan",
  "LICENSE.txt",
  "README.md"
]) {
  assert(fs.existsSync(path.join(extensionRoot, relative)), `VSIX omitted ${relative}`);
}

const language = manifest.contributes.languages.find((candidate) => candidate.id === "sagan");
assert(language && language.extensions.includes(".sagan"), "VSIX omitted the .sagan language association");
assert(language.icon && language.icon.light && language.icon.dark, "VSIX omitted the Sagan file icon");
const grammar = manifest.contributes.grammars.find((candidate) => candidate.language === "sagan");
assert(grammar && grammar.scopeName === "source.sagan", "VSIX omitted the Sagan TextMate grammar");

console.log("Sagan VSIX manifest and packaged-content checks passed.");
