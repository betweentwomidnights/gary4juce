- **YuE2 inside your DAW**: create in your tempo and key, transcribe/remix, or continue from a score or audio. edit the score and drag its lanes out as MIDI. [yuey guide](https://github.com/betweentwomidnights/gary4juce/blob/v5.0.0-mac/docs/YUEY.md)
- **MIDI and saved Ableton clips** can drive remix and continue, with separate melody/chords slots. save timeline clips to the User Library first, then drag the `.alc` from Live's browser. standalone tempo is editable; hosted Yuey tabs display project BPM.
- **stems are built in**, through [stems.cpp](https://github.com/betweentwomidnights/stems.cpp). install its runtime and a model from settings, then separate output or imported tracks up to ten minutes on your own machine. it runs on Metal on Apple silicon and on the CPU on Intel Macs, where htdemucs is the one to use. Carey's extract tab is hidden in favor of this.
- **selection windows zoom and enforce model limits**. local SA3 and Carey allow 380 seconds; remote limits remain 240 seconds. SA3's ending choice can finish a piece or keep it going for another continuation.
- **Yuey shares one prompt** across its tabs and score editor, the wide panel fits the MIDI controls, and result polling recovers when a network transfer stalls.

pair with [gary4local mac v0.4.0](https://github.com/betweentwomidnights/gary-localhost-installer-mac/releases/tag/v0.4.0) for local generation on Apple silicon, including local Yuey through yuey.cpp's Metal runtime.

All three downloads contain universal x86_64/arm64 binaries and include the project license, third-party notices, and exact Corresponding Source information.

[changelog](https://github.com/betweentwomidnights/gary4juce/blob/v5.0.0-mac/docs/CHANGELOG.md)

Included DMGs:

- `gary4juce-v5.0.0-mac-AU.dmg`
- `gary4juce-v5.0.0-mac-VST3.dmg`
- `gary4juce-v5.0.0-mac-STANDALONE.dmg`

SHA-256:

- AU: `8e453e9c8f15eb08c5715f32d7d7c49af87db124d4daa00e455edeeb8065757f`
- VST3: `b7a74051eeade1d6dd365d90e1557879384741215096e8ec8eb7e05ea7bc3920`
- Standalone: `2951f747dc7827269a5fbf7a94aacc27e9462eeefd54737d078944a5d43600f5`

Recommended gary4local:
https://github.com/betweentwomidnights/gary-localhost-installer-mac/releases/tag/v0.4.0

Close your DAW and the standalone app before replacing files with a downloaded update.
