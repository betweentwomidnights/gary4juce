# SA3 Backend Scratchpad

Date: 2026-05-19

## Observations

- Outputs are coming back quite hot. This was noticeable in earlier curl tests too, and the end of a 2 minute `/sa3/generate` output sounded like it may have clipped.
- Frontend smoke tests passed for loops, 2 minute duration generation, fixed seed reuse, random seed echo/reuse, shift selection, and key/scale prompt appending.

## Backend Follow-Up

- Check whether SA3 output amplitude should be normalized, limited, or otherwise gain-managed before returning WAV data.
- If possible, inspect whether the hotness is already present in latent/pretransform output or only appears after decode/render.
- Compare short loop outputs against long duration outputs to see whether clipping risk increases near the end of longer generations.
- Align LoRA defaults with the frontend advanced UX: LoRA strengths should start at 0, and base-model/no-LoRA generation should be possible without the backend silently applying the default LoRA.
- Confirm `loras: []` truly produces base-model behavior with a controlled seed: run once with no LoRAs loaded, reload the LoRA registry, then run the same seed with `loras: []` and compare.
- Audit `/continue` against the private stable-audio-3 Gradio UI implementation. Current JUCE frontend now sends `continuation_seconds = requested_total_duration - source_duration`; the backend returns a WAV of the requested total length, but musical content often fades/stops before the final samples.
- Continue repro notes from DAW: same 21.1s source, target 100s total -> WAV is exactly 1:40, but generated content consistently stops around 1:36 with different seeds/LoRA strengths. Target 60s also seems to fade near the end. Target 120s stops around 1:57.5 with a small tail and about 2 seconds of silence.
- Current backend continue path to review: `continue_audio()` computes `total_duration = source_duration + continuation_seconds`, `target_samples = round(total_duration * sr)`, then sets `params["duration"] = total_duration + 0.5`, `mask_start_seconds = source_duration`, and `mask_end_seconds = total_duration`. Worker passes `duration`, `inpaint_audio`, `inpaint_mask_start_seconds`, and `inpaint_mask_end_seconds` into `pipe.generate()`, then trims/pads to `target_samples`.
- Specific continue questions: confirm mask polarity and units; confirm whether mask start/end are absolute seconds on the full generated timeline; check whether `mask_end_seconds` should be `total_duration` or the padded `duration`; test whether `/continue` should omit the `+0.5` pad or instead set mask end to the padded duration and trim afterward.
- Add backend diagnostics for continue tails: log if `have < target_samples` padding ever occurs, compute RMS/peak for the last 5 seconds before and after trimming, and include continue meta in the response/logs so DAW observations can be matched to source duration, requested continuation, total duration, mask start/end, generated duration, and target samples.
- If the early fade is intrinsic to the model, consider a backend tail strategy for automated continuous streams: request a small extra continuation region and trim/crop to the strongest musically active endpoint, or document that users should crop/continue from before the fade tail.
- Smart dice observation: when one or more LoRA sliders are above 0, some returned prompt rolls appear to include default-pool prompts mixed with LoRA-specific prompts. This may be a good feature rather than a bug, but confirm whether it is intentional. Current `/prompts` behavior starts from defaults, then only replaces buckets a selected LoRA defines, so any buckets not defined by that LoRA remain generic/default.
- Decide/document desired smart dice behavior: should LoRA-active dice roll across returned LoRA-defined + default fallback buckets, or should it constrain rolls to only buckets actually supplied by the selected LoRAs?
- Negative prompting observation: with active LoRA sliders, the model may stop responding to `negative_prompt` or treat it as a much weaker steering signal. Before adding any negative-prompt-driven UI toggles such as drums/instrument exclusions, test them with LoRAs active and document the limitation clearly.

## Frontend Notes

- The JUCE SA3 tab now displays the backend-returned seed from random generations and can resubmit a specific seed via the `use seed` toggle.
- Advanced disclosure now matches Foundation's arrow treatment.
- The SA3 advanced `negative` label tooltip warns that blank uses backend default `low quality`, and active LoRAs may weaken or ignore negative prompting. The textbox itself intentionally has no hover tooltip so it stays easy to edit.
- When LoRA sliders are added, send an explicit `loras: []` array when all strengths are 0. The current API's legacy path applies the default LoRA when `loras` is omitted.
- Remote SA3 LoRA registry can be fetched/cached when the SA3 subtab becomes active. Localhost SA3 now uses the same `/loras` contract and refreshes while the SA3 subtab is active, with a short TTL so registry changes after backend reload/restart appear during a VST session.
- Transform is now its own SA3 nested tab. It sends the selected recording/output WAV as `audio_data`, omits duration, and uses the shared seed/shift/LoRA advanced controls plus a transform-specific init noise strength.
- Continue is now its own SA3 nested tab. It sends selected recording/output WAV as `audio_data`, sets `continuation_seconds`, and intentionally omits loop controls. Remote and local can send `continuation_mode` as `inpaint` or `latent_prefix`.
- Continue's visible duration slider is DAW-facing total output duration. The frontend subtracts the source audio duration before sending backend `continuation_seconds`, because the backend derives `total_duration = source_duration + continuation_seconds` and masks from source end to total end.
- SA3 prompt text is optional in the UI. Empty user prompts still submit because the frontend appends DAW BPM, enabling near-unconditional LoRA tests from the VST.
