# macOS localhost compatibility for gary4juce v4.0.14

This is the compatibility gate for using the `mac` branch after its
2026-09-03 merge of `gary4juce` `main` at `a3f5a26` with
`gary-localhost-installer-mac` at `ecb65c8` (v0.2.0 plus one documentation
commit). The Windows reference inspected here is
`gary-localhost-installer` `042bfa0` (the v0.3.1 tag); its current `main` was
`32f2ede`.

## Result

There is no new request that makes the current Mac services fail outright.
The two new `seed` properties are extra JSON fields: the current Gary Pydantic
models ignore them and Terry reads only the properties it knows. Generation,
continuation, retry, and transform therefore continue to submit and poll.

The minimal feature-complete compatibility work is nevertheless two small
seed paths:

1. Gary must accept, resolve, use, store, and echo `seed` for generate,
   continue, and retry.
2. Terry must accept, resolve, use, store, and echo `seed` for its JUCE async
   transform path.

Without those changes, the services still run, but the new seed controls lie:
Gary ignores an explicitly selected seed, and neither Gary nor Terry can fill
the last-seed readout after a random request. Treat both paths as blockers for
shipping a Mac build that presents the new controls.

No other `gary4juce` v4.0.10-v4.0.14 change requires a Mac service change.

## Required Gary contract

The client sends `seed` in all three JSON requests:

- `POST /api/juce/process_audio`
- `POST /api/juce/continue_music`
- `POST /api/juce/retry_music`

`-1` means choose a new random seed. An explicit non-negative integer,
including zero, means use that value. Missing or blank should retain backward
compatibility by behaving like `-1`; malformed values should return HTTP 400.
Random seeds should be in `0...99999`, matching the Windows, remote, Terry,
Carey, and SA3 readouts.

The smallest Mac implementation is:

- Add `seed: Optional[int] = None` to `AudioRequest`, `ContinueMusicRequest`,
  and `SessionRequest` in
  `audiocraft-mlx/audiocraft/g4l_localhost.py` (a shared seeded base model is
  fine but not required).
- Add a `resolve_seed` helper with the behavior above.
- Resolve once in each HTTP handler. Put the resolved value in the session's
  `parameters.seed`, pass it through `run_audio_processing` or
  `run_continue_processing`, and return it as top-level `seed` in the initial
  success response.
- Add a `seed` argument through `process_audio`, `continue_music`, and the
  internal generation helper in `g4laudio_mlx.py`. Call
  `mx.random.seed(seed)` inside the existing generation lock, immediately
  before `model.generate_continuation`. This placement also makes a retry run
  the selected take rather than inheriting whatever global MLX RNG state was
  left by another request.
- Include the stored value as `generation_seed` in
  `GET /api/juce/poll_status/<session_id>`. Do not call this response field
  plain `seed`: Gary and Terry can share a session in the remote contract, and
  the client deliberately reads Gary's poll value from `generation_seed`.

Keep the Mac-only `quantization_mode` path alongside this work. The merged
client sends both fields; neither replaces the other.

## Required Terry contract

The current client uses only:

- `POST /api/juce/transform_audio`
- `GET /api/juce/poll_status/<session_id>`

The smallest implementation in `melodyflow/localhost_melodyflow.py` is:

- Import `random` and add `resolve_seed(value)`: missing, blank, or any
  negative integer chooses `random.randint(0, 99999)`; a non-negative integer
  is preserved; malformed input becomes a 400-level `AudioProcessingError`.
- Resolve `data.get("seed")` in `juce_transform_audio`.
- Store the resolved seed on the JUCE session, return top-level `seed` with
  the initial success response, and include it in poll responses.
- Thread the value through `_queue_transform_job` into `process_audio`.
- Add `seed` to `process_audio` and call `torch.manual_seed(seed)` immediately
  before the selected MPS or MLX edit call. The Mac MLX edit path currently
  draws its stochastic prompt and regularization noise through PyTorch before
  converting arrays to MLX, so the PyTorch seed covers both Mac engines.

Adding seed support to the separate legacy `/transform` endpoint and its
`X-Melodyflow-Seed` response header would complete parity with Windows v0.3.1,
but is not required by gary4juce.

## Confirmed non-blockers

| gary4juce change | Mac service impact |
| --- | --- |
| Gary advanced CFG, top-k, and description controls | None. These are existing request fields already handled by the Mac Gary route and MLX implementation. |
| Gary panel/layout changes | None. Client UI only. |
| Terry seed-row layout and last-seed presentation | Only the Terry seed contract above. |
| Jerry fresh-instance SAOS fix | None. Mac `/models/status` already returns `cache_status.usage_order` and `model_details`; the client merely consumes them earlier and chooses the active entry. |
| SA3 continuation label changed from total duration to seconds to add | None. The wire field remains `continuation_seconds`, which the Mac SA3 `/continue` route already interprets as added duration and caps with source duration at 300 seconds. |
| SA3 enablement refreshes | None. Client UI/state only. |
| FLAC drag/export option | None. The client still base64-encodes audio files and the service response contract remains base64 WAV. Internal storage/export format selection does not change an endpoint. |
| Storage migration and recorded-buffer picker persistence | None. Local filesystem and plugin-state behavior only. |
| Section borders, compact layout, tooltips, update manifests, release metadata | None. |

## Compatibility tests

These are contract tests and do not require duplicating Windows internals.

### Gary

For each of process, continue, and retry:

1. Submit `seed: 1234`; assert HTTP 200, response `seed == 1234`, and poll
   `generation_seed == 1234`.
2. Submit `seed: 0`; assert zero is preserved rather than treated as false.
3. Submit `seed: -1` twice; assert both responses contain values in
   `0...99999` (they need not be different in a deterministic unit test).
4. Submit malformed seed input; assert HTTP 400 rather than a worker-side 500.
5. At the MLX generation boundary, assert identical model/input/settings and
   seed drive the same categorical RNG sequence.
6. Repeat one case with `quantization_mode` present to guard the Mac-specific
   request path.

### Terry

1. Submit `seed: 1234`; assert the initial response and every poll echo 1234.
2. Submit `seed: 0`; assert zero is preserved.
3. Submit `seed: -1`; assert the response contains a value in `0...99999`.
4. Submit malformed input; assert a 400-class response.
5. Mock or instrument both `mps` and the configured MLX-native engine and
   assert `torch.manual_seed` runs immediately before editing.
6. Assert the seed remains attached to the session through queued, warming,
   processing, completed, and failed poll states.

## Ship gate

A merged Mac plugin is safe for ordinary service smoke-testing immediately:
existing Mac v0.2.0 handlers ignore the extra seed fields instead of rejecting
the requests. Before publishing the merged plugin with visible Gary and Terry
seed controls, land and test both required seed paths above. The rest of the
larger Mac-to-Windows v0.3.1 parity effort is independent of this gary4juce
merge and should not hold this compatibility gate open.
