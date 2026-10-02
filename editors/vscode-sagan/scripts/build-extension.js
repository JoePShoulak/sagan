"use strict";

const path = require("node:path");
const esbuild = require("esbuild");

const root = path.resolve(__dirname, "..");

esbuild.buildSync({
  absWorkingDir: root,
  bundle: true,
  entryPoints: ["./src/extension.js"],
  external: ["vscode"],
  format: "cjs",
  outfile: "./dist/extension.js",
  platform: "node"
});
