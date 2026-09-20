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
        yueyUI->applyPlanMetadata(abc);
        currentYueyBpm = yueyUI->getBpm();
        currentYueyKey = yueyUI->getKey();
        currentYueyMeter = yueyUI->getMeter();
    }
    persistEditorState();
}
