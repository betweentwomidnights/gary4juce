# LoRA Routing — Frontend Handoff

The wrapper now supports per-request LoRA adapter selection. This document covers everything the JUCE frontend needs to integrate.

## Discovery

### List available LoRAs

```
GET /carey/loras
```

Response:
```json
{
  "koan": { "scale": 0.75, "backends": ["base", "turbo"] }
}
```

Use these keys to populate a LoRA dropdown/selector in the plugin UI. The list is dynamic — new LoRAs appear automatically without a plugin update.

### List caption pools (existing)

```
GET /carey/captions/pools
```

Response:
```json
{
  "default": 200,
  "koan": 42
}
```

Pool names match LoRA names. If a LoRA has a caption pool, the dice button should use it.

## Request fields

Two new optional fields on `POST /carey/cover`, `POST /carey/complete`, and `POST /carey/lego`:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `lora` | string | `""` | LoRA adapter name from `/loras`. Empty = no adapter. |
| `lora_scale` | float | `-1` | Override adapter strength 0.0–1.0. -1 = use server default. |

`POST /carey/extract` does **not** support LoRA (no caption conditioning).

### Example: cover with LoRA

```json
{
  "audio_data": "<base64>",
  "bpm": 120,
  "caption": "koan meditation ambient, tibetan singing bowls, gentle drone",
  "lyrics": "[Instrumental]",
  "lora": "koan"
}
```

### Example: cover with LoRA + scale override

```json
{
  "audio_data": "<base64>",
  "bpm": 120,
  "caption": "koan meditation ambient, tibetan singing bowls, gentle drone",
  "lyrics": "[Instrumental]",
  "lora": "koan",
  "lora_scale": 0.5
}
```

### Example: complete (continuation) with LoRA on xl-base

```json
{
  "audio_data": "<base64>",
  "bpm": 90,
  "audio_duration": 60,
  "caption": "koan deep ambient, atmospheric, evolving",
  "model": "xl-base",
  "lora": "koan"
}
```

## Dice button integration

When a LoRA is selected, pass the LoRA name to the captions endpoint:

```
GET /carey/captions?lora=koan    → random koan-style caption
GET /carey/captions              → random default caption (no LoRA selected)
```

The returned `caption` populates the caption text box.

## Error handling

| Error | Cause |
|-------|-------|
| `"error_code": "invalid_lora"` | Unknown LoRA name or LoRA not supported on the resolved backend |

These are non-retryable failures. The plugin should show the error message, which includes the available LoRA list or the backend mismatch reason.

## Suggested UI flow

1. On plugin startup, fetch `GET /carey/loras` → populate LoRA dropdown (first option: "None")
2. User selects LoRA (e.g. "koan") in dropdown
3. Dice button calls `GET /carey/captions?lora=koan` → fills caption box
4. User clicks generate → `POST /carey/cover { ..., lora: "koan" }`
5. (Future) If a scale slider exists, send `lora_scale: 0.5` etc. Otherwise omit the field.

## Notes

- `lora_scale` is for future use — no need to expose a slider immediately. Omitting it (or sending -1) uses the registry default (0.75 for koan).
- LoRA works on both xl-turbo and xl-base backends. The adapter was trained on xl-base but transfers cleanly to turbo.
- The wrapper handles the full load/unload lifecycle per-request. No state persists between generations.
