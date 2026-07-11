# Releasing gary4juce

This is the maintainer checklist for preparing a gary4juce release. Release
staging remains manual under the ignored `.release/` directory, but the final
artifact shape and verification steps should be repeatable.

## Windows VST3 Package

Build the Release VST3 first. Stage the bundle from:

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

Before publishing:

1. Confirm the project version and generated build metadata.
2. Tag the exact commit used to build the binaries.
3. Update `README.md` release links and installation instructions when the
   artifact format changes.
4. Update `docs/updates/gary4juce/stable.json` and, when applicable,
   `stable-mac.json`.
5. Keep updater notes compact and plain text. End binary-release notes with
   the reminder to close the DAW before replacing plugin files.
6. Write fuller GitHub release notes covering the user-visible changes,
   recommended gary4local version, artifact hash, and installation changes.

For the nested Windows package, tell users they may close their DAW and use
**Extract All** directly into `C:\Program Files\Common Files\VST3\`. Windows may
request administrator permission for that destination.

## macOS Note

Any license or notice files placed inside a macOS AU or VST3 bundle must be
added before code signing. Modifying a signed bundle invalidates its signature.
