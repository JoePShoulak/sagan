#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="$(bash "$repo_root/scripts/version.sh" numeric)"
signature_policy="authenticode-required-for-public-release"
if [[ "$version" == 1.0.0 ]]; then
  signature_policy="unsigned-initial-1.0-exception"
fi
commit="$(git -C "$repo_root" rev-parse HEAD)"
installer="$repo_root/build/installer/sagan-$version-windows-x64.exe"
archive="$repo_root/build/release/sagan-$version-windows-x64.zip"
output_dir="$repo_root/build/release"
manifest="$output_dir/sagan-$version-release-manifest.json"
sbom="$output_dir/sagan-$version-sbom.spdx.json"
gcc="$repo_root/build/windows-stage/toolchain/ucrt64/bin/g++.exe"
extension_version="$(npm --prefix "$repo_root/editors/vscode-sagan" pkg get version | tr -d '"[:space:]')"
extension="$output_dir/sagan-language-$extension_version.vsix"

for artifact in "$installer" "$installer.sha256" "$archive" "$archive.sha256" "$extension" "$extension.sha256"; do
  [[ -f "$artifact" ]] || { echo "Missing release artifact: $artifact" >&2; exit 1; }
done
[[ -x "$gcc" ]] || { echo "Missing staged GCC compiler: $gcc" >&2; exit 1; }

installer_hash="$(sha256sum "$installer" | cut -d' ' -f1)"
archive_hash="$(sha256sum "$archive" | cut -d' ' -f1)"
extension_hash="$(sha256sum "$extension" | cut -d' ' -f1)"
installer_size="$(wc -c < "$installer" | tr -d ' ')"
archive_size="$(wc -c < "$archive" | tr -d ' ')"
extension_size="$(wc -c < "$extension" | tr -d ' ')"
gcc_version="$("$gcc" -dumpfullversion)"
created="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"

mkdir -p "$output_dir"
printf '%s\n' \
  '{' \
  '  "schema": "sagan.release-manifest/1",' \
  "  \"version\": \"$version\"," \
  "  \"tag\": \"v$version\"," \
  "  \"commit\": \"$commit\"," \
  "  \"signature_policy\": \"$signature_policy\"," \
  '  "artifacts": [' \
  "    {\"name\": \"$(basename "$installer")\", \"role\": \"windows-installer\", \"media_type\": \"application/vnd.microsoft.portable-executable\", \"size\": $installer_size, \"sha256\": \"$installer_hash\"}," \
  "    {\"name\": \"$(basename "$archive")\", \"role\": \"windows-portable\", \"media_type\": \"application/zip\", \"size\": $archive_size, \"sha256\": \"$archive_hash\"}," \
  "    {\"name\": \"$(basename "$extension")\", \"role\": \"vscode-extension\", \"media_type\": \"application/octet-stream\", \"size\": $extension_size, \"sha256\": \"$extension_hash\"}" \
  '  ]' \
  '}' > "$manifest"

namespace="https://github.com/JoePShoulak/sagan/releases/v$version/sbom/$commit"
printf '%s\n' \
  '{' \
  '  "spdxVersion": "SPDX-2.3",' \
  '  "dataLicense": "CC0-1.0",' \
  '  "SPDXID": "SPDXRef-DOCUMENT",' \
  "  \"name\": \"Sagan $version Windows x64 release\"," \
  "  \"documentNamespace\": \"$namespace\"," \
  '  "creationInfo": {' \
  '    "creators": ["Tool: scripts/release_metadata.sh"],' \
  "    \"created\": \"$created\"" \
  '  },' \
  '  "packages": [' \
  "    {\"name\": \"Sagan\", \"SPDXID\": \"SPDXRef-Package-Sagan\", \"versionInfo\": \"$version\", \"downloadLocation\": \"NOASSERTION\", \"filesAnalyzed\": false, \"licenseConcluded\": \"GPL-3.0-only\", \"licenseDeclared\": \"GPL-3.0-only\", \"copyrightText\": \"NOASSERTION\"}," \
  "    {\"name\": \"Sagan Language for VS Code\", \"SPDXID\": \"SPDXRef-Package-Sagan-VSCode\", \"versionInfo\": \"$extension_version\", \"downloadLocation\": \"NOASSERTION\", \"filesAnalyzed\": false, \"licenseConcluded\": \"GPL-3.0-only\", \"licenseDeclared\": \"GPL-3.0-only\", \"copyrightText\": \"NOASSERTION\"}," \
  "    {\"name\": \"GCC UCRT64 toolchain\", \"SPDXID\": \"SPDXRef-Package-GCC\", \"versionInfo\": \"$gcc_version\", \"downloadLocation\": \"NOASSERTION\", \"filesAnalyzed\": false, \"licenseConcluded\": \"NOASSERTION\", \"licenseDeclared\": \"GPL-3.0-or-later WITH GCC-exception-3.1\", \"copyrightText\": \"NOASSERTION\"}," \
  '    {"name": "uni-algo", "SPDXID": "SPDXRef-Package-UniAlgo", "versionInfo": "NOASSERTION", "downloadLocation": "NOASSERTION", "filesAnalyzed": false, "licenseConcluded": "MIT", "licenseDeclared": "MIT", "copyrightText": "NOASSERTION"},' \
  '    {"name": "Unicode Character Database", "SPDXID": "SPDXRef-Package-Unicode", "versionInfo": "17.0.0", "downloadLocation": "NOASSERTION", "filesAnalyzed": false, "licenseConcluded": "Unicode-3.0", "licenseDeclared": "Unicode-3.0", "copyrightText": "Copyright Unicode, Inc."}' \
  '  ],' \
  '  "relationships": [' \
  '    {"spdxElementId": "SPDXRef-DOCUMENT", "relationshipType": "DESCRIBES", "relatedSpdxElement": "SPDXRef-Package-Sagan"},' \
  '    {"spdxElementId": "SPDXRef-DOCUMENT", "relationshipType": "DESCRIBES", "relatedSpdxElement": "SPDXRef-Package-Sagan-VSCode"},' \
  '    {"spdxElementId": "SPDXRef-Package-Sagan", "relationshipType": "DEPENDS_ON", "relatedSpdxElement": "SPDXRef-Package-GCC"},' \
  '    {"spdxElementId": "SPDXRef-Package-Sagan", "relationshipType": "DEPENDS_ON", "relatedSpdxElement": "SPDXRef-Package-UniAlgo"},' \
  '    {"spdxElementId": "SPDXRef-Package-Sagan", "relationshipType": "DEPENDS_ON", "relatedSpdxElement": "SPDXRef-Package-Unicode"}' \
  '  ]' \
  '}' > "$sbom"

echo "Wrote build/release/$(basename "$manifest")"
echo "Wrote build/release/$(basename "$sbom")"
