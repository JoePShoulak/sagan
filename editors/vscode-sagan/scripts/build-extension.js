"use strict";

const path = require("node:path");
const esbuild = require("esbuild");

const root = path.resolve(__dirname, "..");

esbuild.buildSync({
  absWorkingDir: root,
  bundle: true,
  entryPoints: [path.join(root, "src", "extension.js")],
  external: ["vscode"],
  format: "cjs",
  outfile: path.join(root, "dist", "extension.js"),
  platform: "node"
});
