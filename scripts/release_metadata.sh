#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="$(bash "$repo_root/scripts/version.sh" numeric)"
release_tag="${SAGAN_RELEASE_TAG:-v$version}"
if [[ "$release_tag" != "v$version" && "$release_tag" != "v$version-rc.1" ]]; then
  echo "Release tag $release_tag does not match numeric version $version." >&2
  exit 1
fi
signature_policy="authenticode-required-for-public-release"
if [[ "$version" == 1.0.0 ]]; then
  signature_policy="unsigned-initial-1.0-exception"
elif [[ "${SAGAN_RELEASE_TAG:-}" == v0.88.0-rc.1 && "$version" == 0.88.0 ]]; then
  signature_policy="unsigned-experimental-0.x-preview-exception"
elif [[ "${SAGAN_RELEASE_TAG:-}" == v4.9.5 && "$version" == 4.9.5 ]]; then
  signature_policy="unsigned-experimental-4.9.5-exception"
fi
commit="$(git -C "$repo_root" rev-parse HEAD)"
installer="$repo_root/build/installer/sagan-$version-windows-x64.exe"
archive="$repo_root/build/release/sagan-$version-windows-x64.zip"
output_dir="$repo_root/build/release"
manifest="$output_dir/sagan-$version-release-manifest.json"
sbom="$output_dir/sagan-$version-sbom.spdx.json"
gcc="$repo_root/build/windows-stage/toolchain/ucrt64/bin/g++.exe"
gdb="$repo_root/build/windows-stage/toolchain/ucrt64/bin/gdb.exe"
extension_version="$(npm --prefix "$repo_root/editors/vscode-sagan" pkg get version | tr -d '"[:space:]')"
extension="$output_dir/sagan-language-$extension_version.vsix"

for artifact in "$installer" "$installer.sha256" "$archive" "$archive.sha256" "$extension" "$extension.sha256"; do
  [[ -f "$artifact" ]] || { echo "Missing release artifact: $artifact" >&2; exit 1; }
done
[[ -x "$gcc" ]] || { echo "Missing staged GCC compiler: $gcc" >&2; exit 1; }
[[ -x "$gdb" ]] || { echo "Missing staged GDB debugger: $gdb" >&2; exit 1; }

installer_hash="$(sha256sum "$installer" | cut -d' ' -f1)"
archive_hash="$(sha256sum "$archive" | cut -d' ' -f1)"
extension_hash="$(sha256sum "$extension" | cut -d' ' -f1)"
installer_size="$(wc -c < "$installer" | tr -d ' ')"
archive_size="$(wc -c < "$archive" | tr -d ' ')"
extension_size="$(wc -c < "$extension" | tr -d ' ')"
gcc_version="$("$gcc" -dumpfullversion)"
gdb_version="$("$gdb" --version | head -n 1 | awk '{print $NF}')"
created="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"

mkdir -p "$output_dir"
printf '%s\n' \
  '{' \
  '  "schema": "sagan.release-manifest/1",' \
  "  \"version\": \"$version\"," \
  "  \"tag\": \"$release_tag\"," \
  "  \"commit\": \"$commit\"," \
  "  \"signature_policy\": \"$signature_policy\"," \
  '  "artifacts": [' \
  "    {\"name\": \"$(basename "$installer")\", \"role\": \"windows-installer\", \"media_type\": \"application/vnd.microsoft.portable-executable\", \"size\": $installer_size, \"sha256\": \"$installer_hash\"}," \
  "    {\"name\": \"$(basename "$archive")\", \"role\": \"windows-portable\", \"media_type\": \"application/zip\", \"size\": $archive_size, \"sha256\": \"$archive_hash\"}," \
  "    {\"name\": \"$(basename "$extension")\", \"role\": \"vscode-extension\", \"media_type\": \"application/octet-stream\", \"size\": $extension_size, \"sha256\": \"$extension_hash\"}" \
  '  ]' \
  '}' > "$manifest"

namespace="https://github.com/Sagan-Shoulak/sagan/releases/$release_tag/sbom/$commit"
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
  "    {\"name\": \"GNU GDB\", \"SPDXID\": \"SPDXRef-Package-GDB\", \"versionInfo\": \"$gdb_version\", \"downloadLocation\": \"NOASSERTION\", \"filesAnalyzed\": false, \"licenseConcluded\": \"NOASSERTION\", \"licenseDeclared\": \"GPL-3.0-or-later\", \"copyrightText\": \"Copyright Free Software Foundation, Inc.\"}," \
  '    {"name": "Python runtime", "SPDXID": "SPDXRef-Package-Python", "versionInfo": "3.12", "downloadLocation": "NOASSERTION", "filesAnalyzed": false, "licenseConcluded": "NOASSERTION", "licenseDeclared": "PSF-2.0", "copyrightText": "NOASSERTION"},' \
  '    {"name": "uni-algo", "SPDXID": "SPDXRef-Package-UniAlgo", "versionInfo": "NOASSERTION", "downloadLocation": "NOASSERTION", "filesAnalyzed": false, "licenseConcluded": "MIT", "licenseDeclared": "MIT", "copyrightText": "NOASSERTION"},' \
  '    {"name": "Unicode Character Database", "SPDXID": "SPDXRef-Package-Unicode", "versionInfo": "17.0.0", "downloadLocation": "NOASSERTION", "filesAnalyzed": false, "licenseConcluded": "Unicode-3.0", "licenseDeclared": "Unicode-3.0", "copyrightText": "Copyright Unicode, Inc."}' \
  '  ],' \
  '  "relationships": [' \
  '    {"spdxElementId": "SPDXRef-DOCUMENT", "relationshipType": "DESCRIBES", "relatedSpdxElement": "SPDXRef-Package-Sagan"},' \
  '    {"spdxElementId": "SPDXRef-DOCUMENT", "relationshipType": "DESCRIBES", "relatedSpdxElement": "SPDXRef-Package-Sagan-VSCode"},' \
  '    {"spdxElementId": "SPDXRef-Package-Sagan", "relationshipType": "DEPENDS_ON", "relatedSpdxElement": "SPDXRef-Package-GCC"},' \
  '    {"spdxElementId": "SPDXRef-Package-Sagan", "relationshipType": "DEPENDS_ON", "relatedSpdxElement": "SPDXRef-Package-GDB"},' \
  '    {"spdxElementId": "SPDXRef-Package-GDB", "relationshipType": "DEPENDS_ON", "relatedSpdxElement": "SPDXRef-Package-Python"},' \
  '    {"spdxElementId": "SPDXRef-Package-Sagan", "relationshipType": "DEPENDS_ON", "relatedSpdxElement": "SPDXRef-Package-UniAlgo"},' \
  '    {"spdxElementId": "SPDXRef-Package-Sagan", "relationshipType": "DEPENDS_ON", "relatedSpdxElement": "SPDXRef-Package-Unicode"}' \
  '  ]' \
  '}' > "$sbom"

echo "Wrote build/release/$(basename "$manifest")"
echo "Wrote build/release/$(basename "$sbom")"
