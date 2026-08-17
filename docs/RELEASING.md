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

## macOS Release Flow (mac branch)

The `mac` branch carries macOS-specific packaging on top of `main`'s source.
Cutting a `vX.Y.Z-mac` release means merging `main` into `mac`, then building
and publishing from `mac`.

### 1. Merge and expect real conflicts

```bash
git checkout mac
git merge origin/main -m "Merge origin/main into mac: prep vX.Y.Z-mac"
```

Conflicts in `Source/*.cpp` are not just textual drift — the `mac` branch
carries its own behavioral logic (e.g. `isLocalTerryOp`-aware deferred
health-check polling, GarageBand-specific overrides) that `main` doesn't have.
Resolve these by understanding both sides' intent, not by blindly picking one
side. Grep the *whole* surrounding function for other call sites before
deleting a locally-scoped lambda/helper that a conflict block seems to make
redundant — these functions run long, and a helper removed inside one
conflict block is often still called several times further down in code that
git didn't even flag as conflicting.

`README.md` and `docs/updates/gary4juce/stable-mac.json` will very likely
conflict too (both branches touch the mac companion links/version). Resolve
in favor of whichever side is more current; both get overwritten with the
real release values in a later step anyway.

### 2. Regenerate the Xcode project

`Builds/MacOSX/` is gitignored — it's a local artifact regenerated from
`gary4juce.jucer` via Projucer, not something the merge updates for you.
`JUCE_APP_VERSION` (and other version-derived build settings) are baked into
the generated `.xcodeproj` at resave time, not read live from the `.jucer` at
build time. Skipping this step means the build silently succeeds against the
*old* version:

```bash
/path/to/Projucer.app/Contents/MacOS/Projucer --resave gary4juce.jucer
```

### 3. Build both plugin targets as universal binaries

`build-dmg.sh` does not build anything itself — it expects
`Builds/MacOSX/build/Release/{gary4juce.vst3,gary4juce.component}` to already
exist. Build them with `xcodebuild` first, and pass `ARCHS` explicitly:

```bash
xcodebuild -project Builds/MacOSX/gary4juce.xcodeproj -scheme "gary4juce - VST3" \
  -configuration Release ARCHS="x86_64 arm64" ONLY_ACTIVE_ARCH=NO build
xcodebuild -project Builds/MacOSX/gary4juce.xcodeproj -scheme "gary4juce - AU" \
  -configuration Release ARCHS="x86_64 arm64" ONLY_ACTIVE_ARCH=NO build
```

Without the explicit `ARCHS`/`ONLY_ACTIVE_ARCH` override, `xcodebuild` here
resolves to a single-arch build (x86_64 only was observed) with no error or
warning — the build succeeds, the DMG builds and notarizes fine, and the
resulting plugin is quietly not universal. Verify before packaging:

```bash
lipo -info Builds/MacOSX/build/Release/gary4juce.vst3/Contents/MacOS/gary4juce
```
should print `x86_64 arm64`, not a single architecture.

Aside: Xcode's "Plugin Copy Step" build phase copies the freshly-built,
ad-hoc-signed binary straight into `~/Library/Audio/Plug-Ins/{VST3,Components}`
on every local build. Convenient for smoke-testing in a DAW immediately after
building, but it means a local dev build silently replaces whatever plugin
build you had installed for regular use.

### 4. Package, sign, notarize

```bash
VERSION="vX.Y.Z-mac" ./build-dmg.sh plugins
```

### 5. Publish

Tag and release from `mac`:

```bash
git tag -a vX.Y.Z-mac -m "gary4juce vX.Y.Z-mac"
git push origin mac
git push origin vX.Y.Z-mac
gh release create vX.Y.Z-mac gary4juce-vX.Y.Z-mac-{AU,VST3}.dmg \
  --title "gary4juce vX.Y.Z-mac" --notes-file release-notes-vX.Y.Z-mac.md
```

`docs/updates/gary4juce/stable-mac.json` exists on **both** `mac` and `main`,
but only the copy on `main` is live — GitHub Pages serves `/docs` from
`main`, not `mac`. Editing the `mac` copy during the merge/prep commit does
not publish anything. At actual publish time, land a separate commit
directly on `main` with the real `published_at` timestamp (from
`gh release view vX.Y.Z-mac --json publishedAt`):

```bash
git worktree add /tmp/gary4juce-main-wt main   # or origin/main, see below
# edit /tmp/gary4juce-main-wt/docs/updates/gary4juce/stable-mac.json and README.md
cd /tmp/gary4juce-main-wt && git add -A && git commit -m "Update stable-mac feed for vX.Y.Z-mac" && git push origin main
git worktree remove /tmp/gary4juce-main-wt
```

Before doing this, make sure the local `main` branch ref is actually current
— `git branch -f main origin/main` first if it's been a while since anything
fetched-and-fast-forwarded it locally. A worktree checks out whatever the
local branch ref points to, not automatically `origin/main`, and a stale
local `main` can be a very long way behind without any obvious warning.

### License/signing order

Any license or notice files placed inside a macOS AU or VST3 bundle must be
added before code signing. Modifying a signed bundle invalidates its signature.
