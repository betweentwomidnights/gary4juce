// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
juce::DynamicObject::Ptr makeYueySongPayload(const juce::String& style,
                                             const juce::String& lyrics,
                                             bool instrumental)
{
    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("style", style.trim());
    payload->setProperty("lyrics", instrumental ? juce::String() : lyrics);
    payload->setProperty("instrumental", instrumental);
    payload->setProperty("keep_models", false);
    payload->setProperty("audio_format", "wav");
    return payload;
}


// The planning header carries "Q:1/4=95". Only the right-hand side is the
// tempo; the left is the beat unit the tempo counts, and we leave it alone.
double readAbcTempo(const juce::String& abc)
{
    juce::StringArray lines;
    lines.addLines(abc);
    for (const auto& raw : lines)
    {
        const auto line = raw.trim();
        if (!line.startsWith("Q:"))
            continue;
        const auto rhs = line.fromFirstOccurrenceOf("=", false, false).trim();
        if (rhs.isNotEmpty() && rhs.containsOnly("0123456789."))
            return rhs.getDoubleValue();
    }
    return 0.0;
}

// Rewrites Q: in place, keeping the beat unit and every other line byte-exact.
// We adapt the score to the host; we never ask the host to move.
juce::String retimeAbcTempo(const juce::String& abc, double bpm)
{
    if (bpm <= 0.0)
        return abc;

    juce::StringArray lines;
    lines.addLines(abc);
    bool rewrote = false;
    for (auto& line : lines)
    {
        if (rewrote || !line.trim().startsWith("Q:") || !line.contains("="))
            continue;
        const auto unit = line.upToFirstOccurrenceOf("=", false, false);
        if (!unit.contains("Q:"))
            continue;
        line = unit + "=" + juce::String(juce::roundToInt(bpm));
        rewrote = true;
    }
    return rewrote ? lines.joinIntoString("\n") : abc;
}

// The server always emits melody_vocal and melody_instrumental keys, but their
// values are empty when that lane has no notes. Writing those out would hand
// the DAW a zero-byte .mid, so an empty value counts as absent.
// A rested lane still arrives as a structurally valid MIDI file: the server
// writes the header and an empty track rather than sending nothing. Offering
// that as a drag chip hands the DAW an empty clip, so ask the parser whether
// the lane actually plays anything.
bool midiHasNotes(const juce::MemoryBlock& bytes)
{
    if (bytes.getSize() == 0)
        return false;

    juce::MemoryInputStream stream(bytes, false);
    juce::MidiFile file;
    if (!file.readFrom(stream))
        return false;

    for (int trackIndex = 0; trackIndex < file.getNumTracks(); ++trackIndex)
    {
        const auto* track = file.getTrack(trackIndex);
        if (track == nullptr)
            continue;
        for (int eventIndex = 0; eventIndex < track->getNumEvents(); ++eventIndex)
            if (track->getEventPointer(eventIndex)->message.isNoteOn())
                return true;
    }
    return false;
}

const char* const kYueyMidiLanes[] = {
    "transcription.mid", "melody.mid", "melody_vocal.mid",
    "melody_instrumental.mid", "chords.mid"
};

int countYueyScoreBars(const juce::String& abc)
{
    juce::StringArray lines;
    lines.addLines(abc);
    bool instrumentalVoice = false;
    int bars = 0;
    for (const auto& raw : lines)
    {
        const auto line = raw.trim();
        if (line.startsWith("V:"))
        {
            instrumentalVoice = line.startsWithIgnoreCase("V: Ins");
            continue;
        }
        if (!instrumentalVoice)
            continue;
        for (const auto character : line)
            if (character == '|')
                ++bars;
    }
    return bars;
}
}

void Gary4juceAudioProcessorEditor::updateYueyEnablementSnapshot()
{
    if (!yueyUI)
        return;

    const bool recordingAvailable = savedSamples > 0;
    const bool outputAvailable = hasOutputAudio;
    yueyUI->setAudioSourceAvailability(recordingAvailable, outputAvailable);

    const bool reachable = isServiceReachable(ServiceType::Yuey);
    const bool createReady = reachable && !currentYueyCreatePrompt.trim().isEmpty();
    const bool selectedSourceReady = transformRecording ? recordingAvailable : outputAvailable;
    const bool remixReady = reachable && selectedSourceReady
        && !currentYueyRemixPrompt.trim().isEmpty();
    const bool continueReady = reachable && selectedSourceReady
        && !currentYueyContinuePrompt.trim().isEmpty();
    yueyUI->setGenerateButtonEnabled(createReady, remixReady, continueReady, isGenerating);

    if (reachable)
        refreshYueyNaturalMax();
}

void Gary4juceAudioProcessorEditor::sendToYuey()
{
    if (!yueyUI)
        return;
    if (!isServiceReachable(ServiceType::Yuey))
    {
        showStatusMessage("yuey not reachable - check connection first", 4000);
        return;
    }

    currentYueySubTab = yueyUI->getCurrentSubTab();
    currentYueyContinuationMethod = yueyUI->getContinuationMethod();
    currentYueyCreatePrompt = yueyUI->getCreatePrompt();
    currentYueyRemixPrompt = yueyUI->getRemixPrompt();
    currentYueyContinuePrompt = yueyUI->getContinuePrompt();
    currentYueyCreateInstrumental = yueyUI->getCreateInstrumental();
    currentYueyRemixInstrumental = yueyUI->getRemixInstrumental();
    currentYueyBpm = yueyUI->getBpm();
    if (!juce::JUCEApplicationBase::isStandaloneApp() && audioProcessor.getCurrentBPM() > 0.0)
    {
        currentYueyBpm = audioProcessor.getCurrentBPM();
        yueyUI->setBpm(currentYueyBpm);
    }
    currentYueyKey = yueyUI->getKey();
    currentYueyMeter = yueyUI->getMeter();
    currentYueyFixedBars = yueyUI->getCreateFixedBars();
    currentYueyBars = yueyUI->getCreateBars();
    currentYueyContinueFixedBars = yueyUI->getContinueFixedBars();
    currentYueyContinueBars = yueyUI->getContinueBars();
    currentYueyTranscriptionMode = yueyUI->getTranscriptionMode();

    if (currentYueySubTab == YueyUI::SubTab::Create)
    {
        if (currentYueyCreatePrompt.trim().isEmpty())
        {
            showStatusMessage("add a yuey prompt first", 3000);
            return;
        }

        auto payload = makeYueySongPayload(currentYueyCreatePrompt,
                                            currentCareyLyrics,
                                            currentYueyCreateInstrumental);
        auto planning = std::make_unique<juce::DynamicObject>();
        planning->setProperty("bpm", juce::roundToInt(currentYueyBpm));
        planning->setProperty("key", currentYueyKey);
        const auto meterParts = juce::StringArray::fromTokens(currentYueyMeter, "/", "");
        planning->setProperty("meter_numerator", meterParts.size() == 2 ? meterParts[0].getIntValue() : 4);
        planning->setProperty("meter_denominator", meterParts.size() == 2 ? meterParts[1].getIntValue() : 4);
        payload->setProperty("planning", juce::var(planning.release()));
        payload->setProperty("ending", currentYueyFixedBars ? "outro" : "natural");
        payload->setProperty("target_bars", currentYueyFixedBars ? currentYueyBars : 0);
        payload->setProperty("outro_bars", juce::jmin(4, currentYueyBars));
        payload->setProperty("seed", -1);

        submitYueyJson("/generate", juce::JSON::toString(juce::var(payload.get())),
                        ActiveOp::YueyGenerate, "creating with yuey");
        return;
    }

    const bool remixing = currentYueySubTab == YueyUI::SubTab::Remix;
    const auto& sourcePrompt = remixing ? currentYueyRemixPrompt : currentYueyContinuePrompt;
    if (sourcePrompt.trim().isEmpty())
    {
        showStatusMessage(remixing ? "add a yuey remix prompt first"
                                    : "add a yuey continuation prompt first", 3000);
        return;
    }
    if (!currentYueyRemixInstrumental && currentCareyLyrics.trim().isEmpty())
    {
        showStatusMessage("add the source lyrics or enable instrumental", 5000);
        return;
    }

    const auto sourceFile = transformRecording ? getGaryBufferFile() : getGaryOutputFile();
    juce::MemoryBlock audioBytes;
    if (!sourceFile.existsAsFile() || !sourceFile.loadFileAsData(audioBytes) || audioBytes.getSize() == 0)
    {
        showStatusMessage(transformRecording
            ? "save a recording before asking yuey to use it"
            : "generate or load output audio before asking yuey to use it", 5000);
        return;
    }
    const auto encodedAudio = juce::Base64::toBase64(audioBytes.getData(), audioBytes.getSize());

    if (remixing)
    {
        auto payload = makeYueySongPayload(currentYueyRemixPrompt,
                                            currentCareyLyrics,
                                            currentYueyRemixInstrumental);
        payload->setProperty("audio_data", encodedAudio);
        payload->setProperty("transcription_mode", currentYueyTranscriptionMode);
        submitYueyJson("/cover", juce::JSON::toString(juce::var(payload.get())),
                        ActiveOp::YueyRemix, "transcribing and remixing");
        return;
    }

    if (currentYueyContinuationMethod == YueyUI::ContinuationMethod::Score)
    {
        juce::DynamicObject::Ptr transcription = new juce::DynamicObject();
        transcription->setProperty("audio_data", encodedAudio);
        transcription->setProperty("transcription_mode", currentYueyTranscriptionMode);
        submitYueyJson("/transcribe", juce::JSON::toString(juce::var(transcription.get())),
                        ActiveOp::YueyScoreTranscribe,
                        "transcribing score for continuation");
        return;
    }

    auto payload = makeYueySongPayload(currentYueyContinuePrompt,
                                        currentCareyLyrics,
                                        currentYueyRemixInstrumental);
    payload->setProperty("audio_data", encodedAudio);
    // The audio continuation endpoint still needs a full-score companion plan,
    // but that is an implementation detail rather than a user choice.
    payload->setProperty("transcription_mode", "full");
    payload->setProperty("continuation_bars", currentYueyContinueFixedBars
        ? currentYueyContinueBars : 0);
    payload->setProperty("use_continuation_adapter", true);
    payload->setProperty("seed", -1);
    submitYueyJson("/continue", juce::JSON::toString(juce::var(payload.get())),
                    ActiveOp::YueyContinue, "continuing from source audio");
}

void Gary4juceAudioProcessorEditor::continueYueyFromTranscription(const juce::String& abc)
{
    if (abc.trim().isEmpty())
    {
        handleGenerationFailure("score continuation failed: transcription returned no score");
        return;
    }

    auto payload = makeYueySongPayload(currentYueyContinuePrompt,
                                        currentCareyLyrics,
                                        currentYueyRemixInstrumental);
    payload->setProperty("abc_prefix", abc);
    if (currentYueyContinueFixedBars)
    {
        const int sourceBars = countYueyScoreBars(abc);
        if (sourceBars <= 0)
        {
            handleGenerationFailure("score continuation failed: could not count the transcribed bars");
            return;
        }
        payload->setProperty("ending", "outro");
        payload->setProperty("target_bars", sourceBars + currentYueyContinueBars);
        payload->setProperty("outro_bars", juce::jmin(4, currentYueyContinueBars));
    }
    else
    {
        payload->setProperty("ending", "natural");
        payload->setProperty("target_bars", 0);
    }
    payload->setProperty("seed", -1);

    applyYueyPlanMetadata(abc);
    submitYueyJson("/generate", juce::JSON::toString(juce::var(payload.get())),
                    ActiveOp::YueyContinue, "continuing from transcribed score");
}

void Gary4juceAudioProcessorEditor::submitYueyJson(const juce::String& endpoint,
                                                   const juce::String& json,
                                                   ActiveOp operation,
                                                   const juce::String& activity)
{
    const auto requestUrl = getServiceUrl(ServiceType::Yuey, endpoint);
    setActiveOp(operation);
    isGenerating = true;
    isCurrentlyQueued = true;
    generationProgress = 0;
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;
    audioProcessor.setUndoTransformAvailable(false);
    audioProcessor.setRetryAvailable(false);
    updateAllGenerationButtonStates();
    showStatusMessage(activity + "...", 3000);
    repaint();

    const auto generationToken = beginGenerationAsyncWork();
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, generationToken, requestUrl, json, activity]()
    {
        juce::String responseText;
        int statusCode = 0;
        try
        {
            auto postUrl = juce::URL(requestUrl).withPOSTData(json);
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(20000)
                .withStatusCode(&statusCode)
                .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json");
            if (auto stream = postUrl.createInputStream(options))
                responseText = stream->readEntireStreamAsString();
        }
        catch (...) {}

        juce::MessageManager::callAsync([asyncAlive, editor, generationToken,
                                         responseText, statusCode, activity]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire)) return;
            if (!editor->isGenerationAsyncWorkCurrent(generationToken) || !editor->isGenerating) return;

            auto fail = [editor](const juce::String& message)
            {
                editor->isGenerating = false;
                editor->isCurrentlyQueued = false;
                editor->generationProgress = 0;
                editor->smoothProgressAnimation = false;
                editor->setActiveOp(ActiveOp::None);
                editor->showStatusMessage(message, 6000);
                editor->updateAllGenerationButtonStates();
                editor->repaint();
            };

            if (statusCode < 200 || statusCode >= 300 || responseText.isEmpty())
            {
                fail(statusCode > 0 ? "yuey submit failed (HTTP " + juce::String(statusCode) + ")"
                                    : "yuey backend not responding");
                return;
            }

            auto response = juce::JSON::parse(responseText);
            auto* object = response.getDynamicObject();
            if (object == nullptr || !static_cast<bool>(object->getProperty("success")))
            {
                const auto error = object != nullptr
                    ? object->getProperty("error").toString() : juce::String("invalid response");
                fail("yuey error: " + (error.isEmpty() ? juce::String("unknown error") : error));
                return;
            }

            const auto sessionId = object->getProperty("session_id").toString();
            if (sessionId.isEmpty())
            {
                fail("yuey response missing session id");
                return;
            }

            editor->showStatusMessage(activity + "...", 2500);
            editor->startPollingForResults(sessionId);
        });
    });
}

void Gary4juceAudioProcessorEditor::applyYueyPlanMetadata(const juce::String& abc)
{
    if (abc.trim().isEmpty())
        return;
    if (yueyUI)
    {
        // Meter and key are the model's to report. Tempo is not: in a host the
        // project owns it, and sendToYuey overrides the control from the host on
        // the next submit anyway, so adopting it here would only make the box
        // flicker between the two values.
        const bool hostOwnsTempo = !juce::JUCEApplicationBase::isStandaloneApp()
            && audioProcessor.getCurrentBPM() > 0.0;
        yueyUI->applyPlanMetadata(abc, !hostOwnsTempo);
        currentYueyBpm = yueyUI->getBpm();
        currentYueyKey = yueyUI->getKey();
        currentYueyMeter = yueyUI->getMeter();
    }
    persistEditorState();
}

// ============================================================================
// yuey score lifecycle
//
// The score describes one specific render, so it is stored beside that render
// rather than in the Yuey tab or in the editor's saved state. The output
// waveform is shared chrome: gary, jerry, terry, carey and sa3 all write to
// myOutput.wav, and the editor is destroyed and rebuilt every time the plugin
// window closes. Keeping the score next to the audio it belongs to is what
// makes "this midi matches what you are hearing" true in both cases.
//
// The midi lanes are also held in memory. They are a few kilobytes each, and
// owning the bytes means the score can be rewritten into a different storage
// folder after a migration or a fallback without reaching back to the old one.
// ============================================================================

juce::File Gary4juceAudioProcessorEditor::getYueyMidiFile(const juce::String& laneName) const
{
    return getYueyScoreDirectory().getChildFile(laneName);
}

void Gary4juceAudioProcessorEditor::clearYueyScore()
{
    yueyScore = YueyScore{};

    auto directory = getYueyScoreDirectory();
    if (directory.isDirectory())
        directory.deleteRecursively();

    updateYueyScoreOverlayState();
}

void Gary4juceAudioProcessorEditor::markYueyScoreUnaligned()
{
    if (!hasYueyScore() || !yueyScore.alignedToAudio)
        return;

    yueyScore.alignedToAudio = false;
    persistYueyScore();
    repaint(); // the stale-alignment marker on the midi handle
}

void Gary4juceAudioProcessorEditor::persistYueyScore()
{
    if (!hasYueyScore())
        return;
    if (!ensureGaryDataDirectoryAvailable(false))
        return;

    auto directory = getYueyScoreDirectory();
    const auto created = directory.createDirectory();
    if (!created.wasOk())
    {
        DBG("Failed to create yuey score directory: " + created.getErrorMessage());
        return;
    }

    directory.getChildFile("original.abc").replaceWithText(yueyScore.originalAbc);
    directory.getChildFile("working.abc").replaceWithText(yueyScore.workingAbc);

    juce::StringArray laneNames;
    for (const auto& lane : yueyScore.midi)
    {
        writeDataToFileSafely(directory.getChildFile(lane.name),
                              lane.bytes.getData(), lane.bytes.getSize());
        laneNames.add(lane.name);
    }

    juce::DynamicObject::Ptr meta = new juce::DynamicObject();
    meta->setProperty("sourceOp", yueyScore.sourceOp);
    meta->setProperty("originalTempo", yueyScore.originalTempo);
    meta->setProperty("workingTempo", yueyScore.workingTempo);
    meta->setProperty("syncedToHost", yueyScore.syncedToHost);
    meta->setProperty("alignedToAudio", yueyScore.alignedToAudio);
    meta->setProperty("bars", yueyScore.bars);
    meta->setProperty("midiNames", laneNames.joinIntoString(","));
    directory.getChildFile("meta.json")
        .replaceWithText(juce::JSON::toString(juce::var(meta.get())));
}

void Gary4juceAudioProcessorEditor::attachYueyScore(juce::DynamicObject* completedResponse,
                                                    const juce::String& sourceOp)
{
    if (completedResponse == nullptr)
        return;

    const auto abc = completedResponse->getProperty("abc").toString();
    if (abc.trim().isEmpty())
    {
        // A render without a score is not worth interrupting the user over, but
        // it does mean there is no score to offer for this audio.
        DBG("Yuey " + sourceOp + " completed without an abc score");
        return;
    }

    YueyScore score;
    score.originalAbc = abc;
    score.sourceOp = sourceOp;
    score.originalTempo = readAbcTempo(abc);
    score.bars = countYueyScoreBars(abc);

    // The score keeps a required Q: so it stays portable on its own. Inside a
    // host we retime the working copy to the project instead, because the render
    // has to sit on the grid the user is working to. The original is untouched.
    const double hostBpm = juce::JUCEApplicationBase::isStandaloneApp()
        ? 0.0 : audioProcessor.getCurrentBPM();
    score.workingAbc = hostBpm > 0.0 ? retimeAbcTempo(abc, hostBpm) : abc;
    score.workingTempo = readAbcTempo(score.workingAbc);
    score.syncedToHost = hostBpm > 0.0
        && score.workingTempo == (double) juce::roundToInt(hostBpm);

    if (auto* midiFiles = completedResponse->getProperty("midi_files").getDynamicObject())
    {
        for (const auto* laneName : kYueyMidiLanes)
        {
            const juce::String name(laneName);
            if (!midiFiles->hasProperty(name))
                continue;

            const auto encoded = midiFiles->getProperty(name).toString();
            if (encoded.isEmpty())
                continue; // the key is always sent; an empty value means no notes

            juce::MemoryOutputStream decoded;
            if (!juce::Base64::convertFromBase64(decoded, encoded))
            {
                DBG("Failed to decode yuey midi lane: " + name);
                continue;
            }

            if (!midiHasNotes(decoded.getMemoryBlock()))
                continue; // a rested lane parses fine but plays nothing

            score.midi.push_back({ name, decoded.getMemoryBlock() });
        }
    }

    yueyScore = std::move(score);
    persistYueyScore();

    DBG("Attached yuey score: " + juce::String(yueyScore.bars) + " bars, "
        + juce::String((int) yueyScore.midi.size()) + " midi lanes, tempo "
        + juce::String(yueyScore.workingTempo)
        + (yueyScore.syncedToHost ? " (synced to project)" : ""));
    updateYueyScoreOverlayState();
}

bool Gary4juceAudioProcessorEditor::loadYueyScoreFromDisk()
{
    yueyScore = YueyScore{};

    auto directory = getYueyScoreDirectory();
    if (!directory.isDirectory())
        return false;

    // A score only means anything while the audio it describes is still there.
    // A sidecar next to a missing render is just stale bytes.
    if (!getGaryOutputFile().existsAsFile())
    {
        directory.deleteRecursively();
        return false;
    }

    YueyScore score;
    score.originalAbc = directory.getChildFile("original.abc").loadFileAsString();
    if (score.originalAbc.trim().isEmpty())
    {
        directory.deleteRecursively();
        return false;
    }

    score.workingAbc = directory.getChildFile("working.abc").loadFileAsString();
    if (score.workingAbc.trim().isEmpty())
        score.workingAbc = score.originalAbc;

    const auto meta = juce::JSON::parse(directory.getChildFile("meta.json").loadFileAsString());
    if (auto* object = meta.getDynamicObject())
    {
        score.sourceOp = object->getProperty("sourceOp").toString();
        score.originalTempo = (double) object->getProperty("originalTempo");
        score.workingTempo = (double) object->getProperty("workingTempo");
        score.syncedToHost = (bool) object->getProperty("syncedToHost");
        score.bars = (int) object->getProperty("bars");
        if (object->hasProperty("alignedToAudio"))
            score.alignedToAudio = (bool) object->getProperty("alignedToAudio");
    }

    if (score.originalTempo <= 0.0)
        score.originalTempo = readAbcTempo(score.originalAbc);
    if (score.workingTempo <= 0.0)
        score.workingTempo = readAbcTempo(score.workingAbc);
    if (score.bars <= 0)
        score.bars = countYueyScoreBars(score.workingAbc);

    // Trust the files over the manifest: a lane is available only if its bytes
    // are still readable.
    for (const auto* laneName : kYueyMidiLanes)
    {
        const juce::String name(laneName);
        juce::MemoryBlock bytes;
        const auto file = directory.getChildFile(name);
        if (file.existsAsFile() && file.loadFileAsData(bytes) && midiHasNotes(bytes))
            score.midi.push_back({ name, std::move(bytes) });
    }

    yueyScore = std::move(score);
    DBG("Restored yuey score from disk: " + juce::String(yueyScore.bars) + " bars, "
        + juce::String((int) yueyScore.midi.size()) + " midi lanes");
    updateYueyScoreOverlayState();
    return true;
}

void Gary4juceAudioProcessorEditor::refreshYueyNaturalMax()
{
    if (!yueyUI || !isServiceReachable(ServiceType::Yuey))
        return;

    const auto requestUrl = getServiceUrl(ServiceType::Yuey, "/health");
    const auto nowMs = juce::Time::getCurrentTime().toMilliseconds();
    if (requestUrl == yueyNaturalMaxSource)
        return; // already answered for this backend
    if (nowMs - yueyNaturalMaxLastAttemptMs < 15000)
        return; // a backend that did not answer gets asked again, but not often

    yueyNaturalMaxSource = requestUrl;
    yueyNaturalMaxLastAttemptMs = nowMs;

    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, requestUrl]()
    {
        juce::String responseText;
        int statusCode = 0;
        try
        {
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(4000)
                .withStatusCode(&statusCode)
                .withExtraHeaders("Accept: application/json");
            if (auto stream = juce::URL(requestUrl).createInputStream(options))
                responseText = stream->readEntireStreamAsString();
        }
        catch (...) {}

        juce::MessageManager::callAsync([asyncAlive, editor, requestUrl, responseText, statusCode]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;
            if (editor->yueyNaturalMaxSource != requestUrl)
                return; // the backend changed under us

            if (statusCode < 200 || statusCode >= 300 || responseText.isEmpty())
            {
                // Let a later poll try again rather than caching a failure.
                editor->yueyNaturalMaxSource = {};
                return;
            }

            const auto response = juce::JSON::parse(responseText);
            auto* object = response.getDynamicObject();
            if (object == nullptr || !object->hasProperty("natural_max_seconds"))
            {
                // An older backend simply does not report one. Leave the
                // tooltip generic instead of inventing a number, and stop
                // asking this url.
                editor->yueyNaturalMaxSeconds = -1.0;
                if (editor->yueyUI)
                    editor->yueyUI->setNaturalLengthCeiling(-1.0);
                return;
            }

            editor->yueyNaturalMaxSeconds =
                static_cast<double>(object->getProperty("natural_max_seconds"));
            if (editor->yueyUI)
                editor->yueyUI->setNaturalLengthCeiling(editor->yueyNaturalMaxSeconds);
            DBG("Yuey natural-length ceiling: "
                + juce::String(editor->yueyNaturalMaxSeconds) + "s from " + requestUrl);
        });
    });
}

// ============================================================================
// yuey midi lanes
//
// The lanes are dragged out of a small panel rather than off the waveform.
// The waveform's own drag already means "the audio", and the set of lanes
// varies per render: chords only exist in full mode, and a rested lane is
// not sent at all. An unlabelled drag would be a guess.
// ============================================================================

namespace
{
juce::String yueyLaneLabel(const juce::String& laneName)
{
    if (laneName == "transcription.mid")          return "all";
    if (laneName == "melody.mid")                 return "melody";
    if (laneName == "melody_vocal.mid")           return "vocal";
    if (laneName == "melody_instrumental.mid")    return "instrument";
    if (laneName == "chords.mid")                 return "chords";
    return laneName.upToLastOccurrenceOf(".mid", false, false);
}

juce::String yueyLaneHint(const juce::String& laneName)
{
    if (laneName == "transcription.mid")
        return "every lane, one track each";
    if (laneName == "melody.mid")
        return "both melody lanes together";
    if (laneName == "melody_vocal.mid")
        return "the vocal line alone";
    if (laneName == "melody_instrumental.mid")
        return "the instrument line alone";
    if (laneName == "chords.mid")
        return "the chord voicings";
    return {};
}

// One draggable row. It is its own drag source so the DAW gets a sensible
// highlight, and it asks the editor for the file only once a drag starts.
class YueyLaneRow final : public juce::Component
{
public:
    YueyLaneRow(juce::String lane, std::function<void(const juce::String&, juce::Component*)> drag)
        : laneName(std::move(lane)), onDrag(std::move(drag))
    {
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        const bool active = isMouseOverOrDragging();
        g.setColour(active ? juce::Colours::orange.withAlpha(0.22f)
                           : juce::Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(active ? juce::Colours::orange.withAlpha(0.8f)
                           : juce::Colours::white.withAlpha(0.18f));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        auto text = bounds.reduced(8.0f, 0.0f).toNearestInt();
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(juce::Colours::white.withAlpha(active ? 1.0f : 0.85f));
        g.drawText(yueyLaneLabel(laneName), text, juce::Justification::centredLeft);

        g.setFont(juce::FontOptions(10.0f));
        g.setColour(juce::Colours::lightgrey.withAlpha(0.65f));
        g.drawText(yueyLaneHint(laneName), text, juce::Justification::centredRight);
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (dragging || event.getDistanceFromDragStart() < 6)
            return;
        dragging = true;
        if (onDrag)
            onDrag(laneName, this);
    }

    void mouseUp(const juce::MouseEvent&) override { dragging = false; }

private:
    juce::String laneName;
    std::function<void(const juce::String&, juce::Component*)> onDrag;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YueyLaneRow)
};

class YueyMidiPanel final : public juce::Component
{
public:
    YueyMidiPanel(const juce::StringArray& lanes,
                  bool aligned,
                  std::function<void(const juce::String&, juce::Component*)> drag)
    {
        title.setText("drag midi into your daw", juce::dontSendNotification);
        title.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        title.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        title.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(title);

        if (!aligned)
        {
            // The score still describes the composition, but the audio was
            // cropped or trimmed after it was rendered, so bar 1 has moved.
            warning.setText("the output was edited; this no longer lines up",
                            juce::dontSendNotification);
            warning.setFont(juce::FontOptions(10.0f));
            warning.setColour(juce::Label::textColourId, juce::Colours::orange.withAlpha(0.9f));
            warning.setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(warning);
            showWarning = true;
        }

        for (const auto& lane : lanes)
        {
            auto row = std::make_unique<YueyLaneRow>(lane, drag);
            addAndMakeVisible(*row);
            rows.push_back(std::move(row));
        }
    }

    int preferredHeight() const
    {
        return 8 + 16 + (showWarning ? 14 : 0) + (int)rows.size() * 24 + 8;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0x1a, 0x1a, 0x1a).withAlpha(0.97f));
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(juce::Colours::orange.withAlpha(0.45f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8, 4);
        title.setBounds(area.removeFromTop(16));
        if (showWarning)
            warning.setBounds(area.removeFromTop(14));
        for (auto& row : rows)
            row->setBounds(area.removeFromTop(24).reduced(0, 1));
    }

private:
    juce::Label title;
    juce::Label warning;
    bool showWarning = false;
    std::vector<std::unique_ptr<YueyLaneRow>> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YueyMidiPanel)
};
} // namespace

void Gary4juceAudioProcessorEditor::toggleYueyMidiPanel()
{
    if (yueyMidiPanel != nullptr)
    {
        closeYueyMidiPanel();
        return;
    }
    if (!hasYueyScore() || yueyScore.midi.empty())
        return;

    juce::StringArray lanes;
    for (const auto& lane : yueyScore.midi)
        lanes.add(lane.name);

    auto* editor = this;
    auto panel = std::make_unique<YueyMidiPanel>(
        lanes, yueyScore.alignedToAudio,
        [editor](const juce::String& laneName, juce::Component* source)
        {
            editor->dragYueyMidiLane(laneName, source);
        });

    panel->setSize(
        juce::jmin(260, juce::jmax(180, outputWaveformArea.getWidth() - 20)),
        panel->preferredHeight());
    yueyMidiPanel = std::move(panel);
    positionYueyMidiPanel();
    addAndMakeVisible(*yueyMidiPanel);
    yueyMidiPanel->toFront(false);
    repaint();
}

// The waveform can be as short as 80px while the panel needs more, so the
// panel is clamped to the editor rather than to the waveform. Otherwise a
// compact layout would push it over the transport buttons or off the bottom.
void Gary4juceAudioProcessorEditor::positionYueyMidiPanel()
{
    if (yueyMidiPanel == nullptr)
        return;

    const int width = yueyMidiPanel->getWidth();
    const int height = yueyMidiPanel->getHeight();
    const int x = juce::jlimit(4, juce::jmax(4, getWidth() - width - 4),
                               outputWaveformArea.getX() + 6);
    const int y = juce::jlimit(4, juce::jmax(4, getHeight() - height - 4),
                               outputWaveformArea.getY() + 6);
    yueyMidiPanel->setBounds(x, y, width, height);
}

void Gary4juceAudioProcessorEditor::closeYueyMidiPanel()
{
    yueyMidiPanel.reset();
    repaint();
}

void Gary4juceAudioProcessorEditor::dragYueyMidiLane(const juce::String& laneName,
                                                     juce::Component* source)
{
    const auto lane = std::find_if(
        yueyScore.midi.begin(), yueyScore.midi.end(),
        [&laneName](const YueyMidiLane& entry) { return entry.name == laneName; });
    if (lane == yueyScore.midi.end() || lane->bytes.getSize() == 0)
    {
        showStatusMessage("that midi lane is no longer available", 3000);
        return;
    }

    auto directory = getGaryDraggedMidiDirectory();
    const auto created = directory.createDirectory();
    if (!created.wasOk())
    {
        showStatusMessage("midi drag failed - could not create folder", 3000);
        return;
    }

    // A fresh name per drag: the DAW may keep the file, and a lane name on its
    // own would collide with every previous render.
    const auto stamp = juce::String(juce::Time::getCurrentTime().toMilliseconds());
    const auto dragFile = directory.getChildFile(
        "yuey_" + yueyLaneLabel(laneName) + "_" + stamp + ".mid");

    if (!writeDataToFileSafely(dragFile, lane->bytes.getData(), lane->bytes.getSize())
        || !dragFile.existsAsFile() || dragFile.getSize() <= 0)
    {
        showStatusMessage("midi drag failed - could not write the file", 3000);
        return;
    }

    juce::StringArray files;
    files.add(dragFile.getFullPathName());

    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis = this;
    const auto label = yueyLaneLabel(laneName);
    const bool aligned = yueyScore.alignedToAudio;

    const bool started = juce::DragAndDropContainer::performExternalDragDropOfFiles(
        files, true, source != nullptr ? source : this,
        [safeThis, label, aligned]()
        {
            juce::MessageManager::callAsync([safeThis, label, aligned]()
            {
                if (auto* editor = safeThis.getComponent())
                {
                    editor->showStatusMessage(
                        aligned ? label + " midi dragged!"
                                : label + " midi dragged - check the alignment", 2500);
                    editor->closeYueyMidiPanel();
                }
            });
        });

    if (!started)
    {
        dragFile.deleteFile();
        showStatusMessage("midi drag failed - try again", 2500);
    }
}

void Gary4juceAudioProcessorEditor::updateYueyScoreOverlayState()
{
    const bool haveLanes = hasOutputAudio && hasYueyScore() && !yueyScore.midi.empty();

    if (!haveLanes && yueyMidiPanel != nullptr)
        closeYueyMidiPanel();

    if (yueyMidiButton.isVisible() != haveLanes)
    {
        yueyMidiButton.setVisible(haveLanes);
        repaint();
    }
}
