// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "YueyMidiImport.h"

#include <map>
#include <set>
#include <utility>

namespace yueymidi
{
namespace
{
ReadResult refuse(const juce::String& message)
{
    ReadResult result;
    result.error = message;
    return result;
}

// Controllers a DAW writes at the top of a clip without meaning anything musical to us: bank
// select, volume, pan, the effect sends, and the reset and all-notes-off pair. Anything else
// (mod wheel, expression, sustain...) is performance data this importer would drop, so it says so.
bool isHarmlessController(int number)
{
    static const std::set<int> harmless { 0, 7, 10, 32, 91, 93, 120, 121, 123 };
    return harmless.count(number) != 0;
}
}  // namespace

ReadResult readMidiFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return refuse("that file isn't there any more");

    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return refuse("couldn't open that file");

    juce::MidiFile midi;
    if (!midi.readFrom(stream))
        return refuse("that doesn't look like a MIDI file");

    const int timeFormat = midi.getTimeFormat();
    if (timeFormat <= 0)
        return refuse("this file uses SMPTE timing; export it with beats (ticks per quarter note)");

    ReadResult result;
    result.clip.ppq = timeFormat;

    int tracksWithNotes = 0;
    std::set<int> channels;
    std::int64_t endTick = 0;

    for (int trackIndex = 0; trackIndex < midi.getNumTracks(); ++trackIndex)
    {
        const auto* track = midi.getTrack(trackIndex);
        if (track == nullptr)
            continue;

        // Open notes by channel and pitch; an unmatched one ends where the track does.
        std::map<std::pair<int, int>, std::vector<std::int64_t>> open;
        std::vector<Note> trackNotes;
        std::int64_t trackEnd = 0;

        for (const auto* holder : *track)
        {
            const auto& message = holder->message;
            const auto tick = static_cast<std::int64_t>(std::llround(message.getTimeStamp()));
            trackEnd = std::max(trackEnd, tick);

            if (message.isNoteOn())
            {
                if (message.getChannel() == 10)
                    return refuse("this file has drum notes (MIDI channel 10); export the melody or chords on their own");
                channels.insert(message.getChannel());
                open[{ message.getChannel(), message.getNoteNumber() }].push_back(tick);
            }
            else if (message.isNoteOff())
            {
                auto found = open.find({ message.getChannel(), message.getNoteNumber() });
                if (found == open.end() || found->second.empty())
                    continue;
                const auto start = found->second.front();
                found->second.erase(found->second.begin());
                trackNotes.push_back({ start, tick - start, message.getNoteNumber() });
            }
            else if (message.isPitchWheel())
            {
                return refuse("this file has pitch bends, which can't be written into the score; remove them and export again");
            }
            else if (message.isController())
            {
                if (!isHarmlessController(message.getControllerNumber()))
                    return refuse("this file has controller data (mod wheel, sustain or expression); remove it and export again");
            }
            else if (message.isAftertouch() || message.isChannelPressure())
            {
                return refuse("this file has aftertouch, which can't be written into the score; remove it and export again");
            }
            else if (message.isTimeSignatureMetaEvent())
            {
                int numerator = 0, denominator = 0;
                message.getTimeSignatureInfo(numerator, denominator);
                auto& clip = result.clip;
                if (clip.meterNumerator == 0)
                {
                    clip.meterNumerator = numerator;
                    clip.meterDenominator = denominator;
                }
                else if (clip.meterNumerator != numerator || clip.meterDenominator != denominator)
                {
                    return refuse("the meter changes inside this file; use a clip in a single meter");
                }
            }
            else if (message.isEndOfTrackMetaEvent())
            {
                endTick = std::max(endTick, tick);
            }
        }

        for (auto& entry : open)
            for (const auto start : entry.second)
                trackNotes.push_back({ start, trackEnd - start, entry.first.second });

        if (!trackNotes.empty())
        {
            ++tracksWithNotes;
            result.clip.notes.insert(result.clip.notes.end(), trackNotes.begin(), trackNotes.end());
        }
        endTick = std::max(endTick, trackEnd);
    }

    if (result.clip.notes.empty())
        return refuse("there are no notes in this file");
    if (tracksWithNotes > 1)
        return refuse("more than one track has notes; export the melody or the chords as a single track");
    if (channels.size() > 1)
        return refuse("the notes are on more than one MIDI channel, so this is more than one part; export a single part");

    for (const auto& note : result.clip.notes)
        endTick = std::max(endTick, note.onset + note.duration);
    result.clip.endTick = endTick;
    result.ok = true;
    return result;
}

juce::String summaryText(const Score& score, int bpm)
{
    const auto seconds = juce::roundToInt(score.seconds);
    return juce::String(score.bars) + (score.bars == 1 ? " bar" : " bars") + juce::String::fromUTF8(" \xc2\xb7 ")
         + juce::String(seconds) + "s at " + juce::String(bpm) + " BPM";
}
}  // namespace yueymidi
