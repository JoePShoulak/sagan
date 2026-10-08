#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
binary="./bin/sagan"
mkdir -p build build/tmp
if command -v cygpath >/dev/null 2>&1; then
  repo_tmp_native="$(cygpath -w "$repo_root/build/tmp")"
else
  repo_tmp_native="$repo_root/build/tmp"
fi
export TMPDIR="$repo_tmp_native" TMP="$repo_tmp_native" TEMP="$repo_tmp_native"
work_dir="$(mktemp -d build/cli-test.XXXXXXXXXX)"

cleanup() {
  rm -rf "$work_dir"
}
trap cleanup EXIT

expect_output() {
  local name="$1"
  local expected="$2"
  shift 2
  local output
  output="$("$@" 2>&1)" || {
    echo "[FAIL] $name: command failed" >&2
    echo "$output" >&2
    return 1
  }
  grep -Fq "$expected" <<<"$output" || {
    echo "[FAIL] $name: expected output containing '$expected'" >&2
    echo "$output" >&2
    return 1
  }
  echo "[PASS] $name"
}

expect_failure() {
  local name="$1"
  local expected_status="$2"
  local expected="$3"
  shift 3
  local output
  local status
  set +e
  output="$("$@" 2>&1)"
  status=$?
  set -e
  if [[ "$status" -ne "$expected_status" ]]; then
    echo "[FAIL] $name: expected status $expected_status, got $status" >&2
    echo "$output" >&2
    return 1
  fi
  grep -Fq "$expected" <<<"$output" || {
    echo "[FAIL] $name: expected output containing '$expected'" >&2
    echo "$output" >&2
    return 1
  }
  echo "[PASS] $name"
}

expect_output "default tokenizer input" "tokenizer: tests/fixtures/syntax/tokenizer.sagan" "$binary"
expect_output "version output" "Sagan " "$binary" --version
expect_output "language-service capability discovery" '"schema":"sagan.language-service/1"' \
  "$binary" --capabilities-json
expect_output "Windows application-icon contract" "sagan-resource.o" \
  "$binary" --application-icon windows
expect_output "Linux application-icon contract" "sagan.png" \
  "$binary" --application-icon linux
expect_output "macOS application-icon contract" "sagan.icns" \
  "$binary" --application-icon macos
expect_failure "unsupported application-icon platform" 1 \
  "expected windows, linux, or macos" "$binary" --application-icon plan9
expect_failure "missing application-icon diagnostic" 1 \
  "rebuild or reinstall the Sagan toolchain with application assets" \
  env SAGAN_TOOLCHAIN_ROOT="$work_dir/no-icons" "$binary" --application-icon linux
expect_output "explicit tokenizer input" "KWD_LET" "$binary" --tokens tests/fixtures/syntax/tokenizer.sagan
printf '%s\n' 'let escaped = "\r\t\0"' > "$work_dir/escaped.sagan"
expect_output "escaped token text" '\r\t\0' "$binary" --tokens "$work_dir/escaped.sagan"
expect_output "text AST output" "Program" "$binary" --ast examples/ast.sagan
expect_output "DOT AST output" "digraph SaganAST" "$binary" --ast-dot examples/ast.sagan
expect_output "semantic model output" "SemanticModel" \
  "$binary" --semantic tests/fixtures/semantic/scopes.sagan
expect_output "type model output" "TypeModel" \
  "$binary" --types tests/fixtures/semantic/types.sagan
expect_output "structured diagnostic success" '"state":"complete","diagnostics":[]' \
  "$binary" --diagnostics-json tests/fixtures/semantic/scopes.sagan
expect_failure "structured lexical diagnostic" 1 '"code":"SAG-LEX-0001"' \
  "$binary" --diagnostics-json tests/fixtures/syntax/tokenizer_error.sagan
expect_failure "structured syntax diagnostic" 1 '"code":"SAG-SYN-0001"' \
  "$binary" --diagnostics-json tests/fixtures/syntax/parser_error.sagan
recovery_output="$($binary --diagnostics-json tests/fixtures/syntax/editor_recovery.sagan 2>&1 || true)"
recovery_count="$(grep -o '"code":"SAG-SYN-0001"' <<<"$recovery_output" | wc -l | tr -d ' ')"
[[ "$recovery_count" -eq 2 ]] || {
  echo "[FAIL] recovered structured diagnostics: expected 2 syntax diagnostics, got $recovery_count" >&2
  echo "$recovery_output" >&2
  exit 1
}
grep -Fq '"state":"recovered"' <<<"$recovery_output"
echo "[PASS] recovered structured diagnostics"
limited_errors="$(SAGAN_MAX_ERRORS=1 "$binary" tests/fixtures/syntax/editor_recovery.sagan 2>&1 || true)"
grep -Fq "1 more errors omitted" <<<"$limited_errors"
echo "[PASS] terminal diagnostic limit"
expect_failure "structured type diagnostic" 1 '"code":"SAG-TYP-0001"' \
  "$binary" --diagnostics-json tests/fixtures/semantic/type_error.sagan
expect_output "weak ownership cycle type model" "TypeModel" \
  "$binary" --types tests/fixtures/semantic/type_weak_ownership_cycle.sagan
expect_output "root-file validation" "executable root is valid" \
  "$binary" --entry tests/fixtures/semantic/entry.sagan
expect_output "module graph resolution" "Import(course from guidance as calculate_course)" \
  "$binary" --modules tests/fixtures/modules/module_demo/main.sagan
expect_output "linked module C++ output" "Cross-module answer" \
  "$binary" --emit-cpp-modules tests/fixtures/modules/module_demo/main.sagan
expect_output "package graph resolution" "Package(mission-control 0.1.0)" \
  "$binary" --package examples/package
expect_output "package application mode" "ApplicationMode(console)" \
  "$binary" --package examples/package
expect_output "package application mode query" "console" \
  "$binary" --application-mode examples/package
expect_output "loose source application mode default" "console" \
  "$binary" --application-mode tests/fixtures/runtime/smoke.sagan
expect_output "package auto-discovery from entry" "Package(mission-control 0.1.0)" \
  "$binary" --modules examples/package/src/main.sagan
expect_output "package manifest path resolution" "Module(navigation.guidance" \
  "$binary" --package examples/package/sagan.toml
expect_output "linked package C++ output" "Package answer" \
  "$binary" --emit-cpp-package examples/package
expect_output "package C++ output file" "Wrote linked generated C++" \
  "$binary" --emit-cpp-package examples/package "$work_dir/package.cpp"
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror "$work_dir/package.cpp" -o "$work_dir/package"
package_output="$($work_dir/package)"
grep -Fq "Package answer: 42" <<<"$package_output"
grep -Fq "Package signal: nominal" <<<"$package_output"
expect_output "direct Sagan execution" "The answer from" \
  "$binary" examples/showcase.sagan
expect_failure "source-mapped runtime overflow" 1 "error[SAG-RUN-0101]: Sagan integer addition overflow (127 + 1 cannot fit)" \
  "$binary" tests/fixtures/runtime/source_trace_error.sagan
trace_output="$($binary tests/fixtures/runtime/source_trace_error.sagan 2>&1 || true)"
grep -Fq "note: in overflowing at tests/fixtures/runtime/source_trace_error.sagan" <<<"$trace_output"
grep -Fq "note: in caller at tests/fixtures/runtime/source_trace_error.sagan" <<<"$trace_output"
echo "[PASS] Sagan call trace identifies both frames"
expect_failure "source-mapped assertion" 1 "error[SAG-RUN-0200]: orbit escaped" \
  "$binary" tests/fixtures/runtime/assert_fail.sagan
expect_failure "unhandled scream uses its message" 1 "error[SAG-RUN-0100]: launch aborted by guidance" \
  "$binary" tests/fixtures/runtime/source_scream_error.sagan
expect_output "direct package execution" "Package answer: 42" \
  "$binary" --run-package examples/package
expect_output "Cartesian and spherical geometry execution" "Spherical vector: s<2, 0.25, 0.75>" \
  "$binary" examples/geometry.sagan
expect_output "package entry execution with import linking" "Package answer: 42" \
  "$binary" examples/package/src/main.sagan
printf '%s\n' 'print("exit seven")' 'exit(7)' > "$work_dir/exit-seven.sagan"
expect_failure "native exit-code propagation" 7 "exit seven" \
  "$binary" "$work_dir/exit-seven.sagan"
expect_failure "native compiler failure" 1 "error[SAG-BLD-0001]: Native C++ compilation failed" \
  env CXX=false "$binary" tests/fixtures/semantic/entry.sagan
expect_output "C++ output" "Generated by Sagan" \
  "$binary" --emit-cpp examples/showcase.sagan
expect_output "C++ output file" "Wrote generated C++" \
  "$binary" --emit-cpp examples/showcase.sagan "$work_dir/generated.cpp"
expect_output "runtime-error C++ output file" "Wrote generated C++" \
  "$binary" --emit-cpp tests/fixtures/runtime/runtime_errors.sagan "$work_dir/runtime-error.cpp"
expect_output "generic C++ output file" "Wrote generated C++" \
  "$binary" --emit-cpp tests/fixtures/runtime/generic_sum.sagan "$work_dir/generics.cpp"
grep -Fq "template <typename sagan_54>" "$work_dir/generics.cpp"
grep -Fq "sagan_526573756c74::Tag::sagan_4661696c757265" "$work_dir/generics.cpp"
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror "$work_dir/generics.cpp" -o "$work_dir/generics"
generic_output="$($work_dir/generics)"
grep -Fq "Result: 42" <<<"$generic_output"
grep -Fq "Error: guidance unavailable" <<<"$generic_output"
expect_output "generic class C++ output file" "Wrote generated C++" \
  "$binary" --emit-cpp tests/fixtures/runtime/generic_class.sagan "$work_dir/generic-class.cpp"
grep -Fq "template <typename sagan_54>" "$work_dir/generic-class.cpp"
grep -Fq "std::make_shared<sagan_426f78<std::int8_t>>" "$work_dir/generic-class.cpp"
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror "$work_dir/generic-class.cpp" -o "$work_dir/generic-class"
generic_class_output="$($work_dir/generic-class)"
grep -Fq "Number box through Readable<Int8>: 42" <<<"$generic_class_output"
grep -Fq "Readable value: 42" <<<"$generic_class_output"
grep -Fq "Explicit generic method: echo online" <<<"$generic_class_output"
grep -Fq "Constrained generic function: Readable value: 42" <<<"$generic_class_output"
grep -Fq "Updated number: 43" <<<"$generic_class_output"
grep -Fq "int main()" "$work_dir/generated.cpp"
grep -Fq "enum class sagan_4c61756e6368537461747573" "$work_dir/generated.cpp"
grep -Fq "sagan_4c61756e6368537461747573::sagan_7265616479" "$work_dir/generated.cpp"
grep -Fq "private:" "$work_dir/generated.cpp"
grep -Fq "sagan_726561645f76616c7565" "$work_dir/generated.cpp"
grep -Fq "sagan_6e657874()" "$work_dir/generated.cpp"
grep -Fq "SetConsoleOutputCP(CP_UTF8)" "$work_dir/generated.cpp"
grep -Fq "std::vector<std::int8_t>" "$work_dir/generated.cpp"
grep -Fq "for (auto" "$work_dir/generated.cpp"
grep -Fq ".at(2)" "$work_dir/generated.cpp"
grep -Fq "sagan_stringify" "$work_dir/generated.cpp"
grep -Fq "while (" "$work_dir/generated.cpp"
grep -Fq "sagan_subtract_assign<std::int64_t>" "$work_dir/generated.cpp"
grep -Fq "std::unordered_map<std::string, std::int8_t>" "$work_dir/generated.cpp"
grep -Fq "sagan_array_0.push_back" "$work_dir/generated.cpp"
grep -Fq "sagan_spread_0_0" "$work_dir/generated.cpp"
grep -Fq "sagan_spread_0_2" "$work_dir/generated.cpp"
grep -Fq "sagan_dictionary_1.insert_or_assign" "$work_dir/generated.cpp"
grep -Fq "sagan_dictionary_spread_1_0" "$work_dir/generated.cpp"
grep -Fq "sagan_increment<std::int64_t>(sagan_box_value(sagan_6c61756e63685f6e756d626572), true)" "$work_dir/generated.cpp"
grep -Fq "sagan_increment<std::int64_t>(sagan_box_value(sagan_6c61756e63685f6e756d626572), false)" "$work_dir/generated.cpp"
grep -Fq "sagan_power<std::int8_t>(3, 2)" "$work_dir/generated.cpp"
grep -Fq "sagan_power<double>(2.0, static_cast<std::int8_t>(-1))" "$work_dir/generated.cpp"
grep -Fq "sagan_power_assign<std::int64_t>" "$work_dir/generated.cpp"
grep -Fq "sagan_add_assign<std::int64_t>" "$work_dir/generated.cpp"
grep -Fq "sagan_multiply_assign<std::int64_t>" "$work_dir/generated.cpp"
grep -Fq "sagan_divide_assign<std::int64_t>" "$work_dir/generated.cpp"
grep -Fq "sagan_modulo_assign<std::int64_t>" "$work_dir/generated.cpp"
grep -Fq "sagan_subtract_assign<std::int64_t>" "$work_dir/generated.cpp"
grep -Fq "sagan_multiply<std::int8_t>(6, 7)" "$work_dir/generated.cpp"
grep -Fq "sagan_modulo<double>(5.5, 2.0)" "$work_dir/generated.cpp"
grep -Fq "sagan_vector<double, 3>" "$work_dir/generated.cpp"
grep -Fq "sagan_point<double, 3>" "$work_dir/generated.cpp"
grep -Fq "sagan_dimensioned_spread_0_0" "$work_dir/generated.cpp"
grep -Fq "sagan_add<sagan_vector<double, 3>>" "$work_dir/generated.cpp"
grep -Fq "sagan_multiply<sagan_vector<double, 3>>(2.0" "$work_dir/generated.cpp"
grep -Fq "sagan_negate<sagan_vector<double, 3>>" "$work_dir/generated.cpp"
grep -Fq "[=](std::int64_t" "$work_dir/generated.cpp"
grep -Fq "sagan_box_value(sagan_6c61756e63685f7472616e73666f726d)(40)" "$work_dir/generated.cpp"
grep -Fq "struct sagan_4c61756e6368436f756e746572" "$work_dir/generated.cpp"
grep -Fq "(*this).sagan_76616c7565" "$work_dir/generated.cpp"
grep -Fq "sagan_696e6372656d656e7421()" "$work_dir/generated.cpp"
grep -Fq ".at(2)" "$work_dir/generated.cpp"
grep -Fq ".at(1)" "$work_dir/generated.cpp"
grep -Fq "const auto sagan_match_" "$work_dir/generated.cpp"
grep -Fq "else {" "$work_dir/generated.cpp"
grep -Fq "sagan_scream<std::string>" "$work_dir/generated.cpp"
grep -Fq "sagan_exception_matches<std::string>" "$work_dir/generated.cpp"
grep -Fq "sagan_make_finally" "$work_dir/generated.cpp"
grep -Fq "enum class sagan_runtime_error" "$work_dir/runtime-error.cpp"
grep -Fq "sagan_runtime_error::integer_overflow" "$work_dir/runtime-error.cpp"
grep -Fq "sagan_runtime_error::division_by_zero" "$work_dir/runtime-error.cpp"
grep -Fq "sagan_index(" "$work_dir/runtime-error.cpp"
grep -Fq "sagan_dictionary_at(" "$work_dir/runtime-error.cpp"

expect_output "SVG AST file" "Wrote SVG AST" \
  "$binary" --ast-svg examples/ast.sagan "$work_dir/parser.svg"
grep -Fq "<svg" "$work_dir/parser.svg"

expect_output "HTML AST file" "Wrote visual AST demo" \
  "$binary" --ast-html examples/ast.sagan "$work_dir/parser.html"
grep -Fq "Input source" "$work_dir/parser.html"

expect_failure "invalid arguments" 2 "usage: sagan" "$binary" --ast one.sagan extra.sagan
expect_failure "missing input file" 1 "Could not open" "$binary" "$work_dir/missing.sagan"
expect_failure "lexical diagnostic" 1 "error[SAG-LEX-0001]:" "$binary" --tokens tests/fixtures/syntax/tokenizer_error.sagan
expect_failure "syntax diagnostic" 1 "error[SAG-SYN-0001]:" "$binary" --ast tests/fixtures/syntax/parser_error.sagan
expect_failure "undefined-name semantic diagnostic" 1 "Undefined name 'missing_value'" \
  "$binary" --semantic tests/fixtures/semantic/semantic_undefined_error.sagan
expect_failure "duplicate-name semantic diagnostic" 1 "Duplicate declaration of 'repeated'" \
  "$binary" --semantic tests/fixtures/semantic/semantic_duplicate_error.sagan
expect_failure "type diagnostic" 1 "Function return requires Bool, but received Int" \
  "$binary" --types tests/fixtures/semantic/type_error.sagan
expect_failure "uninferred variable diagnostic" 1 "requires a type annotation or initializer" \
  "$binary" --types tests/fixtures/semantic/type_uninferred_error.sagan
expect_failure "heterogeneous array diagnostic" 1 "Array elements have incompatible types" \
  "$binary" --types tests/fixtures/semantic/type_array_error.sagan
expect_failure "heterogeneous dictionary diagnostic" 1 "Dictionary values have incompatible types" \
  "$binary" --types tests/fixtures/semantic/type_dictionary_error.sagan
expect_failure "vector component diagnostic" 1 "Vector components must be numeric" \
  "$binary" --types tests/fixtures/semantic/type_vector_error.sagan
expect_failure "annotation type diagnostic" 1 "does not name a type" \
  "$binary" --types tests/fixtures/semantic/type_annotation_error.sagan
expect_failure "definite return diagnostic" 1 "may reach the end without returning Int64" \
  "$binary" --types tests/fixtures/semantic/type_return_error.sagan
expect_output "declaration-only root is executable" "executable root is valid" \
  "$binary" --entry tests/fixtures/semantic/entry_missing_error.sagan
expect_failure "uninitialized variable diagnostic" 1 "is used before initialization" \
  "$binary" --types tests/fixtures/semantic/type_uninitialized_error.sagan
expect_failure "unreachable statement diagnostic" 1 "Unreachable statement" \
  "$binary" --types tests/fixtures/semantic/type_unreachable_error.sagan
expect_failure "module cycle diagnostic" 1 "Cyclic module dependency: alpha -> beta -> alpha" \
  "$binary" --modules tests/fixtures/modules/module_cycle/alpha.sagan
expect_failure "missing module export diagnostic" 1 "does not export 'missing'" \
  "$binary" --modules tests/fixtures/modules/module_export_error/main.sagan
expect_failure "module name diagnostic" 1 "declares 'wrong_name', expected 'main'" \
  "$binary" --modules tests/fixtures/modules/module_name_error/main.sagan
expect_failure "missing module file diagnostic" 1 "Could not open module" \
  "$binary" --modules tests/fixtures/modules/module_missing/main.sagan
expect_failure "missing module declaration diagnostic" 1 "must declare 'module support'" \
  "$binary" --modules tests/fixtures/modules/module_declaration_error/main.sagan
expect_failure "undefined module export diagnostic" 1 "exports undefined declaration 'missing'" \
  "$binary" --modules tests/fixtures/modules/module_undefined_export_error/main.sagan
expect_failure "duplicate module export diagnostic" 1 "exports duplicate public name 'value'" \
  "$binary" --modules tests/fixtures/modules/module_duplicate_export_error/main.sagan
expect_failure "module extension diagnostic" 1 "must use the .sagan extension" \
  "$binary" --modules README.md
expect_failure "linked module type diagnostic" 1 "No matching overload for 'guidance__calculate'" \
  "$binary" --emit-cpp-modules tests/fixtures/modules/module_type_error/main.sagan
expect_failure "module namespace visibility diagnostic" 1 "Module 'support' does not export 'private_value'" \
  "$binary" --emit-cpp-modules tests/fixtures/modules/module_namespace_error/main.sagan
mkdir -p "$work_dir/no-package" "$work_dir/package-invalid/src" "$work_dir/package-missing-entry/src" \
  "$work_dir/package-errors/src"
printf '%s\n' '[package]' 'name = "invalid"' 'version = "one"' 'source = "src"' 'entry = "main"' \
  > "$work_dir/package-invalid/sagan.toml"
printf '%s\n' '[package]' 'name = "missing-entry"' 'version = "1.0.0"' 'source = "src"' 'entry = "main"' \
  > "$work_dir/package-missing-entry/sagan.toml"
expect_failure "missing package manifest diagnostic" 1 "Could not find package manifest" \
  "$binary" --package "$work_dir/no-package"
expect_failure "invalid package version diagnostic" 1 "Package version must use MAJOR.MINOR.PATCH" \
  "$binary" --package "$work_dir/package-invalid"
expect_failure "missing package entry diagnostic" 1 "Package entry module does not exist" \
  "$binary" --package "$work_dir/package-missing-entry"
write_manifest() {
  printf '%s\n' "$@" > "$work_dir/package-errors/sagan.toml"
}
write_manifest '[workspace]' 'name = "invalid"'
expect_failure "unknown package section diagnostic" 1 "Unknown package manifest section '[workspace]'" \
  "$binary" --package "$work_dir/package-errors"
write_manifest 'name = "invalid"'
expect_failure "unscoped package value diagnostic" 1 "must appear under [package]" \
  "$binary" --package "$work_dir/package-errors"
write_manifest '[package]' 'not a value'
expect_failure "malformed package line diagnostic" 1 "Invalid package manifest line" \
  "$binary" --package "$work_dir/package-errors"
write_manifest '[package]' 'name = valid'
expect_failure "unquoted package value diagnostic" 1 "must be a quoted string" \
  "$binary" --package "$work_dir/package-errors"
write_manifest '[package]' 'license = "GPL-3.0"'
expect_failure "unknown package key diagnostic" 1 "Unknown package manifest key 'license'" \
  "$binary" --package "$work_dir/package-errors"
write_manifest '[package]' 'name = "valid"' 'version = "1.0.0"' 'source = "src"' 'entry = "main"' \
  '[application]' 'mode = "windowed"'
expect_output "windowed package application mode" "windowed" \
  "$binary" --application-mode "$work_dir/package-errors"
write_manifest '[package]' 'name = "valid"' 'version = "1.0.0"' 'source = "src"' 'entry = "main"' \
  '[application]' 'mode = "silent"'
expect_failure "invalid application mode diagnostic" 1 "Application mode must be 'console' or 'windowed'" \
  "$binary" --application-mode "$work_dir/package-errors"
write_manifest '[package]' 'name = "valid"' 'version = "1.0.0"' 'source = "src"' 'entry = "main"' \
  '[application]' 'theme = "dark"'
expect_failure "unknown application key diagnostic" 1 "Unknown package manifest key 'theme'" \
  "$binary" --application-mode "$work_dir/package-errors"
write_manifest '[package]' 'name = "valid"' 'name = "duplicate"'
expect_failure "duplicate package key diagnostic" 1 "Duplicate package manifest key 'name'" \
  "$binary" --package "$work_dir/package-errors"
write_manifest '[package]' 'name = "valid"'
expect_failure "missing package key diagnostic" 1 "missing required key 'version'" \
  "$binary" --package "$work_dir/package-errors"
write_manifest '[package]' 'name = "9invalid"' 'version = "1.0.0"' 'source = "src"' 'entry = "main"'
expect_failure "invalid package name diagnostic" 1 "Package name must begin with a letter" \
  "$binary" --package "$work_dir/package-errors"
write_manifest '[package]' 'name = "valid"' 'version = "1.0.0"' 'source = "src"' 'entry = "bad..entry"'
expect_failure "invalid package entry name diagnostic" 1 "Package entry must be a qualified module name" \
  "$binary" --package "$work_dir/package-errors"
write_manifest '[package]' 'name = "valid"' 'version = "1.0.0"' 'source = "../outside"' 'entry = "main"'
expect_failure "escaping package source diagnostic" 1 "must stay inside the package root" \
  "$binary" --package "$work_dir/package-errors"
write_manifest '[package]' 'name = "valid"' 'version = "1.0.0"' 'source = "missing"' 'entry = "main"'
expect_failure "missing package source diagnostic" 1 "Package source directory does not exist" \
  "$binary" --package "$work_dir/package-errors"
expect_failure "invalid package path diagnostic" 1 "must name a directory or sagan.toml" \
  "$binary" --package README.md
write_manifest '[package]' 'name = "valid"' 'version = "1.0.0"' 'source = "src"' 'entry = "main"'
printf '%s\n' 'fun main(): Int => 0' > "$work_dir/package-errors/src/main.sagan"
expect_failure "package entry declaration diagnostic" 1 "must declare 'module main'" \
  "$binary" --package "$work_dir/package-errors"
expect_failure "optional coalescing type diagnostic" 1 "Left operand of ?? must be Optional" \
  "$binary" --types tests/fixtures/semantic/type_optional_coalesce_error.sagan
expect_failure "Some arity diagnostic" 1 "Some expects exactly one value" \
  "$binary" --types tests/fixtures/semantic/type_some_arity_error.sagan
expect_failure "safe access requires Optional diagnostic" 1 "Safe member access requires Optional" \
  "$binary" --types tests/fixtures/semantic/type_safe_access_error.sagan
expect_failure "optional pattern subject diagnostic" 1 "Some pattern requires an Optional subject" \
  "$binary" --types tests/fixtures/semantic/type_optional_pattern_error.sagan
expect_failure "weak field type diagnostic" 1 "Weak field 'value' requires a class or face type" \
  "$binary" --types tests/fixtures/semantic/type_weak_scalar_error.sagan
expect_failure "weak field initializer diagnostic" 1 "Weak field 'target' starts empty" \
  "$binary" --types tests/fixtures/semantic/type_weak_initializer_error.sagan
expect_failure "strong ownership cycle diagnostic" 1 "Strong ownership cycle requires an explicit weak field edge" \
  "$binary" --types tests/fixtures/semantic/type_strong_ownership_cycle_error.sagan
expect_failure "nested strong ownership cycle diagnostic" 1 "Strong ownership cycle requires an explicit weak field edge" \
  "$binary" --types tests/fixtures/semantic/type_nested_strong_ownership_cycle_error.sagan
expect_failure "strong face field diagnostic" 1 "cannot use dynamic face type 'Observable'; declare it with weak let" \
  "$binary" --types tests/fixtures/semantic/type_strong_face_field_error.sagan
expect_failure "payload enum arity diagnostic" 1 "Enum case 'Success' expects 1 payload value" \
  "$binary" --types tests/fixtures/semantic/type_payload_enum_arity_error.sagan
expect_failure "payload enum match diagnostic" 1 "Enum case 'Message' belongs to Signal" \
  "$binary" --types tests/fixtures/semantic/type_payload_enum_match_error.sagan
expect_failure "duplicate enum numeric value diagnostic" 1 "cannot share numeric value 201" \
  "$binary" --types tests/fixtures/semantic/type_enum_duplicate_numeric_error.sagan
expect_failure "implicit enum numeric overflow diagnostic" 1 "would overflow Int64" \
  "$binary" --types tests/fixtures/semantic/type_enum_numeric_overflow_error.sagan
expect_failure "explicit enum numeric range diagnostic" 1 "outside the supported Int64 range" \
  "$binary" --types tests/fixtures/semantic/type_enum_numeric_range_error.sagan
expect_failure "runtime error member diagnostic" 1 "Enum 'RuntimeError' has no member 'not_a_runtime_error'" \
  "$binary" --types tests/fixtures/semantic/type_runtime_error_member_error.sagan
expect_failure "generic sum context diagnostic" 1 "Cannot infer every generic argument" \
  "$binary" --types tests/fixtures/semantic/type_generic_sum_context_error.sagan
expect_failure "generic function inference diagnostic" 1 "No matching overload for 'missing'" \
  "$binary" --types tests/fixtures/semantic/type_generic_function_inference_error.sagan
expect_failure "qualified generic enum payload diagnostic" 1 "Enum case payload requires String" \
  "$binary" --types tests/fixtures/semantic/type_generic_qualified_payload_error.sagan
expect_failure "generic class constructor diagnostic" 1 "No matching constructor for 'Box'" \
  "$binary" --types tests/fixtures/semantic/type_generic_class_constructor_error.sagan
expect_failure "generic class inference diagnostic" 1 "Cannot infer every generic argument for class 'Marker'" \
  "$binary" --types tests/fixtures/semantic/type_generic_class_inference_error.sagan
expect_failure "generic face specialization diagnostic" 1 "Variable initializer requires Readable<String>, but received Box<Int8>" \
  "$binary" --types tests/fixtures/semantic/type_generic_face_assignment_error.sagan
expect_failure "generic method inference diagnostic" 1 "Cannot infer every generic argument for called method" \
  "$binary" --types tests/fixtures/semantic/type_generic_method_inference_error.sagan
expect_failure "generic face method syntax diagnostic" 1 "Generic face methods are not supported" \
  "$binary" --ast tests/fixtures/semantic/generic_face_method_error.sagan
expect_failure "generic face constraint diagnostic" 1 "does not satisfy face constraint Readable<Int8>" \
  "$binary" --types tests/fixtures/semantic/type_generic_constraint_error.sagan
expect_failure "unwritable output" 1 "Could not write" \
  "$binary" --ast-svg examples/ast.sagan "$work_dir/missing/output.svg"

echo "All command-line integration tests passed."
