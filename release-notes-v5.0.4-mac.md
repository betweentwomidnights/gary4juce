- **draw SA3 LoRA automation beside the editor.** each active LoRA gets a strength curve, beat/bar snapping and its own prompt, with dice drawn from that LoRA's prompt pool. a four-stop blend runs from smooth to responsive. lanes work for generate, transform and continue, follow output playback and save with the editor state.
- **Carey runs on the new job API.** the generate tab makes audio from a caption, and complete arranges tracks around the recording (lego and complete are still hit or miss). on localhost, pick 2B or XL in each tab's advanced settings.
- **eight more stem models**, through stems.cpp v0.1.5: DeNoise, De-Echo, DeEcho-DeReverb, room dereverb, Kim_Vocal_2 and DrumSep, which splits a drums stem into kick, snare, toms, hi-hat, ride and crash. the stems handle and settings offer to update the v5.0.0 runtime, which can't load them.
- **the saved recording keeps its own lane** while an unsaved take is present, and playback and stem separation use the saved buffer.
- **generation details**: an optional storage setting saves a JSON file beside dragged audio with the model, seed, prompt and request settings.
- gary's model dropdown stays current as local models are downloaded or removed.

pair with [gary4local mac v0.4.1](https://github.com/betweentwomidnights/gary-localhost-installer-mac/releases/tag/v0.4.1), released alongside this one. local Carey and local SA3 lanes need it; gary4local mac v0.4.0 doesn't serve Carey's new API.

All three downloads contain universal x86_64/arm64 binaries and include the project license, third-party notices, and exact Corresponding Source information.

[changelog](https://github.com/betweentwomidnights/gary4juce/blob/v5.0.4-mac/docs/CHANGELOG.md)

Included DMGs:

- `gary4juce-v5.0.4-mac-AU.dmg`
- `gary4juce-v5.0.4-mac-VST3.dmg`
- `gary4juce-v5.0.4-mac-STANDALONE.dmg`

SHA-256:

- AU: `bccee53aed7f63479b82e382f9e69bc7a21cd57141f157f080d0b24d1db248c1`
- VST3: `9a2314e254eb542ebe051447a8f9fd9d8514340f7a7d9a15eb2d7071bdf4fb4b`
- Standalone: `4f9474aeef3a838cfc58b78737d5890b22c995f574cf30b26e96e1b4e2eb61f7`

Recommended gary4local:
https://github.com/betweentwomidnights/gary-localhost-installer-mac/releases/tag/v0.4.1

Close your DAW and the standalone app before replacing files with a downloaded update.
