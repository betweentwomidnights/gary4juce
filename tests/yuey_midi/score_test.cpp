// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

// Checks YueyMidiScore against the upstream event compiler. Every fixture is a pair of clips and
// the ABC the upstream compiler wrote for the same notes (see make_fixtures.py), and the output
// has to match it byte for byte. The rejection cases are the handoff's: what the importer
// must refuse rather than guess at.
//
//   cl /std:c++17 /EHsc /W4 score_test.cpp ..\..\Source\Yuey\YueyMidiScore.cpp
//   score_test.exe fixtures

#include "../../Source/Yuey/YueyMidiScore.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace yueymidi;

namespace
{
int failures = 0;
int checks = 0;

void check(bool condition, const std::string& what)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::cout << "FAIL: " << what << "\n";
    }
}

std::string slurp(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    // Fixtures may arrive with Windows line endings from a checkout; the ABC is compared without them.
    auto result = text.str();
    result.erase(std::remove(result.begin(), result.end(), ''), result.end());
    return result;
}

Clip loadClip(const std::string& path)
{
    Clip clip;
    std::ifstream in(path);
    std::string word;
    while (in >> word)
    {
        if (word == "ppq")
            in >> clip.ppq;
        else if (word == "end")
            in >> clip.endTick;
        else if (word == "meter")
            in >> clip.meterNumerator >> clip.meterDenominator;
        else if (word == "note")
        {
            Note note;
            in >> note.onset >> note.duration >> note.pitch;
            clip.notes.push_back(note);
        }
    }
    return clip;
}

std::string firstDifference(const std::string& got, const std::string& want)
{
    std::istringstream a(got), b(want);
    std::string x, y;
    int line = 1;
    while (true)
    {
        const bool hasX = static_cast<bool>(std::getline(a, x));
        const bool hasY = static_cast<bool>(std::getline(b, y));
        if (!hasX && !hasY)
            return "same lines, different ending";
        if (hasX != hasY || x != y)
            return "line " + std::to_string(line) + "\n  got:  " + x + "\n  want: " + y;
        ++line;
    }
}

void expectAbc(const Score& score, const std::string& want, const std::string& what)
{
    if (!score.ok)
    {
        check(false, what + ": refused (" + score.error + ")");
        return;
    }
    check(score.abc == want, what + ": " + firstDifference(score.abc, want));
}

void expectRefused(const Score& score, const std::string& needle, const std::string& what)
{
    check(!score.ok, what + ": should have been refused");
    check(score.error.find(needle) != std::string::npos,
          what + ": error should mention \"" + needle + "\", got \"" + score.error + "\"");
}

Clip melodyOf(int ppq, std::int64_t end, std::vector<Note> notes, int meterN = 4, int meterD = 4)
{
    Clip clip;
    clip.ppq = ppq;
    clip.endTick = end;
    clip.meterNumerator = meterN;
    clip.meterDenominator = meterD;
    clip.notes = std::move(notes);
    return clip;
}

Score justMelody(const Clip& clip, int bpm = 120) { return buildScore(&clip, nullptr, bpm, 4, 4); }
Score justChords(const Clip& clip, int bpm = 120) { return buildScore(nullptr, &clip, bpm, 4, 4); }

// A root-position triad with the root doubled, every note the same length.
std::vector<Note> triad(std::int64_t onset, std::int64_t duration, int root, bool minor)
{
    return { { onset, duration, root }, { onset, duration, root + 12 },
             { onset, duration, root + (minor ? 3 : 4) }, { onset, duration, root + 7 } };
}

void fixtureCases(const std::string& dir)
{
    std::istringstream manifest(slurp(dir + "/manifest.txt"));
    std::string name, kind;
    int bpm = 0, meterN = 0, meterD = 0;
    int cases = 0;
    while (manifest >> name >> bpm >> meterN >> meterD >> kind)
    {
        ++cases;
        const Clip melody = loadClip(dir + "/" + name + ".melody.clip");
        const Clip chords = loadClip(dir + "/" + name + ".chords.clip");
        if (kind == "real")
        {
            expectAbc(buildScore(&melody, nullptr, bpm, 4, 4), slurp(dir + "/real.imported.abc"), "real melody");
            expectAbc(buildScore(nullptr, &chords, bpm, 4, 4), slurp(dir + "/real.chords-only.abc"), "real chords only");
            expectAbc(buildScore(&melody, &chords, bpm, 4, 4), slurp(dir + "/real.combined.abc"), "real melody and chords");

            // A different tempo changes Q: and nothing else: the notes stay where they are in beats.
            auto retimed = slurp(dir + "/real.imported.abc");
            retimed.replace(retimed.find("Q:1/4=120"), 9, "Q:1/4=96");
            const auto slower = buildScore(&melody, nullptr, 96, 4, 4);
            expectAbc(slower, retimed, "real melody at 96 bpm");
            check(slower.ok && slower.bars == 10 && slower.seconds > 24.99 && slower.seconds < 25.01,
                  "10 bars at 96 bpm last 25 seconds");
        }
        else
        {
            expectAbc(buildScore(&melody, nullptr, bpm, 4, 4), slurp(dir + "/" + name + ".melody.abc"), name + " melody");
            expectAbc(buildScore(&melody, &chords, bpm, 4, 4), slurp(dir + "/" + name + ".both.abc"), name + " melody and chords");
        }
    }
    check(cases > 0, "the manifest lists cases");
    std::cout << cases << " fixture cases\n";
}

void rejectionCases()
{
    const int ppq = 96;
    const std::int64_t bar = 4 * ppq;

    // Two notes at once in the melody.
    expectRefused(justMelody(melodyOf(ppq, bar, { { 0, 96, 60 }, { 48, 96, 64 } })),
                  "more than one note", "overlapping melody notes");
    // Triplet eighths: not on a power-of-two grid.
    expectRefused(justMelody(melodyOf(ppq, bar, { { 0, 32, 60 }, { 32, 32, 62 }, { 64, 32, 64 } })),
                  "straight grid", "triplets");
    // A single note is not a chord.
    expectRefused(justChords(melodyOf(ppq, bar, { { 0, 192, 60 } })),
                  "major or minor triad", "single note as a chord");
    // First inversion: lowest note isn't the root.
    expectRefused(justChords(melodyOf(ppq, bar, { { 0, 192, 64 }, { 0, 192, 67 }, { 0, 192, 72 } })),
                  "major or minor triad", "inverted triad");
    // A seventh chord.
    expectRefused(justChords(melodyOf(ppq, bar, { { 0, 192, 48 }, { 0, 192, 52 }, { 0, 192, 55 }, { 0, 192, 58 } })),
                  "major or minor triad", "seventh chord");
    // Chord notes that don't release together.
    expectRefused(justChords(melodyOf(ppq, bar, { { 0, 192, 48 }, { 0, 96, 52 }, { 0, 192, 55 } })),
                  "end together", "staggered release");
    // Rolled chord: the notes start apart, so they arrive as separate one-note groups.
    expectRefused(justChords(melodyOf(ppq, bar, { { 0, 192, 48 }, { 6, 186, 52 }, { 12, 180, 55 } })),
                  "major or minor triad", "rolled chord");
    // No notes at all.
    expectRefused(justMelody(melodyOf(ppq, bar, {})), "no notes", "empty melody");
    // Nothing given.
    expectRefused(buildScore(nullptr, nullptr, 120, 4, 4), "nothing", "no lanes");

    // Lanes of different length: the handoff says report it, never crop or repeat.
    const Clip tenBars = melodyOf(ppq, 10 * bar, { { 0, 96, 60 } });
    const Clip eightBars = melodyOf(ppq, 8 * bar, triad(0, 192, 48, false));
    expectRefused(buildScore(&tenBars, &eightBars, 120, 4, 4), "10 bars and the chords are 8", "length mismatch");

    const Clip threeBeats = melodyOf(ppq, 3 * ppq, { { 0, ppq, 60 } });
    const Clip fourBeats = melodyOf(ppq, bar, triad(0, bar, 48, false));
    expectRefused(buildScore(&threeBeats, &fourBeats, 120, 4, 4),
                  "different lengths within", "unequal lengths that round to the same bar count");
    expectRefused(justMelody(threeBeats), "partway through a bar", "partial bar is not padded");

    auto gaps = triad(0, ppq, 48, false);
    const auto second = triad(2 * ppq, 2 * ppq, 55, false);
    gaps.insert(gaps.end(), second.begin(), second.end());
    expectRefused(justChords(melodyOf(ppq, bar, gaps)), "chord gaps", "gap between chords");
    auto overlaps = triad(0, 3 * ppq, 48, false);
    overlaps.insert(overlaps.end(), second.begin(), second.end());
    expectRefused(justChords(melodyOf(ppq, bar, overlaps)), "overlaps", "overlapping chords");
    expectRefused(justChords(melodyOf(ppq, bar, triad(0, 3 * ppq, 48, false))),
                  "chord gaps", "last chord releases before clip end");

    // Different meters.
    const Clip waltz = melodyOf(ppq, 3 * 3 * ppq, { { 0, 96, 60 } }, 3, 4);
    expectRefused(buildScore(&waltz, &eightBars, 120, 4, 4), "in 3/4 but the chords are in 4/4", "meter mismatch");

    // A bar line that falls inside a tick.
    expectRefused(justMelody(melodyOf(ppq, bar, { { 0, 96, 60 } }, 4, 3)),
                  "isn't supported", "meter with a denominator that isn't a power of two");
}

void behaviourCases()
{
    const int ppq = 480;
    const std::int64_t quarter = ppq;

    // No meter in the file: the UI's is used.
    Clip bare = melodyOf(ppq, 3 * quarter, { { 0, quarter, 60 } }, 0, 0);
    auto waltz = buildScore(&bare, nullptr, 100, 3, 4);
    check(waltz.ok && waltz.meterNumerator == 3 && waltz.abc.find("M:3/4\n") != std::string::npos,
          "a file without a meter uses the fallback");

    // One file with a meter and one without: the bare one takes the other's.
    Clip chordsBare = melodyOf(ppq, 3 * quarter, triad(0, 3 * quarter, 48, true), 0, 0);
    Clip melody34 = melodyOf(ppq, 3 * quarter, { { 0, quarter, 60 } }, 3, 4);
    auto both = buildScore(&melody34, &chordsBare, 100, 4, 4);
    check(both.ok && both.meterNumerator == 3 && both.abc.find("\"Cm\"") != std::string::npos,
          "a file without a meter takes the other lane's");

    // Chords alone leave the Ins lane resting, with no invented melody.
    auto onlyChords = buildScore(nullptr, &chordsBare, 100, 3, 4);
    check(onlyChords.ok && onlyChords.hasChords && !onlyChords.hasMelody
              && onlyChords.abc.find("V: Ins\nZ|") != std::string::npos,
          "chords alone rest the melody lane");

    // The clip's own length is kept even when the last bars are empty.
    Clip padded = melodyOf(ppq, 4 * 4 * quarter, { { 0, quarter, 60 } });
    auto four = buildScore(&padded, nullptr, 120, 4, 4);
    check(four.ok && four.bars == 4 && four.abc.find("C8z24|Z3|") != std::string::npos,
          "empty trailing bars are kept, not trimmed");

    // A note that crosses a bar line is tied across it.
    Clip tied = melodyOf(ppq, 2 * 4 * quarter, { { 3 * quarter, 2 * quarter, 61 } });
    auto crossing = buildScore(&tied, nullptr, 120, 4, 4);
    check(crossing.ok && crossing.abc.find("z24^C8-|^C8z24|") != std::string::npos,
          "a note across a bar line is tied, accidental repeated");

    // Same notes, new tempo: only Q: moves.
    auto at120 = buildScore(&tied, nullptr, 120, 4, 4);
    auto at90 = buildScore(&tied, nullptr, 90, 4, 4);
    auto swapped = at120.abc;
    swapped.replace(swapped.find("Q:1/4=120"), 9, "Q:1/4=90");
    check(at120.ok && at90.ok && swapped == at90.abc && at90.seconds > at120.seconds, "a new bpm only changes Q:");

    // A clip that ends off the grid is refused, not rounded.
    Clip ragged = melodyOf(ppq, 4 * quarter + 1, { { 0, quarter, 60 } });
    expectRefused(justMelody(ragged), "straight grid", "a clip that ends a tick past a bar");

    Clip otherPpq = melodyOf(96, 4 * 96, triad(0, 4 * 96, 48, false));
    Clip oneBar = melodyOf(ppq, 4 * quarter, { { 0, quarter, 60 } });
    check(buildScore(&oneBar, &otherPpq, 120, 4, 4).ok, "equal beat lengths with different PPQs are accepted");
    otherPpq.endTick = 3 * 96;
    otherPpq.notes = triad(0, 3 * 96, 48, false);
    expectRefused(buildScore(&oneBar, &otherPpq, 120, 4, 4), "different lengths within",
                  "different beat lengths across PPQs are rejected");

    // A leading rest can be written exactly before the first chord symbol.
    auto leading = justChords(melodyOf(ppq, 4 * quarter, triad(quarter, 3 * quarter, 48, false)));
    check(leading.ok && leading.abc.find("z8\"C\"z24|") != std::string::npos, "leading chord silence is preserved");
}
}  // namespace

int main(int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : "fixtures";
    fixtureCases(dir);
    rejectionCases();
    behaviourCases();
    std::cout << checks << " checks, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
