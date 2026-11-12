# Carey backend contract

A handoff for whoever maintains Carey's remote Python wrapper (the ACE-Step
service behind `https://g4l.thecollabagepatch.com/carey`).

## What changes and why

gary4juce's Carey tab has been rebuilt around **ace-server's job API**: the
HTTP API of acestep.cpp (`betweentwomidnights/acestep.cpp`, v0.1.3), which is
what gary4local now runs for Carey instead of Python. The plugin speaks one
contract to both backends. On localhost that is ace-server itself; remotely it
is your wrapper.

So the wrapper must serve **two contracts side by side, on the same base URL**:

- **Legacy**: the routes released gary4juce versions call today (`/lego`,
  `/complete`, `/cover`, `/extract`, their `/status/{task_id}` routes, `/loras`,
  `/captions`). Keep these exactly as they are. Every gary4juce build already
  in users' hands depends on them, and they stay until we decide otherwise.
- **New**: `POST /synth`, `GET /job`, `POST /job` and `GET /props`, described
  below. The new plugin calls only these, never a legacy route.

The route names don't collide, so both can live in one FastAPI app. Deploy the
new routes **before** the gary4juce release that uses them ships.

**Reference implementations.** If this document and the code disagree, the code wins:

| What | Where |
|---|---|
| The client (exactly what the plugin sends and reads) | gary4juce `Source/PluginEditorCareyHelpers.h` (namespace `carey_native`) and `Source/PluginEditor.Carey.cpp` (one sender per tab) |
| The server it was built against | acestep.cpp `tools/ace-server.cpp` (`handle_synth`, `handle_props`, the `/job` handlers, `job_status_json`) |
| What each request field means | acestep.cpp `docs/ARCHITECTURE.md`: the request field reference, "Model compatibility" and "DiT context per mode" |

You only need the subset of ace-server described here. ace-server has much
more (`/lm`, `/understand`, `/caption`, latents, batches) that the plugin
never calls.

## Routes

Base: `https://g4l.thecollabagepatch.com/carey`.

| Method and path | Contract | Purpose |
|---|---|---|
| `POST /synth` | new | submit one generation, returns a job id |
| `GET /job?id=ID` | new | job status and progress |
| `GET /job?id=ID&result=1` | new | the finished audio |
| `POST /job?id=ID&cancel=1` | new | cancel |
| `GET /props` | new | the models and LoRAs this backend offers |
| `POST /lego`, `/complete`, `/cover`, `/extract` | legacy | keep |
| `GET /lego/status/{id}` (and the other modes') | legacy | keep |
| `GET /loras`, `GET /captions`, `GET /captions/pools` | legacy | keep |
| `GET /health` | both | keep |

Reachability: on remote the plugin does not probe Carey separately. It uses
the backend-wide connection check. On localhost it calls `GET /health` and
treats any 2xx as online.

## `POST /synth`

**Request.** `multipart/form-data` with these parts:

| Part | Content | When |
|---|---|---|
| `request` | one JSON object, the AceRequest (see [Request fields](#request-fields)); sent with `Content-Type: application/json` and **no filename** | always |
| `audio` | WAV file (`filename="audio.wav"`, `Content-Type: audio/wav`), the source audio | every tab except generate |
| `ref_audio` | WAV file, the timbre reference. Today it is always byte-identical to `audio` | continue and cover, when "use source as reference" is on |

ace-server also accepts a plain JSON body, or a JSON array (a batch) in the
`request` part. The plugin sends neither: always multipart, always a single
object. Read `request` as a form field. Some clients send it as a file, so
accept both if that's cheap.

The source WAV is whatever the DAW recorded (`myBuffer.wav`): any sample rate,
16-, 24- or 32-bit, mono or stereo. With loop assist it is a looped copy the
plugin already built. Resample to 48 kHz stereo as ACE-Step expects. Size: a
240 s stereo 32-bit-float buffer at 48 kHz is about 92 MB, so let the proxy
accept **at least 100 MB** bodies. The plugin allows 120 s for the upload.

**Response.** `200 {"id": "<job id>"}`, returned immediately; the work runs
in the background.

- **The id must be 1 to 64 ASCII letters or digits.** The plugin rejects
  anything else, including hyphens, as "carey returned no job". `uuid4().hex`
  is fine; `str(uuid4())` is not.
- **On a bad request**, return a 4xx with `{"error": "short reason"}`. The
  plugin shows `error` verbatim in its status bar, so keep it short and human.
  If the body isn't JSON with an `error` field, the user sees "carey refused
  the request".
- **What ace-server rejects with 400:** a missing `request` part, invalid JSON,
  undecodable `audio`, an unknown `output_format`, and an empty `caption` for
  any task except `lego`, `extract`, `complete` and `repaint`. That last one
  matters for generate and cover. An unknown `synth_model` or `adapter` fails
  the job rather than the submit; either is fine for the plugin.

## `GET /job?id=ID`

**Response.** `200` with one of:

```json
{"status": "running", "progress": {"stage": "dit", "step": 12, "total": 50}}
{"status": "done"}
{"status": "failed", "error": "short reason"}
{"status": "cancelled"}
```

- **`status`** is one of `running`, `done`, `failed` or `cancelled`. There is
  no separate queued status: a job waiting for the GPU is `running` with stage
  `queued`.
- **`progress`** appears only while running. It has `stage`, `step` and `total`:

  | Stage | Meaning | `step`/`total` | Status bar | Bar |
  |---|---|---|---|---|
  | `queued` | waiting for the GPU worker | 0/0 | "queued" | 0% |
  | `load` | loading models or the LoRA | 0/0 | "loading models..." | 1% |
  | `encode` | VAE-encoding the source | optional | "encoding source audio..." | 2-10% |
  | `dit` | diffusion | step `n` of `total` | "n/total steps" | 10-95% |
  | `decode` | VAE decode | optional | "decoding audio..." | 95-99% |

  `dit` is the one that matters: report each diffusion step, for example from
  a per-step callback in the ACE-Step pipeline. An unknown or missing stage
  reads as `queued`.
- **`error`** appears only when failed. The plugin shows it verbatim; without
  one it says "carey generation failed".
- **Unknown id:** return `404` with `{"error": "Job not found"}`. The plugin
  reports the `error` and stops.

**Polling.** The plugin polls every 500 ms with a 15 s timeout per request.
It gives up and cancels after 30 minutes. Keep this endpoint cheap: never
block it on the GPU.

## `GET /job?id=ID&result=1`

- **Not done yet:** `404 {"error": "Result not ready"}`. The plugin only asks
  after it has seen `done`.
- **Done:** `200`, `Content-Type: multipart/mixed; boundary=<b>`. ace-server
  uses the boundary `ace-batch-boundary`; the plugin reads whatever boundary
  the header names. The body is one part per generated track:

  ```
  --ace-batch-boundary\r\n
  Content-Type: audio/wav\r\n
  \r\n
  <WAV bytes>\r\n
  --ace-batch-boundary--\r\n
  ```

The plugin takes the **first part whose headers contain
`Content-Type: audio/`** and ignores the rest. ace-server adds a raw latent
part (`application/octet-stream`) after each audio part; you don't need to.
The plugin requests `output_format: "wav16"`, so return 16-bit PCM WAV at
48 kHz stereo, and return the **full generated length**: the plugin trims on
its side when asked (see [Behavior that moved into the plugin](#behavior-that-moved-into-the-plugin)).
The plugin allows 120 s to download it.

Keep results until fetched, plus a grace period (minutes, not seconds).

## `POST /job?id=ID&cancel=1`

Stop the job if it is running and answer `200 {"status": "cancelled"}`. A job
that already finished answers with its own status. The plugin cancels when the
user stops the generation, starts another one or closes the editor, and after
its 30-minute timeout. It doesn't wait for the cancel to take effect, so
cancel cooperatively (check between diffusion steps) and free the GPU for the
next job.

## `GET /props`

The plugin calls this **before every generation**, and when the Carey tab
opens or the backend changes. So it must be fast and must not touch the GPU.
The plugin reads only these fields:

```json
{
  "models": {
    "dit": ["acestep-v15-xl-base", "acestep-v15-xl-sft", "acestep-v15-xl-turbo"]
  },
  "adapters": ["billie-xlbase-v1", "koan-xl"],
  "adapter_info": {
    "billie-xlbase-v1": {
      "model_family": "xl",
      "captions": ["dreamy bedroom pop, whispered vocals, soft synth pads", "..."]
    }
  }
}
```

ace-server sends much more (`version`, `hardware`, `devices`, `capabilities`,
`models.lm` / `embedding` / `vae`, `cli`). Add any of it if useful, but the
plugin ignores it.

### `models.dit`: which DiT names the plugin will request

The plugin picks a DiT by name from this list and sends that exact string
back as `synth_model`:

1. **Variant.** Each tab needs a variant: `base`, `sft` or `turbo` (see
   [Per tab](#per-tab)).
2. **Family.** On the remote backend the plugin **always uses the XL family**;
   the 2B/XL choice is hidden there. The 2B family is localhost only.
3. **Name match.** It looks for names equal to `acestep-v15-xl-<variant>`, or
   starting with `acestep-v15-xl-<variant>-` followed by a precision tag. A
   trailing `.gguf` is ignored. When several match, the plugin prefers BF16,
   then Q8_0, Q6_K, Q5_K_M, Q4_K_M; a name with no precision tag ranks like
   BF16.
4. **No match.** The user sees "this backend has no XL sft model - download it
   in gary4local or switch size", and nothing is submitted.

For the wrapper, list the checkpoint folder names you can load:
`acestep-v15-xl-base`, `acestep-v15-xl-sft` and `acestep-v15-xl-turbo`. Only
list the ones actually loadable. If sft isn't deployed, leave it out and the
plugin says so instead of failing mid-job. `models.dit` can be the only key
under `models`.

### `adapters` and `adapter_info`: LoRAs

- **`adapters`** lists the LoRA names the backend accepts as `adapter`.
- **`adapter_info`** is keyed by adapter name. The plugin reads two fields:
  - `model_family`: `"xl"` or `"standard"` (2B). The plugin hides LoRAs whose
    family doesn't match the size in use. On remote that means only `"xl"`
    ones appear. A LoRA with **no** `model_family` shows for both sizes.
  - `captions`: the captions the LoRA was trained with. The dice button
    beside the caption box picks one at random **on the client**. This replaces
    the legacy `GET /captions?lora=` round trip. Send the whole pool, keeping
    the response reasonable (ace-server caps each `adapter_info.json` at 1 MiB).
    A LoRA without captions makes the dice fall back to the plugin's built-in
    captions.

`adapter_info` is the same content gary4local publishes beside each local
LoRA as `adapter_info.json`. If your legacy `/loras` already knows each
LoRA's family and caption pool, `/props` can be built from the same data.

## Request fields

The AceRequest JSON in the `request` part. Every field the plugin sends:

| Field | Type | Sent by | Meaning |
|---|---|---|---|
| `task_type` | string | all | `text2music`, `lego`, `complete`, `repaint`, `cover-nofsq`, or `extract` (hidden tab) |
| `caption` | string | all | style prompt; may be empty for lego, complete and repaint (see the empty-caption rule above) |
| `lyrics` | string | all | may be empty; `[Instrumental]` is ACE-Step's no-vocals marker |
| `bpm` | int | all | always present |
| `keyscale` | string | when set | e.g. `"F# minor"`, `"C major"`; omitted when the user picked auto |
| `timesignature` | string | when set | the numerator only: `"2"`, `"3"`, `"4"` or `"6"` (6/8); omitted for auto |
| `vocal_language` | string | all | e.g. `"en"`; defaults to `"en"` |
| `inference_steps` | int | all | 8 for turbo; 32-100 for base and sft |
| `guidance_scale` | float | all | 1.0 for turbo; 3.0-10.0 for base and sft |
| `seed` | int | all | 0 to 2^32-1, **always chosen by the plugin**; honor it so results reproduce |
| `output_format` | string | all | always `"wav16"` |
| `synth_model` | string | all | a name from `models.dit`, verbatim |
| `adapter` | string | when a LoRA is on | a name from `adapters`, verbatim |
| `adapter_scale` | float | with `adapter` | 0.0-1.0; 0 means no LoRA effect |
| `duration` | float s | all | output length; equals the source length for lego, complete and cover |
| `track` | string | lego, complete, extract | lowercase names: `vocals`, `backing_vocals`, `drums`, `bass`, `guitar`, `keyboard`, `percussion`, `strings`, `synth`, `fx`, `brass`, `woodwinds`. For complete, several joined by `" \| "`, e.g. `"drums \| bass"` |
| `repainting_start` | float s | lego, repaint | lego: `0.0`; repaint: the source length |
| `repainting_end` | float s | lego, repaint | lego: `-1.0` (to the end of the source); repaint: the target duration |
| `audio_cover_strength` | float | complete, cover-nofsq | 0.0-1.0: the fraction of diffusion steps that see the source |
| `cover_noise_strength` | float | cover-nofsq | 0.0-1.0: how close the starting noise is to the source (1 = nearly the source) |

Ignore any field you don't use.

**Mapping to ACE-Step's Python API.** ace-server's semantics follow upstream
ACE-Step 1.5.
- **Track instructions.** ace-server upper-cases `track` into the DiT
  instruction: lego is `Generate the VOCALS track based on the audio context:`,
  complete is `Complete the input track with DRUMS | BASS:`. Upstream
  ACE-Step's complete takes a list of track classes; split the `" | "` string
  into that list.
- **`cover-nofsq`** is the cover variant without the FSQ round trip, which
  stays close to the source. Your legacy `/cover` already runs it (see
  `COVER_NOISE_STRENGTH_VARIANTS.md` in the gary4local repo), so route
  `cover-nofsq` to that same path.
- **`repaint`** is ACE-Step's repainting task. It is what your legacy
  `/complete` already does: keep the source, generate from its end to the
  target duration.

## Per tab

The plugin's tab names differ from ACE-Step's task names. That's deliberate:
gary4juce's **"continue"** is ACE-Step's `repaint`, and gary4juce's
**"complete"** is ACE-Step's `complete`.

| gary4juce tab | `task_type` | DiT variant | Audio parts | Notes |
|---|---|---|---|---|
| **generate** | `text2music` | user's choice: turbo, base or sft | none | `duration` 30-240 s on remote; caption required |
| **lego** | `lego` | base | `audio` | `track` is one name; `repainting_start` 0, `repainting_end` -1. With an empty caption box the plugin fills in a caption for the track itself |
| **complete** | `complete` | base | `audio` | `track` is one or more names; `audio_cover_strength` is the "strength" slider (default 1.0). New task, no legacy equivalent |
| **continue** | `repaint` | user's choice: turbo, base or sft | `audio`, optional `ref_audio` | `repainting_start` = source length, `repainting_end` = `duration` = target 30-240 s on remote; target > source + 1 s |
| **cover** | `cover-nofsq` | user's choice: turbo (default) or base | `audio`, optional `ref_audio` | `audio_cover_strength`, `cover_noise_strength` |
| extract (hidden) | `extract` | base | `audio` | not visible in released builds; supporting it is optional |

Turbo can't take lego, complete or extract, and the plugin never asks for it
there. If a request asks anyway, fail it with a clear `error`.

## Behavior that moved into the plugin

Several things the legacy wrapper did server-side, the plugin now does itself.
**The new routes must not repeat them**:

- **Loop assist.** The plugin loops a short recording to at least 32 bars and
  2 minutes before upload. Don't loop the `audio` part again.
- **Trim to input.** The plugin trims the result to the recording's length
  after download. Return the full generated length.
- **Lego's default caption.** The plugin picks a per-track caption itself.
- **LoRA caption dice.** The plugin picks from `adapter_info.captions`, with
  no `/captions` call.
- **Seed.** Always sent. The plugin no longer reads a seed back.
- **Turbo's fixed settings.** 8 steps, cfg 1.0. Take what's sent.
- **Model choice.** The plugin names the exact DiT, from your `/props`, in
  `synth_model`. The wrapper has no "default model" logic for these routes.

The legacy routes keep all of their current behavior.

## Errors the user sees

Every `error` string reaches the status bar as is, and some reach a dialog.
Write them for musicians: "lora koan-xl failed to load" rather than a stack
trace. Put the detail in your logs.

## Test checklist

Build a smoke script (Python `requests` is enough) that runs these against
the deployed `/carey` base URL:

1. **`/props`**: `GET /props` lists only DiTs that load, and every
   `adapters` entry has `adapter_info` with `model_family`.
2. **generate**: `POST /synth` with only a `request` part (`text2music`, XL
   turbo, 8 steps, cfg 1, 30 s). Poll `/job` and see `running` with `dit`
   steps counting to 8, then `done`. `&result=1` returns `multipart/mixed`
   whose first part is a 16-bit 48 kHz stereo WAV of about 30 s.
3. **The other tabs**: lego (track `vocals`, a 2-minute source), complete
   (track `drums | bass`), continue (`repaint` from 20 s to 60 s on a 20 s
   source; the result is 60 s and begins with the source), and cover (XL turbo,
   with `ref_audio`).
4. **LoRA**: any request with `adapter` set to a listed XL LoRA and
   `adapter_scale` 0.8.
5. **Seed**: the same request with the same `seed` twice gives the same
   audio, as far as the pipeline is deterministic.
6. **Cancel**: `POST /job?id=...&cancel=1` mid-`dit` gives `cancelled` on the
   next poll, and the GPU takes the next job right away.
7. **Errors**: an empty-caption `text2music` or an unknown `synth_model`
   returns or fails with a readable `error`. An unknown id returns 404 with
   `error`.
8. **Ids**: every job id is alphanumeric and at most 64 characters.
9. **Legacy**: a released gary4juce build still works for lego, complete,
   cover, `/loras` and the caption dice.

Then the real test: the new gary4juce build with the backend set to remote,
running each of the five tabs.
