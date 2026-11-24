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
3. Stage and verify the archives, calculate their hashes, and prepare the GitHub
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

### Pre-releases

A pre-release — a build handed to a tester directly, usually paired with a
gary4local ROCm preview — runs steps 1 through 4 and **stops there**. Package
and verify the archives exactly as a stable release: same staging, same legal
material, same checks. What it skips is the promotion in step 5.

Concretely, a pre-release does not touch `README.md`, `SHA256SUMS.txt`, or
`docs/updates/gary4juce/stable.json`. Those describe the current *stable*
release, and pointing them at a pre-release would offer it to everyone through
the updater. Mark the GitHub release as a prerelease (`gh release create
--prerelease`) and record the asset hashes in the release notes only.

The changelog entry and the version bump still belong in the source-release
commit, so the tag identifies the build.

## Windows VST3 Package

After creating the source-release commit and local tag, build the Release VST3
from that clean tree.

**Build from the command line with the 64-bit MSBuild,**
`...\MSBuild\Current\Bin\amd64\MSBuild.exe`, not `...\Bin\MSBuild.exe`. The
VST3 post-build step only writes `moduleinfo.json` when MSBuild reports a
64-bit host; under the 32-bit one it skips it ("VST3 manifest generation is
disabled ... host system processor detected to be x86") and the bundle keeps
whatever manifest the last 64-bit build left, with the old version in it. The
build still succeeds. Building in the Visual Studio IDE is fine, since it's
64-bit itself. Looking MSBuild up with `vswhere -find "MSBuild\**\Bin\MSBuild.exe"`
returns the 32-bit one first. that's how v5.0.0-rc.1's first build came out
with a 4.0.15 manifest, before the `moduleinfo.json` check below caught it.

```powershell
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
& "$vs\MSBuild\Current\Bin\amd64\MSBuild.exe" Builds\VisualStudio2022\gary4juce.sln /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m
```

**Run the packaging scripts with PowerShell 7 (`pwsh`),** not Windows
PowerShell 5.1 (`powershell.exe`). JUCE writes `moduleinfo.json` with trailing
commas, which 5.1's `ConvertFrom-Json` rejects with "Invalid JSON primitive";
7 parses it. The `.release/package-*.ps1` scripts start with
`#Requires -Version 7.0`, so 5.1 refuses them before they stage anything. Keep
that line when copying a script forward. 5.1 also lacks `&&`, `||` and `?:`, and
wraps a native command's stderr as an error when it is redirected with `2>&1`.
Install 7 with `winget install --id Microsoft.PowerShell --source winget`.

```powershell
pwsh -NoProfile -File .release\package-vX.Y.Z.ps1
```

Stage the bundle from:

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

   **Rewrite this file every release. It does not come from the build.** The
   `Legal/` folder inside
   `Builds/VisualStudio2022/x64/Release/VST3/gary4juce.vst3/Contents/Resources/`
   survives rebuilds, so the freshly built bundle copied in step 1 still carries
   whatever `SOURCE.md` was staged there last — during the v4.0.13 release it
   was still naming the `v4.0.3` tag. Copying the bundle without regenerating
   this file ships an AGPL Corresponding Source offer pointing at the wrong
   source tree. Check the version inside the file, not just that it exists.
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
only an empty directory. The staleness above is quiet enough to be worth
checking mechanically rather than by eye:

```powershell
$stage = ".release\package-v$version-vst3"
$src = Get-Content "$stage\gary4juce.vst3\Contents\Resources\Legal\SOURCE.md" -Raw
if ($src -notmatch [regex]::Escape("v$version")) { throw "SOURCE.md does not name v$version" }
if ((Get-ChildItem "$stage\gary4juce.vst3\Contents\Resources\Legal\licenses" -File).Count -eq 0) {
    throw "Legal/licenses is empty"
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

## Windows Standalone Package

Ship the standalone app as a second ZIP on the same Windows release. Do not put
it inside the VST3 ZIP: that archive must remain safe to extract directly into
the system VST3 directory.

Stage the freshly built executable from:

```text
Builds/VisualStudio2022/x64/Release/Standalone Plugin/gary4juce.exe
```

Required standalone layout:

```text
gary4juce-standalone/
|-- gary4juce.exe
|-- README.txt
|-- VC_redist.x64.exe
`-- Legal/
    |-- LICENSE
    |-- SOURCE.md
    |-- THIRD_PARTY_NOTICES.md
    `-- licenses/
        |-- JUCE-LICENSE.md
        `-- applicable dependency license files
```

Use the same release-specific `SOURCE.md` and license set as the VST3 bundle.
Download the latest supported v14 x64 redistributable from Microsoft's stable
URL, `https://aka.ms/vc14/vc_redist.x64.exe`, for every release. Keep the
installer unmodified, confirm its Authenticode signature is valid and issued to
Microsoft Corporation, and record its file version and SHA-256 in the release
verification log. `README.txt` should tell users to run it only when Windows
reports missing Visual C++ runtime DLLs.

Verify that the ZIP has exactly one top-level `gary4juce-standalone/` entry,
that the app reports the release version, and that the redistributable and every
required legal file are present and nonempty. Extract it into a clean directory
and launch the extracted app before upload.

Name the assets consistently:

```text
gary4juce-vX.Y.Z-win-vst3.zip
gary4juce-vX.Y.Z-win-standalone.zip
```

Record both uploaded asset hashes in `SHA256SUMS.txt`. The Windows stable
updater manifest continues to use the VST3 ZIP hash because its download flow
is for the plugin update.

## Release Metadata

In the source-release commit:

1. Confirm the project version and generated build metadata.
2. Review the commits and diff since the previous release tag. Account for
   every user-visible change rather than relying on memory or only describing
   the most recent commit.
3. Update `docs/CHANGELOG.md` with the release's user-visible changes. Add an
   entry for every release, even when it is only a small fix. The
   release commit must include this entry.
4. Replace the README's entire `latest update` section with copy for this
   release. Do this for every release, including small fixes; do not leave the
   previous release's title or summary in place. Label it `upcoming vX.Y.Z`
   while the GitHub release is still a draft.
5. Update README installation instructions when the artifact format changes.
   Do not change its stable-download or recommended-companion links yet.
6. Write fuller GitHub release notes covering the user-visible changes,
   recommended gary4local version, artifact hashes, and installation changes.
7. Tag the exact source-release commit used to build the binaries.

In the post-publication promotion commit:

1. Update both the stable release link and the recommended Windows gary4local
   link in `README.md`, and remove `upcoming` from the `latest update` heading.
   Confirm the companion tag URL is live before committing it.
2. Confirm the README `latest update`, GitHub release notes, and updater notes
   all name the same recommended gary4local version.
3. Update `SHA256SUMS.txt` from the exact uploaded ZIPs.
4. Update `docs/updates/gary4juce/stable.json` and, when applicable,
   `stable-mac.json`.
5. Keep updater notes compact and plain text. End binary-release notes with
   the reminder to close the DAW before replacing plugin files.

Before pushing either release commit, search `README.md`, `docs/CHANGELOG.md`,
and the updater manifest for the outgoing plugin and companion versions. Old
versions are expected in changelog history, but not in the README's stable
links, recommended-companion links, or `latest update` section after promotion.

For the nested Windows package, tell users they may close their DAW and use
**Extract All** directly into `C:\Program Files\Common Files\VST3\`. Windows may
request administrator permission for that destination.

## macOS Release Flow

macOS builds come from `main`, like the Windows and Linux builds. The macOS
differences in the source are guarded with `JUCE_MAC`: the standalone's Dock
icon, the `stable-mac.json` update feed and the infinite tail that keeps
GarageBand's master track processing. A `vX.Y.Z-mac` release is the same
source as `vX.Y.Z`, packaged on a Mac. Local services come from the unified
gary4local, which speaks the same localhost API on every platform.

### 1. Check out the release commit

```bash
git fetch origin
git switch --detach origin/main   # or the commit tagged vX.Y.Z
```

The `.jucer` expects JUCE at `../JUCE`. On a Mac that keeps JUCE elsewhere,
link it once: `ln -s ~/JUCE ../JUCE`.

### 2. Regenerate the Xcode project

`Builds/MacOSX/` is gitignored. It is regenerated from `gary4juce.jucer` by
Projucer. `JUCE_APP_VERSION` and the other version-derived settings are baked
into the generated `.xcodeproj` at resave time, so skipping this step builds
the *old* version without any error:

```bash
/path/to/Projucer.app/Contents/MacOS/Projucer --resave gary4juce.jucer
```

### 3. Build all three macOS targets as universal binaries

The packaging script does not build anything. It expects
`Builds/MacOSX/build/Release/{gary4juce.vst3,gary4juce.component,gary4juce.app}`
to exist. Build them with `xcodebuild`, passing `ARCHS` explicitly:

```bash
xcodebuild -project Builds/MacOSX/gary4juce.xcodeproj -scheme "gary4juce - VST3" \
  -configuration Release ARCHS="x86_64 arm64" ONLY_ACTIVE_ARCH=NO build
xcodebuild -project Builds/MacOSX/gary4juce.xcodeproj -scheme "gary4juce - AU" \
  -configuration Release ARCHS="x86_64 arm64" ONLY_ACTIVE_ARCH=NO build
xcodebuild -project Builds/MacOSX/gary4juce.xcodeproj -scheme "gary4juce - Standalone Plugin" \
  -configuration Release ARCHS="x86_64 arm64" ONLY_ACTIVE_ARCH=NO build
```

Without the explicit `ARCHS`/`ONLY_ACTIVE_ARCH` override, `xcodebuild`
resolves to a single-arch build (x86_64 only was observed) with no error or
warning. The DMG still builds and notarizes, and the plugin is quietly not
universal. Verify before packaging:

```bash
lipo -info Builds/MacOSX/build/Release/gary4juce.vst3/Contents/MacOS/gary4juce
lipo -info Builds/MacOSX/build/Release/gary4juce.component/Contents/MacOS/gary4juce
lipo -info Builds/MacOSX/build/Release/gary4juce.app/Contents/MacOS/gary4juce
```

Each command should print `x86_64 arm64`.

Xcode's "Plugin Copy Step" copies each freshly built, ad-hoc-signed plugin into
`~/Library/Audio/Plug-Ins/{VST3,Components}`. That is convenient for a quick
DAW check, but it replaces whatever build you had installed for regular use.

### 4. Tag, then package, sign and notarize

Tag the source commit **before** packaging. The packaging script writes
`git rev-parse HEAD` into each DMG's `licenses/CORRESPONDING_SOURCE.txt`, so
the tag must sit on that commit or the DMGs' source links point at an untagged
commit:

```bash
git tag -a vX.Y.Z-mac -m "gary4juce vX.Y.Z-mac"
VERSION="vX.Y.Z-mac" ./build-dmg.sh all
```

`build-dmg.sh` is the maintainer's local signing script and is not tracked. It
signs each bundle with the Developer ID identity, builds the DMGs, notarizes
them with the `notarytool` keychain profile, and staples and validates them.
The AU, VST3 and standalone DMGs must each contain the `licenses` directory.

### 5. Publish

```bash
git push origin vX.Y.Z-mac
gh release create vX.Y.Z-mac gary4juce-vX.Y.Z-mac-{AU,VST3,STANDALONE}.dmg \
  --title "gary4juce vX.Y.Z-mac" --notes-file release-notes-vX.Y.Z-mac.md
gh release edit vX.Y.Z --latest
```

`gh release create` marks the new `-mac` release as Latest. Move Latest back to
the Windows release, as the last command does.

Then commit the release notes and `SHA256SUMS-vX.Y.Z-mac.txt` on `main`. Then
make a separate promote commit on `main` that updates the README's macOS links
and `docs/updates/gary4juce/stable-mac.json`, using the real `published_at`
from `gh release view vX.Y.Z-mac --json publishedAt`. GitHub Pages serves
`main`'s `docs/`. Confirm the live feed after the Pages deployment.

### License/signing order

Any license or notice files placed inside a macOS AU or VST3 bundle must be
added before code signing. The AU, VST3 and standalone bundles must each be
Developer ID-signed; signing only the outer DMG is not a substitute for signing
the executable bundle that the user installs.

The AU and VST3 also use Finder custom-icon metadata, because Finder does not
honor their `CFBundleIconFile` the way it honors the standalone app's icon.
Apply that custom icon only after the clean bundle passes
`codesign --verify --deep --strict`. The intentional `com.apple.FinderInfo`
and resource-fork metadata makes later `--strict` verification fail, so verify
the finished AU/VST3 with ordinary `codesign --verify --deep`, Gatekeeper and
the notarized DMG instead. The standalone uses its embedded signed icon and
should continue to pass strict verification.

## Linux preview flow (linux branch)

Merge current `main` into `linux`, preserving the host keyboard-focus flag,
copy-only external drags, stable drag-list lifetime and the export-copy button.
The current storage helpers replace the old branch's ad-hoc directory helpers;
do not reintroduce the obsolete backend request implementations while resolving
those conflicts. `docs/LINUX_PREVIEW.md` documents supported builds and limits.

The Linux CMake build requests keyboard focus and supplies the full
`GARY4JUCE_RELEASE_VERSION` to the updater. Keep this version, the release tag,
changelog and preview feed consistent. A bare numeric 5.0.5 would compare newer
than 5.0.5-linux-preview.2 and prevent preview-to-preview updates.

Commit final source, tag it `vX.Y.Z-linux-preview.N`, and push the Linux branch
and tag. Register the workflow definition on main before its first use; manual
dispatch should select the exact Linux preview tag. The Linux preview workflow builds natively on Ubuntu 24.04 x64 and
ARM64. It does not install plugins into the runner's user directories. Download
the workflow artifacts, verify every ZIP against its architecture's checksum
file, and prepare a GitHub draft marked as a prerelease. Check legal/source
material and executable modes, and retain the workflow's build information.

After publication and public asset verification, add or update
`docs/updates/gary4juce/preview-linux.json` in a separate commit on **main**.
GitHub Pages serves main's `docs/`, not the Linux branch. This platform preview
feed is an explicit exception to the generic prerelease rule above: it promotes
only Linux previews, never the Windows or macOS stable feeds. Do not change the
README stable-download links or top-level stable `SHA256SUMS.txt` for a preview.
Record both architectures' VST3 and standalone hashes in the release and feed.
Use the real release publication timestamp. Confirm the public feed and its
release/assets after the Pages deployment completes.

The preview's virtual-display checks do not replace manual DAW typing, Bitwig
file drag and GPU generation checks with the Linux gary4local desktop build.
