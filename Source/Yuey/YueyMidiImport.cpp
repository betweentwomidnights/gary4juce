// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "YueyMidiImport.h"

#include <map>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
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

struct AlcFailure { juce::String message; };
[[noreturn]] void alcFail(const juce::String& message) { throw AlcFailure { message }; }

std::vector<juce::XmlElement*> descendants(juce::XmlElement& root, const char* tag)
{
    std::vector<juce::XmlElement*> found, pending { &root };
    while (!pending.empty())
    {
        auto* element = pending.back();
        pending.pop_back();
        if (element->hasTagName(tag)) found.push_back(element);
        for (auto* child : element->getChildIterator()) pending.push_back(child);
    }
    return found;
}

double alcNumber(const juce::String& text)
{
    double number = 0;
    std::istringstream input(text.trim().toStdString());
    input.imbue(std::locale::classic());
    input >> number;
    if (input.fail() || !input.eof() || !std::isfinite(number))
        alcFail("this Live Clip has invalid note or timing data; export it as MIDI instead");
    return number;
}

double alcValue(const juce::XmlElement& parent, const char* tag)
{
    const auto* element = parent.getChildByName(tag);
    if (element == nullptr || !element->hasAttribute("Value"))
        alcFail("this Live Clip uses an unsupported layout; export it as MIDI instead");
    return alcNumber(element->getStringAttribute("Value"));
}

bool alcBool(const juce::String& text)
{
    if (text == "true") return true;
    if (text == "false") return false;
    alcFail("this Live Clip has an unsupported setting; export it as MIDI instead");
}

// YuE's finest supported grid is a 1/1024 whole note: 256 ticks per quarter.
// The tolerance only absorbs decimal serialization noise, not loose timing.
constexpr int alcPpq = 256;
std::int64_t alcTicks(double beats)
{
    const double ticks = beats * alcPpq;
    if (!std::isfinite(ticks) || ticks < 0 || ticks >= static_cast<double>(std::numeric_limits<std::int64_t>::max()))
        alcFail("this Live Clip has invalid note or timing data; export it as MIDI instead");
    const auto rounded = std::llround(ticks);
    if (std::abs(ticks - static_cast<double>(rounded)) > 0.000001)
        alcFail("this Live Clip isn't on a straight grid (triplets or loose timing); quantize it in Live and save again");
    return rounded;
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
    // JUCE's default inserts note-offs at repeated same-pitch note-ons. Preserve
    // the source events so our own pairing and monophony checks see overlaps.
    if (!midi.readFrom(stream, false))
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

ReadResult readAbletonClip(const juce::File& file)
{
    if (!file.existsAsFile()) return refuse("that file isn't there any more");
    if (file.getSize() > 16 * 1024 * 1024)
        return refuse("this Live Clip is too large to import; export its notes as MIDI instead");
    juce::FileInputStream input(file);
    if (!input.openedOk()) return refuse("couldn't open that file");

    constexpr int maxXmlBytes = 32 * 1024 * 1024;
    juce::GZIPDecompressorInputStream gzip(&input, false, juce::GZIPDecompressorInputStream::gzipFormat);
    juce::MemoryBlock bytes;
    gzip.readIntoMemoryBlock(bytes, maxXmlBytes + 1);
    if (bytes.getSize() == 0 || bytes.getSize() > maxXmlBytes)
        return refuse("that isn't a readable Live Clip; export it as MIDI instead");
    auto root = juce::XmlDocument::parse(juce::String::fromUTF8(static_cast<const char*>(bytes.getData()),
                                                              static_cast<int>(bytes.getSize())));
    if (root == nullptr || !root->hasTagName("Ableton"))
        return refuse("that isn't a readable Ableton Live Clip; export it as MIDI instead");

    try
    {
        const auto clips = descendants(*root, "MidiClip");
        if (!descendants(*root, "AudioClip").empty())
            alcFail("this is an audio Live Clip; use audio import or a MIDI Live Clip instead");
        if (clips.size() != 1)
            alcFail("this Live Clip needs exactly one MIDI clip; save the melody or chords on their own");
        if (!descendants(*root, "DrumGroupDevice").empty())
            alcFail("this Live Clip uses a Drum Rack; import pitched melody or chords instead");
        auto& clipXml = *clips.front();
        if (const auto* disabled = clipXml.getChildByName("Disabled"))
            if (alcBool(disabled->getStringAttribute("Value")))
                alcFail("this Live Clip is disabled; enable it in Live and save again");
        if (const auto* groove = clipXml.getChildByName("GrooveSettings"))
            if (alcValue(*groove, "GrooveId") >= 0)
                alcFail("this Live Clip has a groove assigned; commit or remove the groove in Live and save again");
        if (!descendants(clipXml, "ClipEnvelope").empty())
            alcFail("this Live Clip has automation envelopes; export plain MIDI or remove the envelopes");

        const auto* notes = clipXml.getChildByName("Notes");
        const auto* keys = notes != nullptr ? notes->getChildByName("KeyTracks") : nullptr;
        if (keys == nullptr)
            alcFail("this Live Clip uses an unsupported note layout; export it as MIDI instead");
        if (const auto* expression = notes->getChildByName("PerNoteEventStore"))
        {
            const auto* lists = expression->getChildByName("EventLists");
            if (lists == nullptr || lists->getNumChildElements() != 0)
                alcFail("this Live Clip has note expression; export plain MIDI or remove the expression");
        }
        if (const auto* groups = notes->getChildByName("NoteProbabilityGroups"))
            if (groups->getNumChildElements() != 0)
                alcFail("this Live Clip has note probability groups; make the notes deterministic in Live first");

        const auto* loop = clipXml.getChildByName("Loop");
        if (loop == nullptr)
            alcFail("this Live Clip has no readable clip boundaries; export it as MIDI instead");
        const auto* loopOn = loop->getChildByName("LoopOn");
        if (loopOn == nullptr)
            alcFail("this Live Clip uses an unsupported loop layout; export it as MIDI instead");
        const bool looped = alcBool(loopOn->getStringAttribute("Value"));
        if (alcValue(*loop, "StartRelative") != 0)
            alcFail("this Live Clip starts at an offset inside its region; crop it in Live or export MIDI first");
        // One saved region, never repeated to fill an arrangement. CurrentStart/End
        // are arrangement positions, not coordinates in the stored note sequence.
        const double start = alcValue(*loop, "LoopStart");
        const double end = alcValue(*loop, looped ? "LoopEnd" : "OutMarker");
        if (start < 0 || end <= start)
            alcFail("this Live Clip has invalid clip boundaries; crop it in Live and save again");
        // Bound work from clip metadata independently of compressed file size.
        // 4096 quarter-note beats is over half an hour at 120 BPM.
        if (end - start > 4096)
            alcFail("this Live Clip's saved region is too long to import; crop a shorter region in Live");
        alcTicks(start);
        alcTicks(end);

        ReadResult result;
        result.clip.ppq = alcPpq;
        result.clip.endTick = alcTicks(end - start);
        for (auto* signature : descendants(clipXml, "RemoteableTimeSignature"))
        {
            const double n = alcValue(*signature, "Numerator"), d = alcValue(*signature, "Denominator");
            if (n < 1 || n > 64 || d < 1 || d > 32 || n != std::floor(n) || d != std::floor(d))
                alcFail("this Live Clip has an unsupported meter; export it as MIDI instead");
            if (result.clip.meterNumerator != 0
                && (result.clip.meterNumerator != static_cast<int>(n) || result.clip.meterDenominator != static_cast<int>(d)))
                alcFail("the meter changes inside this Live Clip; use a clip in a single meter");
            result.clip.meterNumerator = static_cast<int>(n);
            result.clip.meterDenominator = static_cast<int>(d);
        }

        for (auto* key : keys->getChildIterator())
        {
            if (!key->hasTagName("KeyTrack"))
                alcFail("this Live Clip uses an unsupported note layout; export it as MIDI instead");
            const double pitch = alcValue(*key, "MidiKey");
            if (pitch < 0 || pitch > 127 || pitch != std::floor(pitch))
                alcFail("this Live Clip has an invalid note pitch; export it as MIDI instead");
            const auto* events = key->getChildByName("Notes");
            if (events == nullptr)
                alcFail("this Live Clip uses an unsupported note layout; export it as MIDI instead");
            for (auto* event : events->getChildIterator())
            {
                if (!event->hasTagName("MidiNoteEvent"))
                    alcFail("this Live Clip uses an unsupported note event; export it as MIDI instead");
                if (event->hasAttribute("IsEnabled") && !alcBool(event->getStringAttribute("IsEnabled"))) continue;
                const double on = alcNumber(event->getStringAttribute("Time"));
                const double duration = alcNumber(event->getStringAttribute("Duration"));
                if (duration <= 0 || !std::isfinite(on + duration))
                    alcFail("this Live Clip has a note with no length; fix it in Live and save again");
                if (on >= end || on + duration <= start) continue;
                if (event->hasAttribute("Probability") && alcNumber(event->getStringAttribute("Probability")) != 1)
                    alcFail("this Live Clip has note probability; set it to 100 percent in Live first");
                const auto from = alcTicks(std::max(start, on) - start);
                const auto to = alcTicks(std::min(end, on + duration) - start);
                if (to <= from) alcFail("this Live Clip has a note with no length; fix it in Live and save again");
                result.clip.notes.push_back({ from, to - from, static_cast<int>(pitch) });
            }
        }
        if (result.clip.notes.empty()) alcFail("there are no enabled notes in this Live Clip's saved region");
        result.ok = true;
        return result;
    }
    catch (const AlcFailure& failure) { return refuse(failure.message); }
}

ReadResult readInputFile(const juce::File& file)
{
    return file.hasFileExtension("alc") ? readAbletonClip(file) : readMidiFile(file);
}

juce::String summaryText(const Score& score, int bpm)
{
    const auto seconds = juce::roundToInt(score.seconds);
    return juce::String(score.bars) + (score.bars == 1 ? " bar" : " bars") + juce::String::fromUTF8(" \xc2\xb7 ")
         + juce::String(seconds) + "s at " + juce::String(bpm) + " BPM";
}
}  // namespace yueymidi
