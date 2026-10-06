# yuey guide

> like the other guides, this'll drift as i keep learning what this model is
> good at inside a session. if you find a clever way to use it, this is a great
> place to add a contribution.

yuey is [YuE2](https://github.com/multimodal-art-projection/YuE) inside
gary4juce. it runs on [yuey.cpp](https://github.com/betweentwomidnights/yuey.cpp),
our native C++/GGML build of the model, so there's no Python anywhere in it.

status:

- **remote backend:** for everyone.
- **windows gary4local:** v0.4.0 and up, on NVIDIA (CUDA), AMD and Intel
  (Vulkan).
- **mac localhost:** being worked on.

---

## how it differs from official YuE

official YuE is a full song generator. you hand it lyrics and tags, it plans a
whole song, and it renders it. inside a DAW you usually want something else: a
piece in your project's tempo and key, a remix of what you just recorded, or
more of a song that's already going. so yuey bends YuE2 toward that. none of
this changes the model; it changes what we ask it for.

### create: a chord scaffold instead of a plan

by default, create doesn't let the model plan the song. gary4juce writes the
score itself from the tempo, key, meter and bar count you've set. it puts chord
symbols over rests in the Vocal lane and leaves the instrument lane empty. the
model writes its own melody and arrangement over that harmony, and it renders
straight away. letting YuE2 plan can take anywhere from 40 seconds to over four
minutes.

the chords aren't random. they come from a pool of the familiar loops from pop,
rock, dance, jazz and film, spelled in your key:

- **15 major progressions**, like I–V–vi–IV, I–vi–IV–V, vi–IV–I–V, a
  ii7–V7–Imaj7–vi7 jazz turnaround, the Pachelbel loop, and two-chord vamps
  such as I–IV, I–bVII and Imaj7–IVmaj7.
- **16 minor progressions**, like i–VI–III–VII, i–iv–VII–III, the Andalusian
  i–VII–VI–V, a iiø7–V7–i7–VImaj7 jazz minor turnaround, and vamps such as a
  dorian i–IV and i7–iv7.

each render rolls a pair from that pool: one progression for the verse and a
different one for the chorus. no two neighbouring bars share a chord. it's the chord changes
that hold yuey to your tempo; in testing, restating one chord per bar drifted
just like writing no chords at all.

two ways out of the scaffold:

- **let yuey plan** hands the score back to the model, like official YuE. it's
  musically freer and a lot slower.
- **key "none"** sends no score and no plan at all. the model writes freely from
  your prompt, and the tempo goes into the prompt as a nudge rather than a
  rule.

### instrumental

instrumental is best effort. YuE2 can still sing when the score says not to,
which is why the
[instrumental LoRA](https://huggingface.co/Mothersuperior/YuE2-instrumental-cot-full-loras)
exists, and gary4local uses it when it's downloaded.

when a score has a vocal melody (from a plan, or from a remix's
transcription), instrumental moves it onto an instrument, the way official YuE
does, so you still hear the tune. gary4local can switch that to "our original",
which takes the melody out and keeps only the backing part.

### remix: transcribe + remix

remix runs your recording through SheetSage2, which writes it down as a score:
the melody, and the chords too in full mode. yuey then renders that score
again in your style prompt. the tempo comes from the transcription, and the
render starts on the first downbeat it finds, so you don't get a bar of silence
first.

SheetSage2 transcribes pitched melody. rap and spoken vocals usually come out
with no vocal notes at all, so a remix of a rap track follows the beat's
melodic parts, not the voice.

### continue

- **score continuation** transcribes your audio and composes what comes next
  from the score. it's a fresh render, so it won't sound exactly like your
  audio, but it follows its harmony and melody.
- **audio continuation** carries the audio itself forward through the
  [real-audio adapter pair](https://huggingface.co/Mothersuperior/yue2-mothersuperior-realaudio-tokenizer-v4).
  it holds on to your sound more closely, but the whole result is rebuilt, so
  it can lose a little fidelity.

### midi instead of audio

remix and continue can start from midi instead of a recording. pick "midi" next
to recording and output, and two small slots show up: **melody** and **chords**.
drop a `.mid` file on each, or click one to browse. either slot can be empty.
there's no transcription, because your midi is already the score, and the
tab looks the same as ever until you pick it.

- **remix** renders your score as it is, in your style prompt.
- **continue** is a score continuation that starts from your midi. yuey extends
  it, with the same "let yuey choose" or "add bars" length, and then renders the
  whole expanded piece fresh.

yuey uses the length of the file you give it. there's no cropping here and
nothing is looped or stretched, so trim your clip to the bars you want in your
DAW before you export it. if you give it both files they have to be the same
length and meter, and it'll tell you if they aren't. the tempo is the project's
(or the bpm on the create tab, in the standalone), and your notes keep their
place in beats, so changing it changes how long the piece lasts and nothing
else.

it only takes what it can write down exactly, and it says so when it can't:

- the melody has to be one note at a time, on one track.
- rhythms have to be on a straight grid. triplets and loose timing get
  refused, so quantize first.
- chords have to be root-position major or minor triads. inversions, sevenths
  and rolled chords aren't supported yet.
- no drums, pitch bends or controller moves (mod wheel, sustain, expression).

the melody goes on the instrument lane and the chords are written as chord
symbols, so with chords it renders in full-score mode and the chords count. i've
only tried this instrumental. i don't know yet what a vocal render does with an
imported melody.

### the score window

every yuey render keeps its score. two handles on the output waveform get at
it: "midi" drags every lane (melody, vocal, instrument and chords) straight into
your DAW, and "score" opens the score window:

- read and edit the ABC directly, or copy it out, have an agent work on it, and
  paste it back.
- quick edits: `/2` and `x2` for half and double time, a transpose knob,
  "to inst" to move the melody onto the instrument, "swap lanes", and "no
  chords" to let yuey harmonise the melody itself. hover any of them for what
  it does.
- "render this score" sends it back to yuey.

half time on an odd tempo has to round, because ABC tempos are whole numbers.
the window says so when it happens.

### length and seeds

- **let yuey choose** lets the model pick the length, up to the backend's
  ceiling: 96 seconds by default on both the remote backend and gary4local,
  where you can change it. a continuation counts only what it adds, so
  continuing a 30-second clip can come back about two minutes long.
- **choose bars** fixes it.
- **use seed** repeats a render. the last seed is kept, so you can get back to
  one you liked.

---

## models

- **YuE2 GGUFs, every tier:**
  [thepatch/YuE2-3B-GGUF](https://huggingface.co/thepatch/YuE2-3B-GGUF).
  gary4local downloads them for you.
- **instrumental LoRA:**
  [Mothersuperior/YuE2-instrumental-cot-full-loras](https://huggingface.co/Mothersuperior/YuE2-instrumental-cot-full-loras).
- **real-audio adapter pair:**
  [Mothersuperior/yue2-mothersuperior-realaudio-tokenizer-v4](https://huggingface.co/Mothersuperior/yue2-mothersuperior-realaudio-tokenizer-v4).
- **the original model:** [m-a-p/YuE2-3B](https://huggingface.co/m-a-p/YuE2-3B),
  with [SheetSage2](https://huggingface.co/m-a-p/SheetSage2) for
  transcription.

the YuE2, SheetSage2 and MERT2 checkpoints are CC BY-NC 4.0.
