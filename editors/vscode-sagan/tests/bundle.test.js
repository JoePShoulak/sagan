"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const bundle = fs.readFileSync(path.resolve(__dirname, "..", "dist", "extension.js"), "utf8");
assert(bundle.includes("Sagan Language Server"), "bundle omitted the extension entry point");
assert(bundle.includes("vscode-languageclient"), "bundle omitted the standard VS Code language client");
assert(bundle.length > 100_000, "bundle is unexpectedly small and may be missing runtime dependencies");
console.log("Sagan extension bundle integrity test passed.");
