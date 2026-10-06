# Yuey MIDI import handoff

## Requested UX

Keep **Create unchanged**. Add **MIDI** as a third source beside **Recording / Output**
in **Remix (transcribe/remix)** and **Continue**. Show MIDI inputs only while MIDI is
selected. Other models keep their current audio-only behavior.

Use two compact slots in the existing visual style; no piano roll or MIDI editor:

```text
source    recording    output    midi

melody                         chords
[ drop .mid here       folder ] [ drop .mid here       folder ]
```

- Empty: lane label, subtle outlined drop area, `drop .mid here`, folder icon.
- Folder icon opens a MIDI file picker; dropping and browsing use the same loader.
- Loaded: filename, small clear/remove control; optionally a compact bar count.
- Either slot may be empty. Disable Generate when MIDI is selected and neither
  slot contains valid imported data. Preserve existing busy/service/prompt gates.
- Invalid input does not count as populated; give a short actionable error.
- Dropping a replacement updates only that slot. Switching sources hides the
  section and retains its files; it must not overwrite recording/output audio.
- Respect the supplied file length. No crop UI, note editing, piano-roll display,
  silent truncation, looping, or automatic extension of imported input.
- A small `10 bars · 20s at 120 BPM` summary is useful, not an editing surface.
  The user crops/edits MIDI in their DAW before exporting.

For MIDI Continue, use score continuation and hide/disable the audio-continuation
method selector. Retain the existing choice of added bars or natural length.

## Request routing

Convert MIDI locally to YuE2's native two-voice ABC. **No new yuey.cpp release/API
is required.** Installed v0.2.0 was tested accepting an ABC prefix and extending
it while preserving the imported melody/chord events.

| Operation | Existing endpoint | Score input |
|---|---|---|
| MIDI Remix | `POST /generate` | Complete `abc`; no transcription |
| MIDI Continue | `POST /generate` | `abc_prefix`; planner composes beyond it |

Continue follows existing `continueYueyFromTranscription`: fixed length sends
`target_bars = imported_bars + added_bars`, `ending="outro"`, and the existing
outro policy; natural length uses the existing natural settings. Do not use
the `/continue` audio endpoint for MIDI. Never send `abc` and `abc_prefix` together.

Retain current style, seed, lyrics/instrumental and adapter behavior. Both lanes
populated requires full symbolic conditioning so chord symbols participate.
Melody-only may use melody mode; the current instrumental-adapter policy can
force full mode. Continuation renders the **whole expanded piece afresh**.

## Converter scope and timing

- Use JUCE `MidiFile`; implement in C++ without introducing Python runtime deps.
- Melody: selected input must be a single monophonic part. Preserve pitch and
  beat-domain onset/duration; serialize rests and ties across bar boundaries.
  Tested instrumental input goes in `Ins`, with `Vocal` resting. Vocal-mode
  routing must follow the existing instrumental/lyrics choice and needs testing.
- Chords: convert simultaneous note groups to supported quoted ABC chord symbols.
  Symbols live on `Vocal`, including while Vocal rests; native MIDI export routes
  them to `chords.mid`. Do not put polyphonic chord stacks in `Ins`.
- Either lane alone is valid. Chords-only must serialize two resting melody lanes
  with chord symbols, rather than inserting dummy melody notes.
- Initial proven chord scope: simultaneous root-position major/minor triads,
  including doubled roots. Reject ambiguous/unsupported shapes rather than guess.
- Reject unsupported polyphonic melody, off-grid rhythms, mixed/multiple note
  tracks or expressive bends/controllers with a concise explanation. Support can
  broaden separately; don't silently discard musical content.
- Align both files at MIDI tick zero. If populated files differ in musical
  length/meter, report it rather than silently crop, repeat, or guess alignment.
- Set ABC `Q:` from the effective UI/host BPM. Preserve beats when BPM changes;
  do not convert MIDI timestamps to seconds first. MIDI tempo metadata is optional.
  Complete `abc` cannot be combined with backend `planning` controls.
- Preserve sounding pitches when spelling a key; changing a serialization key
  must not implicitly transpose notes. Use file meter when supported, UI meter
  as a missing-metadata fallback. Initial tests were 4/4 and needed no quantization.

## Integration points

- `Source/Components/Yuey/YueyUI.{h,cpp}`: source selector, conditional slots,
  file picker/drop callbacks, layout and enablement.
- `Source/PluginEditor.Yuey.cpp`: request builders, score continuation and render
  paths. `midiHasNotes` already demonstrates JUCE MIDI parsing.
- `Source/PluginEditor.{h,cpp}`: imported input state and generate gating. Current
  global file drop targets the audio recording buffer; MIDI slots need their own
  handlers. Existing audio-source state is shared with other models: keep MIDI
  selection Yuey-specific rather than changing their source behavior.
- Imported input scores need state independent of current output audio; current
  Yuey score sidecars describe an existing render. Share MIDI input state between
  Remix/Continue and retain it across editor recreation using existing patterns.

## Evidence and focused verification

Spike files are under `C:/dev/gary-localhost-installer/artifacts/`:

- `midi-import-melody-test/`: upstream event compiler sources, MIDI parser,
  exact 68-note roundtrip and BPM retiming checks.
- `midi-import-chord-test/`: root-position chord recognizer, chords-only and
  combined ABC. All 72 chord pitches/onsets/durations roundtripped exactly.
- `midi-import-continuation-test/`: expanded score, audio, preserved prefix checks
  and installed v0.2.0 compatibility check.

Treat these as references, not production-ready import code. Native continuation
ABC can exceed the portable upstream inspector's accepted layout; native exports
were used to verify its prefix. Returned MIDI represents the symbolic plan;
audio can vary, and invented audio notes aren't automatically transcribed back.

Verify empty/one/both slots, drop and browse parity, replacement/clear, hidden
state retention, unsupported input, missing tempo, BPM changes, MIDI Remix and
Continue, unchanged audio-source flows, and layout at the supported window size.
Compare MIDI -> ABC -> native exported MIDI pitches/onsets/durations; don't claim
exact audio adherence from a successful score roundtrip.

## Follow-up fixes

The importer now rejects chord gaps/overlaps, preserves raw same-pitch overlaps
for validation, and compares exact beat lengths across PPQs. Clips must end on
whole bars; partial endings are refused rather than padded.

Standalone Yuey has one shared tempo wheel on all three tabs. Hosted tabs show
read-only project BPM, and imported-slot summaries refresh when host BPM changes.
Audio remix now transcribes first, retimes the score, and renders it through
`/generate`; score continuation retimes its prefix the same way. Audio
continuation inherits source tempo, so its standalone wheel is visible but
disabled. MIDI continuation remains editable regardless of the stored audio
continuation-method choice. These changes use existing backend endpoints.

## Saved Ableton Live Clips

The existing slots now accept `.alc` beside `.mid` and `.midi`, through both
file drops and the picker. `readInputFile` dispatches to the native JUCE gzip/XML
reader. The same dispatch handles session restore. There is no Python runtime
or backend change.

Scope: exactly one MIDI clip, using its underlying note sequence. Instruments
and effects are not loaded. Looping clips import one LoopStart..LoopEnd region;
unlooped clips use LoopStart..OutMarker. Nonzero StartRelative is refused until
its playback semantics are tested. Notes crossing boundaries are clipped to
the saved region and rebased to zero; notes outside are ignored intentionally.
Muted notes remain silent. UI/host BPM determines the score tempo.

Audio clips and identifiable DrumGroupDevice clips are refused. Active groove,
clip envelopes, per-note expression and probabilistic notes/groups are refused.
Existing monophony, root-position triad, straight-grid, meter and exact-length
checks remain authoritative. Other drum plugins aren't identified by pitch;
the user still needs to provide pitched melody or chords.

The local Live 12.4.6 `Instrument Melody.alc` produces exactly the reference
ten-bar, 68-note ABC. Synthetic fixtures cover boundaries, loops, both lanes,
muting, unsupported features and malformed input. A Live 12.0 factory Drum Rack
clip is also rejected explicitly. Factory device state isn't copied into tests.

Windows JUCE receives CF_HDROP file paths or CF_UNICODETEXT. No Ableton-private
drag protocol has been added. The maintainer verified saved-browser .alc drops
in Live: drag a timeline clip into the User Library first, then from the
browser into Gary. Direct timeline drags do not offer a file to the plugin.
