# Yuey MIDI import review

Reviewed `feature/yuey-midi-import` at `1daf072`, against rc.2 `d9ef397`.
Product code, commits and PRs were not changed by this review.

The initial review found three silent input-conversion bugs. All three are
resolved in the follow-up below; the findings are retained as review history.

## Findings

### P2: Chord release times are discarded

`Source/Yuey/YueyMidiScore.cpp`, `readChords`, lines 189–200.

The function checks equal durations within each simultaneous chord, but stores
only onset and symbol. It never checks one chord's release against the next
onset or the clip end. `chordBar` holds that symbol until the next change/end.

Reproduced with PPQ 96, one 4/4 bar: C-major triad at tick 0 lasting 96 ticks,
G-major triad at tick 192 lasting 96 ticks, clip end 384. The importer accepts
it and writes `"C"z16"G"z16`, sustaining each chord for 192 ticks and filling
the two intentional gaps. A C chord lasting 288 ticks overlapped by G at 192
is also accepted and shortened into exactly that same ABC.

Within the current narrow scope, reject gaps, overlaps and a chord releasing
before the declared clip end. Alternatively represent those changes exactly
if the native dialect supports a tested implementation. Add note-event
roundtrip tests covering releases, not merely symbol/onset parity with upstream.

### P2: JUCE repairs same-pitch overlap before validation

`Source/Yuey/YueyMidiImport.cpp`, line 41 (`midi.readFrom(stream)`).

JUCE defaults `createMatchingNoteOffs` to true. Its `updateMatchedPairs` inserts
a note-off when another same-pitch note-on appears, even when the original
note-off occurs later. The reader therefore sees modified events and its
monophony check can no longer detect the original overlap.

Reproduced using a raw SMF with two C notes on channel 1, intended as tick
0..192 and 96..288. The reader returns 0..96 and 96..192; the converter accepts
them. Both note lengths changed silently. Different-pitch overlap tests miss it.

Read raw events with `readFrom(stream, false)` and let the importer's pairing/
validation reject unsupported overlap. Add a raw same-pitch-overlap fixture
so the test writer does not repair it before the reader sees it.

### P2: Different lengths pass when rounded bar counts match

`Source/Yuey/YueyMidiScore.cpp`, lines 408–416.

The pair check compares ceiling-rounded bar counts, not actual clip lengths in
beats. Reproduced with a 288-tick melody file and 384-tick chord file at PPQ 96
in 4/4. Both round to one bar, so Generate is allowed and the shorter input is
padded despite the stated no-extension/length-mismatch policy.

Compare end positions as exact beat fractions across differing PPQs before
rounding. Also make partial-bar treatment explicit: reject unsupported lengths
rather than silently completing the bar if the no-extension policy is retained.

## Additional UI observation

Host tempo changes can leave imported-slot summaries stale. The editor timer
calls `YueyUI::setBpm`, which deliberately suppresses `onPlanningChanged`.
`refreshYueyMidi` therefore does not run on that path; the displayed duration/BPM
can remain at the old tempo, while `buildYueyMidiScore` uses the current host BPM
when Generate is pressed. Refresh cached summaries when the effective tempo
actually changes; avoid rebuilding them on every 50 ms tick.

The standalone tempo control and instrumental-off melody routing are still the
agent's acknowledged UX/product questions. They are distinct from the three
reproduced conversion bugs above. Invalid-slot fallback also remains an explicit
behavior choice: a usable lane can generate while the other shows an error.

## Verification completed

- Clean Debug/x64 shared-code **Rebuild** through MSBuild: passed.
- Fresh converter build/run: **118 checks, 0 failures** (41 fixture rows,
  including the randomized melody/combined cases and spike inputs).
- Fresh reader build/run: **22 checks, 0 failures** on the actual Downloads files.
- Fresh UI snapshot build/run: eight states rendered. Inspected the compact
  mismatch/error and MIDI Continue layouts; no blocking layout defect found.
- Additional converter and raw-file reader reproductions confirm all three
  findings. Existing tests passing does not cover these cases.

Reproduction source, build scripts, build log and snapshots are retained at
`C:/dev/gary-localhost-installer/artifacts/yuey-midi-review/`:

- `converter_repro.cpp`, `build-review.cmd`: chord gaps/overlaps and length mismatch.
- `same-pitch-overlap.mid`, `reader_repro.cpp`, `build-reader-repro.cmd`:
  same-pitch note truncation. `prepare_reader_repro.py` recreates that raw SMF.
- `rebuild-shared.cmd`, `shared-rebuild.log`: clean Windows compilation.
- `snapshots/`: eight freshly generated layouts.

Not verified: real DAW/plugin drag-and-drop, native file-picker interaction,
session restore in a live host, a full render initiated through the plugin UI,
or a macOS build. The offscreen snapshot test does not exercise actual mouse,
file-drop, enablement or networking behavior.

## Fix verification

All three findings above are fixed in the working tree on
`feature/yuey-midi-import`. New regressions cover chord gaps, overlaps and early
release; raw same-pitch overlap; exact length comparison across PPQs; and
partial-bar refusal. Supported spike/random fixtures still match upstream ABC.

Standalone tempo editing is available on all three tabs. Hosted tabs use a
read-only project BPM label (including fractional/out-of-wheel-range values),
and MIDI summaries refresh only when the effective host tempo changes. Audio
remix uses `/transcribe` then `/generate` so the score can use the chosen tempo;
score continuation retimes its prefix too. Audio continuation retains source
tempo and disables standalone editing. No backend API change is required.

Verified:

- Clean Debug/x64 shared-code Rebuild: passed.
- Debug/x64 solution build, including standalone, VST3 and manifest: passed.
- Converter: 132 checks, zero failures, including 41 upstream fixture rows.
- Reader and score-tempo edits: 34 checks, zero failures.
- UI: 18 checks, zero failures; 12 compact-layout snapshots rendered and the
  standalone Create, hosted Create and MIDI Continue layouts inspected.

Build logs and screenshots are in the review artifact directory above:
`targets-fixed.log`, `shared-rebuild.log`, and `snapshots-fixed/`.
Live DAW interaction, session restore, file picker/drop and a complete plugin
transcribe/render round trip remain unverified; compilation and offscreen UI
checks do not establish those behaviors.

## ALC extension

Native .alc decoding and the standard file-drop/picker paths now reuse the MIDI
score converter. Tested against the user's saved 68-note melody (exact reference
ABC) and a factory Drum Rack clip (explicit rejection), plus synthetic clip
regions, loops, melody/chord pairing and unsupported/malformed data. The short
MIDI file in Downloads has now been correctly cropped to 68 notes; reader tests
use `melody_test.mid` for the original 443-note partial-bar case and verify the
cropped short MIDI separately.

This is saved-file support. The maintainer subsequently verified saved-browser
.alc drops in Live; timeline clips must be saved to the User Library first.
Windows JUCE's standard drag bridge accepts file paths or text; no proprietary
or virtual-file protocol is assumed.

## Wide layout follow-up

Yuey now leaves the editor's shared dark-grey panel visible, measures its actual
content height in every sub-tab, and reports height changes to the wide editor.
MIDI summaries and wrapped errors grow the panel; short windows retain scrolling.
No temporary tall viewport size or unused scrollbar gutter remains.

Converter: 132 checks passed. MIDI/ALC reader and tempo: 103 checks passed.
UI: 49 checks passed, including wide audio/MIDI states, dynamic slot heights,
returning to the prompt, compact scrolling and shared background pixels.
