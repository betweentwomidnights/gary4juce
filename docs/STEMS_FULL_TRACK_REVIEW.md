# Full-track imports and model input limits

Implemented 2026-10-06 on `feature/full-track-audio-import` after reviewing
gary4juce, gary4local and stems.cpp v0.1.2.

## Behavior

Drop or choose a mono/stereo audio file in the recording buffer. Up to ten
minutes imports directly, independent of the selected model and backend. Larger
files open the existing selection window, whose maximum selection is ten minutes.
Double-click the buffer to choose a shorter section. The window starts at the
active model's maximum input duration (or the file length, if shorter), and both
handles enforce that maximum. The original file remains
the source for later reselection, so choosing a section does not edit the file.

Live recording remains five minutes. Long imports grow buffer storage on demand;
ordinary instances retain the existing allocation. Replacing a long import with
shorter audio releases that expanded allocation. Imports are not marked saved
until the WAV replacement succeeds.

When the buffer exceeds the current model's input limit, the status area shows
an orange hint: the full track is usable for stems; double-click to select a
shorter model input. Request handlers validate the saved file's duration before
encoding/uploading it, without silently cropping. SA3 continuation still checks
source plus added duration against its total limit. The output crop tool handles
an oversized output source.

Carey and SA3 use 380 seconds local and 240 remote, for source audio and output
duration. SA3 continuation reserves source duration within that total. Gary/Terry
retain 30-second input windows; Yuey and Foundation retain the prior 240-second
import ceiling. Darius retains its bar-aligned context-copy behavior. MIDI and
pure text generation are independent of these audio input checks.

## Stems and memory

Stems run inside gary4juce on the user's computer, including when generation
uses the remote backend. Neither a gary4local change nor a stems.cpp release
is required.

No checkboxes were added. HTDemucs computes all its sources together; the
published RoFormers infer vocals and derive instrumental by subtraction.
Selecting fewer outputs does not skip their inference.
[Upstream Demucs confirms the two-stem limitation](https://github.com/facebookresearch/demucs#separating-tracks).
The four-member `htdemucs_ft` bag could potentially skip unused members with a
future source-mask extension; that is a separate runtime feature.

The frontend reads separator input and writes output in 16,384-frame blocks,
avoiding additional full-track copies. Stem previews read peaks from disk using
bounded buffers instead of retaining every decoded stem. Allocation failures
report a failed job; RAII releases runtime contexts/results on exceptions.
Cancellation also applies while reading and writing.

Neural graphs use fixed overlapping segments (HTDemucs: 7.8 seconds). Longer
tracks increase work and CPU buffers rather than graph dimensions. The API still
holds full separation results in memory; it is not unlimited streaming. GPU
memory was not profiled here. Kostas's integrated AMD GPU shares system RAM and
needs a real long-track test.

## Validation

- Windows Debug and Release standalone/VST3 builds succeed.
- Integration harness: 39 checks pass, covering ten-minute boundaries, final
  samples, saving/playback, repeated host preparation, releasing expanded
  storage, 330-second file import/resampling, request guards and a waveform
  peak at an uneven file ending.
- The real JUCE service wrapper separated a 330-second stereo/44.1 kHz synthetic
  source with installed v0.1.2/F16 HTDemucs. All four WAVs preserve the complete
  frame count, sample rate and channel count.
- Existing MIDI reader/tempo suite: 103 checks pass.
- Final import/UI-only run: 50 checks pass, including local/remote duration sliders, continuation totals,
  source guards and the selection dialog's initial length and both handle caps.
- Compact/wide editor renders check the long-input hint.

An earlier native CLI test on RTX 5070 Laptop/Vulkan measured 32.3 seconds for
330 seconds of synthetic audio, 1,832 MiB peak working set and 2,186 MiB private
commit. These are CLI figures, not plugin/AMD performance or quality claims.
Repro files are in the sibling installer workspace's
`artifacts/stems-full-track-review/`.

Run `tests/audio_import/build-full-track-test.cmd` after building Debug shared
code. Arguments: the 330-second fixture WAV, an isolated test directory, and the
gary data directory containing the installed separator/model. The helper sets
the existing debug storage override, including isolated updater preferences.
Optional fourth argument `--skip-stems` rechecks import/UI without more inference.

Ableton drag/drop, interactive selection and AMD long-track performance remain
manual validation before merging/releasing.

## Longer generation

SA3 Medium is documented at approximately 380 seconds, not 388. The local SA3
API default and Carey completion wrapper now allow 380 seconds to match the
frontend. These backend updates must ship with the next gary4local release.
ACE-Step advertises up to 600 seconds with GPU-dependent limits; the frontend
retains a 380-second local ceiling. Treat further extensions as model-specific
work rather than a global longer-local limit.

Sources: [Stability's SA3 duration comparison](https://stability.ai/explainers/ai-temp-track-replacement-generating-music-for-a-rough-cut),
[ACE-Step GPU compatibility](https://ace-step.github.io/ACE-Step-1.5/en/GPU_COMPATIBILITY).
