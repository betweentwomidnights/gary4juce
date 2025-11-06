# gary4juce

a VST3/AU plugin for AI-assisted music production. four models, one interface.

https://thepatch.gumroad.com/l/gary4juce (v1 until I update it this week)

built with JUCE 8.0.8 by kev @ [thecollabagepatch.com](https://thecollabagepatch.com)

videos about it here when i can:

https://youtube.com/@thepatch_dev

---

## what is this?

gary4juce gives you four AI music models directly in your DAW:

- **gary** (musicgen) - continuation/anti-looper. extends your audio in creative directions
- **jerry** (stable-audio-open-small) - BPM-aware 12-second loop generation in under a second
- **terry** (melodyflow) - audio transformation. turn your guitar into an orchestra
- **darius** (magenta-realtime) - high-quality 48kHz continuations with style control

put it on your master, press play, record some audio, and start iterating.

---

## what's new in V2

### no more ASIO hell
switched from system audio to host audio for playback. no more driver conflicts, no more crashes when your audio interface is unplugged, no more FlexASIO warnings. just works.

### jerry got smart
jerry can now load finetunes from hugging face repos when you're running localhost. on remote backend, you get whatever finetune i have loaded (for now, until we get a bigger GPU).

### darius joined the party
magenta-realtime is here. 48kHz continuations with style/centroid steering when using finetunes. massive model (needs 24GB+ VRAM ideally), but worth it. run it on a separate backend or duplicate the hugging face space.

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
   - `Gary4Juce.component` → Components (Audio Unit)
   - `Gary4Juce.vst3` → VST3
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
- docker & docker compose
- windows only for the control center/docker compose network... (working on some MLX stuff if i can)

**control center (windows):**

download from: https://thepatch.gumroad.com/l/gary4juce



**heads up:** not codesigned (i'm just one underfunded guy lol), so windows defender will be mean. repo is public so you can verify/build yourself.

build from source: https://github.com/betweentwomidnights/gary-localhost-installer

installs python environments for all three architectures (musicgen, melodyflow, stable-audio-open-small). lives in system tray, easy start/stop.

**manual docker compose:**

clone: https://github.com/betweentwomidnights/gary-backend-combined and follow the instructions there to build the containers and run with docker compose. 

then switch the plugin to 'local' mode.

designed for concurrent usage so might be overkill. the localhost-control-center is ideal for single usage.

### darius backend

darius is too chonky to run alongside the other models. separate backend required.

**option 1: hugging face space (easiest)**

duplicate this space: https://huggingface.co/spaces/thecollabagepatch/magenta-retry

requirements:
- L40s or A100-class GPU runtime (smaller is ok here because we're not expecting realtime generation)
- 24GB+ VRAM (48GB recommended)
- space uses ~87GB on my DGX spark

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
- vanya_ai_dnb_0.1 (default, remote + localhost)
- bleeps-medium (remote + localhost)
- gary_orchestra_2 (remote + localhost)
- hoenn_lofi (localhost only, needs 16GB+ VRAM)

learn more: https://github.com/facebookresearch/audiocraft

### jerry tab (stable-audio-open-small)

- generates 12-second loops aligned to your DAW's BPM
- text prompt based generation
- **smart loop mode** with type selection (auto/drums/instruments)
- drag output directly to timeline

**finetune support (localhost only):**
- click "+" button to add custom finetunes
- enter hugging face repo (e.g., `thepatch/jerry_grunge`)
- fetch checkpoints, select one, add to models

learn more: https://huggingface.co/stabilityai/stable-audio-open-small

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

the good news: once trained, finetuned models work seamlessly in the plugin (localhost mode).

### stable-audio-open-small (jerry)

training is multi-step (more involved than musicgen):
- requires stable-audio-tools: https://github.com/Stability-AI/stable-audio-tools
- encode → train → checkpoint selection

**coming soon:** simpler training guide on yt (hopefully)

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
- profit

---

## project structure

```
gary4juce/
├── Source/
│   ├── PluginProcessor.cpp/h     # audio processing, host audio playback
│   ├── PluginEditor.cpp/h        # UI, tab management, network requests
│   ├── Components/
│   │   ├── Gary/GaryUI.cpp/h     # musicgen interface
│   │   ├── Jerry/JerryUI.cpp/h   # stable-audio interface
│   │   ├── Terry/TerryUI.cpp/h   # melodyflow interface
│   │   └── Darius/DariusUI.cpp/h # magenta interface
│   └── Utils/
│       ├── Theme.h               # color scheme
│       ├── IconFactory.cpp/h     # SVG icons
│       └── BarTrim.cpp/h         # audio quantization utilities for darius
└── gary4juce.jucer               # projucer project file
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

## TODO

### high priority
- [ ] dice button for darius styles & weights (import from hf space magenta-prompts.js)
- [ ] dice button for jerry prompts (import from untitled jamming app for standard SAOS)
- [ ] system to load prompts from finetune metadata (jerry custom models)

### documentation
- [ ] create finetuning guide for stable-audio-open-small (youtube + written)
- [ ] update gumroad page for v2: https://thepatch.gumroad.com/l/gary4juce

---

## technical notes

### host audio vs system audio
V2 switched to host audio (processBlock mixing) instead of system audio (AudioDeviceManager). benefits:
- no ASIO driver conflicts
- no crashes when audio interface unplugged
- simpler architecture
- better reliability across DAWs

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
- **melodyflow:** meta AI / audiocraft team
- **magenta-realtime:** google magenta team
- **JUCE:** JUCE framework (juce.com)
- **community finetunes:** lyra, vanya, hoenn, CZ-84 and everyone contributing models

---

## license

this plugin is free to use. model licenses vary:
- musicgen: MIT-ish (check audiocraft repo)
- stable-audio-open-small: stability AI license
- melodyflow: meta research license
- magenta-realtime: apache 2.0



---

## known issues

- **windows defender flags control center:** not codesigned (building yourself recommended if paranoid)
- **darius needs beefy hardware:** 24GB+ VRAM or use hugging face space
- **terry occasionally weird:** melodyflow is still experimental, results vary
- **jerry finetunes:** remote backend limited to whatever i have loaded (GPU constraints)

---

## support the project

this is a passion project. if it's useful to you:
- share your creations (tag @thepatch_kev)
- contribute finetunes to the community
- help with documentation
- gumroad: https://thepatch.gumroad.com/l/gary4juce (pay what you want)

no pressure though. i just iterate on this for myself and hope that you'll have as much fun with it as i do.