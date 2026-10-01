"use strict";

const path = require("node:path");
const { spawnSync } = require("node:child_process");

for (const test of ["grammar.test.js", "capabilities.test.js", "operations.test.js",
  "testing.test.js", "extension.test.js"]) {
  const result = spawnSync(process.execPath, [path.join(__dirname, test)], { stdio: "inherit" });
  if (result.status !== 0) process.exit(result.status ?? 1);
}
