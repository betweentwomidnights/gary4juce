# gary4juce Auto-Update Plan

## Goal

Give the plugin a friendly "a new version is available" flow without asking a VST3/AU plugin to overwrite itself while the DAW is running.

This should follow the same low-risk Phase 1 pattern that worked well for `gary4local`:

- static manifest JSON as the source of truth
- in-app release notes and update prompt
- browser handoff to GitHub Releases
- no self-install or relaunch logic

## Recommendation

Do not put update metadata behind the model backend, the remote inference stack, or a docker-compose service.

Update checks should stay available even if the backend is down, overloaded, or intentionally offline.

The clean plan is:

1. publish releases on GitHub as usual
2. publish a tiny manifest JSON at a stable URL
3. let the plugin fetch that manifest from the UI side
4. if a newer version exists, show a small update prompt with notes and buttons

## Source Of Truth

Preferred hosting:

- GitHub Pages serving `main /docs`

Planned manifest location:

- `docs/updates/gary4juce/stable.json`

Optional maintainer testing lane:

- `docs/updates/gary4juce/preview.json`

If we ever want a branded URL later, put Cloudflare or a custom domain in front of the same static files. Do not make the updater depend on the inference backend.

## Why The Plugin Should Start With "Notify, Not Self-Install"

`gary4juce` is loaded inside a DAW as a VST3 or AU.

That makes true in-place auto-install awkward because:

- the plugin binary is often in use while the host is open
- different DAWs handle plugin scans and caches differently
- replacing plugin files while loaded is riskier than replacing a standalone app
- users may need to choose the right asset for their platform or format anyway

So Phase 1 should be an update checker, not a self-updater.

## Phase 1 Behavior

- Fetch a small JSON manifest from a stable HTTPS URL.
- Compare the current plugin version against `latest_version`.
- If a newer version exists and it is not skipped, show a small prompt with:
  - current version
  - latest version
  - short changelog from the manifest `notes`
  - `download update` button
  - `not now` button
  - `skip this version` button
- Add a manual `check for updates` entry somewhere sensible in the UI.
- Optionally add an `auto-check on open` toggle if the settings surface already supports it.

Important v1 behavior:

- `download update` should open the GitHub release page in the browser
- do not add a separate `view release notes` button in v1
- do not try to self-install or relaunch from the plugin

## Plugin Integration Notes

- Perform checks from the editor/UI side, not the audio thread.
- Check once per editor session or first UI open, not on every repaint.
- Time out fast and fail silently if offline.
- Do not interrupt users mid-generation just to show an update prompt.
- Keep the copy short and upbeat.

## Persisted State

Store the minimum needed for a smooth experience:

- last check time
- skipped version
- optional `auto-check on open` preference

## Manifest Shape

Use the same simple shape we landed on for `gary4local` Phase 1:

```json
{
  "channel": "stable",
  "latest_version": "3.0.1",
  "download_url": "https://github.com/betweentwomidnights/gary4juce/releases/tag/v3.0.1",
  "sha256": null,
  "published_at": "2026-04-02T00:00:00Z",
  "notes": [
    "Carey cover mode now uses turbo for much better results.",
    "Cover progress reporting is smoother.",
    "Remote error popup now supports copy."
  ]
}
```

Notes:

- `download_url` can point to the GitHub release page instead of a direct asset
- `sha256` is optional for this v1 browser-handoff flow
- `notes` is the main release-notes surface
- no `release_notes_url` field is needed in v1

## Release Workflow

1. Build the plugin release artifacts as usual.
2. Publish the GitHub release.
3. Update `docs/updates/gary4juce/stable.json` with:
   - version
   - release page URL
   - published timestamp
   - changelog notes
4. Commit that manifest update to `main`.
5. GitHub Pages publishes the new JSON.
6. The plugin sees the new manifest on next check and offers the update.

## Optional Preview Workflow

If we want a safer maintainer test lane before touching `stable.json`:

1. publish a preview release on GitHub
2. update `docs/updates/gary4juce/preview.json`
3. point a local/dev build at that preview manifest
4. validate the popup UX and button behavior
5. copy the same flow to `stable.json` when ready

This is optional for `gary4juce`, but it mirrors what worked well for `gary4local`.

## Phase 2

Only consider anything more complex if users clearly need it.

Possible later ideas:

- download the ZIP to Downloads
- show a guided "close your DAW, then replace the plugin" flow
- ship a tiny external helper app later if manual install becomes a real pain point

That is not the target for the first implementation.

## First Implementation Target

Build the `gary4local`-style Phase 1 only:

- static manifest on GitHub Pages
- startup or first-open check
- manual `check for updates`
- update prompt with manifest notes
- `download update`, `not now`, and `skip this version`

That gets most of the user value with very little risk.
