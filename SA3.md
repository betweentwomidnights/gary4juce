# SA3 Guide

SA3 is the Stable Audio 3 workflow inside gary4juce v4. It lives in the Jerry
tab, to the left of the original SAOS tab.

Upstream repo: https://github.com/stability-ai/stable-audio-3

Status:

- Remote backend and Windows gary4local are supported.
- Local gary4local mac support is planned.
- Output loudness, continuation tails, and LoRA behavior are still being tuned.

---

## Quick Start

1. Switch the backend button to `remote` or `local` on Windows gary4local.
2. Open `jerry`.
3. Select the `sa3` sub-tab.
4. Choose `generate`, `transform`, or `continue`.
5. Write a prompt or roll the dice.
6. Optionally choose key/scale.
7. Generate.

The plugin automatically appends the DAW BPM to every SA3 prompt. If key/scale
is selected, it appends that too.

Example final prompt sent to the backend:

```text
warm shoegaze guitars, 124 bpm, F# minor
```

The text box should stay focused on genre, vibe, instrument, arrangement, and
texture. Do not type BPM or key unless you are intentionally overriding the
automatic DAW context.

---

## Generate

Use `generate` for text-to-audio.

Controls:

- `prompt` - optional user prompt. Empty prompts still generate because the DAW BPM is appended.
- dice button - rolls a backend prompt.
- `duration` - total generation length in seconds, up to 300.
- `loop` - switches to bar-aligned loop generation.
- `4 / 8 / 16` - loop length in bars when loop mode is enabled.

Loop mode disables the duration slider because loop length is derived from bars
and DAW BPM.

Good uses:

- long beds
- intros
- texture passes
- loopable riffs
- prompt-only LoRA testing

---

## Transform

Use `transform` to restyle existing audio. It behaves a bit like an AI effects
pedal.

Controls:

- `recording` - transform the saved recording buffer.
- `output` - transform the current output waveform.
- `init noise` - lower preserves more of the source, higher transforms more.
- prompt/dice/key/advanced controls work the same as generate.

Transform output keeps the same duration as the source audio.

Tips:

- Subtle guitar, synth, and stem transformations can be more useful than extreme ones.
- Fuller source audio gives the model more to work with.
- If the transform is too delicate, increase init noise and make the prompt more specific.

---

## Continue

Use `continue` to extend existing audio.

Controls:

- `recording` - continue the saved recording buffer.
- `output` - continue the current output waveform.
- `duration` - total target output length, including the source audio.
- `continuation mode` - advanced `standard` / `latent_prefix` selector.
- prompt/dice/key/advanced controls work the same as generate.

Important: the continue duration is the final total length. If the source is
21 seconds and the slider is set to 100 seconds, the backend receives a request
for roughly 79 seconds of new continuation.

Current launch note:

Some long continuations fade or become quiet before the final samples. Until the
backend behavior is fully audited, use the waveform crop tools to continue from
the strongest musical endpoint when needed.

---

## Advanced Controls

The defaults are intentionally conservative:

- `steps` defaults to `8` and caps at `16`
- `cfg` defaults to `1`
- `shift` defaults to `full`

These values matter for SA3. Treat them like model-native defaults rather than
normal "more is better" controls.

### Negative Prompt

The `negative` field is shared across generate, transform, and continue. Leave
it blank to use the backend default of `low quality`.

Early testing suggests active LoRAs can make negative prompting much weaker, and
in some cases it may feel like the model is not listening to the negative prompt
at all. Treat this as a gentle steering control, not a hard exclusion system,
especially when one or more LoRA sliders are above `0`.

### Continue Mode

The continue tab has an experimental `continuation mode` selector in advanced
settings. `standard` uses the default `inpaint` mode. `latent_prefix` asks the
backend to pin the encoded source as a fixed latent prefix, which may make the
continuation inherit the source tempo and timbre more strongly.

Use the same source and seed when A/B testing this mode. It may be useful, it
may be strange, and it may barely change a take.

### Seed

Enable `use seed` to submit a fixed seed.

If `use seed` is off, the backend picks a random seed and the plugin displays
the seed that was used. Re-enable `use seed` with that value to reproduce the
same take, assuming the rest of the settings are the same.

### Shift

`shift` changes the sampling distribution. Options:

- `full` - default in the plugin
- `none`
- `logsnr`
- `flux`

The difference is audible. Use it as a character switch more than a correction
tool.

### LoRA Sliders

Enable `use lora` to reveal one strength slider per available SA3 LoRA.

Behavior:

- Every slider starts at `0`.
- A LoRA only affects generation when its slider is above `0`.
- Multiple LoRAs can be active at the same time.
- The current plugin range is `0` to `2`.

Suggested starting points:

- `0.25` - light color
- `0.5` - clear influence
- `1.0` - strong intended distribution
- `1.5+` - experimental

SA3 LoRA outputs can be hot. Keep an eye on gain staging and the DAW meters.

---

## Smart Dice

The dice button always fetches prompt pools from the backend.

Behavior:

- If all LoRA sliders are `0`, dice uses the default SA3 prompt pool.
- If one or more LoRA sliders are above `0`, dice sends those LoRA names to the backend.
- If multiple LoRAs are active, the backend can roll from prompt pools associated with all of them.

The dice prompt itself does not include BPM or key. The plugin adds those only
when you submit the generation.

Useful workflow:

1. Turn on `use lora`.
2. Raise one or more LoRA sliders above `0`.
3. Roll dice until the prompt feels in-distribution.
4. Generate with a random seed.
5. Reuse the returned seed if you want to test shift, key, or strength changes.

---

## Prompting Tips

SA3 likes concise music descriptions.

Good:

```text
textural post-rock guitars, slow drums, saturated tape
```

```text
minimal modular techno, dry percussion, rubbery bass
```

```text
ambient pedal steel, soft piano, wide room
```

Avoid:

- very long paragraphs
- doubled BPM or key information
- too many contradictory genres
- expecting LoRAs to behave identically at every strength

For LoRA testing, try leaving the user prompt blank. The plugin will still append
the DAW BPM, giving you a near-unconditional generation with the current LoRA
settings.

---

## Known Launch Notes

- SA3 runs on the remote backend and the Windows gary4local SA3 service.
- Some outputs are loud or may clip. Backend normalization/limiting is under review.
- Long continuation tails may fade or go quiet before the final duration.
- `latent_prefix` continuation mode is experimental and may or may not improve a given source.
- Negative prompting may be weak or ineffective with active LoRAs.
- Layer filter and per-LoRA interval controls are backend concepts but not yet exposed in the plugin UI.

---

## Follow-Up Notes

Release/backend follow-ups:

- add SA3 to gary4local mac
- confirm base-model behavior with `loras: []`
- compare continue behavior against the upstream SA3 UI
- A/B test `standard` and `latent_prefix` continuation modes on fixed seeds and source audio
- decide whether smart dice should mix default fallback buckets with LoRA buckets
- tune loudness or add backend gain management
