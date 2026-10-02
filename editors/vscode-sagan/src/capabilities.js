"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { execFile } = require("node:child_process");

const schema = "sagan.language-service/1";
const capabilityNames = [
  "strictDocumentCheck",
  "structuredDiagnostics",
  "utf16Positions",
  "cancellation",
  "recovery",
  "documentOverlays",
  "semanticIndex",
  "languageServer",
  "nativeCheck",
  "recoveredFormatting",
  "rangeFormatting",
  "onTypeFormatting",
  "nativeBuild",
  "nativeRun",
  "testDocumentDiscovery",
  "testProjectDiscovery",
  "testDocumentRun",
  "testProjectRun",
  "testExplorer",
  "packageIndexReader",
  "packageQuery",
  "packageCatalog",
  "packageCompletion",
  "packageNavigation",
  "packageAutoImport",
  "operationTransport",
  "operationCancellation",
  "sourceMaps",
  "debugMetadata",
  "debugLaunchPlan",
  "debugAdapter",
  "debugLaunch",
  "debugAttach",
  "debugBreakpoints",
  "debugStepping",
  "debugVariables",
  "debugEvaluate",
  "debugExceptions",
  "optimizedLocalEvaluation"
];

const requiredCapabilityNames = capabilityNames.slice(0, 8);

function validateCapabilities(value) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error("Capability response must be a JSON object.");
  }
  if (value.schema !== schema) {
    throw new Error(`Unsupported Sagan language-service schema: ${String(value.schema)}`);
  }
  if (!Array.isArray(value.positionEncodings) || !value.positionEncodings.includes("utf-16")) {
    throw new Error("Sagan compiler does not advertise the required UTF-16 position encoding.");
  }
  if (!value.capabilities || typeof value.capabilities !== "object" || Array.isArray(value.capabilities)) {
    throw new Error("Capability response is missing its capabilities object.");
  }
  for (const name of requiredCapabilityNames) {
    if (typeof value.capabilities[name] !== "boolean") {
      throw new Error(`Capability ${name} must be a boolean.`);
    }
  }
  for (const name of capabilityNames.slice(requiredCapabilityNames.length)) {
    if (value.capabilities[name] === undefined) value.capabilities[name] = false;
    else if (typeof value.capabilities[name] !== "boolean") {
      throw new Error(`Capability ${name} must be a boolean.`);
    }
  }
  return value;
}

function parseCapabilities(text) {
  let value;
  try {
    value = JSON.parse(text);
  } catch (error) {
    throw new Error(`Sagan compiler returned invalid capability JSON: ${error.message}`);
  }
  return validateCapabilities(value);
}

function executableCandidates(configuredPath, workspaceFolders, platform = process.platform) {
  const executable = platform === "win32" ? "sagan.exe" : "sagan";
  const candidates = [];
  if (configuredPath && configuredPath.trim()) candidates.push(path.resolve(configuredPath.trim()));
  for (const folder of workspaceFolders) {
    candidates.push(path.join(folder, "bin", executable));
  }
  candidates.push("sagan");
  return [...new Set(candidates)];
}

function serverExecutableCandidates(configuredServerPath, configuredCompilerPath, workspaceFolders,
                                    platform = process.platform) {
  const serverName = platform === "win32" ? "sagan-lsp.exe" : "sagan-lsp";
  const candidates = [];
  if (configuredServerPath && configuredServerPath.trim()) {
    candidates.push(path.resolve(configuredServerPath.trim()));
  }
  if (configuredCompilerPath && configuredCompilerPath.trim()) {
    candidates.push(path.join(path.dirname(path.resolve(configuredCompilerPath.trim())), serverName));
  }
  for (const folder of workspaceFolders) candidates.push(path.join(folder, "bin", serverName));
  candidates.push("sagan-lsp");
  return [...new Set(candidates)];
}

function executableOnPath(command, envPath = process.env.PATH || "", platform = process.platform,
                          pathExtensions = process.env.PATHEXT || ".COM;.EXE;.BAT;.CMD") {
  const extensions = platform === "win32" && !path.extname(command)
    ? pathExtensions.split(";").filter(Boolean)
    : [""];
  for (const directory of envPath.split(path.delimiter).filter(Boolean)) {
    const cleanDirectory = directory.replace(/^"|"$/g, "");
    for (const extension of extensions) {
      const candidate = path.join(cleanDirectory, `${command}${extension.toLowerCase()}`);
      if (fs.existsSync(candidate)) return candidate;
      if (extension) {
        const originalCase = path.join(cleanDirectory, `${command}${extension}`);
        if (fs.existsSync(originalCase)) return originalCase;
      }
    }
  }
  return undefined;
}

function selectServerExecutable(candidates, envPath, platform = process.platform, pathExtensions) {
  for (const candidate of candidates) {
    if (path.isAbsolute(candidate)) {
      if (fs.existsSync(candidate)) return candidate;
      continue;
    }
    const resolved = executableOnPath(candidate, envPath, platform, pathExtensions);
    if (resolved) return resolved;
  }
  return undefined;
}

function runCapabilities(executable) {
  return new Promise((resolve, reject) => {
    execFile(executable, ["--capabilities-json"], { timeout: 5000, maxBuffer: 1024 * 1024, windowsHide: true },
      (error, stdout, stderr) => {
        if (error) {
          const detail = stderr.trim() || error.message;
          reject(new Error(detail));
          return;
        }
        try {
          resolve(parseCapabilities(stdout));
        } catch (parseError) {
          reject(parseError);
        }
      });
  });
}

async function discoverCompiler(candidates, probe = runCapabilities) {
  const failures = [];
  for (const executable of candidates) {
    if (path.isAbsolute(executable) && !fs.existsSync(executable)) {
      failures.push(`${executable}: not found`);
      continue;
    }
    try {
      return { executable, capabilities: await probe(executable), failures };
    } catch (error) {
      failures.push(`${executable}: ${error.message}`);
    }
  }
  return { executable: undefined, capabilities: undefined, failures };
}

module.exports = {
  capabilityNames,
  discoverCompiler,
  executableOnPath,
  executableCandidates,
  parseCapabilities,
  runCapabilities,
  selectServerExecutable,
  serverExecutableCandidates,
  schema,
  validateCapabilities
};
