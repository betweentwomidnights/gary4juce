// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

// Runs real .mid files through the JUCE reader and the converter. The two files are the MIDI the
// import spikes used: a 443-note melody that runs well past ten bars, and a ten-bar chord track.
// The chord file must come out as the spike's reference ABC, byte for byte; the melody's first two
// four-bar blocks must match the spike's excerpt after an explicit test-only crop. Its original
// partial-bar ending is refused by the converter. The rest exercise reader rejection and pairing.
//
// Needs the JUCE build (see build-reader-test.cmd): it links against the plugin's shared code.

#include "../../Source/Yuey/YueyMidiImport.h"
#include "../../Source/Yuey/YueyScoreTempo.h"

#include <algorithm>
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

// Synthetic clips contain no Ableton device state or factory-pack material.
void alcCases(const juce::File& temp, const juce::File& fixtures)
{
    const juce::String xml = R"(<Ableton MajorVersion="5"><LiveSet><MidiClip>
      <Loop><LoopStart Value="0"/><LoopEnd Value="4"/><StartRelative Value="0"/>
        <LoopOn Value="true"/><OutMarker Value="4"/></Loop>
      <TimeSignature><TimeSignatures><RemoteableTimeSignature>
        <Numerator Value="4"/><Denominator Value="4"/><Time Value="0"/>
      </RemoteableTimeSignature></TimeSignatures></TimeSignature>
      <GrooveSettings><GrooveId Value="-1"/></GrooveSettings>
      <Envelopes><Envelopes/></Envelopes>
      <Notes><KeyTracks><KeyTrack><MidiKey Value="60"/><Notes>
        <MidiNoteEvent Time="0" Duration="1" Probability="1"/>
        <MidiNoteEvent Time="1" Duration="1" IsEnabled="true"/>
      </Notes></KeyTrack></KeyTracks><PerNoteEventStore><EventLists/></PerNoteEventStore>
      <NoteProbabilityGroups/></Notes>
    </MidiClip><Devices><PluginDevice/></Devices></LiveSet></Ableton>)";
    int serial = 0;
    auto read = [&](const juce::String& source)
    {
        const auto file = temp.getChildFile("clip-" + juce::String(++serial) + ".alc");
        juce::MemoryOutputStream output;
        { juce::GZIPCompressorOutputStream zip(output, 6, 31); zip.writeText(source, false, false, nullptr); }
        check(file.replaceWithData(output.getData(), output.getDataSize()), "write gzip Live Clip fixture");
        return yueymidi::readInputFile(file);
    };
    auto reject = [&](const juce::String& source, const juce::String& needle, const juce::String& what)
    {
        const auto result = read(source);
        check(!result.ok && result.error.containsIgnoreCase(needle), what + ": " + result.error);
    };
    const auto clean = read(xml);
    check(clean.ok && clean.clip.ppq == 256 && clean.clip.endTick == 1024 && clean.clip.notes.size() == 2,
          "one saved loop is read in beat coordinates, without loading a plugin device");
    check(clean.ok && yueymidi::buildScore(&clean.clip, nullptr, 120, 4, 4).ok,
          "Live Clip reuses the normal melody converter");
    const auto chordXml = xml.replace("Duration=\"1\" Probability", "Duration=\"4\" Probability")
        .replace("<MidiNoteEvent Time=\"1\" Duration=\"1\" IsEnabled=\"true\"/>", "")
        .replace("</KeyTracks>", "<KeyTrack><MidiKey Value=\"64\"/><Notes><MidiNoteEvent Time=\"0\" Duration=\"4\"/>"
            "</Notes></KeyTrack><KeyTrack><MidiKey Value=\"67\"/><Notes><MidiNoteEvent Time=\"0\" Duration=\"4\"/>"
            "</Notes></KeyTrack></KeyTracks>");
    const auto chord = read(chordXml);
    const auto chordScore = yueymidi::buildScore(nullptr, &chord.clip, 120, 4, 4);
    check(chord.ok && chordScore.ok && chordScore.abc.find("\"C\"z32|") != std::string::npos,
          "chord Live Clip writes chord symbols through the normal converter");
    check(clean.ok && chord.ok && yueymidi::buildScore(&clean.clip, &chord.clip, 120, 4, 4).ok,
          "melody and chord Live Clips combine when their lengths and meter agree");

    auto offsetXml = xml.replace("LoopStart Value=\"0\"", "LoopStart Value=\"8\"")
        .replace("LoopEnd Value=\"4\"", "LoopEnd Value=\"12\"")
        .replace("Time=\"0\" Duration", "Time=\"8\" Duration")
        .replace("Time=\"1\" Duration", "Time=\"9\" Duration");
    const auto offset = read(offsetXml);
    check(offset.ok && offset.clip.endTick == 1024 && offset.clip.notes[0].onset == 0
          && offset.clip.notes[1].onset == 256, "nonzero loop region is rebased to tick zero");
    const auto crossing = read(xml.replace("Time=\"0\" Duration=\"1\"", "Time=\"-1\" Duration=\"2\"")
        .replace("Time=\"1\" Duration=\"1\"", "Time=\"3\" Duration=\"2\""));
    check(crossing.ok && crossing.clip.notes[0].onset == 0 && crossing.clip.notes[0].duration == 256
          && crossing.clip.notes[1].onset == 768 && crossing.clip.notes[1].duration == 256,
          "notes crossing saved boundaries are clipped to the selected region");
    const auto singlePass = read(xml.replace("<Loop>", "<CurrentStart Value=\"100\"/><CurrentEnd Value=\"124\"/><Loop>")
        .replace("OutMarker Value=\"4\"", "OutMarker Value=\"24\""));
    check(singlePass.ok && singlePass.clip.endTick == 1024, "loop is imported once, not repeated over arrangement positions");
    const auto unlooped = read(xml.replace("LoopOn Value=\"true\"", "LoopOn Value=\"false\"")
        .replace("OutMarker Value=\"4\"", "OutMarker Value=\"8\""));
    check(unlooped.ok && unlooped.clip.endTick == 2048, "unlooped clip uses its out marker");
    const auto muted = read(xml.replace("IsEnabled=\"true\"", "IsEnabled=\"false\""));
    check(muted.ok && muted.clip.notes.size() == 1, "disabled notes stay silent");
    const auto outside = read(xml.replace("Time=\"1\" Duration", "Time=\"20\" Duration"));
    check(outside.ok && outside.clip.notes.size() == 1 && outside.clip.endTick == 1024,
          "notes outside the saved region do not lengthen the clip");
    const auto fine = read(xml.replace("Duration=\"1\"", "Duration=\"0.00390625\""));
    const auto fineScore = yueymidi::buildScore(&fine.clip, nullptr, 120, 4, 4);
    check(fine.ok && fineScore.ok && fineScore.abc.find("L:1/1024") != std::string::npos,
          "the finest supported rhythm is preserved without rounding");

    reject(xml.replace("<Devices>", "<Devices><DrumGroupDevice/>"), "Drum Rack", "drum rack clip");
    reject(xml.replace("<Devices>", "<AudioClip/><Devices>"), "audio Live Clip", "audio clip");
    reject(xml.replace("<Devices>", "<MidiClip/><Devices>"), "exactly one", "multiple MIDI clips");
    reject(xml.replace("GrooveId Value=\"-1\"", "GrooveId Value=\"5\""), "groove", "active groove");
    reject(xml.replace("<Envelopes/>", "<Envelopes><ClipEnvelope/></Envelopes>"), "automation", "clip automation");
    reject(xml.replace("<EventLists/>", "<EventLists><EventList/></EventLists>"), "expression", "per-note expression");
    reject(xml.replace("Probability=\"1\"", "Probability=\"0.5\""), "probability", "probabilistic note");
    reject(xml.replace("<NoteProbabilityGroups/>", "<NoteProbabilityGroups><Group/></NoteProbabilityGroups>"),
           "probability groups", "note probability group");
    reject(xml.replace("StartRelative Value=\"0\"", "StartRelative Value=\"1\""), "offset", "phased clip start");
    reject(xml.replace("<Loop>", "<Disabled Value=\"true\"/><Loop>"), "disabled", "disabled clip");
    reject(xml.replace("MidiKey Value=\"60\"", "MidiKey Value=\"128\""), "pitch", "invalid pitch");
    reject(xml.replace("Duration=\"1\"", "Duration=\"0\""), "no length", "zero-duration note");
    reject(xml.replace("Duration=\"1\"", "Duration=\"0.333333333\""), "straight grid", "triplet is not rounded");
    reject(xml.replace("Time=\"0\" Duration", "Time=\"1oops\" Duration"), "invalid", "malformed numeric field");
    reject(xml.replace("LoopEnd Value=\"4\"", "LoopEnd Value=\"nan\""), "invalid", "nonfinite boundary");
    reject(xml.replace("LoopEnd Value=\"4\"", "LoopEnd Value=\"1000000000000\""), "too long", "huge declared region is bounded before score allocation");
    reject(xml.replace("<Numerator Value=\"4\"/>", "<Numerator Value=\"3.5\"/>"), "meter", "fractional meter");
    reject(xml.replace("</TimeSignatures>", "<RemoteableTimeSignature><Numerator Value=\"3\"/>"
        "<Denominator Value=\"4\"/><Time Value=\"2\"/></RemoteableTimeSignature></TimeSignatures>"), "meter changes", "meter change");
    reject(xml.replace("<KeyTracks>", "<KeyTracksOther>").replace("</KeyTracks>", "</KeyTracksOther>"),
           "unsupported note layout", "unknown note layout");
    reject("<Ableton>", "readable", "damaged XML");
    reject("<Other/>", "readable", "wrong root");
    const auto bad = temp.getChildFile("not-gzip.alc");
    bad.replaceWithText("not a Live Clip");
    const auto badResult = yueymidi::readInputFile(bad);
    check(!badResult.ok, "invalid compressed input is refused");

    const auto userLibrary = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Ableton/User Library/Instrument Melody.alc");
    if (userLibrary.existsAsFile())
    {
        const auto real = yueymidi::readInputFile(userLibrary);
        check(real.ok && real.clip.notes.size() == 68, "actual saved melody reads as 68 notes: " + real.error);
        const auto score = yueymidi::buildScore(&real.clip, nullptr, 120, 4, 4);
        check(score.ok && score.bars == 10 && normalised(score.abc)
              == normalised(fixtures.getChildFile("real.imported.abc").loadFileAsString()),
              "actual Live Clip writes the spike's melody ABC byte for byte");
        const auto slower = yueymidi::buildScore(&real.clip, nullptr, 96, 4, 4);
        check(slower.ok && slower.bars == 10 && std::abs(slower.seconds - 25.0) < 0.001,
              "Live Clip uses the UI BPM, retaining its note positions in beats");
    }
    else std::cout << "SKIP: optional saved melody Live Clip not present\n";
    const auto drums = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(
        "Ableton/Factory Packs/Trap Drums by Sound Oracle/MIDI Clips/Traverse Kit 70 bpm.alc");
    if (drums.existsAsFile())
    {
        const auto result = yueymidi::readInputFile(drums);
        check(!result.ok && result.error.contains("Drum Rack"), "actual factory drum Live Clip is refused explicitly");
    }
}
}  // namespace

int main(int argc, char** argv)
{
    const juce::File downloads = argc > 1 ? juce::File(argv[1]) : juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Downloads");
    const juce::File fixtures = argc > 2 ? juce::File(argv[2]) : juce::File("fixtures");

    // The transcribe -> generate path must change tempo without changing notes,
    // beat unit, line endings or the presence of a final newline.
    for (const auto& ending : { juce::String("\n"), juce::String("\r\n") })
    {
        const auto abc = juce::String("X:1") + ending + "Q:1/4=120" + ending + "K:C" + ending + "C8D8|" + ending;
        check(yuey::retimeAbcTempo(abc, 96) == abc.replace("Q:1/4=120", "Q:1/4=96"),
              "tempo edit preserves all other score bytes and line endings");
        check(yuey::retimeAbcTempo(abc, 120.6) == abc.replace("Q:1/4=120", "Q:1/4=121"),
              "fractional host tempo rounds to the backend's integer Q tempo");
    }
    check(yuey::retimeAbcTempo("Q:1/8=120", 96) == "Q:1/8=96", "tempo edit keeps the beat unit without final newline");
    check(yuey::retimeAbcTempo("K:C\nC8|", 96) == "K:C\nC8|", "missing tempo leaves the score alone");
    check(yuey::retimeAbcTempo("Q:1/4=120\nC8|", 0) == "Q:1/4=120\nC8|", "invalid target tempo leaves the score alone");

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
        const auto result = yueymidi::readMidiFile(downloads.getChildFile("melody_test.mid"));
        check(result.ok, "original melody_test.mid reads: " + result.error);
        if (result.ok)
        {
            check(result.clip.notes.size() == 443, "melody file has 443 notes, got " + juce::String((int) result.clip.notes.size()));
            const auto original = yueymidi::buildScore(&result.clip, nullptr, 120, 4, 4);
            check(!original.ok && original.error.find("partway through a bar") != std::string::npos,
                  "the uncropped melody's partial bar is refused, not padded");
            auto cropped = result.clip;
            cropped.endTick = 8 * 4 * cropped.ppq;
            cropped.notes.erase(std::remove_if(cropped.notes.begin(), cropped.notes.end(),
                [&](const yueymidi::Note& note) { return note.onset >= cropped.endTick; }), cropped.notes.end());
            for (auto& note : cropped.notes)
                note.duration = std::min(note.duration, cropped.endTick - note.onset);
            const auto score = yueymidi::buildScore(&cropped, nullptr, 120, 4, 4);
            check(score.ok, "melody file converts: " + juce::String(score.error));
            if (score.ok)
            {
                std::cout << "melody_test.mid (test crop) -> " << yueymidi::summaryText(score, 120).toStdString() << "\n";
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

    // The short file has now been cropped in Live; verify the real exported ten-bar MIDI too.
    {
        const auto result = yueymidi::readInputFile(downloads.getChildFile("melody_test_short.mid"));
        check(result.ok && result.clip.notes.size() == 68, "cropped melody MIDI has 68 notes");
        const auto score = yueymidi::buildScore(&result.clip, nullptr, 120, 4, 4);
        check(score.ok && score.bars == 10 && normalised(score.abc)
              == normalised(fixtures.getChildFile("real.imported.abc").loadFileAsString()),
              "cropped MIDI and Live Clip both write the same reference ABC");
    }

    // --- what the reader must refuse ---
    const auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("yuey-midi-reader-test");
    temp.createDirectory();
    alcCases(temp, fixtures);

    // Raw SMF: C overlaps another C. Using a sequence helper with matching-note
    // repair could hide the bug this fixture is meant to catch.
    {
        const unsigned char bytes[] = {
            'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
            'M','T','r','k', 0,0,0,20,
            0,0x90,60,100, 96,0x90,60,100,
            96,0x80,60,0, 96,0x80,60,0, 96,0xff,0x2f,0
        };
        const auto file = temp.getChildFile("same-pitch-overlap.mid");
        check(file.replaceWithData(bytes, sizeof(bytes)), "write raw overlapping-note fixture");
        const auto result = yueymidi::readMidiFile(file);
        check(result.ok && result.clip.notes.size() == 2, "raw same-pitch overlap reads as two notes");
        if (result.ok && result.clip.notes.size() == 2)
        {
            check(result.clip.notes[0].onset == 0 && result.clip.notes[0].duration == 192
                  && result.clip.notes[1].onset == 96 && result.clip.notes[1].duration == 192,
                  "reader preserves original overlap durations without JUCE repair");
            const auto score = yueymidi::buildScore(&result.clip, nullptr, 120, 4, 4);
            check(!score.ok && score.error.find("more than one note") != std::string::npos,
                  "same-pitch overlap is refused by the melody converter");
        }
    }

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
