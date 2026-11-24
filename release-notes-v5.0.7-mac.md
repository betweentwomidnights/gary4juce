- **one gary4juce on every platform.** the Mac DMGs now build from the same source as Windows and Linux. local mode pairs with the unified [gary4local v0.7.6](https://github.com/betweentwomidnights/gary-localhost-installer/releases/tag/v0.7.6), which runs every service natively on Metal and replaces gary4local-mac. it needs an Apple Silicon Mac; on an Intel Mac the plugin still works with the remote backend.
- **staying on gary4local-mac for now?** update it to [v0.4.2](https://github.com/betweentwomidnights/gary-localhost-installer-mac/releases/tag/v0.4.2), released alongside this one. it keeps local jerry working with this version; v0.4.1 and earlier don't.
- **output shows as playing once your DAW renders it**, not as soon as you press play. in GarageBand, put gary on the master track; it keeps processing there, and gary shows a hint if no audio callback arrives.
- previewing a stem or playing the saved buffer cancels an output play that's still waiting on the host, so the wrong player can't light up or move its cursor.
- **local mode can reach gary4local on another machine**: set `gary4local host...` in gary settings to a hostname or IP. leave it empty for this computer.
- localhost connection errors for gary, terry and jerry point at gary4local instead of Docker Compose.

All three downloads contain universal x86_64/arm64 binaries and include the project license, third-party notices, and exact Corresponding Source information.

[changelog](https://github.com/betweentwomidnights/gary4juce/blob/v5.0.7-mac/docs/CHANGELOG.md)

Included DMGs:

- `gary4juce-v5.0.7-mac-AU.dmg`
- `gary4juce-v5.0.7-mac-VST3.dmg`
- `gary4juce-v5.0.7-mac-STANDALONE.dmg`

SHA-256:

- AU: `61a410864cfd2074bd9b8277dffc5109f8dbada4baace941675633bd1ac45db6`
- VST3: `64a1a1b0eb43a5ad3da475ee3f54581cc5a8a78737ce4667d0ff6dd8692cf40f`
- Standalone: `eea70b5cd01b26b8a223935594329f7dcf14ad91c4d3084113ed3dd5a71e7416`

Recommended gary4local:
https://github.com/betweentwomidnights/gary-localhost-installer/releases/tag/v0.7.6

Close your DAW and the standalone app before replacing files with a downloaded update.
