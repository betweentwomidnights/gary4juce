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
}

void Gary4juceAudioProcessorEditor::markYueyScoreUnaligned()
{
    if (!hasYueyScore() || !yueyScore.alignedToAudio)
        return;

    yueyScore.alignedToAudio = false;
    persistYueyScore();
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

            if (decoded.getDataSize() == 0)
                continue;

            score.midi.push_back({ name, decoded.getMemoryBlock() });
        }
    }

    yueyScore = std::move(score);
    persistYueyScore();

    DBG("Attached yuey score: " + juce::String(yueyScore.bars) + " bars, "
        + juce::String((int) yueyScore.midi.size()) + " midi lanes, tempo "
        + juce::String(yueyScore.workingTempo)
        + (yueyScore.syncedToHost ? " (synced to project)" : ""));
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
        if (file.existsAsFile() && file.loadFileAsData(bytes) && bytes.getSize() > 0)
            score.midi.push_back({ name, std::move(bytes) });
    }

    yueyScore = std::move(score);
    DBG("Restored yuey score from disk: " + juce::String(yueyScore.bars) + " bars, "
        + juce::String((int) yueyScore.midi.size()) + " midi lanes");
    return true;
}
