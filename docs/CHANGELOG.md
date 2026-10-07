# gary4juce changelog

release history for gary4juce. the README keeps the current release notes near
the top so it does not turn into a museum hallway.

## v5.0.0 - 2026-10-07 - yuey and stems

**yue2 is now inside the DAW.** the yuey tab runs
[YuE2](https://github.com/multimodal-art-projection/YuE) through
[yuey.cpp](https://github.com/betweentwomidnights/yuey.cpp), a native C++/GGML
build of the model, on the remote backend and on Windows gary4local v0.4.0
(CUDA on NVIDIA, Vulkan on AMD and Intel). there's no Python in it at all, and
it's the first of the native runtimes every gary4local service is moving to.

it doesn't work quite the way official YuE does, on purpose. create renders
over a chord scaffold built from your tempo, key and bar count, rolled from a
pool of familiar progressions, instead of waiting 40 seconds to four minutes
for the model to plan a whole song. "let yuey plan" brings the planning back,
and key "none" drops both. remix transcribes your audio with SheetSage2 and
renders the score again; continue can work from the score or from the audio
itself. every render keeps its score, which you can drag in as MIDI or edit in
the score window, with quick edits for tempo, transposition, lanes and chords.
[docs/YUEY.md](YUEY.md) goes through each of these choices.

instrumental is still best effort. YuE2 can sing when the score says not to,
which is what the instrumental LoRA is for, and gary4local uses it when it's
downloaded.

sa3's generate and continue also get an "ending" choice. "ends here" asks SA3
to plan the ending where the audio stops. "keeps going" composes six seconds
past the end and throws them away, so the audio cuts off in full swing; chain
continuations on it, then switch the last one to "ends here" for an outro.

**stems are built in.** the output waveform and the recording buffer each get a
`stems` handle that splits the audio into stems on your own computer, with
[stems.cpp](https://github.com/betweentwomidnights/stems.cpp), our native
C++/GGML build of HTDemucs and the RoFormer separators. there's no Python and
no gary4local in it, nothing is uploaded, and it works the same with the remote
backend. the runtime (about 19 MB on windows, 6 MB on mac) installs from the
settings menu, from stems.cpp's GitHub release and checked against hashes
pinned in the plugin. the models download from Hugging Face: htdemucs (four
stems, six with guitar and piano, or the slower fine-tuned bag) and two
RoFormers that split vocals from everything else. on windows it runs on Vulkan,
so NVIDIA, AMD and Intel GPUs all work, with a toggle for the CPU. on apple
silicon it runs on Metal, and intel macs are CPU only.

stems are ephemeral. the popup works in its own temporary folder, deletes it
when it closes, and keeps only the stems you drag out. dropping a stem on the
recording buffer replaces it, which is how you pull the drums out of something
you dragged in from the DAW. one of the RoFormers, viperx's, has no license
upstream, and the readme says so.

**carey's extract tab is hidden.** the stem separator does what extract was
attempting, and does it better, so the tab is gone from carey. the code is still
there behind one switch, in case something changes for ace-step, but i expect to
remove it. if you closed a session with extract selected, it reopens on lego.

**the yuey prompt is shared across create, remix and continue.** each sub-tab
used to keep its own, and the score window had none, so re-rendering a score
sent whatever the create tab held, which was nothing for a score that came out
of a remix. there's one prompt now: switching sub-tabs keeps it, and the score
window shows the same box and dice. if an older session saved three different
prompts, it restores the one from the sub-tab you were last on.

**the audio selection window zooms.** the magnifier buttons halve or double the
visible range down to about one sample per pixel, ctrl or cmd plus the wheel
zooms around the mouse, and the wheel alone pans. a click on the waveform seeks,
as it does on the output.

a job no longer sits frozen when the network drops its result partway through.
yuey renders on the remote backend kept parking at 91-95 per cent with the
result still waiting on the server: on Windows, a read the network had
abandoned could block indefinitely, and the plugin never noticed. the result
poll now has a deadline and a watchdog that cancels a stuck read and asks
again. every tab shares that poll, so it isn't only yuey that benefits.

**MIDI and Live Clips can be the source.** remix and continue have a third
source choice with melody and chords slots. the importer accepts straight-grid,
monophonic melodies and root-position major/minor triads; unsupported input gets
an explanation. saved Ableton MIDI clips carry notes only. save a timeline clip
to the User Library before dragging it from Live's browser. standalone tempo is
editable on every yuey sub-tab; hosted tabs show the project BPM read-only.
the wide panel now fits those controls on the shared dark-grey background.

**full-track stem imports go up to ten minutes.** audio and stems are read and
written in bounded blocks, and waveform peaks come from disk. the recording
buffer grows for imports; live recording stays at five minutes. generation
keeps each model's limit, shown by an orange hint and checked before upload.
double-click selection starts at that limit and cannot expand beyond it.
local SA3 and Carey allow 380 seconds, while remote requests stay at 240.
the local connection indicator shows the seven managed services as `/7 online`;
Darius keeps its separate backend.

pair this with [gary4local v0.4.0](https://github.com/betweentwomidnights/gary-localhost-installer/releases/tag/v0.4.0).
after updating the app, press `update runtime` on yuey's row to install
[yuey.cpp v0.2.2](https://github.com/betweentwomidnights/yuey.cpp/releases/tag/v0.2.2).
that fixes the short MIDI score failure reported as
`invalid YuE2 AR sampling configuration`.

## v5.0.0-rc.4 - full-track audio import

the recording buffer accepts imports up to ten minutes for stem separation,
independently of the selected model or backend. longer files open the selection
window. the buffer grows only when needed; live recording remains five minutes.
an orange hint explains when a model needs a shorter section, and requests
validate the source duration before upload. double-click selection starts at the
active model's input limit and cannot expand beyond it. SA3 and Carey allow up
to 380 seconds on localhost; their remote limits remain 240 seconds.
no stem checkboxes: the shipped Demucs and RoFormer models would
still perform the same inference.

stem input/output uses bounded blocks, and the popup builds waveform peaks from
disk rather than keeping every full stem decoded in memory. long imports are
saved and played in full, with no silent five-minute truncation.

the local connection indicator now reads `/7 online`, matching the seven
managed services. Darius keeps its separate backend and health indicator.

pair this with [gary4local v0.4.0-rc.4](https://github.com/betweentwomidnights/gary-localhost-installer/releases/tag/v0.4.0-rc.4)
or [gary4local-rocm v0.4.0-rocm.4](https://github.com/betweentwomidnights/gary-localhost-installer/releases/tag/v0.4.0-rocm.4).
after updating gary4local, press `update runtime` on yuey's row for v0.2.2's
short MIDI score fix. the plugin still reports version 5.0.0; the tag identifies
this preview.

## v5.0.0-rc.3 - MIDI and Live Clips for yuey

this Windows pre-release includes the embedded stem separator, selection-window
zoom and shared yuey prompt from rc.2, plus MIDI as a source for yuey's remix
and continue tabs. choose `midi`, then drop or pick a melody file, a chords file,
or both. remix renders the imported score; continue extends it before rendering.
create stays driven by the prompt, key and BPM.

saved Ableton MIDI Live Clips (`.alc`) work too, as notes only: save a timeline
clip to the User Library first, then drag it from Live's browser into Gary.
instruments and effects are not imported. Drum Rack clips, active groove,
envelopes, per-note expression and probability are refused. straight-grid,
monophonic melody and root-position major/minor triads are supported. crop to
whole bars, and give both lanes the same length; unsupported input gets an
explanation rather than silently changing the score.

standalone now shows the BPM wheel on remix and continue as well as create.
hosted yuey tabs show the project's BPM read-only. MIDI, audio transcribe/remix
and score continuation render at that tempo; audio continuation follows its
source. imported file paths are saved with the session and reread on restore.

the wide yuey panel uses the same dark-grey background as the other models and
fits its visible controls, growing for MIDI summaries or errors. the prompt
stays at the top when everything fits, with no unnecessary scrollbar. compact
windows still scroll when they need to.

the plugin still reports version 5.0.0; the release tag identifies this preview.
the stable download links and updater feeds remain unchanged.

## v4.0.15

adjusted duration limits for sa3 and ace-step.
added the official RoyalCities sampler type to foundation-1.

if you were fine with durations and don't use foundation-1 much, you can go
ahead and skip this release.

## v4.0.14 - clearer SA3 continuations

SA3's continue slider now chooses how many seconds of new audio to add and
tracks the selected recording or output source. This pairs with the backend
continuation improvements in gary4local v0.3.1.

## v4.0.13 - gary's advanced controls

gary's tab has an advanced section now: cfg, top k, a description box, and a
seed that works the same way terry's does. the defaults match what the plugin
used to send, so nothing changes unless you open it.

gary also says when it's downloading a model instead of sitting on
'processing audio...' for the length of a multi-gigabyte pull.

jerry's SAOS tab used to sit on 'loading models...' on a fresh instance until
you left the tab and came back, and then drove a finetune with the standard
model's cfg and steps. both fixed.

## v4.0.12 - minor Terry and preset fixes

Terry can now reuse an exact seed. Jerry's SAOS model list now refreshes
correctly after loading presets, including older presets saved while models
were still loading.

## v4.0.11 - waveform ranges and FLAC drag storage

the recording buffer and output waveform now share the same double-click
start/end range editor, making it possible to trim either source directly in
gary4juce.

storage settings can now create lossless FLAC files in `dragged_audio` while
keeping WAV as the compatibility default and as the internal/backend format.
the FLAC option includes guidance for DAWs that do not document FLAC import.

REAPER preset restores now revalidate backend health so an open plugin does
not remain stuck on a stale disconnected status.

## v4.0.10 - file picker and preset fixes

the recorded-audio file picker now remembers the last folder you used.
saved presets in REAPER now update open plugin instances like they should.

## v4.0.9 - wide mode for our friends who hate scroll bars

v4.0.9 adds an optional wide layout with the input and output audio on the
left and model controls on the right. compact mode is still available from the
new settings menu, and the selected layout is remembered.

## v4.0.8 - input playback, standalone polish, and movable storage

v4.0.8 adds input-buffer playback in both the plugin and standalone app, plus
exact segment selection for short input audio. standalone tempo controls now
share one persistent BPM wheel across the relevant tabs, including Carey
complete mode, with steadier Foundation-1 layout when its BPM warning appears.

the `gary4juce` audio folder can now be moved out of Documents to another
location or external drive. migration reports progress, keeps the original
copy, preserves conflicting files, and uses recovery storage when the chosen
location is unavailable. the standalone app now ships beside the VST3 as a
separate asset in the regular Windows release.

## v4.0.7 - popup lifecycle cleanup

v4.0.7 makes plugin-owned popups and asynchronous callbacks shut down safely
with their editor or tab. the audit covers update reminders, backend/support
dialogs, prompt and lyrics popouts, preset choosers, audio-selection windows,
and related popup menus.

backend outage messaging now also distinguishes local gary4local failures from
the self-hosted remote backend and provides the appropriate recovery guidance.

## v4.0.6 - terminal failure cleanup

v4.0.6 is a small reliability release. terminal generation and polling failures
now use one cleanup path, so malformed responses and failed jobs return the UI
to a ready state consistently instead of leaving stale generation controls
behind.

## v4.0.5 - SA3 default tuning and drag handoff

v4.0.5 changes SA3's default distribution shift to `logsnr` and lowers the
default transform strength to `0.5` for more controllable transformations.

dragging generated audio to a DAW now preserves the handoff file long enough
for the host to receive it reliably.

## v4.0.4 - Carey lego LoRAs and SA3 prompt popouts

v4.0.4 adds LoRA selection and strength controls to Carey's lego mode, with
the selected adapter persisted with the rest of the editor state. it also adds
prompt popouts to SA3 generate, transform, and continue workflows, making
longer prompt editing more practical inside a DAW session.

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
