# gary4juce changelog

older release notes for gary4juce. the README keeps the current release notes
near the top so it does not turn into a museum hallway.

## v4.0.3 - carey seeds and SA3 polish

v4.0.3 adds reproducible seed controls to Carey so supported Carey workflows
can reuse a known seed and show the last seed returned by the backend.

this release also fixes two SA3 UI edge cases. restored sessions that reopen
directly to the SA3 subtab now refresh available LoRAs correctly, and dragging
audio in with SA3 active now keeps the longer SA3/Carey-style selection window
instead of falling back to the shorter model limit.

the Windows VST3 ZIP now nests license and Corresponding Source files inside
`gary4juce.vst3`, so Windows **Extract All** can target a VST3 folder without
leaving loose files beside the plugin.

## v4.0.2 - open-source licensing and source access

v4.0.2 explicitly releases gary4juce under the GNU Affero General Public
License v3.0 only. the repository now includes the canonical AGPLv3 text,
copyright and SPDX notices, pinned JUCE 8.0.8 licensing information, and an
in-plugin about dialog with direct access to the source and license.

release packages now include the applicable license and third-party notice
files plus exact links to the Corresponding Source used for the build. this
release does not change any music models, request formats, or backend
requirements.

## v4.0.1 - UI persistence and localhost responsiveness

v4.0.1 is a focused maintenance release. it does not add or change any music
models. its purpose is to make the plugin remember its UI settings when the
editor is closed and reopened, including when a DAW temporarily removes the
editor while navigating between plugins.

settings now persist across editor reopens throughout Gary, Jerry, SA3, Terry,
Carey, Darius, and Foundation-1. this includes prompts, generation controls,
selected tabs and models, advanced sections, SA3 seeds and LoRAs, and the shared
recording/output source selector.

the local service status also survives editor recreation. on Windows, local
health checks now update per service and bypass the slower shared HTTP path, so
a running Gary, Terry, Jerry, Carey, Foundation-1, or SA3 service is reflected
in the UI immediately instead of waiting for every offline port to time out.

## v4.0.0 - stable-audio-3

gary4juce entered v4 with a new **sa3** sub-tab inside Jerry, positioned
alongside the original SAOS and Foundation-1 workflows.

SA3 includes:

- **generate** - text-to-audio up to 300 seconds, plus 4/8/16-bar loop mode
- **transform** - restyle the recording buffer or current output audio
- **continue** - continue the recording buffer or current output audio to a target total duration
- **seed recall** - random generations show the backend-returned seed so a take can be reproduced
- **key/scale prompting** - optional Carey-style key and mode dropdown appended to the final prompt
- **LoRA sliders** - one strength slider per available SA3 LoRA, defaulting to 0
- **smart dice** - prompt rolls come from the default pool or from every LoRA whose slider is above 0

practical guide: [SA3.md](SA3.md)

launch notes:

- SA3 outputs can be hot, especially with LoRAs. treat gain staging like part of the instrument for now.
- continue results can leave a quiet/fading tail near the end of longer continuations. this is being audited against the upstream SA3 UI.
- local SA3 is available in gary4local on Windows and macOS, including LoRAs and both continuation modes.

## v3 highlights

v3 brought Carey, Foundation-1, and the first pass at the modern multi-model
workflow:

- Carey joined with lego, complete, cover, extract, lyrics, language, key/scale, time signature, LoRA selection, LoRA dice captions, and caption popouts.
- Foundation-1 became `rc-jerry`, a structured BPM/key-aware loop generator inside the Jerry tab.
- Foundation-1 landed in gary4local mac on Apple silicon.
- plugin-safe update checks and editor lifecycle hardening made the app much harder to crash during in-flight requests.

carey guide: [CAREY.md](CAREY.md)
