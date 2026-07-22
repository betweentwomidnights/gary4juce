# Releasing gary4juce

This is the maintainer checklist for preparing a gary4juce release. Release
staging remains manual under the ignored `.release/` directory, but the final
artifact shape and verification steps should be repeatable.

## Release Order

Use two commits so the release tag identifies the exact source used to build
the binary without making the public updater point at an unpublished asset:

1. Prepare a source-release commit containing the final project version,
   generated build metadata, changelog entry, and current-release copy. Leave
   the README stable-download link and public updater feed on the currently
   published release.
2. Tag that source-release commit locally, then build the Release binary from
   its clean tree.
3. Stage and verify the archive, calculate its hash, and prepare the GitHub
   release as a draft. Push the source commit/tag when the draft needs the tag
   on GitHub.
4. Confirm every recommended companion-release URL is already live. Publish
   the GitHub release and verify its uploaded asset before promoting it through
   the updater.
5. Create a promotion commit that updates `README.md`, `SHA256SUMS.txt`, and
   the stable updater manifest. This commit intentionally comes after the
   tagged source-release commit.
6. Verify the public updater manifest, release URL, downloaded filename, and
   downloaded SHA-256 after the promotion commit is live.

This ordering prevents a public updater manifest from linking to a missing
release and keeps checksum-only metadata changes out of the tagged build
source.

## Windows VST3 Package

After creating the source-release commit and local tag, build the Release VST3
from that clean tree. Stage the bundle from:

```text
Builds/VisualStudio2022/x64/Release/VST3/gary4juce.vst3
```

The ZIP must contain exactly one top-level entry: `gary4juce.vst3/`. Put the
license and Corresponding Source material inside the bundle rather than beside
it so Windows users can use **Extract All** directly into their VST3 folder.

Required staging layout:

```text
gary4juce.vst3/
`-- Contents/
    |-- Resources/
    |   |-- moduleinfo.json
    |   `-- Legal/
    |       |-- LICENSE
    |       |-- SOURCE.md
    |       |-- THIRD_PARTY_NOTICES.md
    |       `-- licenses/
    |           |-- JUCE-LICENSE.md
    |           `-- applicable dependency license files
    `-- x86_64-win/
        `-- gary4juce.vst3
```

For each release:

1. Copy the freshly built `gary4juce.vst3` bundle into a clean versioned
   `.release/package-vX.Y.Z/` staging directory.
2. Create `gary4juce.vst3/Contents/Resources/Legal/licenses/`.
3. Copy the repository `LICENSE` and `THIRD_PARTY_NOTICES.md` into `Legal/`.
4. Create `SOURCE.md` with exact links to the release tag, source archive,
   build instructions, pinned JUCE source, and pinned JUCE license.
5. Copy the applicable JUCE and dependency license texts into
   `Legal/licenses/`. Refresh these whenever JUCE or another bundled dependency
   changes; do not blindly reuse an older set.
   When copying from a previous staged package in PowerShell, copy the child
   files explicitly (for example, `Get-ChildItem $oldLicenses -File |
   Copy-Item -Destination $newLicenses`) rather than relying on a wildcard path
   that may only recreate the directory shell and silently leave `licenses/`
   empty.
6. Create the ZIP from `gary4juce.vst3` only. Do not zip the staging directory
   or leave legal files as siblings of the bundle.

Example final ZIP command from the repository root:

```powershell
$version = "X.Y.Z"
$stage = ".release\package-v$version"
$zip = ".release\gary4juce-v$version-win-vst3.zip"
Compress-Archive -Path "$stage\gary4juce.vst3" -DestinationPath $zip -Force
```

Verify the archive before uploading it:

```powershell
$entries = tar -tf $zip
$unexpected = $entries | Where-Object { $_ -notlike "gary4juce.vst3/*" }
if ($unexpected) { throw "ZIP contains entries outside gary4juce.vst3" }

$required = @(
    "gary4juce.vst3/Contents/Resources/moduleinfo.json",
    "gary4juce.vst3/Contents/Resources/Legal/LICENSE",
    "gary4juce.vst3/Contents/Resources/Legal/SOURCE.md",
    "gary4juce.vst3/Contents/Resources/Legal/THIRD_PARTY_NOTICES.md",
    "gary4juce.vst3/Contents/x86_64-win/gary4juce.vst3"
)
foreach ($path in $required) {
    if ($entries -notcontains $path) { throw "ZIP is missing $path" }
}
```

Also verify that `moduleinfo.json` contains the release version, `SOURCE.md`
names the release tag, and `Legal/licenses/` contains actual files rather than
only an empty directory.

Also extract the ZIP into a clean temporary directory, confirm the bundle
loads in a DAW, and confirm that **Extract All** can target a VST3 directory
without creating loose files beside `gary4juce.vst3`.

After final verification, update `SHA256SUMS.txt` from the exact uploaded ZIP:

```powershell
$name = Split-Path $zip -Leaf
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $name" | Set-Content SHA256SUMS.txt
```

## Release Metadata

In the source-release commit:

1. Confirm the project version and generated build metadata.
2. Update `docs/CHANGELOG.md` with the release's user-visible changes. The
   release commit must include this entry.
3. Update the README's current-release copy and installation instructions when
   the artifact format changes. Do not change its stable-download link yet.
4. Write fuller GitHub release notes covering the user-visible changes,
   recommended gary4local version, artifact hash, and installation changes.
5. Tag the exact source-release commit used to build the binaries.

In the post-publication promotion commit:

1. Update the stable release and companion links in `README.md`.
2. Update `SHA256SUMS.txt` from the exact uploaded ZIP.
3. Update `docs/updates/gary4juce/stable.json` and, when applicable,
   `stable-mac.json`.
4. Keep updater notes compact and plain text. End binary-release notes with
   the reminder to close the DAW before replacing plugin files.

For the nested Windows package, tell users they may close their DAW and use
**Extract All** directly into `C:\Program Files\Common Files\VST3\`. Windows may
request administrator permission for that destination.

## macOS Note

Any license or notice files placed inside a macOS AU or VST3 bundle must be
added before code signing. Modifying a signed bundle invalidates its signature.
