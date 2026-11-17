// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "YueyMidiScore.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <numeric>
#include <tuple>

namespace yueymidi
{
namespace
{
struct Failure
{
    std::string message;
};

[[noreturn]] void fail(const std::string& message)
{
    throw Failure { message };
}

bool isPowerOfTwo(std::int64_t value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

// The note lengths YuE2's ABC dialect writes, in units of L:. Anything else is split into
// these, with ties for notes and plain rests for silence.
const int kDurations[] = { 48, 32, 24, 16, 12, 8, 6, 4, 3, 2, 1 };

std::vector<int> lengths(std::int64_t units)
{
    std::vector<int> parts;
    for (const int part : kDurations)
        while (units >= part)
        {
            parts.push_back(part);
            units -= part;
        }
    return parts;
}

std::string countText(int count)
{
    return count == 1 ? std::string() : std::to_string(count);
}

void appendRests(std::string& out, std::int64_t units)
{
    for (const int part : lengths(units))
        out += "z" + countText(part);
}

// ABC spells a pitch from middle C: C is 60, c is 72, c' is 84, and commas go down. The key is
// always C here, which writes sounding pitches exactly and makes no claim about the tonal
// key; every black key gets an explicit accidental, sharps preferred. This mirrors the
// upstream compiler's rule, accidental carry-over within a bar included.
std::string spell(int pitch, std::map<char, int>& active)
{
    static const char kLetters[] = "CDEFGAB";
    static const int kPitchClass[] = { 0, 2, 4, 5, 7, 9, 11 };

    using Candidate = std::tuple<bool, int, bool, char, int, int>;
    Candidate best;
    bool found = false;
    for (int i = 0; i < 7; ++i)
        for (int alteration = -1; alteration <= 1; ++alteration)
        {
            const int base = pitch - alteration - 60 - kPitchClass[i];
            if (((base % 12) + 12) % 12 != 0)
                continue;
            const Candidate candidate { alteration != 0, std::abs(alteration), alteration < 0,
                                        kLetters[i], alteration, base / 12 };
            if (!found || candidate < best)
            {
                best = candidate;
                found = true;
            }
        }

    const char letter = std::get<3>(best);
    const int alteration = std::get<4>(best);
    const int octave = std::get<5>(best);

    std::string accidental;
    const auto known = active.find(letter);
    if (alteration != (known != active.end() ? known->second : 0))
    {
        accidental = alteration < 0 ? "_" : alteration == 0 ? "=" : "^";
        active[letter] = alteration;
    }

    std::string name(1, letter);
    if (octave < 0)
        name += std::string(static_cast<std::size_t>(-octave), ',');
    else if (octave > 0)
    {
        name[0] = static_cast<char>(name[0] - 'A' + 'a');
        name += std::string(static_cast<std::size_t>(octave - 1), '\'');
    }
    return accidental + name;
}

// What a lane has to say, in the score's own units once the grid is known.
struct UnitNote
{
    std::int64_t on = 0;
    std::int64_t off = 0;
    int pitch = 60;
};

struct UnitChord
{
    std::int64_t on = 0;
    std::string symbol;
};

struct Lane
{
    const Clip* clip = nullptr;
    const char* name = "";
    std::string prefix;
    int meterNumerator = 4;
    int meterDenominator = 4;
    std::int64_t barTicks = 0;
    std::int64_t endTick = 0;
    std::vector<Note> notes;  // sorted by onset, then pitch
};

std::string whereIs(const Lane& lane, std::int64_t tick)
{
    const std::int64_t beatTicks = std::max<std::int64_t>(1, 4LL * lane.clip->ppq / lane.meterDenominator);
    const std::int64_t bar = tick / lane.barTicks + 1;
    const std::int64_t beat = (tick % lane.barTicks) / beatTicks + 1;
    return "bar " + std::to_string(bar) + ", beat " + std::to_string(beat);
}

// The denominator of ticks/(4*ppq), a fraction of a whole note, in lowest terms. The score's
// grid has to be at least that fine and a power of two.
std::int64_t gridDenominator(const Clip& clip, std::int64_t ticks)
{
    const std::int64_t whole = 4LL * clip.ppq;
    return whole / std::gcd(ticks, whole);
}

std::int64_t toUnits(const Clip& clip, std::int64_t ticks, std::int64_t grid)
{
    return ticks * grid / (4LL * clip.ppq);
}

const char* const kMajorNames[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
const char* const kMinorNames[12] = { "Cm", "C#m", "Dm", "Ebm", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "Bbm", "Bm" };

// Simultaneous notes -> one chord symbol, for a root-position major or minor triad (doubled
// roots are fine). Anything else is refused, not guessed at. Empty string means "not one".
std::string triadSymbol(std::vector<int> pitches)
{
    std::sort(pitches.begin(), pitches.end());
    const int root = pitches.front() % 12;
    bool seen[12] = {};
    for (const int pitch : pitches)
        seen[((pitch % 12) - root + 12) % 12] = true;

    int count = 0;
    for (const bool present : seen)
        count += present ? 1 : 0;
    if (count != 3 || !seen[0] || !seen[7])
        return {};
    if (seen[4])
        return kMajorNames[root];
    if (seen[3])
        return kMinorNames[root];
    return {};
}

std::vector<UnitChord> readChords(const Lane& lane, std::int64_t grid)
{
    std::vector<UnitChord> chords;
    const auto& notes = lane.notes;
    for (std::size_t i = 0; i < notes.size();)
    {
        std::size_t j = i;
        std::vector<int> pitches;
        while (j < notes.size() && notes[j].onset == notes[i].onset)
        {
            if (notes[j].duration != notes[i].duration)
                fail(lane.prefix + "the notes of the chord at " + whereIs(lane, notes[i].onset)
                     + " don't end together. hold them for the same length and export again");
            pitches.push_back(notes[j].pitch);
            ++j;
        }
        const auto symbol = triadSymbol(pitches);
        if (symbol.empty())
            fail(lane.prefix + "the chord at " + whereIs(lane, notes[i].onset)
                 + " isn't a root-position major or minor triad. those are the only chords supported for now");
        // A symbol lasts until the next symbol (or the end of the score). There is no
        // chord-release token in this dialect, so accepting a gap or overlap would
        // change the MIDI's harmony timing.
        const auto off = notes[i].onset + notes[i].duration;
        const auto next = j < notes.size() ? notes[j].onset : lane.endTick;
        if (off < next)
            fail(lane.prefix + "the chord at " + whereIs(lane, notes[i].onset)
                 + " ends before the next chord or clip end. chord gaps aren't supported; hold it to that point and export again");
        if (off > next)
            fail(lane.prefix + "the chord at " + whereIs(lane, notes[i].onset)
                 + " overlaps the next chord. end it at the next chord and export again");
        chords.push_back({ toUnits(*lane.clip, notes[i].onset, grid), symbol });
        i = j;
    }
    return chords;
}

std::vector<UnitNote> readMelody(const Lane& lane, std::int64_t grid)
{
    std::vector<UnitNote> melody;
    const auto& notes = lane.notes;
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        if (i + 1 < notes.size() && notes[i].onset + notes[i].duration > notes[i + 1].onset)
            fail(lane.prefix + "more than one note sounds at once at " + whereIs(lane, notes[i + 1].onset)
                 + ". the melody needs a single line, so keep one voice and export again");
        melody.push_back({ toUnits(*lane.clip, notes[i].onset, grid),
                           toUnits(*lane.clip, notes[i].onset + notes[i].duration, grid),
                           notes[i].pitch });
    }
    return melody;
}

std::string melodyBar(const std::vector<UnitNote>& melody, std::int64_t start, std::int64_t end)
{
    std::string out;
    std::int64_t cursor = start;
    std::map<char, int> active;
    bool any = false;
    for (const auto& note : melody)
    {
        if (note.on >= end)
            break;
        if (note.off <= start)
            continue;
        any = true;
        const std::int64_t from = std::max(start, note.on);
        const std::int64_t to = std::min(end, note.off);
        if (from > cursor)
            appendRests(out, from - cursor);
        const auto parts = lengths(to - from);
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            const bool tied = i + 1 < parts.size() || to < note.off;
            out += spell(note.pitch, active) + countText(parts[i]) + (tied ? "-" : "");
        }
        cursor = to;
    }
    if (!any)
        return "Z";
    if (cursor < end)
        appendRests(out, end - cursor);
    return out;
}

// The chord symbols live on the Vocal lane, which rests underneath them. The exporter routes
// them to their own chords.mid; a lane's name doesn't make the model sing.
std::string chordBar(const std::vector<UnitChord>& chords, std::int64_t start, std::int64_t end)
{
    std::string current;
    for (const auto& chord : chords)
        if (chord.on <= start)
            current = chord.symbol;

    std::vector<UnitChord> changes { { start, current } };
    for (const auto& chord : chords)
        if (chord.on > start && chord.on < end)
            changes.push_back(chord);

    std::string out;
    bool any = false;
    for (std::size_t i = 0; i < changes.size(); ++i)
    {
        const std::int64_t stop = i + 1 < changes.size() ? changes[i + 1].on : end;
        if (!changes[i].symbol.empty())
        {
            out += "\"" + changes[i].symbol + "\"";
            any = true;
        }
        appendRests(out, stop - changes[i].on);
    }
    return any ? out : "Z";
}

std::string compress(const std::vector<std::string>& bars)
{
    std::string out;
    for (std::size_t i = 0; i < bars.size();)
    {
        if (bars[i] != "Z")
        {
            out += bars[i++] + "|";
            continue;
        }
        std::size_t end = i + 1;
        while (end < bars.size() && bars[end] == "Z")
            ++end;
        const std::size_t count = end - i;
        out += "Z" + (count > 1 ? std::to_string(count) : std::string()) + "|";
        i = end;
    }
    return out;
}

int resolveMeterDenominator(int denominator)
{
    return isPowerOfTwo(denominator) && denominator <= 32 ? denominator : 0;
}
}  // namespace

Score buildScore(const Clip* melodyClip, const Clip* chordClip, int bpm,
                 int fallbackMeterNumerator, int fallbackMeterDenominator)
{
    Score score;
    try
    {
        if (melodyClip == nullptr && chordClip == nullptr)
            fail("nothing to convert");
        if (bpm <= 0)
            fail("the tempo needs to be above zero");

        const bool both = melodyClip != nullptr && chordClip != nullptr;
        Lane melodyLane, chordLane;
        std::vector<Lane*> lanes;
        if (melodyClip != nullptr)
        {
            melodyLane.clip = melodyClip;
            melodyLane.name = "melody";
            lanes.push_back(&melodyLane);
        }
        if (chordClip != nullptr)
        {
            chordLane.clip = chordClip;
            chordLane.name = "chords";
            lanes.push_back(&chordLane);
        }

        // The meter comes from the files. A file without one takes the other lane's, and only
        // when neither says is the UI's meter used.
        int meterN = 0, meterD = 0;
        for (const auto* lane : lanes)
        {
            const Clip& clip = *lane->clip;
            if (clip.meterNumerator <= 0 || clip.meterDenominator <= 0)
                continue;
            if (meterN != 0 && (meterN != clip.meterNumerator || meterD != clip.meterDenominator))
                fail("the melody is in " + std::to_string(meterN) + "/" + std::to_string(meterD)
                     + " but the chords are in " + std::to_string(clip.meterNumerator) + "/"
                     + std::to_string(clip.meterDenominator) + ". export them in the same meter");
            meterN = clip.meterNumerator;
            meterD = clip.meterDenominator;
        }
        if (meterN == 0)
        {
            meterN = fallbackMeterNumerator;
            meterD = fallbackMeterDenominator;
        }
        if (meterN < 1 || meterN > 64 || resolveMeterDenominator(meterD) == 0)
            fail("the meter " + std::to_string(meterN) + "/" + std::to_string(meterD) + " isn't supported");

        std::int64_t grid = std::max<std::int64_t>(32, 4LL * meterD);

        for (auto* lane : lanes)
        {
            const Clip& clip = *lane->clip;
            lane->prefix = both ? std::string(lane->name) + ": " : std::string();
            lane->meterNumerator = meterN;
            lane->meterDenominator = meterD;
            if (clip.ppq <= 0)
                fail(lane->prefix + "the file's timing resolution isn't one this importer reads");
            if (clip.notes.empty())
                fail(lane->prefix + "there are no notes in this file");

            lane->barTicks = 4LL * clip.ppq * meterN / meterD;
            if (lane->barTicks <= 0 || (4LL * clip.ppq * meterN) % meterD != 0)
                fail(lane->prefix + "a bar doesn't land on a whole tick in this file");

            lane->notes = clip.notes;
            std::sort(lane->notes.begin(), lane->notes.end(), [](const Note& a, const Note& b)
            {
                return std::tie(a.onset, a.pitch) < std::tie(b.onset, b.pitch);
            });
            for (const auto& note : lane->notes)
            {
                if (note.onset < 0 || note.duration <= 0)
                    fail(lane->prefix + "a note at " + whereIs(*lane, std::max<std::int64_t>(0, note.onset))
                         + " has no length");
                for (const std::int64_t ticks : { note.onset, note.duration })
                {
                    const auto denominator = gridDenominator(clip, ticks);
                    if (!isPowerOfTwo(denominator))
                        fail(lane->prefix + "the rhythm at " + whereIs(*lane, note.onset)
                             + " isn't on a straight grid (triplets or unquantized timing). quantize it in your DAW and export again");
                    grid = std::max(grid, denominator);
                }
            }
            const auto endDenominator = gridDenominator(clip, clip.endTick);
            if (!isPowerOfTwo(endDenominator))
                fail(lane->prefix + "the clip doesn't end on a straight grid. trim it to whole bars and export again");
            grid = std::max(grid, endDenominator);
        }
        if (grid > 1024)
            fail("the rhythm is finer than a 1/1024 note");

        // Compare exact beat lengths across PPQs before counting bars. Rounding each
        // lane up would hide mismatched partial bars and silently add rests.
        int bars = 0;
        std::vector<int> laneBars;
        std::vector<std::pair<std::int64_t, std::int64_t>> beatLengths;
        for (auto* lane : lanes)
        {
            std::int64_t end = lane->clip->endTick;
            for (const auto& note : lane->notes)
                end = std::max(end, note.onset + note.duration);
            lane->endTick = end;
            const auto divisor = std::gcd(end, static_cast<std::int64_t>(lane->clip->ppq));
            beatLengths.emplace_back(end / divisor, lane->clip->ppq / divisor);
            const std::int64_t endUnits = toUnits(*lane->clip, end, grid);
            const std::int64_t barUnits = grid * meterN / meterD;
            laneBars.push_back(static_cast<int>(std::max<std::int64_t>(1, (endUnits + barUnits - 1) / barUnits)));
        }
        if (both && beatLengths[0] != beatLengths[1])
        {
            if (laneBars[0] != laneBars[1])
                fail("the melody is " + std::to_string(laneBars[0]) + " bars and the chords are "
                     + std::to_string(laneBars[1]) + ". trim them to the same length in your DAW and export again");
            fail("the melody and chords have different lengths within the last bar. trim them to the same length in your DAW and export again");
        }
        for (const auto* lane : lanes)
            if (lane->endTick % lane->barTicks != 0)
                fail(lane->prefix + "the clip ends partway through a bar. trim it to whole bars in your DAW and export again");
        bars = laneBars[0];

        std::vector<UnitNote> melody;
        std::vector<UnitChord> chords;
        if (melodyClip != nullptr)
            melody = readMelody(melodyLane, grid);
        if (chordClip != nullptr)
            chords = readChords(chordLane, grid);

        const std::int64_t barUnits = grid * meterN / meterD;
        std::vector<std::string> vocalBars, insBars;
        for (int bar = 0; bar < bars; ++bar)
        {
            const std::int64_t start = bar * barUnits;
            const std::int64_t end = start + barUnits;
            vocalBars.push_back(chords.empty() ? std::string("Z") : chordBar(chords, start, end));
            insBars.push_back(melody.empty() ? std::string("Z") : melodyBar(melody, start, end));
        }

        std::string abc;
        abc += "X:1\nT:\n";
        abc += "M:" + std::to_string(meterN) + "/" + std::to_string(meterD) + "\n";
        abc += "L:1/" + std::to_string(grid) + "\n";
        abc += "Q:1/4=" + std::to_string(bpm) + "\n";
        abc += "V: Vocal clef=treble name=\"Vocal Melody\" snm=\"Vocal\"\n";
        abc += "V: Ins clef=treble name=\"Ins Melody\" snm=\"Inst.\"\n";
        abc += "K:C\n% instrumental\n";
        for (int index = 0; index < bars; index += 4)
        {
            const int stop = std::min(bars, index + 4);
            const std::vector<std::string> vocal(vocalBars.begin() + index, vocalBars.begin() + stop);
            const std::vector<std::string> ins(insBars.begin() + index, insBars.begin() + stop);
            abc += "V: Vocal\n" + compress(vocal) + "\n";
            abc += "V: Ins\n" + compress(ins) + "\n";
        }

        score.ok = true;
        score.abc = std::move(abc);
        score.bars = bars;
        score.meterNumerator = meterN;
        score.meterDenominator = meterD;
        score.seconds = bars * (4.0 * meterN / meterD) * 60.0 / bpm;
        score.hasMelody = melodyClip != nullptr;
        score.hasChords = chordClip != nullptr;
    }
    catch (const Failure& failure)
    {
        score = Score {};
        score.error = failure.message;
    }
    return score;
}
}  // namespace yueymidi
