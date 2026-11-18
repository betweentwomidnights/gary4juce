# dragged audio generation details

in **audio storage**, choose how **generation details** travel with dragged audio:

- **off - audio only** (default)
- **JSON - save matching generation details**
- **embedded - save generation details in audio tags**

both metadata modes work with WAV and FLAC and keep timestamped filenames.
existing JSON users keep JSON selected after upgrading.

with JSON selected, each audio drag gets a matching file in `dragged_audio`,
for example:

```text
gary4juce_1794012345678.wav
gary4juce_1794012345678.json
```

only the audio file is handed to the DAW; the JSON stays beside the copy in
gary's storage folder.

with embedded selected, no export JSON is created. WAV stores the full record
in its RIFF INFO comment (`ICMT`) and in an ID3v2.4 `GARY4JUCE_METADATA` custom
text tag. ID3 custom text tags also expose `PROMPT`, `SEED`, `MODEL` and `SOFTWARE`;
lyrics use the standard unsynchronised lyrics (`USLT`) frame. FLAC stores the
full record in its Comments field: identical `COMMENT` and `DESCRIPTION` Vorbis
comments support tag editors and Windows Explorer, respectively. `PROMPT`, `SEED`,
`MODEL`, `SOFTWARE` and `LYRICS` remain available as individual tags. lyrics retain
line breaks and Unicode, including non-Latin scripts. only lyrics supplied with
the render are included; this does not transcribe audio.

embedded records use compact JSON without indentation or formatting line breaks,
so metadata viewers don't turn that padding into large gaps. prompt and lyric
line breaks remain intact when the JSON is read. separate JSON files stay formatted
for reading.

which tags an audio app displays or preserves when it re-exports varies.
WAV tagging copies the audio bytes without re-encoding. FLAC tagging preserves
the encoded audio frames after the existing lossless export conversion.

both modes hold the service, operation, model, full prompt, lyrics and request settings,
including sampler settings and LoRAs where the request has them. the actual seed
is included when known: from the explicit request, a plugin-chosen seed, or the
backend's response. a backend that doesn't report its random seed gets `null`,
not a guessed seed from the current controls. upload payloads and credentials
are excluded. conditioning audio is identified by a hash rather than copied
into the JSON.

details belong to the successful render, not to the controls as they look when
you drag. changing tabs or prompts, submitting a request that fails, or closing
and reopening the editor doesn't relabel an existing output. the internal record
is bound to `myOutput.wav` by its hash. older audio, manually replaced audio and
stems without tracked generation details are explicitly marked as unavailable.

cropping preserves the original generation details and appends a crop to `edits`.
each crop's start and duration are relative to the previous output. selecting a
range uses the render shown when the selection dialog opened, even if another
render finishes before confirming. Terry's undo restores the source output's
details when they are known.

a snapshot is maintained for the current output even while export is off, so
choosing either metadata mode later still works. both modes include the export's
filename. JSON also includes the final export's whole-file hash. embedded records
include the verified source audio hash instead: a whole-file hash cannot be
embedded in the same file it hashes. source audio, the same models and compatible runtime settings
may still be needed to reproduce an audio-conditioned result; a seed and prompt
alone aren't always enough.

## regression checks

build the Debug Shared Code target, then run:

```powershell
$testRoot = Join-Path $env:TEMP ('gary-metadata-' + [Guid]::NewGuid())
./tests/output_metadata/build-metadata-test.cmd $testRoot
```

the test requires its isolated storage override and generates its own audio.
it covers request snapshots, resolved seeds, failed and stale requests, cropping,
undo provenance, JSON and embedded WAV/FLAC exports, multiline Unicode lyrics,
unchanged audio samples, legacy preference migration, editor recreation and
rejection of stale metadata after audio is replaced. it doesn't contact a generation backend.
