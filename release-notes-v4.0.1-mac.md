gary4juce v4.0.1-mac brings the v4.0.1 plugin line to macOS in AU and VST3 form.

This is a focused maintenance release. It does not add or change any music models. Its purpose is to make the plugin remember its UI settings when the editor is closed and reopened, including when a DAW temporarily removes the editor while navigating between plugins.

Settings now persist across editor reopens throughout Gary, Jerry, SA3, Terry, Carey, Darius, and Foundation-1. This includes prompts, generation controls, selected tabs and models, advanced sections, SA3 seeds and LoRAs, and the shared recording/output source selector.

The local service status also survives editor recreation. On macOS, this release also makes the GarageBand output-playback edge case clearer by waiting for a real host audio callback before presenting waveform playback as active.

Included DMGs:
- AU
- VST3

Standalone is intentionally omitted from this release while we continue stabilizing it.

Recommended gary4local version:
https://github.com/betweentwomidnights/gary-localhost-installer-mac/releases/tag/v0.1.11

Close your DAW before replacing plugin files with a downloaded update.
