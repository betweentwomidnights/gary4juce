gary4juce v4.0.7-mac brings the v4.0.5–v4.0.7 windows line over to macOS in AU and VST3 form.

This release carries over three windows maintenance releases:

- **v4.0.7 (popup lifecycle cleanup):** plugin-owned popups and asynchronous callbacks now shut down safely with their editor or tab, covering update reminders, backend/support dialogs, prompt and lyrics popouts, preset choosers, and audio-selection windows. Backend outage messages now also distinguish a local gary4local failure from the self-hosted remote backend and give the right recovery steps for each.
- **v4.0.6 (terminal failure cleanup):** terminal generation and polling failures now go through one shared cleanup path, so malformed responses and failed jobs return the UI to a ready state consistently instead of leaving stale generation controls behind.
- **v4.0.5 (SA3 default tuning and drag handoff):** SA3's default distribution shift is now `logsnr` and the default transform strength is `0.5` for more controllable transformations. Dragging generated audio to a DAW now preserves the handoff file long enough for the host to receive it reliably.

This release is paired with [gary4local mac v0.2.0](https://github.com/betweentwomidnights/gary-localhost-installer-mac/releases/tag/v0.2.0).

Included DMGs:
- AU
- VST3

Standalone is not part of this public mac plugin release.

Recommended gary4local version:
https://github.com/betweentwomidnights/gary-localhost-installer-mac/releases/tag/v0.2.0

Close your DAW before replacing plugin files with a downloaded update.
