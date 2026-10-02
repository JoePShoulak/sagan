#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

output="$(bin/sagan tests/fixtures/runtime/constants.sagan)"
output="${output//$'\r'/}"
[[ "$output" == $'Retries: 5\nSpeed: 5 meter / second\nRadius: 3\nStandard gravity: 9.80665 meter / second^2\n(1, 2, 3)\nAlias value: 7\nGeneric retries: 5' ]]

imported="$(bin/sagan tests/fixtures/modules/constants/main.sagan)"
imported="${imported//$'\r'/}"
[[ "$imported" == 'Imported speed: 3' ]]

bin/sagan --emit-cpp tests/fixtures/runtime/constants.sagan build/constants.cpp
grep -Eq '^std::shared_ptr<std::int[0-9]+_t> sagan_' build/constants.cpp
grep -Eq '^std::shared_ptr<double> sagan_' build/constants.cpp
grep -Eq '^  const std::int[0-9]+_t sagan_' build/constants.cpp
grep -Fq 'const sagan_vector<' build/constants.cpp
grep -Fq 'const sagan_point<' build/constants.cpp
grep -Fq 'const std::shared_ptr<' build/constants.cpp

bin/sagan --ast tests/fixtures/runtime/constants.sagan > build/constants.ast.txt
bin/sagan --ast-dot tests/fixtures/runtime/constants.sagan > build/constants.ast.dot
bin/sagan --ast-svg tests/fixtures/runtime/constants.sagan build/constants.ast.svg
bin/sagan --ast-html tests/fixtures/runtime/constants.sagan build/constants.ast.html
grep -Fq 'Const(SPEED_OF_LIGHT)' build/constants.ast.txt
grep -Fq 'Const' build/constants.ast.dot
grep -Fq 'Const' build/constants.ast.svg
grep -Fq 'Const' build/constants.ast.html

echo "Constants test passed: native execution, imports, generated C++, and text/DOT/SVG/HTML AST views."
