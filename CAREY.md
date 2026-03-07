# carey (ace-step) - guide

carey brings the [ACE-Step](https://github.com/ace-step/ACE-Step-1.5) music generation model into gary4juce with three modes: **lego**, **complete**, and **cover**. each mode uses the same underlying DiT (diffusion transformer) model but approaches generation differently.

backend repo: [ace-lego](https://github.com/betweentwomidnights/ace-lego)

---

## lego mode

**what it does:** generates a single stem (vocals, backing vocals, etc.) over your existing audio.

**best with:** ~2 minutes of recorded audio. shorter clips work thanks to loop assist (see below).

### tips

- **vocals and backing vocals are the sweet spot.** these produce genuinely impressive results. the other stem options (synth, keyboard, etc.) will generate layers that harmonically "fit" your audio, but tend to sound generic. the chords are correct though, so they can serve as a compositional reference.
- **backing vocals trick:** sound out words that match the syllables you hear in the existing vocal layer. alternatively, if you generated vocals with lyrics, use the same lyrics for backing vocals - the model will create harmonies.
- **lyrics are optional.** leave them blank for wordless vocal generation (humming, "la la la", etc.). this produces surprisingly musical results and is great as a starting point for the cover mode workflow described below.

### loop assist & trim to input

the model doesn't perform well with audio shorter than ~1 minute. loop assist duplicates your input audio up to a length the model prefers (~2 minutes).

- **loop assist ON + trim to input ON:** the model generates over the full looped audio, then trims the output back to match your original input length. seamless - you won't notice the model actually worked with 2 minutes of audio.
- **trim to input OFF:** hear the full generation. useful even with short input audio - sometimes the model's iteration over the repeated sections produces interesting variations.

---

## complete mode

**what it does:** produces a continuation from your recorded audio, extending it into new territory.

**best with:** any length of input audio. unlike musicgen, ace-step doesn't seem to perform better or worse based on input length.

### tips

- **this mode is experimental.** results vary wildly between generations. when it converges on something good, it really slaps.
- **the model prefers "full" input audio.** if you record just a solo guitar layer and try to continue it (like you would with musicgen/gary), the model may decide to overwrite your conditioning audio entirely. it works best with denser arrangements as input.
- **duration slider** controls how long the output will be (30-180 seconds). the model generates the full duration including your input audio as the beginning.
- **use source as reference** passes your audio as both the conditioning input and a style reference, encouraging the continuation to stay closer to your original timbre and feel.

---

## cover mode

**what it does:** remixes/restyles your input audio, similar to a melodyflow (terry) transformation but with different strengths and characteristics.

### key parameters

- **noise strength** (0.0-1.0): lower values = more creative departure from source. higher = stays closer to the original structure. default 0.2 works well.
- **audio strength** (0.0-1.0): fraction of diffusion steps that use your source audio's semantic codes. 0.3 for instrumental, 0.5-0.7 if vocals are present.
- **cfg scale** (3.0-10.0): how strictly the model follows your caption. 7 is balanced, 9+ is very literal.
- **use source as reference:** forces the model to adhere more strictly to your input audio at the cost of some audio quality due to the encoding process.

### tips

- **vocals make everything better.** the model produces noticeably higher quality cover outputs when vocals are present in the input audio.
- **the gibberish-to-lyrics workflow:** generate wordless vocals in lego mode (no lyrics set), then switch to cover mode and type actual lyrics. the model will fill in the vocal melody with real words. this gets powerful with iteration.
- **loop assist is less reliable here.** cover mode has the same loop assist/trim-to-input functionality as lego, but the results are less consistent with looped short audio. if possible, give it a full minute+ of source material.

---

## shared features across all modes

### lyrics

- one shared lyrics editor across all three tabs - click the **lyrics** button on any tab
- supports [structure tags] like `[Verse 1]`, `[Chorus]`, etc.
- use one line per phrase
- leave blank for instrumental/wordless generation
- lyrics persist across plugin restarts (saved in your DAW project)

### language

- 50 languages supported via a dropdown in the lyrics dialog
- defaults to English
- **important:** the language setting tells the model how to vocalize - it does NOT translate. write your lyrics in the target language.

### key & scale

- select from the dropdown for best results if you know your song's key
- use your ears to find the root note of your project's "home" chord
- this guides the model's harmonic choices

### BPM

- automatically picks up your DAW's global BPM (like jerry/stable-audio)
- standalone mode has a manual BPM slider

### cfg scale

- available on all three tabs (lego, complete, cover)
- controls how strictly the model follows your caption
- 3.0 = loose interpretation, 7.0 = balanced, 10.0 = very strict
- recommended range: 7-9

### inference steps

- more steps = higher quality but slower generation
- 50 is a solid default
- 32 is fine for quick previews
- cover mode supports down to 8 steps for rapid experimentation

---

## a note for musicians

this model, like jerry (stable-audio-open-small), uses your DAW's global BPM as input. for best results, know what key and scale your song is in.

if you want pure text-to-music without a DAW, use the [official ace-step repo](https://github.com/ace-step/ACE-Step-1.5) directly - it can generate full songs from just a text prompt. carey in gary4juce is designed specifically for the DAW workflow: record audio, generate stems, iterate.
