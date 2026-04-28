/*
  ==============================================================================
    Foundation-specific PluginEditor method definitions.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

void Gary4juceAudioProcessorEditor::updateFoundationEnablementSnapshot()
{
    if (!foundationUI)
        return;

    const bool canGenerate = isServiceReachable(ServiceType::Foundation);
    foundationUI->setGenerateButtonEnabled(canGenerate, isGenerating);

    // Clear the "generating at X BPM..." info text once generation finishes
    if (!isGenerating)
        foundationUI->refreshDurationInfo();

    // Sync BPM from DAW
    if (!juce::JUCEApplicationBase::isStandaloneApp())
    {
        double bpm = audioProcessor.getCurrentBPM();
        if (bpm > 0.0)
            foundationUI->setBpm(bpm);
    }

    // Persist Foundation UI state to processor (for DAW save/load)
    // Only serialize when the built prompt has changed (cheap string comparison)
    {
        juce::String currentPrompt = foundationUI->getBuiltPrompt();
        if (currentPrompt != lastFoundationPromptSnapshot)
        {
            lastFoundationPromptSnapshot = currentPrompt;
            audioProcessor.setFoundationState(foundationUI->serializeState());
        }
    }
}

void Gary4juceAudioProcessorEditor::sendToFoundation()
{
    if (!isServiceReachable(ServiceType::Foundation))
    {
        showStatusMessage("foundation not reachable - check connection first");
        return;
    }

    if (!foundationUI)
        return;

    const auto cancelOp = [this]()
    {
        isGenerating = false;
        isCurrentlyQueued = false;
        generationProgress = 0;
        smoothProgressAnimation = false;
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
        repaint();
    };

    setActiveOp(ActiveOp::FoundationGenerate);
    isGenerating = true;
    isCurrentlyQueued = true;
    generationProgress = 0;
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;
    audioProcessor.setUndoTransformAvailable(false);
    audioProcessor.setRetryAvailable(false);
    updateRetryButtonState();
    updateContinueButtonState();
    updateAllGenerationButtonStates();
    repaint();

    showStatusMessage(foundationUI->getAudio2AudioEnabled()
        ? "submitting audio2audio request..." : "submitting foundation-1 request...", 2500);

    // Build JSON payload
    juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
    jsonRequest->setProperty("seed", foundationUI->getSeed());
    jsonRequest->setProperty("bars", foundationUI->getSelectedBars());

    // Get host BPM
    double bpm = foundationUI->getHostBpm();
    if (!juce::JUCEApplicationBase::isStandaloneApp())
    {
        double dawBpm = audioProcessor.getCurrentBPM();
        if (dawBpm > 0.0) bpm = dawBpm;
    }
    jsonRequest->setProperty("host_bpm", bpm);

    jsonRequest->setProperty("key_root", foundationUI->getKeyRoot());
    jsonRequest->setProperty("key_mode", foundationUI->getKeyMode());
    jsonRequest->setProperty("family", foundationUI->getFamily());
    jsonRequest->setProperty("subfamily", foundationUI->getSubfamily());
    jsonRequest->setProperty("descriptor_knob_a", foundationUI->getDescriptorA());
    jsonRequest->setProperty("descriptor_knob_b", foundationUI->getDescriptorB());
    jsonRequest->setProperty("descriptor_knob_c", foundationUI->getDescriptorC());

    // Descriptors extra
    {
        auto extras = foundationUI->getDescriptorsExtraValues();
        if (extras.size() > 0)
        {
            juce::Array<juce::var> extArr;
            for (auto& e : extras)
                extArr.add(e);
            jsonRequest->setProperty("descriptors_extra", extArr);
        }
    }

    jsonRequest->setProperty("reverb_enabled", foundationUI->getReverbEnabled());
    jsonRequest->setProperty("reverb_amount", foundationUI->getReverbEnabled() ? foundationUI->getReverbAmount() : juce::String());
    jsonRequest->setProperty("delay_enabled", foundationUI->getDelayEnabled());
    jsonRequest->setProperty("delay_type", foundationUI->getDelayEnabled() ? foundationUI->getDelayType() : juce::String());
    jsonRequest->setProperty("distortion_enabled", foundationUI->getDistortionEnabled());
    jsonRequest->setProperty("distortion_amount", foundationUI->getDistortionEnabled() ? foundationUI->getDistortionAmount() : juce::String());
    jsonRequest->setProperty("phaser_enabled", foundationUI->getPhaserEnabled());
    jsonRequest->setProperty("phaser_amount", foundationUI->getPhaserEnabled() ? foundationUI->getPhaserAmount() : juce::String());
    jsonRequest->setProperty("bitcrush_enabled", foundationUI->getBitcrushEnabled());
    jsonRequest->setProperty("bitcrush_amount", foundationUI->getBitcrushEnabled() ? foundationUI->getBitcrushAmount() : juce::String());

    // Behavior tags as array
    auto behaviorTags = foundationUI->getSelectedBehaviorTags();
    juce::var tagsArray;
    for (auto& tag : behaviorTags)
        tagsArray.append(tag);
    jsonRequest->setProperty("behavior_tags", tagsArray);

    jsonRequest->setProperty("custom_prompt_override", foundationUI->getCustomPromptOverride());
    jsonRequest->setProperty("guidance_scale", foundationUI->getGuidance());
    jsonRequest->setProperty("steps", foundationUI->getSteps());

    // Audio2Audio mode — read myBuffer.wav, base64 encode, add init_noise_level
    const bool isAudio2Audio = foundationUI->getAudio2AudioEnabled();
    juce::String base64Audio;

    if (isAudio2Audio)
    {
        auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        auto garyDir = documentsDir.getChildFile("gary4juce");
        auto bufferFile = garyDir.getChildFile("myBuffer.wav");

        if (!bufferFile.existsAsFile())
        {
            showStatusMessage("no recording buffer found - save your recording first", 4000);
            cancelOp();
            return;
        }

        juce::MemoryBlock audioData;
        if (!bufferFile.loadFileAsData(audioData) || audioData.getSize() == 0)
        {
            showStatusMessage("failed to read recording buffer", 4000);
            cancelOp();
            return;
        }

        base64Audio = juce::Base64::toBase64(audioData.getData(), audioData.getSize());
        jsonRequest->setProperty("audio_data", base64Audio);
        jsonRequest->setProperty("init_noise_level", foundationUI->getTransformation());

        DBG("[foundation] audio2audio mode - buffer: " + juce::String(audioData.getSize())
            + " bytes, init_noise_level: " + juce::String(foundationUI->getTransformation()));
    }

    auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

    DBG("[foundation] JSON payload (" + juce::String(isAudio2Audio ? "audio2audio" : "generate")
        + "): " + jsonString.substring(0, 300));

    juce::String endpoint = isAudio2Audio ? "/audio2audio" : "/generate";
    const auto requestUrl = getServiceUrl(ServiceType::Foundation, endpoint);
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, jsonString, requestUrl]()
    {
        juce::URL url(requestUrl);
        juce::URL postUrl = url.withPOSTData(jsonString);

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(15000)
            .withExtraHeaders("Content-Type: application/json");

        juce::String responseText;
        bool ok = false;

        try
        {
            auto stream = postUrl.createInputStream(options);
            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();
                ok = true;
            }
        }
        catch (...) {}

        juce::MessageManager::callAsync([asyncAlive, editor, responseText, ok]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            auto cancelFoundationOperation = [editor]()
            {
                editor->isGenerating = false;
                editor->isCurrentlyQueued = false;
                editor->generationProgress = 0;
                editor->smoothProgressAnimation = false;
                editor->setActiveOp(ActiveOp::None);
                editor->updateAllGenerationButtonStates();
                editor->repaint();
            };

            if (!editor->isGenerating) return;

            if (!ok || responseText.isEmpty())
            {
                editor->showStatusMessage("foundation-1 backend not responding", 4000);
                cancelFoundationOperation();
                return;
            }

            auto responseVar = juce::JSON::parse(responseText);
            auto* responseObj = responseVar.getDynamicObject();
            if (!responseObj)
            {
                editor->showStatusMessage("invalid response from foundation-1", 4000);
                cancelFoundationOperation();
                return;
            }

            bool success = responseObj->getProperty("success");
            if (!success)
            {
                juce::String error = responseObj->getProperty("error").toString();
                editor->showStatusMessage("foundation-1 error: " + error, 5000);
                cancelFoundationOperation();
                return;
            }

            juce::String sessionId = responseObj->getProperty("session_id").toString();
            int foundationBpm = (int)responseObj->getProperty("foundation_bpm");
            juce::String prompt = responseObj->getProperty("prompt").toString();

            DBG("[foundation] session=" + sessionId + " foundation_bpm=" + juce::String(foundationBpm));
            DBG("[foundation] prompt=" + prompt);

            if (editor->foundationUI)
                editor->foundationUI->setInfoText("generating at " + juce::String(foundationBpm) + " BPM...");

            editor->showStatusMessage("foundation-1 generating...", 2000);
            editor->startPollingForResults(sessionId);
        });
    });
}

void Gary4juceAudioProcessorEditor::randomizeFoundation()
{
    if (!isServiceReachable(ServiceType::Foundation))
    {
        showStatusMessage("foundation not reachable", 2000);
        return;
    }

    if (foundationUI)
    {
        foundationUI->setRandomizeEnabled(false);
        foundationUI->setInfoText("randomizing...");
    }

    // Build optional request body
    auto jsonObj = std::make_unique<juce::DynamicObject>();
    jsonObj->setProperty("seed", -1);
    jsonObj->setProperty("mode", "standard");

    juce::String jsonString = juce::JSON::toString(juce::var(jsonObj.release()));
    DBG("[foundation] randomize request: " + jsonString);

    juce::URL url(getServiceUrl(ServiceType::Foundation, "/randomize"));
    juce::URL postUrl = url.withPOSTData(jsonString);

    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, postUrl]()
    {
        juce::String responseText;
        bool ok = false;

        try
        {
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(10000)
                .withExtraHeaders("Content-Type: application/json");

            auto stream = postUrl.createInputStream(options);
            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();
                ok = true;
            }
        }
        catch (...) {}

        juce::MessageManager::callAsync([asyncAlive, editor, responseText, ok]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            if (editor->foundationUI)
                editor->foundationUI->setRandomizeEnabled(true);

            if (!ok || responseText.isEmpty())
            {
                editor->showStatusMessage("randomize failed — check connection", 3000);
                if (editor->foundationUI) editor->foundationUI->setInfoText("");
                return;
            }

            auto responseVar = juce::JSON::parse(responseText);
            auto* responseObj = responseVar.getDynamicObject();
            if (responseObj == nullptr)
            {
                editor->showStatusMessage("randomize: invalid response", 3000);
                if (editor->foundationUI) editor->foundationUI->setInfoText("");
                return;
            }

            bool success = responseObj->getProperty("success");
            if (!success)
            {
                juce::String error = responseObj->getProperty("error").toString();
                editor->showStatusMessage("randomize: " + (error.isEmpty() ? "unknown error" : error), 3000);
                if (editor->foundationUI) editor->foundationUI->setInfoText("");
                return;
            }

            DBG("[foundation] randomize response: " + responseText);

            if (editor->foundationUI)
            {
                editor->foundationUI->applyRandomizeResponse(responseVar);
                editor->foundationUI->setInfoText("");
                editor->showStatusMessage("prompt randomized!", 1500);
            }
        });
    });
}
