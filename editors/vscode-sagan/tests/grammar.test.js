const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const textmate = require("vscode-textmate");
const oniguruma = require("vscode-oniguruma");

const extensionRoot = path.resolve(__dirname, "..");
const repositoryRoot = path.resolve(extensionRoot, "..", "..");
const grammarPath = path.join(extensionRoot, "syntaxes", "sagan.tmLanguage.json");

async function loadGrammar() {
  const wasm = fs.readFileSync(require.resolve("vscode-oniguruma/release/onig.wasm"));
  await oniguruma.loadWASM(wasm.buffer.slice(wasm.byteOffset, wasm.byteOffset + wasm.byteLength));

  const registry = new textmate.Registry({
    onigLib: Promise.resolve({
      createOnigScanner: (patterns) => new oniguruma.OnigScanner(patterns),
      createOnigString: (source) => new oniguruma.OnigString(source)
    }),
    loadGrammar: async (scopeName) => {
      if (scopeName !== "source.sagan") return null;
      return textmate.parseRawGrammar(fs.readFileSync(grammarPath, "utf8"), grammarPath);
    }
  });

  return registry.loadGrammar("source.sagan");
}

function tokenize(grammar, source) {
  let ruleStack = textmate.INITIAL;
  const result = [];

  for (const [lineNumber, line] of source.split(/\r?\n/).entries()) {
    const tokenized = grammar.tokenizeLine(line, ruleStack);
    ruleStack = tokenized.ruleStack;
    for (const token of tokenized.tokens) {
      result.push({
        line: lineNumber + 1,
        text: line.slice(token.startIndex, token.endIndex),
        scopes: token.scopes
      });
    }
  }

  return result;
}

function readFixture(name) {
  return fs.readFileSync(path.join(__dirname, "fixtures", name), "utf8");
}

function assertScoped(tokens, text, scope) {
  const matches = tokens.filter((token) => token.text === text || token.text.includes(text));
  assert(matches.length > 0, `Expected token containing ${JSON.stringify(text)}`);
  assert(
    matches.some((token) => token.scopes.some((candidate) => candidate === scope || candidate.startsWith(`${scope}.`))),
    `Expected ${JSON.stringify(text)} to include ${scope}; got ${JSON.stringify(matches)}`
  );
}

function assertNeverScoped(tokens, text, scopeFragment) {
  const matches = tokens.filter((token) => token.text === text || token.text.includes(text));
  assert(matches.length > 0, `Expected token containing ${JSON.stringify(text)}`);
  assert(
    matches.every((token) => token.scopes.every((scope) => !scope.includes(scopeFragment))),
    `Expected ${JSON.stringify(text)} not to use a ${scopeFragment} scope; got ${JSON.stringify(matches)}`
  );
}

async function main() {
  const grammar = await loadGrammar();
  assert(grammar, "Sagan grammar did not load");

  const vocabulary = tokenize(grammar, readFixture("vocabulary.sagan"));
  const expectedScopes = {
    let: "keyword.declaration.variable.sagan",
    hidden: "variable.other.definition.sagan",
    fun: "keyword.declaration.function.sagan",
    new: "keyword.declaration.constructor.sagan",
    class: "keyword.declaration.type.sagan",
    face: "keyword.declaration.type.sagan",
    enum: "keyword.declaration.type.enum.sagan",
    module: "keyword.declaration.module.sagan",
    import: "keyword.control.import.sagan",
    from: "keyword.control.import.from.sagan",
    as: "keyword.control.import.as.sagan",
    export: "keyword.control.export.sagan",
    if: "keyword.control.conditional.sagan",
    else: "keyword.control.conditional.sagan",
    match: "keyword.control.conditional.sagan",
    case: "keyword.control.conditional.sagan",
    for: "keyword.control.loop.sagan",
    in: "keyword.control.loop.sagan",
    while: "keyword.control.loop.sagan",
    until: "keyword.control.loop.sagan",
    break: "keyword.control.loop.sagan",
    continue: "keyword.control.loop.sagan",
    return: "keyword.control.flow.sagan",
    yield: "keyword.control.flow.sagan",
    hope: "keyword.control.exception.sagan",
    unless: "keyword.control.exception.sagan",
    finally: "keyword.control.exception.sagan",
    scream: "keyword.control.exception.sagan",
    and: "keyword.operator.logical.sagan",
    or: "keyword.operator.logical.sagan",
    not: "keyword.operator.logical.sagan",
    is: "keyword.operator.type.sagan",
    has: "keyword.operator.type.sagan",
    self: "variable.language.self.sagan",
    true: "constant.language.boolean.sagan",
    false: "constant.language.boolean.sagan",
    inf: "constant.language.numeric.sagan",
    nan: "constant.language.numeric.sagan"
  };

  for (const [word, scope] of Object.entries(expectedScopes)) assertScoped(vocabulary, word, scope);
  for (const operator of ["...", "?.", ":=", "=>", "==", "!=", "<=", ">=", "++", "--", "+=", "-=", "*=", "/=", "%=", "^="])
    assertScoped(vocabulary, operator, "keyword.operator");

  const strings = tokenize(grammar, readFixture("strings.sagan"));
  assertScoped(strings, "\\u{1F680}", "constant.character.escape.sagan");
  assertScoped(strings, "${", "punctuation.section.interpolation.begin.sagan");
  assertScoped(strings, "call", "entity.name.function.call.sagan");
  assertScoped(strings, "${literal}\\n", "string.quoted.double.raw.sagan");
  assertNeverScoped(strings, "literal", "variable.other.sagan");

  const unicode = tokenize(grammar, readFixture("unicode.sagan"));
  for (const identifier of ["Δx", "変数", "🚀", "🌌distance", "calculate🪐", "速度"])
    assertScoped(unicode, identifier, identifier === "calculate🪐" ? "entity.name.function.sagan" : "variable");

  const comments = tokenize(grammar, readFixture("comments.sagan"));
  assertScoped(comments, "Documentation line", "comment.line.documentation.sagan");
  assertScoped(comments, "Documentation block", "comment.block.documentation.sagan");
  assertScoped(comments, "outer continues", "comment.block.sagan");
  assertScoped(comments, "ready", "variable.other.enummember.sagan");

  const malformed = tokenize(grammar, readFixture("malformed.sagan"));
  assertNeverScoped(malformed, "@", "tag");
  assertNeverScoped(malformed, "`", "string");
  assertScoped(malformed, "!", "keyword.operator.logical.sagan");
  assertNeverScoped(malformed, "event", "event");
  assert(!malformed.some((token) => [".5", "5."].includes(token.text) && token.scopes.includes("constant.numeric.float.sagan")));
  assertScoped(malformed, "5", "constant.numeric.integer.sagan");

  const parserDemo = fs.readFileSync(path.join(repositoryRoot, "examples", "parser_demo.sagan"), "utf8");
  const parserTokens = tokenize(grammar, parserDemo);
  assertScoped(parserTokens, "yield", "keyword.control.flow.sagan");
  assertScoped(parserTokens, "ready", "variable.other.enummember.sagan");
  assertScoped(parserTokens, "NetworkError", "variable.other.sagan");
  assertScoped(parserTokens, "flush!", "entity.name.function.mutating.sagan");
  assert(parserTokens.every((token) => token.scopes.every((scope) => !scope.startsWith("invalid.illegal"))), "Parser demo contains invalid scopes");

  console.log("All Sagan TextMate grammar tests passed.");
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
