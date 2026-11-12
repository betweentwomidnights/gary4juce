# dragged audio generation details

in **audio storage**, set **generation details** to **on - save matching JSON
generation details**. each audio drag then keeps its timestamped filename and
gets a matching JSON file in `dragged_audio`, for example:

```text
gary4juce_1794012345678.wav
gary4juce_1794012345678.json
```

this works with WAV and FLAC. only the audio file is handed to the DAW; the JSON
stays beside the copy in gary's storage folder. the setting defaults to off.

the JSON holds the service, operation, model, full prompt and request settings,
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
turning the setting on later still works. the JSON also includes the export's
filename and hash. source audio, the same models and compatible runtime settings
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
undo provenance, WAV/FLAC exports, editor recreation and rejection of stale
metadata after audio is replaced. it doesn't contact a generation backend.
