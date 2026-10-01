"use strict";

const vscode = require("vscode");

function registerTesting(context, client, output, capabilities) {
  if (!capabilities.testDocumentDiscovery) return;
  const controller = vscode.tests.createTestController("saganTests", "Sagan Tests");
  context.subscriptions.push(controller);
  const byProtocolId = new Map();

  function replaceDocument(uri, tests) {
    const documentId = `document:${uri.toString()}`;
    controller.items.delete(documentId);
    for (const [id, item] of [...byProtocolId]) {
      if (item.uri?.toString() === uri.toString()) byProtocolId.delete(id);
    }
    if (!tests.length) return;
    const parent = controller.createTestItem(documentId,
      vscode.workspace.asRelativePath(uri, false), uri);
    controller.items.add(parent);
    for (const discovered of tests) {
      const itemUri = vscode.Uri.parse(discovered.uri);
      const item = controller.createTestItem(discovered.id, discovered.name, itemUri);
      item.range = new vscode.Range(discovered.range.start.line, discovered.range.start.character,
        discovered.range.end.line, discovered.range.end.character);
      parent.children.add(item);
      byProtocolId.set(discovered.id, item);
    }
  }

  async function discoverDocument(document) {
    if (document.languageId !== "sagan") return;
    const result = await client.sendRequest("sagan/tests/discover", {
      scope: "document",
      textDocument: { uri: document.uri.toString(), version: document.version }
    });
    replaceDocument(document.uri, result.tests || []);
  }

  async function discoverProject(folder) {
    const files = await vscode.workspace.findFiles(new vscode.RelativePattern(folder, "**/*.sagan"),
      "**/{.git,node_modules,build,Build}/**", 1);
    if (!files.length) return;
    const anchor = await vscode.workspace.openTextDocument(files[0]);
    const result = await client.sendRequest("sagan/tests/discover", {
      scope: "project",
      projectUri: folder.uri.toString(),
      textDocument: { uri: anchor.uri.toString(), version: anchor.version }
    });
    const grouped = new Map();
    for (const discovered of result.tests || []) {
      if (!grouped.has(discovered.uri)) grouped.set(discovered.uri, []);
      grouped.get(discovered.uri).push(discovered);
    }
    for (const [uri, tests] of grouped) replaceDocument(vscode.Uri.parse(uri), tests);
  }

  async function refresh() {
    if (capabilities.testProjectDiscovery && vscode.workspace.workspaceFolders?.length) {
      const existing = [];
      controller.items.forEach((item) => existing.push(item.id));
      existing.forEach((id) => controller.items.delete(id));
      byProtocolId.clear();
      await Promise.all(vscode.workspace.workspaceFolders.map(discoverProject));
      return;
    }
    const documents = await vscode.workspace.findFiles("**/*.sagan", "**/{.git,node_modules,build,Build}/**");
    await Promise.all(documents.map(async (uri) => discoverDocument(await vscode.workspace.openTextDocument(uri))));
  }
  controller.refreshHandler = refresh;

  if (capabilities.testDocumentRun) {
    const profile = controller.createRunProfile("Run", vscode.TestRunProfileKind.Run,
      async (request, token) => {
        const run = controller.createTestRun(request);
        const selected = [];
        const collect = (item) => {
          if (item.children.size === 0 && byProtocolId.has(item.id)) selected.push(item);
          else item.children.forEach(collect);
        };
        if (request.include?.length) request.include.forEach(collect);
        else controller.items.forEach(collect);
        const groups = new Map();
        for (const item of selected) {
          if (request.exclude?.includes(item)) continue;
          const key = item.uri.toString();
          if (!groups.has(key)) groups.set(key, []);
          groups.get(key).push(item);
          run.enqueued(item);
        }
        try {
          for (const [uri, items] of groups) {
            if (token.isCancellationRequested) break;
            const document = await vscode.workspace.openTextDocument(vscode.Uri.parse(uri));
            items.forEach((item) => run.started(item));
            const result = await client.sendRequest("sagan/tests/run", {
              textDocument: { uri, version: document.version },
              testIds: items.map((item) => item.id)
            }, token);
            for (const test of result.tests || []) {
              const item = byProtocolId.get(test.id);
              if (!item) continue;
              const duration = test.durationMilliseconds;
              if (test.stdout) run.appendOutput(test.stdout.replace(/\n/g, "\r\n"), undefined, item);
              if (test.stderr) run.appendOutput(test.stderr.replace(/\n/g, "\r\n"), undefined, item);
              if (test.state === "passed") run.passed(item, duration);
              else if (test.state === "skipped") run.skipped(item);
              else if (test.state === "cancelled") run.skipped(item);
              else {
                const message = new vscode.TestMessage(test.message || `Sagan test ${test.state}`);
                (test.state === "failed" ? run.failed : run.errored).call(run, item, message, duration);
              }
            }
          }
        } catch (error) {
          for (const item of selected) run.errored(item, new vscode.TestMessage(error.message));
          output.appendLine(`Sagan test run failed: ${error.message}`);
        } finally { run.end(); }
      });
    context.subscriptions.push(profile);
  }

  context.subscriptions.push(vscode.workspace.onDidOpenTextDocument((document) => {
    void discoverDocument(document).catch((error) => output.appendLine(`Test discovery failed: ${error.message}`));
  }));
  context.subscriptions.push(vscode.workspace.onDidSaveTextDocument((document) => {
    void discoverDocument(document).catch((error) => output.appendLine(`Test discovery failed: ${error.message}`));
  }));
  void refresh().catch((error) => output.appendLine(`Test discovery failed: ${error.message}`));
}

module.exports = { registerTesting };
