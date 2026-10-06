// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

// Runs real .mid files through the JUCE reader and the converter. The two files are the MIDI the
// import spikes used: a 443-note melody that runs well past ten bars, and a ten-bar chord track.
// The chord file must come out as the spike's reference ABC, byte for byte; the melody's first two
// four-bar blocks must match the spike's excerpt. The rest are files the reader has to refuse.
//
// Needs the JUCE build (see build-reader-test.cmd): it links against the plugin's shared code.

#include "../../Source/Yuey/YueyMidiImport.h"

#include <iostream>

namespace
{
int failures = 0, checks = 0;

void check(bool condition, const juce::String& what)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::cout << "FAIL: " << what << "\n";
    }
}

juce::String normalised(juce::String text)
{
    return text.replace("\r", "");
}

// A one-track format 0 file with whatever events the test needs.
juce::File writeMidi(const juce::File& dir, const juce::String& name, const juce::MidiMessageSequence& sequence)
{
    juce::MidiFile midi;
    midi.setTicksPerQuarterNote(96);
    midi.addTrack(sequence);
    const auto file = dir.getChildFile(name);
    file.deleteFile();
    juce::FileOutputStream out(file);
    midi.writeTo(out);
    return file;
}

juce::MidiMessageSequence notes(std::initializer_list<std::array<int, 3>> list, int channel = 1)
{
    juce::MidiMessageSequence sequence;
    for (const auto& n : list)
    {
        sequence.addEvent(juce::MidiMessage::noteOn(channel, n[2], (juce::uint8) 100), n[0]);
        sequence.addEvent(juce::MidiMessage::noteOff(channel, n[2]), n[0] + n[1]);
    }
    return sequence;
}
}  // namespace

int main(int argc, char** argv)
{
    const juce::File downloads = argc > 1 ? juce::File(argv[1]) : juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Downloads");
    const juce::File fixtures = argc > 2 ? juce::File(argv[2]) : juce::File("fixtures");

    // --- the spike's chord file: exactly ten bars, so it has to equal the reference ---
    {
        const auto result = yueymidi::readMidiFile(downloads.getChildFile("chords_test_short.mid"));
        check(result.ok, "chords_test_short.mid reads: " + result.error);
        if (result.ok)
        {
            check(result.clip.ppq == 96 && result.clip.meterNumerator == 4 && result.clip.meterDenominator == 4,
                  "chord file: ppq 96, 4/4");
            check(result.clip.notes.size() == 72, "chord file has 72 notes, got " + juce::String((int) result.clip.notes.size()));
            const auto score = yueymidi::buildScore(nullptr, &result.clip, 120, 4, 4);
            check(score.ok && score.bars == 10, "chord file is ten bars: " + juce::String(score.error));
            check(normalised(score.abc) == normalised(fixtures.getChildFile("real.chords-only.abc").loadFileAsString()),
                  "chord file converts to the spike's chords-only ABC");
            std::cout << "chords_test_short.mid -> " << yueymidi::summaryText(score, 120).toStdString() << "\n";
        }
    }

    // --- the spike's melody file: much longer than ten bars; its first eight bars are the excerpt's ---
    {
        const auto result = yueymidi::readMidiFile(downloads.getChildFile("melody_test_short.mid"));
        check(result.ok, "melody_test_short.mid reads: " + result.error);
        if (result.ok)
        {
            check(result.clip.notes.size() == 443, "melody file has 443 notes, got " + juce::String((int) result.clip.notes.size()));
            const auto score = yueymidi::buildScore(&result.clip, nullptr, 120, 4, 4);
            check(score.ok, "melody file converts: " + juce::String(score.error));
            if (score.ok)
            {
                std::cout << "melody_test_short.mid -> " << yueymidi::summaryText(score, 120).toStdString() << "\n";
                // Both end in complete four-bar blocks for the first 8 bars; compare those two blocks' lines.
                const auto full = juce::StringArray::fromLines(normalised(score.abc));
                const auto excerpt = juce::StringArray::fromLines(normalised(fixtures.getChildFile("real.imported.abc").loadFileAsString()));
                bool same = full.size() >= 15 && excerpt.size() >= 15;
                for (int i = 0; i < 14 && same; ++i)  // header, then two Vocal/Ins blocks
                    same = full[i] == excerpt[i] || i == 4;  // line 4 is the Q: line
                check(same, "the melody's first eight bars match the excerpt");
            }
        }
    }

    // --- what the reader must refuse ---
    const auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("yuey-midi-reader-test");
    temp.createDirectory();

    auto expectRefused = [&](const juce::File& file, const juce::String& needle, const juce::String& what)
    {
        const auto result = yueymidi::readMidiFile(file);
        check(!result.ok && result.error.containsIgnoreCase(needle), what + ": expected \"" + needle + "\", got ok=" + juce::String((int) result.ok) + " \"" + result.error + "\"");
    };

    expectRefused(writeMidi(temp, "drums.mid", notes({ { 0, 48, 36 } }, 10)), "drum", "channel 10");
    {
        auto seq = notes({ { 0, 48, 60 } });
        seq.addEvent(juce::MidiMessage::pitchWheel(1, 9000), 10);
        expectRefused(writeMidi(temp, "bend.mid", seq), "pitch bend", "pitch bend");
    }
    {
        auto seq = notes({ { 0, 48, 60 } });
        seq.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 10);
        expectRefused(writeMidi(temp, "sustain.mid", seq), "controller", "sustain pedal");
    }
    {
        auto seq = notes({ { 0, 48, 60 } });
        seq.addEvent(juce::MidiMessage::controllerEvent(1, 7, 100), 0);  // volume at the top: harmless
        seq.addEvent(juce::MidiMessage::controllerEvent(1, 10, 64), 0);
        const auto result = yueymidi::readMidiFile(writeMidi(temp, "setup.mid", seq));
        check(result.ok, "volume and pan at the top of a clip are ignored: " + result.error);
    }
    {
        auto seq = notes({ { 0, 48, 60 } }, 1);
        seq.addSequence(notes({ { 0, 48, 64 } }, 2), 0);
        expectRefused(writeMidi(temp, "two-channels.mid", seq), "more than one", "two channels");
    }
    expectRefused(writeMidi(temp, "empty.mid", juce::MidiMessageSequence()), "no notes", "no notes");
    {
        auto seq = notes({ { 0, 48, 60 } });
        seq.addEvent(juce::MidiMessage::timeSignatureMetaEvent(3, 4), 0);
        seq.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 192);
        expectRefused(writeMidi(temp, "meter-change.mid", seq), "meter changes", "meter change");
    }
    {
        juce::File notMidi = temp.getChildFile("notes.txt");
        notMidi.replaceWithText("not a midi file");
        expectRefused(notMidi, "doesn't look like a MIDI", "a text file");
    }
    expectRefused(temp.getChildFile("missing.mid"), "isn't there", "a missing file");

    // --- a clean file keeps its meter, its length (end of track) and its note count ---
    {
        auto seq = notes({ { 0, 96, 60 }, { 96, 96, 62 } });
        seq.addEvent(juce::MidiMessage::timeSignatureMetaEvent(3, 4), 0);
        seq.addEvent(juce::MidiMessage::endOfTrack(), 288 * 2);  // two bars of 3/4: longer than the notes
        const auto result = yueymidi::readMidiFile(writeMidi(temp, "clean.mid", seq));
        check(result.ok, "a clean clip reads: " + result.error);
        if (result.ok)
        {
            check(result.clip.meterNumerator == 3 && result.clip.meterDenominator == 4, "its 3/4 is kept");
            check(result.clip.endTick == 288 * 2, "the end-of-track event sets the length, got " + juce::String((juce::int64) result.clip.endTick));
            const auto score = yueymidi::buildScore(&result.clip, nullptr, 100, 4, 4);
            check(score.ok && score.bars == 2 && score.meterNumerator == 3, "and it converts as two bars of 3/4");
        }
    }

    temp.deleteRecursively();
    std::cout << checks << " checks, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
