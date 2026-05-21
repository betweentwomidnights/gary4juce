# gary4juce

**gary v4 has begun: stable-audio-3 is now inside the DAW.**

a VST3/AU plugin for musicians who want AI to meet them where they actually
live. for me, that's ableton. for you, it might be fl studio or, if you're
insane, reaper lol

https://thepatch.gumroad.com/l/gary4juce

**latest stable release:** [gary4juce v3.0.7 (windows VST3)](https://github.com/betweentwomidnights/gary4juce/releases/tag/v3.0.7)

**v4 pre-release:** [gary4juce v4.0.0-pre (windows VST3)](https://github.com/betweentwomidnights/gary4juce/releases/tag/v4.0.0-pre) - stable-audio-3 has entered the DAW.

![gary4juce demo](docs/media/gary_v3_readme_720w.gif)

localhost backends:

- windows: [gary4local](https://github.com/betweentwomidnights/gary-localhost-installer)
- macOS: [gary4local mac](https://github.com/betweentwomidnights/gary-localhost-installer-mac)

videos about it here when i can: https://youtube.com/@thepatch_dev

---

## Built For Musicians

If you just want pure text-to-music, you probably do not need a VST. This
project exists for people who want AI to sit inside the session with them.

gary4juce now gives you **seven AI music models** directly in your DAW:

- **sa3** ([stable-audio-3](https://github.com/stability-ai/stable-audio-3)) - text-to-audio, loops, transforms, continuations, LoRA blending, seed recall, key/BPM-aware prompting
- **gary** ([musicgen](https://github.com/facebookresearch/audiocraft)) - continuation/anti-looper. Extends your audio in creative directions
- **jerry** ([stable-audio-open-small](https://huggingface.co/stabilityai/stable-audio-open-small)) - BPM-aware 12-second loop generation in under a second
- **rc-jerry** ([foundation-1](https://huggingface.co/RoyalCities/Foundation-1)) - BPM and key-aware 4/8-bar loop generation with structured prompt assembly
- **carey** ([ace-step](https://github.com/ace-step/ACE-Step-1.5)) - stem generation, extraction, audio continuation, and remix/cover with lyrics and multilingual support
- **terry** ([melodyflow](https://huggingface.co/spaces/facebook/melodyflow)) - audio transformation. Turn your guitar into an orchestra
- **darius** ([magenta-realtime](https://github.com/magenta/magenta-realtime)) - high-quality 48 kHz continuations with style control

Put it on your master, press play, record some audio, and start iterating.

> SA3 is remote-only for now. Localhost support belongs in the next
> gary4local pass.

---

## What's New In V4

### v4.0.0 - stable-audio-3

gary4juce has entered v4 with a new **sa3** sub-tab inside Jerry, positioned
alongside the original SAOS and Foundation-1 workflows.

SA3 currently includes:

- **generate** - text-to-audio up to 300 seconds, plus 4/8/16-bar loop mode
- **transform** - restyle the recording buffer or current output audio
- **continue** - continue the recording buffer or current output audio to a target total duration
- **seed recall** - random generations show the backend-returned seed so a take can be reproduced
- **key/scale prompting** - optional Carey-style key and mode dropdown appended to the final prompt
- **LoRA sliders** - one strength slider per available remote LoRA, defaulting to 0
- **smart dice** - prompt rolls come from the default pool or from every LoRA whose slider is above 0

Read the practical guide: [SA3.md](SA3.md)

Launch notes:

- SA3 outputs can be hot, especially with LoRAs. Treat gain staging like part of the instrument for now.
- Continue results can leave a quiet/fading tail near the end of longer continuations. This is being audited against the upstream SA3 UI.
- Local SA3 in gary4local is not enabled yet.

### V3 Highlights

v3 brought Carey, Foundation-1, and the first pass at the modern multi-model
workflow:

- Carey joined with lego, complete, cover, extract, lyrics, language, key/scale, time signature, LoRA selection, LoRA dice captions, and caption popouts.
- Foundation-1 became `rc-jerry`, a structured BPM/key-aware loop generator inside the Jerry tab.
- Foundation-1 landed in gary4local mac on Apple silicon.
- Plugin-safe update checks and editor lifecycle hardening made the app much harder to crash during in-flight requests.

Carey guide: [CAREY.md](CAREY.md)

---

## Roadmap

- [ ] add SA3 to [gary4local](https://github.com/betweentwomidnights/gary-localhost-installer)
- [ ] release the mac AU/VST3 build
- [x] add the first SA3 usage guide: [SA3.md](SA3.md)
- [ ] add SA3's second continuation mode once the backend path is ready
- [ ] improve SA3 LoRA loudness handling on the backend and build a custom training script for the pre-encoding step
- [ ] revisit Carey complete mode so it can do the upstream-style accompaniment workflow
- [ ] enable the Carey `xl-sft` model on the remote backend

---

## Installation

### Windows

1. Close your DAW.
2. Extract the `gary4juce.vst3` folder from the ZIP.
3. Copy the entire folder to `C:\Program Files\Common Files\VST3\`.
4. Reopen your DAW and rescan plugins if needed.

You can put the VST3 literally anywhere as long as your DAW scans that
location. `C:\Program Files\Common Files\VST3\` is just the default path most
DAWs already check.

If permission errors appear, run Command Prompt as admin:

```bat
xcopy "path\to\extracted\gary4juce.vst3" "C:\Program Files\Common Files\VST3\gary4juce.vst3" /E /I /Y
```

### macOS AU/VST3

1. Quit your DAW.
2. Open the DMG.
3. Drag to the matching folder:
   - `Gary4Juce.component` -> Components (Audio Unit)
   - `Gary4Juce.vst3` -> VST3
4. Reopen your DAW and rescan.

GarageBand and Logic use AU. Ableton, FL, Reaper, Cubase, and Bitwig can use
VST3.

---

## Backends

The plugin can use either:

- **remote backend** - my server, free, on a spot VM, limited to the models I have loaded
- **localhost** - your machine, requires GPU, full control

Remote base URL: `https://g4l.thecollabagepatch.com`

### gary4local

Use the dedicated apps for localhost:

- windows: [gary4local](https://github.com/betweentwomidnights/gary-localhost-installer)
- macOS: [gary4local mac](https://github.com/betweentwomidnights/gary-localhost-installer-mac)

They manage local envs for gary, terry, jerry, carey, and foundation-1. SA3 is
remote-only until the v4 local backend work lands.

Recommended hardware:

- 10 GB+ GPU VRAM minimum
- 16 GB+ recommended for heavier local models
- 24 GB+ recommended for Darius-style separate backends

### SA3 Backend

SA3 currently runs only on the remote backend through:

`https://g4l.thecollabagepatch.com/sa3`

It uses the remote Stable Audio 3 backend we have been testing. Public upstream
repo: https://github.com/stability-ai/stable-audio-3

### Darius Backend

Darius is too heavy to run alongside the other gary4local services.

Easiest path: duplicate this Hugging Face Space:

https://huggingface.co/spaces/thecollabagepatch/magenta-retry

Use an L40s or A100-class runtime. Enter the duplicated Space URL in the Darius
tab.

Local Docker option:

```bash
git clone https://github.com/betweentwomidnights/magenta-rt
cd magenta-rt
docker build -f Dockerfile.cuda -t magenta-rt .
docker run --gpus all -p 7860:7860 magenta-rt
```

---

## How To Use

1. Put gary4juce on your master track, a bus, or any track you want to listen to.
2. Press play in your DAW.
3. Save the recording buffer when you have audio you want a model to react to.
4. Generate, transform, continue, crop, drag, and repeat.

### Jerry Tab

The Jerry tab now has three sub-tabs:

#### sa3 - stable-audio-3

- Generate text-to-audio up to 300 seconds.
- Toggle loop mode for 4/8/16-bar loop generation.
- Transform either the saved recording buffer or the current output audio.
- Continue either the saved recording buffer or the current output audio.
- Use optional key/scale and automatic DAW BPM prompting.
- Use seed recall for reproducible takes.
- Use LoRA sliders in advanced settings. Sliders default to 0.
- Use dice prompts from the default pool, or from selected LoRA pools when one or more LoRA sliders are above 0.

Full guide: [SA3.md](SA3.md)

#### jerry - stable-audio-open-small

- Generates short loops aligned to your DAW BPM.
- Smart loop mode can bias toward drums or instruments.
- Localhost supports custom finetunes through the model picker.

Learn more: https://huggingface.co/stabilityai/stable-audio-open-small

#### rc-jerry - foundation-1

- Generates 4 or 8 bar loops synced to BPM and key.
- Uses structured prompt assembly through knobs, toggles, and tag controls.
- Randomize builds coherent presets through the backend prompt engine.
- Presets can be saved/loaded as `.f1preset` files.
- Available on remote, gary4local windows, and gary4local mac on Apple silicon.

Learn more: https://huggingface.co/RoyalCities/Foundation-1

### Gary Tab

Gary uses MusicGen continuation models.

- Uses the first selected seconds of your recording buffer.
- Generates continuation audio.
- Output can be cropped, continued, retried, or dragged to the DAW timeline.

Model lists are fetched dynamically from the backend, so the menu depends on
what is loaded locally or remotely.

Learn more: https://github.com/facebookresearch/audiocraft

### Carey Tab

Carey uses ACE-Step.

- **lego** - generate vocals/backing vocals over your audio
- **complete** - extend audio into a full continuation
- **cover** - remix/restyle with caption guidance
- **extract** - attempt target stem extraction from your recording buffer

Shared lyrics editor, 50-language support, key/scale/time signature selection,
caption popouts, and LoRA support live here.

Full guide: [CAREY.md](CAREY.md)

Learn more: https://github.com/ace-step/ACE-Step-1.5

### Terry Tab

- Transforms audio with style presets or custom prompts.
- Can use either the recording buffer or current output audio.
- Undo is available after transforms.

Learn more: https://huggingface.co/spaces/facebook/Melodyflow

### Darius Tab

- High-quality 48 kHz continuations.
- Style steering with prompts, base model, or custom weights.
- Works on the recording buffer or current output audio.
- Requires a separate backend.

Learn more: https://github.com/magenta/magenta-realtime

---

## Finetuning

gary4juce gets better as more finetunes exist.

### MusicGen

Train through Audiocraft:

https://github.com/facebookresearch/audiocraft

As of late 2025, Google Colab is painful for Audiocraft training due to
dependency conflicts. Local training is the practical path.

### Stable Audio Open Small

Train with stable-audio-tools:

https://github.com/Stability-AI/stable-audio-tools

Encode, train, select checkpoints, upload to Hugging Face, then load through the
Jerry localhost finetune picker.

### Stable Audio 3

SA3 LoRA workflows are new and still settling. The v4 plugin UI is already
shaped around multi-LoRA strength sliders and LoRA-aware dice pools so the
backend can grow into that workflow cleanly.

Upstream repo: https://github.com/stability-ai/stable-audio-3

### Magenta Realtime

Magenta Realtime is one of the friendlier finetuning paths:

https://github.com/magenta/magenta-realtime

Upload weights to a Hugging Face Space and point the Darius tab at it.

---

## Project Structure

```text
gary4juce/
+-- Source/
|   +-- PluginProcessor.cpp/h
|   +-- PluginEditor.cpp/h
|   +-- Components/
|   |   +-- Gary/GaryUI.cpp/h
|   |   +-- Jerry/JerryUI.cpp/h
|   |   +-- Jerry/SA3UI.cpp/h
|   |   +-- Foundation/FoundationUI.cpp/h
|   |   +-- Carey/CareyUI.cpp/h
|   |   +-- Terry/TerryUI.cpp/h
|   |   \-- Darius/DariusUI.cpp/h
|   \-- Utils/
|       +-- Theme.h
|       +-- IconFactory.cpp/h
|       \-- BarTrim.cpp/h
+-- CAREY.md
+-- SA3.md
+-- docs/
\-- gary4juce.jucer
```

### Building From Source

Requirements:

- JUCE 8.0.8
- Visual Studio 2022 on Windows
- Xcode on macOS

Steps:

1. Open `gary4juce.jucer` in Projucer.
2. Save the project to regenerate build files.
3. Open the generated IDE project.
4. Build release configuration.

---

## Known Issues

- **SA3 launch notes:** remote-only, output loudness and continuation tails are still being tuned.
- **SA3 local backend:** not in gary4local yet.
- **Windows Defender:** not codesigned, so Windows may complain.
- **Darius hardware:** 24 GB+ VRAM is strongly recommended.
- **Terry variability:** Melodyflow is experimental and can be wonderfully strange.
- **Carey complete mode:** useful, but the upstream-style accompaniment workflow still needs a proper UI/backend pass.

---

## Community

**discord:** https://discord.gg/VECkyXEnAd

**musicgen community:** https://discord.gg/Mxd3nYQre9

**email:** kev@thecollabagepatch.com

**twitter/x:** https://twitter.com/@thepatch_kev

---

## Credits

- **stable-audio-3:** Stability AI ([repo](https://github.com/stability-ai/stable-audio-3))
- **musicgen:** Meta AI / Audiocraft team
- **stable-audio-open-small:** Stability AI
- **foundation-1:** RoyalCities ([model](https://huggingface.co/RoyalCities/Foundation-1), [tools](https://github.com/RoyalCities/RC-stable-audio-tools))
- **ace-step:** ACE-Step team ([repo](https://github.com/ace-step/ACE-Step-1.5))
- **melodyflow:** Meta AI / Audiocraft team
- **magenta-realtime:** Google Magenta team
- **JUCE:** JUCE framework
- **community finetunes:** lyra, vanya, hoenn, CZ-84, and everyone contributing models

Special thanks to Zach and the guys at Stability for letting me be part of the
Stable Audio 3 beta while this integration came together.

---

## License

gary4juce is free to use. Model licenses vary:

- stable-audio-3: check the Stability AI repo/license
- musicgen: check Audiocraft
- stable-audio-open-small: Stability AI license
- foundation-1: check [RoyalCities/Foundation-1](https://huggingface.co/RoyalCities/Foundation-1)
- ace-step: Apache 2.0
- melodyflow: Meta research license
- magenta-realtime: Apache 2.0

---

## Support The Project

If gary4juce is useful to you:

- share your creations and tag `@thepatch_kev`
- contribute finetunes to the community
- help improve docs
- Gumroad: https://thepatch.gumroad.com/l/gary4juce

No pressure. I mostly build this because I want these models to feel like
instruments.
