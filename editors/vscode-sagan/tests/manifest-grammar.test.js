"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const textmate = require("vscode-textmate");
const oniguruma = require("vscode-oniguruma");

const extensionRoot = path.resolve(__dirname, "..");

async function main() {
  const wasm = fs.readFileSync(require.resolve("vscode-oniguruma/release/onig.wasm"));
  await oniguruma.loadWASM(wasm.buffer.slice(wasm.byteOffset, wasm.byteOffset + wasm.byteLength));
  const registry = new textmate.Registry({
    onigLib: Promise.resolve({
      createOnigScanner: (sources) => new oniguruma.OnigScanner(sources),
      createOnigString: (source) => new oniguruma.OnigString(source)
    }),
    loadGrammar: async (scopeName) => {
      if (scopeName !== "source.sagan-manifest") return null;
      return textmate.parseRawGrammar(
        fs.readFileSync(path.join(extensionRoot, "syntaxes", "sagan-toml.tmLanguage.json"), "utf8"),
        "sagan-toml.tmLanguage.json");
    }
  });
  const grammar = await registry.loadGrammar("source.sagan-manifest");
  assert(grammar, "Sagan manifest grammar did not load");

  const source = fs.readFileSync(path.join(__dirname, "fixtures", "sagan.toml"), "utf8");
  const tokens = [];
  let stack = textmate.INITIAL;
  for (const line of source.split(/\r?\n/)) {
    const result = grammar.tokenizeLine(line, stack);
    stack = result.ruleStack;
    for (const token of result.tokens) {
      tokens.push({ text: line.slice(token.startIndex, token.endIndex), scopes: token.scopes });
    }
  }
  const scoped = (text, scope) => tokens.some((token) => token.text.trim() === text &&
    token.scopes.some((candidate) => candidate === scope || candidate.startsWith(`${scope}.`)));

  assert(scoped("package", "entity.name.section.package.sagan-manifest"));
  assert(scoped("application", "entity.name.section.application.sagan-manifest"));
  assert(scoped("dependencies", "entity.name.section.dependencies.sagan-manifest"));
  assert(scoped("name", "support.type.property-name.package.sagan-manifest"));
  assert(scoped("mode", "support.type.property-name.application.sagan-manifest"));
  assert(scoped("render", "entity.name.namespace.dependency.sagan-manifest"));
  assert(scoped("package", "support.type.property-name.dependency.sagan-manifest"));
  assert(tokens.some((token) => token.text.includes("Representative Sagan package manifest") &&
    token.scopes.includes("comment.line.number-sign.sagan-manifest")));
  assert(tokens.every((token) => token.scopes.every((scope) => !scope.startsWith("invalid.illegal"))),
    "Valid Sagan manifest received an invalid scope");

  console.log("All Sagan manifest TextMate grammar tests passed.");
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
