- pair gary with headless gary4local over your network: type `listen on` in headless Gary, then enter the address and pairing token it prints in `gary4local host...`. every service goes through its one port, 8020, so you don't need Tailscale or an SSH tunnel on a trusted network. without a token nothing changes.
- the host field takes the listener's address as Gary prints it, `:8020` included. while the listener answers but nothing has been started yet, the status line reads `paired` instead of `disconnected`.
- yuey checks a source's length before uploading it. continuing an output that's already a couple of minutes long gets a popup saying what to crop, instead of rendering for ten minutes.
- when yuey refuses a request, gary shows yuey's reason instead of `yuey submit failed (HTTP 400)`.

pair it with [gary4local v0.7.7](https://github.com/betweentwomidnights/gary-localhost-installer/releases/tag/v0.7.7) on Apple Silicon Macs. on an Intel Mac the plugin still works with the remote backend.

All three downloads contain universal x86_64/arm64 binaries and include the project license, third-party notices, and exact Corresponding Source information.

[changelog](https://github.com/betweentwomidnights/gary4juce/blob/v5.0.8-mac/docs/CHANGELOG.md)

Included DMGs:

- `gary4juce-v5.0.8-mac-AU.dmg`
- `gary4juce-v5.0.8-mac-VST3.dmg`
- `gary4juce-v5.0.8-mac-STANDALONE.dmg`

SHA-256:

- AU: `90062233dc7a7b8ed57c2bf2138cdfaeb5459fbe5003baceeabe23f1757adfcb`
- VST3: `f403c5bc151cb22c46236344f2c6b7756c9043974a77b28c6ac9a422e54abaa4`
- Standalone: `2af6be5313ea3263dd2682eb76501f9fe93ca7908d5a1c7e2475cb52c9b92d03`

Recommended gary4local:
https://github.com/betweentwomidnights/gary-localhost-installer/releases/tag/v0.7.7

Close your DAW and the standalone app before replacing files with a downloaded update.
