# gary4juce

a VST3/AU plugin for AI-assisted music production. six models, one interface.

https://thepatch.gumroad.com/l/gary4juce

localhost backends:
- windows: [gary4local](https://github.com/betweentwomidnights/gary-localhost-installer)
- macOS: [gary4local mac](https://github.com/betweentwomidnights/gary-localhost-installer-mac) - foundation-1 support still pending there

built with JUCE 8.0.8 by kev @ [thecollabagepatch.com](https://thecollabagepatch.com)

videos about it here when i can: https://youtube.com/@thepatch_dev

---

## what is this?

gary4juce gives you six AI music models directly in your DAW:

- **gary** (musicgen) - continuation/anti-looper. extends your audio in creative directions
- **jerry** (stable-audio-open-small) - BPM-aware 12-second loop generation in under a second
- **rc-jerry** ([foundation-1](https://huggingface.co/RoyalCities/Foundation-1)) - BPM and key-aware 4/8-bar loop generation with structured prompt assembly
- **carey** (ace-step) - stem generation, audio continuation, and remix/cover with lyrics + multilingual support
- **terry** (melodyflow) - audio transformation. turn your guitar into an orchestra
- **darius** (magenta-realtime) - high-quality 48kHz continuations with style control

put it on your master, press play, record some audio, and start iterating.

---

## TODO

- [ ] add foundation-1 to [gary4local mac](https://github.com/betweentwomidnights/gary-localhost-installer-mac)
- [ ] finish assimilating the mac branch of gary4juce
- [ ] release gary4juce v3.0.2 mac AU/VST3
- [ ] potentially add 'extract' mode if we can get it to be more reliable
- [x] release official gary4juce v3 on github and [gumroad](https://thepatch.gumroad.com/l/gary4juce)
- [ ] produce better carey dice captions
- [x] introduce time signature option to the carey UI
- [ ] create finetuning guide for stable-audio-open-small (youtube + written)

---

## what's new in v3

### v3.0.2 - plugin-safe updates + ace-step xl models

gary4juce now includes a plugin-safe update checker: the UI can check the GitHub release feed and open the download page without trying to self-replace files while a DAW is running.

carey's complete mode on the remote backend now uses ACE-Step v1.5 XL models, defaulting to [acestep-v15-xl-turbo](https://huggingface.co/ACE-Step/acestep-v15-xl-turbo). advanced settings can switch to [acestep-v15-xl-base](https://huggingface.co/ACE-Step/acestep-v15-xl-base) for editable steps/cfg; turbo is fixed at 8 steps and 1.0 cfg.

### rc-jerry (foundation-1) - structured loop generation

[Foundation-1](https://huggingface.co/RoyalCities/Foundation-1) by RoyalCities joins the jerry tab as a sub-tab. where jerry (SAOS) excels at quick drum loops, rc-jerry generates BPM and key-aware synth/instrument loops of 4 or 8 bars using a structured prompt engine. it can also iterate on input audio â€” record or drag in up to 8 bars, toggle "use input audio" in advanced settings, and the model will re-voice your recording with the target timbre/FX while preserving rhythmic and melodic structure.

the UI is built around composable controls that assemble prompts from tagged categories:

- **family/type** - instrument family (synth, bass, keys, mallet, brass, etc.) and subfamily
- **character** - up to 3 timbre descriptor knobs from 50 tags (warm, gritty, crisp, metallic, etc.) with +/- to add/remove
- **extra descriptors** - freeform text tags with +/- rows (like darius style embeddings)
- **behavior** - speed, rhythm, contour, density as toggle+knob controls (some with optional 2nd knob via +/-)
- **spatial, style, frequency band, synthesis** - toggle+knob for spatial character, style tags, freq bands, and wave/tech
- **effects** - reverb, delay, distortion, phaser, bitcrush as toggle+knob pedals
- **randomize** - backend `/randomize` endpoint uses RoyalCities' weighted prompt engine to populate the entire UI with musically coherent presets
- **audio2audio** - toggle "use input audio" to re-voice your recording buffer with the selected timbre/FX (transformation slider controls how much to change)
- **presets** - save/load `.f1preset` files to `~/Documents/gary4juce/foundation-presets/`
- **state persistence** - all settings saved/restored with your DAW project

BPM is synced from DAW and snapped to supported values (100-150), with automatic time-stretching when needed. key and scale are user-controlled.

powered by [RC-stable-audio-tools](https://github.com/RoyalCities/RC-stable-audio-tools). currently available on the remote backend and in the windows build of gary4local. the macOS build of gary4local still needs foundation-1 support.

### carey (ace-step) joined the party

the biggest addition to gary4juce yet. three modes powered by the [ACE-Step 1.5](https://github.com/ace-step/ACE-Step-1.5) diffusion transformer:

- **lego** - generate vocals or backing vocals over your existing audio. loop assist handles short clips automatically.
- **complete** - extend any audio into a full continuation. experimental, wildly creative when it converges.
- **cover** - remix/restyle your audio with a text caption. chain with lego for a gibberish-to-lyrics workflow.

plus: shared lyrics editor with 50-language support, key/scale/time signature selection, per-tab cfg control, and inference step tuning.

v3.0.2 adds remote complete-mode selection between [acestep-v15-xl-turbo](https://huggingface.co/ACE-Step/acestep-v15-xl-turbo) as the default and [acestep-v15-xl-base](https://huggingface.co/ACE-Step/acestep-v15-xl-base) for editable steps/cfg.

**[detailed carey guide](CAREY.md)** - tips, parameter explanations, and workflow tricks.

backend repo: [ace-lego](https://github.com/betweentwomidnights/ace-lego)

---

## installation

### windows

1. close your DAW
2. extract `gary4juce.vst3` folder from the ZIP
3. copy entire folder to: `C:\Program Files\Common Files\VST3\`
4. reopen DAW and rescan plugins if needed

**notes:**
- most DAWs auto-detect this location (Ableton, Bitwig, Cubase, Reaper, etc.)
- admin rights may be required. if permission errors, run cmd as admin:
```
xcopy "path\to\extracted\gary4juce.vst3" "C:\Program Files\Common Files\VST3\gary4juce.vst3" /E /I /Y
```

### mac (AU/VST3)

1. quit your DAW
2. open the DMG
3. drag to matching folder:
   - `Gary4Juce.component` -> Components (Audio Unit)
   - `Gary4Juce.vst3` -> VST3
4. reopen DAW and rescan

**notes:**
- garageband/logic: use AU (.component)
- ableton/FL: either works
- reaper/cubase/bitwig: use VST3

---

## backends

the plugin can use either:
- **remote backend** (my server, free, on a spot VM so @ me if it's down)
- **localhost** (your machine, requires GPU, full control)

### remote backend (default)

just works. no setup. limited to whatever models i have loaded at the moment.

backend runs at: `https://g4l.thecollabagepatch.com`

### localhost backend

requires:
- 10GB+ GPU VRAM minimum (16GB+ recommended for melodyflow/hoenn_lofi)
- enough disk space for model weights + python envs

**windows:**

use [gary4local](https://github.com/betweentwomidnights/gary-localhost-installer).

**heads up:** not codesigned (i'm just one underfunded guy lol), so windows defender will be mean. repo is public so you can verify/build yourself.

it manages local envs for gary, terry, jerry, carey, and foundation-1 and lives in the system tray for easy start/stop.

**macOS:**

use [gary4local mac](https://github.com/betweentwomidnights/gary-localhost-installer-mac).

same idea, same plugin workflow. current known gap: foundation-1 has not been added there yet.

for most people, the dedicated gary4local apps are the right localhost path now.

### foundation-1 backend (rc-jerry)

rc-jerry uses the [RC-stable-audio-tools](https://github.com/RoyalCities/RC-stable-audio-tools) fork running in a docker container.

**remote:** already set up on the remote backend (DGX Spark). just switch to the foundation-1 sub-tab inside jerry.

**localhost (windows):** available through [gary4local](https://github.com/betweentwomidnights/gary-localhost-installer).

**localhost (macOS):** not there yet in [gary4local mac](https://github.com/betweentwomidnights/gary-localhost-installer-mac).

### carey backend (ace-step)

carey runs on a separate backend from the other models.

**remote:** already set up on the remote backend. just use it.

**localhost:** available through gary4local. if you want to run the backend directly or hack on the API patches, see [ace-lego](https://github.com/betweentwomidnights/ace-lego).

### darius backend (magenta-realtime)

darius is too chonky to run alongside the other models. separate backend required.

**option 1: hugging face space (easiest)**

duplicate this space: https://huggingface.co/spaces/thecollabagepatch/magenta-retry

requirements:
- L40s or A100-class GPU runtime
- 24GB+ VRAM (48GB recommended)

once duplicated, enter the space URL in darius tab.

**option 2: localhost docker**

clone: https://github.com/betweentwomidnights/magenta-rt

```bash
# x86_64 CUDA
docker build -f Dockerfile.cuda -t magenta-rt .
docker run --gpus all -p 7860:7860 magenta-rt

# arm64 CUDA (jetson, etc.)
docker build -f Dockerfile.arm64 -t magenta-rt .
docker run --runtime nvidia -p 7860:7860 magenta-rt
```

**darius needs serious hardware.** if you don't have 24GB+ VRAM, use the hugging face space option.

---

## how to use

1. **put it on your master track** (or anywhere, really)
2. **press play in your DAW** (solo specific tracks if you want - i do this a lot with percussion)
3. **save your recording buffer** when you have something you like

### gary tab (musicgen continuations)

- uses first X seconds of your recording buffer
- click "send to gary" to generate a continuation
- once in output waveform, you can:
  - **crop** at playback position
  - **continue** from the end
  - **retry** last continuation with different settings

models available:
- model lists are fetched dynamically from the backend, so the exact menu depends on what you've got loaded locally/remotely
- common examples: `thepatch/vanya_ai_dnb_0.1`, `thepatch/bleeps-medium`, `thepatch/gary_orchestra_2`, `thepatch/keygen-gary-v2-small-*`, `thepatch/keygen-gary-v2-large-*`
- `thepatch/hoenn_lofi` is still a heavier localhost-only favorite and likes 16GB+ VRAM
- naming scheme note: in names like `thepatch/bleeps-large-6`, the trailing number is the checkpoint export epoch. lower can generalize better; higher can get more overfit to the training set
- kev's current personal favorites are the `keygen-gary-v2-` checkpoints, both small and large. musicgen is older architecture at this point, but it's still ridiculously fun to finetune

learn more: https://github.com/facebookresearch/audiocraft

### jerry tab (two sub-tabs)

the jerry tab contains two models that complement each other:

#### jerry (SAOS) - stable-audio-open-small

- generates 12-second loops aligned to your DAW's BPM
- text prompt based generation
- **smart loop mode** with type selection (auto/drums/instruments)
- drag output directly to timeline
- excels at drum loops and percussive patterns

**finetune support (localhost only):**
- click "+" button to add custom finetunes
- enter hugging face repo (e.g., `thepatch/jerry_grunge`)
- fetch checkpoints, select one, add to models
- notable finetunes: `thepatch/jerry_grunge` from kev, and [S3Sound/kickbass](https://huggingface.co/S3Sound/kickbass) by CZ-84 if you want nasty good bass loops
- we train these in discord with help from zach at stable audio, so if you want in on the chaos, come hang

learn more: https://huggingface.co/stabilityai/stable-audio-open-small

#### rc-jerry (foundation-1)

- generates 4 or 8 bar loops synced to DAW BPM and key
- structured prompt assembly via knobs, toggles, and tag controls
- randomize button generates musically coherent presets via backend weighted prompt engine
- save/load presets as `.f1preset` files
- excels at synth, bass, keys, mallet, and brass loops
- available on remote and in the windows build of gary4local
- macOS localhost support is still pending in gary4local mac

learn more: https://huggingface.co/RoyalCities/Foundation-1

### carey tab (ace-step)

three modes for stem generation, continuation, and remix:

- **lego** - generate vocals/backing vocals over your audio
- **complete** - extend audio into a full continuation
- **cover** - remix/restyle with caption guidance

shared lyrics editor, 50-language support, key/scale selection, and per-tab cfg control.

on the remote backend, complete mode defaults to [acestep-v15-xl-turbo](https://huggingface.co/ACE-Step/acestep-v15-xl-turbo) and can switch to [acestep-v15-xl-base](https://huggingface.co/ACE-Step/acestep-v15-xl-base) from advanced settings.

**[full guide with tips and workflows](CAREY.md)**

learn more: https://github.com/ace-step/ACE-Step-1.5

### terry tab (melodyflow)

- transforms audio with style presets or custom prompts
- works on either recording buffer OR output audio
- presets like: accordion_folk, piano_classical, trap_808, orchestral_glitch, etc.
- **undo** button available after transforms

learn more: https://huggingface.co/spaces/facebook/Melodyflow

### darius tab (magenta-realtime)

- high-quality 48kHz continuations
- style steering with prompts using base model or custom weights
- centroid-based control (when assets loaded)
- works on recording buffer OR output audio
- **note:** requires separate backend (see backends section)

learn more: https://github.com/magenta/magenta-realtime

---

## finetuning

this plugin gets better the more finetunes we create.

### musicgen (gary)

**important:** as of november 2025, google colab no longer supports audiocraft training runs due to dependency conflicts. you'll need to train locally.

training locally:
- clone audiocraft: https://github.com/facebookresearch/audiocraft
- follow their training guide
- 80-100 tracks minimum, 30-second chunks
- consistency in style matters more than quantity

### stable-audio-open-small (jerry)

training is multi-step (more involved than musicgen):
- requires stable-audio-tools: https://github.com/Stability-AI/stable-audio-tools
- encode -> train -> checkpoint selection

once trained:
- upload to hugging face
- use custom finetune loader in plugin (localhost mode)
- repo should contain checkpoint files

### melodyflow (terry)

no official training code available yet (model hasn't been released in audiocraft repo). this might end up being the most fun one to finetune when it becomes available.

### magenta-realtime (darius)

easiest to finetune of the bunch.

google colab notebook: https://github.com/magenta/magenta-realtime

once trained:
- upload to hugging face following space setup guide
- point darius tab to your space URL

---

## project structure

```text
gary4juce/
+-- Source/
|   +-- PluginProcessor.cpp/h     # audio processing, host audio playback
|   +-- PluginEditor.cpp/h        # UI, tab management, network requests
|   +-- Components/
|   |   +-- Gary/GaryUI.cpp/h         # musicgen interface
|   |   +-- Jerry/JerryUI.cpp/h       # stable-audio interface
|   |   +-- Foundation/FoundationUI.h # foundation-1 interface (jerry sub-tab)
|   |   +-- Carey/CareyUI.h           # ace-step interface (lego/complete/cover)
|   |   +-- Terry/TerryUI.cpp/h       # melodyflow interface
|   |   \-- Darius/DariusUI.cpp/h     # magenta interface
|   \-- Utils/
|       +-- Theme.h               # color scheme
|       +-- IconFactory.cpp/h     # SVG icons
|       \-- BarTrim.cpp/h         # audio quantization utilities
|-- CAREY.md                      # detailed ace-step guide
\-- gary4juce.jucer               # projucer project file
```

### building from source

requirements:
- JUCE 8.0.8 (download from juce.com)
- visual studio 2022 (windows) or xcode (mac)

steps:
1. open `gary4juce.jucer` in projucer
2. save project (generates build files)
3. open in IDE:
   - windows: open `.sln` in visual studio (the purple one)
   - mac: open `.xcodeproj` in xcode
4. build release configuration

---

## known issues

- **windows defender flags control center:** not codesigned (building yourself recommended if paranoid)
- **darius needs beefy hardware:** 24GB+ VRAM or use hugging face space
- **terry occasionally weird:** melodyflow is still experimental, results vary
- **carey cover mode:** progress display uses time-based estimation (ace-step doesn't report step counts for cover tasks)
- **carey complete mode:** experimental - prefers fuller/denser input audio for best results
- **foundation-1 on mac localhost:** still pending in gary4local mac

---

## community

**discord:** https://discord.gg/VECkyXEnAd (@ kev if you need something)

**musicgen community:** https://discord.gg/Mxd3nYQre9 (where this all started - shoutout to lyra, vanya, hoenn, and baltigor for getting me hooked on audio models)

**email:** kev@thecollabagepatch.com

**twitter/x:** https://twitter.com/@thepatch_kev

---

## credits

- **musicgen:** meta AI / audiocraft team
- **stable-audio-open-small:** stability AI
- **foundation-1:** RoyalCities ([model](https://huggingface.co/RoyalCities/Foundation-1), [tools](https://github.com/RoyalCities/RC-stable-audio-tools))
- **ace-step:** ACE-Step team ([repo](https://github.com/ace-step/ACE-Step-1.5))
- **melodyflow:** meta AI / audiocraft team
- **magenta-realtime:** google magenta team
- **JUCE:** JUCE framework (juce.com)
- **community finetunes:** lyra, vanya, hoenn, CZ-84 and everyone contributing models

---

## license

this plugin is free to use. model licenses vary:
- musicgen: MIT-ish (check audiocraft repo)
- stable-audio-open-small: stability AI license
- foundation-1: check [RoyalCities/Foundation-1](https://huggingface.co/RoyalCities/Foundation-1)
- ace-step: apache 2.0
- melodyflow: meta research license
- magenta-realtime: apache 2.0

---

## support the project

this is a passion project. if it's useful to you:
- share your creations (tag @thepatch_kev)
- contribute finetunes to the community
- help with documentation
- gumroad: https://thepatch.gumroad.com/l/gary4juce (pay what you want)

no pressure though. i just iterate on this for myself and hope that you'll have as much fun with it as i do.

