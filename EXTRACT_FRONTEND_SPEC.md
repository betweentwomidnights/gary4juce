# Extract Mode — JUCE Frontend Tab Spec

Extract isolates a single stem (drums or vocals) from source audio. Same submit/poll pattern as lego.

## API Contract

**Submit:** `POST http://ace-step-wrapper:8003/extract`

```json
{
  "audio_data": "<base64-encoded source audio>",
  "track_name": "drums",
  "bpm": 120,
  "guidance_scale": 10.0,
  "inference_steps": 80,
  "batch_size": 1,
  "audio_format": "wav"
}
```

**Poll:** `GET http://ace-step-wrapper:8003/extract/status/{task_id}`

Response while generating:
```json
{
  "success": true,
  "generation_in_progress": true,
  "progress": 42,
  "status": "processing",
  "queue_status": { "message": "generating..." }
}
```

Response on completion:
```json
{
  "success": true,
  "generation_in_progress": false,
  "status": "completed",
  "audio_data": "<base64 result>",
  "audio_format": "wav",
  "track_name": "drums",
  "bpm": 120
}
```

Response on failure:
```json
{
  "success": false,
  "status": "failed",
  "error": "...",
  "retryable": false
}
```

## UI Layout

Add an "Extract" tab alongside the existing Lego/Complete/Cover tabs.

### Controls

| Control | Type | Values | Default | Notes |
|---------|------|--------|---------|-------|
| Track Name | Dropdown | `drums`, `vocals` | `drums` | Only these two are reliable. See note below. |
| BPM | Spinner | 20-300 | from loaded audio | Required |

No caption, no lyrics, no key/scale fields. Extract doesn't use them.

### Track Name Dropdown

Show only `drums` and `vocals` as primary options. These are the only stems that extract reliably on xl-base.

Other stems (`bass`, `guitar`, `piano`, `synth`, `keyboard`, `strings`, `percussion`, `brass`, `woodwinds`, `backing_vocals`) exist in the API but produce inconsistent results — tonal stems bleed into each other. If you want to expose them, put them behind an "Advanced" toggle or a secondary section with a warning label.

### Behavior

1. User loads source audio (same as lego — from the plugin's current audio buffer)
2. User picks track name from dropdown
3. User clicks "Extract"
4. Plugin sends `POST /extract` with base64 audio, gets `task_id`
5. Plugin polls `GET /extract/status/{task_id}` on the same interval as lego
6. On completion, decoded audio replaces the buffer (same as lego/cover/complete)
7. On failure, show error — user can retry. Extract is nondeterministic; ~50% of generations are clean, the rest have artifacts. Retrying is expected workflow.

### Differences from Lego

- No caption field (extract doesn't use it)
- No lyrics field (lyrics actively degrade extract quality — confirmed by testing)
- No repainting start/end controls
- Fixed defaults: 80 steps, CFG 10.0 (higher than lego's 50/7.0)
- Fewer track options (only drums/vocals recommended)
- Results are nondeterministic — some generations will have artifacts. This is normal.
