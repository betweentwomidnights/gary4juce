#!/usr/bin/env python3
"""Write the fixtures score_test.cpp checks YueyMidiScore against.

The expected ABC comes from the upstream event compiler the MIDI-import spikes used
(YuE's skills/yue2-music/instrumental/scripts/compile_score.py), run on the same notes the C++
sees. This script is only needed to regenerate the fixtures; the committed fixtures are all the
test needs. It reads the upstream helpers and the spike's MIDI inspections from where the spikes
left them:

    python make_fixtures.py [UPSTREAM_DIR] [SPIKE_ARTIFACTS_DIR]
"""
import json
import random
import sys
from fractions import Fraction
from pathlib import Path

ROOT = Path(__file__).parent
OUT = ROOT / 'fixtures'
ARTIFACTS = Path(sys.argv[2] if len(sys.argv) > 2 else r'C:\dev\gary-localhost-installer\artifacts')
UPSTREAM = Path(sys.argv[1] if len(sys.argv) > 1 else ARTIFACTS / 'midi-import-melody-test' / 'upstream')
sys.path.insert(0, str(UPSTREAM))
from compile_score import compile_events  # noqa: E402

MAJOR = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']
MINOR = [n + 'm' for n in ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'G#', 'A', 'Bb', 'B']]


def write(path, text):
    path.write_bytes(text.encode('utf-8'))  # LF everywhere, whatever the platform


def write_clip(path, ppq, end, meter, notes):
    lines = [f'ppq {ppq}', f'end {end}', f'meter {meter[0]} {meter[1]}']
    lines += [f'note {on} {dur} {pitch}' for on, dur, pitch in notes]
    write(path, '\n'.join(lines) + '\n')


def compile_abc(bpm, meter, bars, notes, ppq, chords=()):
    events = {
        'bpm': bpm, 'key': 'C',
        'bars': [{'meter': f'{meter[0]}/{meter[1]}', 'section': 'instrumental'} for _ in range(bars)],
        'notes': [[str(Fraction(on, ppq)), str(Fraction(dur, ppq)), pitch] for on, dur, pitch in notes],
        'chords': [[str(Fraction(on, ppq)), symbol] for on, symbol in chords],
    }
    abc, _ = compile_events(events)
    return abc


def real_fixtures(manifest):
    """The spikes' own MIDI: the 10-bar melody excerpt and the chord file."""
    melody = json.loads((ARTIFACTS / 'midi-import-melody-test' / 'midi-inspection.json').read_text())
    chords = json.loads((ARTIFACTS / 'midi-import-chord-test' / 'midi-inspection.json').read_text())
    ppq = melody['ppq']
    crop = 40 * ppq  # prepare.py keeps the first 40 quarter notes: 10 bars at 4/4
    notes = [(n['onset_ticks'], min(n['duration_ticks'], crop - n['onset_ticks']), n['pitch'])
             for n in melody['tracks'][0]['notes'] if n['onset_ticks'] < crop]
    write_clip(OUT / 'real.melody.clip', ppq, crop, (4, 4), notes)
    chord_notes = [(n['onset_ticks'], n['duration_ticks'], n['pitch']) for n in chords['tracks'][0]['notes']]
    write_clip(OUT / 'real.chords.clip', chords['ppq'], 40 * chords['ppq'], (4, 4), chord_notes)
    for name in ('imported.abc', 'chords-only.abc', 'combined.abc'):
        src = ARTIFACTS / ('midi-import-melody-test' if name == 'imported.abc' else 'midi-import-chord-test') / name
        write(OUT / f'real.{name}', src.read_text())
    manifest.append('real 120 4 4 real')


def random_case(index, rng, manifest):
    ppq = rng.choice([96, 192, 480, 960])
    meter = rng.choice([(4, 4), (3, 4), (2, 4), (6, 8), (5, 4), (12, 8)])
    bpm = rng.choice([80, 96, 100, 120, 128, 140])
    bars = rng.randint(1, 11)
    bar_ticks = 4 * ppq * meter[0] // meter[1]
    total = bars * bar_ticks
    # Most cases sit on 1/32-note units; one in five goes a step finer (1/64 note).
    step = ppq // 8 if rng.random() > 0.2 else ppq // 16
    cursor, notes = 0, []
    while True:
        cursor += rng.choice([0, 0, 0, 1, 2, 4, 8]) * step
        length = rng.choice([1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 80]) * step
        if cursor + length > total:
            break
        notes.append((cursor, length, rng.randint(36, 96)))
        cursor += length
    if not notes:
        notes = [(0, step * 4, 60)]
    # Progression: a triad per change, doubled root, held until the next change.
    chord_notes, chord_events, tick = [], [], 0
    while tick < total:
        hold = min(total - tick, rng.choice([1, 2, 4, 8]) * bar_ticks // 4)
        root = rng.randint(0, 11)
        minor = rng.random() < 0.4
        base = 36 + root
        pitches = [base, base + 12, base + (3 if minor else 4), base + 7]
        chord_notes += [(tick, hold, p) for p in pitches]
        chord_events.append((tick, (MINOR if minor else MAJOR)[root]))
        tick += hold
    name = f'rand{index:02d}'
    write_clip(OUT / f'{name}.melody.clip', ppq, total, meter, notes)
    write_clip(OUT / f'{name}.chords.clip', ppq, total, meter, chord_notes)
    write(OUT / f'{name}.melody.abc', compile_abc(bpm, meter, bars, notes, ppq))
    write(OUT / f'{name}.both.abc', compile_abc(bpm, meter, bars, notes, ppq, chord_events))
    manifest.append(f'{name} {bpm} {meter[0]} {meter[1]} rand')


def main():
    OUT.mkdir(exist_ok=True)
    for old in OUT.iterdir():
        old.unlink()
    manifest = []
    real_fixtures(manifest)
    rng = random.Random(20261005)
    for index in range(40):
        random_case(index, rng, manifest)
    write(OUT / 'manifest.txt', '\n'.join(manifest) + '\n')
    print(f'{len(manifest)} cases in {OUT}')


main()
