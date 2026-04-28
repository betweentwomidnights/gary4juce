/*
  ==============================================================================
    Jerry-specific PluginEditor method definitions.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

void Gary4juceAudioProcessorEditor::switchJerrySubTab(JerrySubTab sub)
{
    // Foundation sub-tab now available on both localhost and remote
    if (jerrySubTab == sub && currentTab == ModelTab::Jerry) return;

    jerrySubTab = sub;

    // If we're not on the Jerry tab, switch to it first
    if (currentTab != ModelTab::Jerry)
    {
        switchToTab(ModelTab::Jerry);
        return; // switchToTab will call resized() which handles everything
    }

    updateJerrySubTabStates();

    bool showSAOS = (sub == JerrySubTab::SAOS);
    if (jerryUI) jerryUI->setVisibleForTab(showSAOS);
    if (foundationUI)
    {
        foundationUI->setVisibleForTab(!showSAOS);
        if (!showSAOS) updateFoundationEnablementSnapshot();
    }

    // Update help button visibility
    if (helpIcon)
    {
        jerryHelpButton.setVisible(showSAOS);
        foundationHelpButton.setVisible(!showSAOS);
    }

    resized();
    repaint();
}

void Gary4juceAudioProcessorEditor::updateJerrySubTabStates()
{
    jerrySubTabSAOS.setButtonStyle(jerrySubTab == JerrySubTab::SAOS
        ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);

    jerrySubTabFoundation.setEnabled(true);
    {
        jerrySubTabFoundation.setButtonStyle(jerrySubTab == JerrySubTab::Foundation
            ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
        jerrySubTabFoundation.setTooltip("");
    }
}

void Gary4juceAudioProcessorEditor::fetchJerryAvailableModels()
{
    if (!isServiceReachable(ServiceType::Jerry))
    {
        DBG("Jerry not reachable - skipping model fetch");
        return;
    }

    if (jerryModelsFetchInFlight.exchange(true))
    {
        DBG("Jerry models fetch already in flight - skipping");
        return;
    }

    const juce::String endpoint = audioProcessor.getIsUsingLocalhost()
        ? "/models/status"
        : "/audio/models/status";
    const auto modelsUrl = getServiceUrl(ServiceType::Jerry, endpoint);
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, modelsUrl]()
        {
            juce::String responseText;

            try
            {
                juce::URL url(modelsUrl);

                std::unique_ptr<juce::InputStream> stream(url.createInputStream(
                    juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(10000)
                ));

                if (stream != nullptr)
                    responseText = stream->readEntireStreamAsString();
            }
            catch (...)
            {
            }

            juce::MessageManager::callAsync([asyncAlive, editor, responseText]()
                {
                    const auto alive = asyncAlive.lock();
                    if (alive == nullptr || !alive->load(std::memory_order_acquire))
                        return;

                    editor->jerryModelsFetchInFlight.store(false);
                    editor->handleJerryModelsResponse(responseText);
                });
        });
}

void Gary4juceAudioProcessorEditor::handleJerryModelsResponse(const juce::String& responseText)
{
    if (responseText.isEmpty())
    {
        DBG("Empty models response");
        if (jerryUI)
            jerryUI->setLoadingModel(false);
        return;
    }

    DBG("=== RAW MODELS RESPONSE ===");
    DBG("Response length: " + juce::String(responseText.length()) + " characters");

    try
    {
        auto parsed = juce::JSON::parse(responseText);

        if (!parsed.isObject())
        {
            DBG("Invalid models response format - not an object");
            return;
        }

        auto* obj = parsed.getDynamicObject();
        if (!obj || !obj->hasProperty("model_details"))
        {
            DBG("Response missing 'model_details' property");
            return;
        }

        auto modelDetails = obj->getProperty("model_details");
        if (!modelDetails.isObject())
        {
            DBG("model_details is not an object");
            return;
        }

        // Parse models into component arrays
        juce::StringArray modelNames;
        juce::StringArray modelKeys;
        juce::StringArray modelTypes;
        juce::StringArray modelRepos;
        juce::StringArray modelCheckpoints;
        juce::StringArray modelSamplerProfiles;
        juce::Array<bool> isFinetune;

        const auto determineSamplerProfile = [](const juce::String& modelType,
                                                const juce::String& modelSource,
                                                juce::int64 sampleSize)
        {
            const auto source = modelSource.toLowerCase();

            if (sampleSize > 524288)
                return juce::String("sao10");

            if (source.contains("stable-audio-open-1.0")
                || source.contains("stable_audio_open_1_0")
                || source.contains("stableaudioopen1.0")
                || source.contains("sao1"))
            {
                return juce::String("sao10");
            }

            if (modelType == "finetune")
                return juce::String("saos_finetune");

            return juce::String("standard");
        };

        auto* detailsObj = modelDetails.getDynamicObject();
        if (!detailsObj)
        {
            DBG("Failed to get model_details as dynamic object");
            if (jerryUI)
                jerryUI->setLoadingModel(false);
            return;
        }

        DBG("Found " + juce::String(detailsObj->getProperties().size()) + " models in cache");

        for (auto& entry : detailsObj->getProperties())
        {
            juce::String modelKey = entry.name.toString();
            auto* modelData = entry.value.getDynamicObject();

            if (modelData)
            {
                juce::String source = modelData->getProperty("source").toString();
                juce::String type = modelData->getProperty("type").toString();
                const juce::int64 sampleSize = modelData->getProperty("sample_size").toString().getLargeIntValue();
                const juce::String samplerProfile = determineSamplerProfile(type, source, sampleSize);

                DBG("Processing model - Key: " + modelKey + ", Source: " + source + ", Type: " + type);

                juce::String displayName;
                juce::String repo;
                juce::String checkpoint;

                if (type == "standard")
                {
                    displayName = "Standard SAOS";
                    repo = "";
                    checkpoint = "";
                }
                else if (type == "finetune")
                {
                    juce::StringArray parts;
                    parts.addTokens(source, "/", "");

                    if (parts.size() >= 3)
                    {
                        repo = parts[0] + "/" + parts[1];
                        checkpoint = parts[2];

                        juce::String baseName = parts[1].replace("_", " ");

                        juce::StringArray words;
                        words.addTokens(baseName, " ", "");
                        baseName.clear();
                        for (auto& word : words)
                        {
                            if (word.isNotEmpty())
                                baseName += word.substring(0, 1).toUpperCase() + word.substring(1).toLowerCase() + " ";
                        }
                        baseName = baseName.trim();

                        juce::String checkpointInfo = extractCheckpointInfo(checkpoint);

                        if (checkpointInfo.isNotEmpty())
                            displayName = baseName + " (" + checkpointInfo + ")";
                        else
                            displayName = baseName;
                    }
                    else
                    {
                        displayName = "Unknown Finetune";
                        repo = source;
                        checkpoint = "";
                    }
                }
                else
                {
                    displayName = "Unknown Model";
                    repo = "";
                    checkpoint = "";
                }

                modelNames.add(displayName);
                modelKeys.add(modelKey);
                modelTypes.add(type);
                modelRepos.add(repo);
                modelCheckpoints.add(checkpoint);
                modelSamplerProfiles.add(samplerProfile);
                isFinetune.add(type == "finetune");

                DBG("Added model: " + displayName);
                DBG("  Type: " + type + ", Repo: " + repo + ", Checkpoint: " + checkpoint);
                DBG("  Sample size: " + juce::String(sampleSize) + ", Sampler profile: " + samplerProfile);
            }
        }

        if (jerryUI && modelNames.size() > 0)
        {
            jerryUI->setAvailableModels(modelNames, isFinetune, modelKeys,
                modelTypes, modelRepos, modelCheckpoints, modelSamplerProfiles);
            jerryUI->setLoadingModel(false);

            if (audioProcessor.getIsUsingLocalhost() && jerryUI->getSelectedModelIsFinetune())
            {
                const auto selectedRepo = jerryUI->getSelectedFinetuneRepo();
                const auto selectedCheckpoint = jerryUI->getSelectedFinetuneCheckpoint();
                if (selectedRepo.isNotEmpty() && selectedCheckpoint.isNotEmpty())
                    fetchJerryPrompts(selectedRepo, selectedCheckpoint);
            }

            DBG("=== SUCCESS: Updated Jerry UI with " + juce::String(modelNames.size()) + " models ===");
        }
        else if (jerryUI)
        {
            jerryUI->setLoadingModel(false);
        }
    }
    catch (...)
    {
        DBG("Exception parsing models response");
        if (jerryUI)
            jerryUI->setLoadingModel(false);
    }
}

void Gary4juceAudioProcessorEditor::fetchJerryCheckpoints(const juce::String& repo)
{
    if (jerryUI)
        jerryUI->setFetchingCheckpoints(true);

    juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
    jsonRequest->setProperty("finetune_repo", repo);
    const auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));
    const auto checkpointsUrl = getServiceUrl(ServiceType::Jerry, "/models/checkpoints");
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, checkpointsUrl, jsonString]() {
        juce::URL url(checkpointsUrl);
        juce::URL postUrl = url.withPOSTData(jsonString);

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(10000)
            .withExtraHeaders("Content-Type: application/json");

        std::unique_ptr<juce::InputStream> stream(postUrl.createInputStream(options));

        juce::String responseText;
        if (stream != nullptr)
            responseText = stream->readEntireStreamAsString();

        juce::MessageManager::callAsync([asyncAlive, editor, responseText]() {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            editor->handleJerryCheckpointsResponse(responseText);
        });
    });
}

void Gary4juceAudioProcessorEditor::handleJerryCheckpointsResponse(const juce::String& responseText)
{
    if (jerryUI)
        jerryUI->setFetchingCheckpoints(false);

    if (responseText.isEmpty()) {
        showStatusMessage("failed to fetch checkpoints", 3000);
        return;
    }

    try {
        auto parsed = juce::JSON::parse(responseText);
        if (auto* obj = parsed.getDynamicObject()) {
            bool success = obj->getProperty("success");

            if (success) {
                juce::StringArray checkpoints;
                auto checkpointsArray = obj->getProperty("checkpoints");

                if (auto* arr = checkpointsArray.getArray()) {
                    for (auto& item : *arr) {
                        checkpoints.add(item.toString());
                    }
                }

                if (jerryUI)
                    jerryUI->setAvailableCheckpoints(checkpoints);

                showStatusMessage(juce::String(checkpoints.size()) + " checkpoints found", 2500);
            } else {
                auto error = obj->getProperty("error").toString();
                showStatusMessage("fetch failed: " + error, 4000);
            }
        }
    } catch (...) {
        showStatusMessage("invalid checkpoint response", 3000);
    }
}

juce::URL Gary4juceAudioProcessorEditor::buildPromptsUrl(const juce::String& repo,
    const juce::String& checkpoint) const
{
    const bool isLocal = audioProcessor.getIsUsingLocalhost();
    const juce::String endpoint = isLocal ? "/models/prompts" : "/audio/models/prompts";

    juce::URL url(getServiceUrl(ServiceType::Jerry, endpoint));

    url = url.withParameter("repo", repo)
        .withParameter("checkpoint", checkpoint);

    return url;
}

void Gary4juceAudioProcessorEditor::fetchJerryPrompts(const juce::String& repo,
    const juce::String& checkpoint)
{
    const auto cacheKey = repo + "|" + checkpoint;
    if (promptsCache.contains(cacheKey))
    {
        DBG("[prompts] cache hit for " + cacheKey + " - applying");
        applyJerryPromptsToUI(repo, checkpoint, promptsCache[cacheKey]);
        return;
    }

    const auto promptsUrl = buildPromptsUrl(repo, checkpoint);
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, repo, checkpoint, promptsUrl]()
        {
            juce::URL url = promptsUrl;

            int statusCode = 0;
            juce::StringPairArray responseHeaders;

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(10000)
                .withStatusCode(&statusCode)
                .withResponseHeaders(&responseHeaders);

            std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));
            juce::String responseText;
            if (stream) responseText = stream->readEntireStreamAsString();

            DBG("[prompts] GET " + url.toString(true) +
                " status=" + juce::String(statusCode) +
                " bytes=" + juce::String((int)responseText.getNumBytesAsUTF8()));

            if (statusCode < 200 || statusCode >= 300)
                DBG("[prompts] non-2xx - first512: " + responseText.substring(0, 512));

            juce::MessageManager::callAsync([asyncAlive, editor, repo, checkpoint, responseText, statusCode]()
                {
                    const auto alive = asyncAlive.lock();
                    if (alive == nullptr || !alive->load(std::memory_order_acquire))
                        return;

                    editor->applyJerryPromptsToUI(repo, checkpoint, responseText, statusCode);
                });
        });
}

void Gary4juceAudioProcessorEditor::maybeFetchRemoteJerryPrompts()
{
    DBG("[prompts] maybeFetchRemoteJerryPrompts called");
    if (promptsFetchInFlight) { DBG("[prompts] in flight - skipping"); return; }

    const auto now = static_cast<std::int64_t>(juce::Time::getCurrentTime().toMilliseconds());
    if (now - lastPromptsFetchMs < kPromptsTTLms) {
        DBG("[prompts] TTL not expired - skipping");
        return;
    }

    if (jerryUI)
    {
        const auto repo = jerryUI->getSelectedFinetuneRepo();
        const auto ckpt = jerryUI->getSelectedFinetuneCheckpoint();
        if (repo.isNotEmpty() && ckpt.isNotEmpty())
        {
            const auto key = repo + "|" + ckpt;
            if (promptsCache.contains(key))
            {
                DBG("[prompts] cache hit for " + key + " - applying");
                applyJerryPromptsToUI(repo, ckpt, promptsCache[key]);
                lastPromptsFetchMs = now;
                return;
            }
        }
    }

    promptsFetchInFlight = true;

    juce::URL promptsUrl(getServiceUrl(ServiceType::Jerry, "/audio/models/prompts"));
    promptsUrl = promptsUrl.withParameter("prefer", "finetune");
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, promptsUrl]()
        {
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(8000);

            std::unique_ptr<juce::InputStream> stream(promptsUrl.createInputStream(options));
            juce::String responseText;
            if (stream) responseText = stream->readEntireStreamAsString();

            juce::MessageManager::callAsync([asyncAlive, editor, responseText]()
                {
                    const auto alive = asyncAlive.lock();
                    if (alive == nullptr || !alive->load(std::memory_order_acquire))
                        return;

                    editor->promptsFetchInFlight = false;
                    editor->lastPromptsFetchMs = static_cast<std::int64_t>(juce::Time::getCurrentTime().toMilliseconds());
                    if (responseText.isEmpty()) return;

                    auto parsed = juce::JSON::parse(responseText);
                    if (auto* obj = parsed.getDynamicObject())
                    {
                        if (!obj->getProperty("success")) return;

                        const auto promptsVar = obj->getProperty("prompts");
                        const auto repo = obj->getProperty("source").toString();
                        const auto ckpt = obj->getProperty("checkpoint").toString();

                        if (promptsVar.isObject() && repo.isNotEmpty() && ckpt.isNotEmpty())
                        {
                            const auto cacheKey = repo + "|" + ckpt;
                            editor->promptsCache.set(cacheKey, responseText);

                            if (editor->jerryUI)
                                editor->jerryUI->setFinetunePromptBank(repo, ckpt, promptsVar);
                        }
                    }
                });
        });
}

void Gary4juceAudioProcessorEditor::applyJerryPromptsToUI(const juce::String& repo,
    const juce::String& checkpoint,
    const juce::String& jsonText,
    int statusCode)
{
    juce::ignoreUnused(statusCode);

    if (jsonText.isEmpty())
    {
        DBG("[prompts] empty response - skipping");
        return;
    }

    juce::var parsed;
    try
    {
        parsed = juce::JSON::parse(jsonText);
    }
    catch (...)
    {
        DBG("[prompts] JSON parse error - first512: " + jsonText.substring(0, 512));
        return;
    }

    if (auto* obj = parsed.getDynamicObject())
    {
        if (!obj->getProperty("success"))
        {
            DBG("[prompts] success=false - payload first512: " + jsonText.substring(0, 512));
            return;
        }

        auto promptsVar = obj->getProperty("prompts");
        if (!promptsVar.isObject())
        {
            DBG("[prompts] prompts missing/invalid - payload first512: " + jsonText.substring(0, 512));
            return;
        }

        auto resolvedRepo = obj->getProperty("source").toString();
        auto resolvedCkpt = obj->getProperty("checkpoint").toString();
        if (resolvedRepo.isEmpty()) resolvedRepo = repo;
        if (resolvedCkpt.isEmpty()) resolvedCkpt = checkpoint;

        const auto cacheKey = resolvedRepo + "|" + resolvedCkpt;
        promptsCache.set(cacheKey, jsonText);

        if (jerryUI)
            jerryUI->setFinetunePromptBank(resolvedRepo, resolvedCkpt, promptsVar);

        DBG("[prompts] stored bank for " + cacheKey);
    }
}

void Gary4juceAudioProcessorEditor::addCustomJerryModel(const juce::String& repo, const juce::String& checkpoint)
{
    juce::String checkpointInfo = extractCheckpointInfo(checkpoint);
    juce::String loadingMessage = checkpointInfo.isNotEmpty()
        ? "loading " + checkpointInfo
        : "loading " + checkpoint;

    showStatusMessage(loadingMessage + "...", 15000);

    if (jerryUI)
        jerryUI->setLoadingModel(true, checkpointInfo);

    juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
    jsonRequest->setProperty("model_type", "finetune");
    jsonRequest->setProperty("finetune_repo", repo);
    jsonRequest->setProperty("finetune_checkpoint", checkpoint);
    const auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));
    const auto switchUrl = getServiceUrl(ServiceType::Jerry, "/models/switch");
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, repo, checkpoint, switchUrl, jsonString]() {
        juce::URL url(switchUrl);
        juce::URL postUrl = url.withPOSTData(jsonString);

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(300000)
            .withExtraHeaders("Content-Type: application/json");

        std::unique_ptr<juce::InputStream> stream(postUrl.createInputStream(options));

        juce::String responseText;
        if (stream != nullptr)
            responseText = stream->readEntireStreamAsString();

        juce::MessageManager::callAsync([asyncAlive, editor, responseText, repo, checkpoint]() {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            editor->handleAddCustomModelResponse(responseText, repo, checkpoint);
        });
    });
}

void Gary4juceAudioProcessorEditor::handleAddCustomModelResponse(const juce::String& responseText,
                                                                 const juce::String& repo,
                                                                 const juce::String& checkpoint)
{
    if (responseText.isEmpty()) {
        showStatusMessage("failed to switch model - connection timeout", 5000);

        if (jerryUI)
            jerryUI->setLoadingModel(false);
        return;
    }

    try {
        auto parsed = juce::JSON::parse(responseText);
        if (auto* obj = parsed.getDynamicObject()) {
            bool success = obj->getProperty("success");

            if (success) {
                juce::String checkpointInfo = extractCheckpointInfo(checkpoint);
                juce::String successMsg = checkpointInfo.isNotEmpty()
                    ? "model loaded: " + checkpointInfo
                    : "model loaded successfully";

                showStatusMessage(successMsg + "!", 4000);

                DBG("Model switch successful - refreshing model list");
                fetchJerryAvailableModels();

                const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
                auto* editor = this;
                juce::Timer::callAfterDelay(500, [asyncAlive, editor, repo, checkpoint]() {
                    const auto alive = asyncAlive.lock();
                    if (alive == nullptr || !alive->load(std::memory_order_acquire))
                        return;

                    if (editor->jerryUI)
                    {
                        editor->jerryUI->setLoadingModel(false);
                        editor->jerryUI->selectModelByRepo(repo);
                        editor->fetchJerryPrompts(repo, checkpoint);
                    }
                    });
            } else {
                auto error = obj->getProperty("error").toString();
                showStatusMessage("load failed: " + error, 5000);

                if (jerryUI)
                    jerryUI->setLoadingModel(false);

                DBG("Model switch failed: " + error);
            }
        }
    } catch (...) {
        showStatusMessage("invalid response from server", 4000);

        if (jerryUI)
            jerryUI->setLoadingModel(false);
    }
}

juce::String Gary4juceAudioProcessorEditor::extractCheckpointInfo(const juce::String& checkpoint)
{
    if (checkpoint.contains("step="))
    {
        auto stepStart = checkpoint.indexOf("step=") + 5;
        auto stepEnd = checkpoint.indexOfChar(stepStart, '.');
        if (stepEnd < 0) stepEnd = checkpoint.indexOfChar(stepStart, '-');
        if (stepEnd < 0) stepEnd = checkpoint.length();

        auto stepValue = checkpoint.substring(stepStart, stepEnd);
        if (stepValue.containsOnly("0123456789"))
            return "step " + stepValue;
    }

    if (checkpoint.contains("epoch="))
    {
        auto epochStart = checkpoint.indexOf("epoch=") + 6;
        auto epochEnd = checkpoint.indexOfChar(epochStart, '.');
        if (epochEnd < 0) epochEnd = checkpoint.indexOfChar(epochStart, '-');
        if (epochEnd < 0) epochEnd = checkpoint.length();

        auto epochValue = checkpoint.substring(epochStart, epochEnd);
        if (epochValue.containsOnly("0123456789"))
            return "epoch " + epochValue;
    }

    return "";
}

void Gary4juceAudioProcessorEditor::sendToJerry()
{
    if (!isServiceReachable(ServiceType::Jerry))
    {
        showStatusMessage("jerry not reachable - check connection first");
        return;
    }

    if (currentJerryPrompt.trim().isEmpty())
    {
        showStatusMessage("please enter a text prompt for jerry");
        return;
    }

    double bpm = audioProcessor.getCurrentBPM();

    bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();
    if (isStandalone && jerryUI)
    {
        bpm = jerryUI->getManualBpm();
        DBG("Using manual BPM in standalone: " + juce::String(bpm));
    }
    else
    {
        DBG("Using DAW BPM in plugin: " + juce::String(bpm));
    }

    juce::String fullPrompt = currentJerryPrompt + " " + juce::String((int)bpm) + "bpm";

    const bool useLocalAsyncPolling = audioProcessor.getIsUsingLocalhost();
    juce::String endpoint;

    if (useLocalAsyncPolling)
        endpoint = generateAsLoop ? "/generate/loop/async" : "/generate/async";
    else
        endpoint = generateAsLoop ? "/audio/generate/loop" : "/audio/generate";

    juce::String statusText = generateAsLoop ?
        "cooking a smart loop with jerry..." : "baking with jerry...";

    if (useLocalAsyncPolling)
    {
        setActiveOp(ActiveOp::JerryGenerate);
        isGenerating = true;
        generationProgress = 0;
        lastKnownProgress = 0;
        targetProgress = 0;
        smoothProgressAnimation = false;
        isCurrentlyQueued = false;
        resetStallDetection();

        audioProcessor.clearCurrentSessionId();
        audioProcessor.setRetryAvailable(false);
        updateRetryButtonState();
        updateContinueButtonState();
        updateAllGenerationButtonStates();
        repaint();
    }

    DBG("=== JERRY GENERATION REQUEST ===");
    DBG("Jerry generating with prompt: " + fullPrompt);
    DBG("Endpoint: " + endpoint);
    DBG("Model key: " + currentJerryModelKey);
    DBG("Model is finetune: " + juce::String(currentJerryIsFinetune ? "true" : "false"));
    DBG("Sampler type: " + currentJerrySamplerType);
    DBG("CFG: " + juce::String(currentJerryCfg, 1) + ", Steps: " + juce::String(currentJerrySteps));

    if (generateAsLoop)
        DBG("Loop Type: " + currentLoopType);

    auto cancelJerryOperation = [this]()
    {
        stopPolling();
        isGenerating = false;
        isCurrentlyQueued = false;
        generationProgress = 0;
        smoothProgressAnimation = false;
        if (jerryUI)
            jerryUI->setGenerateButtonText("generate with jerry");
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
        repaint();
    };

    if (jerryUI)
        jerryUI->setGenerateButtonText("generating");
    showStatusMessage(statusText, 2000);

    juce::String requestId;
    if (useLocalAsyncPolling)
        requestId = juce::Uuid().toString();

    const int jerrySteps = currentJerrySteps;
    const float jerryCfg = currentJerryCfg;
    const juce::String modelType = currentJerryModelType;
    const bool isFinetune = currentJerryIsFinetune;
    const juce::String finetuneRepo = currentJerryFinetuneRepo;
    const juce::String finetuneCheckpoint = currentJerryFinetuneCheckpoint;
    const juce::String samplerType = currentJerrySamplerType;
    const bool requestGenerateAsLoop = generateAsLoop;
    const juce::String requestLoopType = currentLoopType;
    const auto requestUrl = getServiceUrl(ServiceType::Jerry, endpoint);
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, fullPrompt, requestUrl, jerrySteps, jerryCfg, modelType,
                          isFinetune, finetuneRepo, finetuneCheckpoint, samplerType,
                          requestGenerateAsLoop, requestLoopType, useLocalAsyncPolling,
                          requestId, cancelJerryOperation]() {
        auto startTime = juce::Time::getCurrentTime();

        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("prompt", fullPrompt);
        jsonRequest->setProperty("steps", jerrySteps);
        jsonRequest->setProperty("cfg_scale", jerryCfg);
        jsonRequest->setProperty("return_format", "base64");
        jsonRequest->setProperty("seed", -1);
        jsonRequest->setProperty("model_type", modelType);
        if (isFinetune)
        {
            jsonRequest->setProperty("finetune_repo", finetuneRepo);
            jsonRequest->setProperty("finetune_checkpoint", finetuneCheckpoint);
        }
        jsonRequest->setProperty("sampler_type", samplerType);

        if (requestGenerateAsLoop)
            jsonRequest->setProperty("loop_type", requestLoopType);

        if (useLocalAsyncPolling)
            jsonRequest->setProperty("request_id", requestId);

        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        DBG("Jerry JSON payload: " + jsonString);

        juce::URL url(requestUrl);

        juce::String responseText;
        int statusCode = 0;

        try
        {
            juce::URL postUrl = url.withPOSTData(jsonString);

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(useLocalAsyncPolling ? 15000 : 30000)
                .withExtraHeaders("Content-Type: application/json");

            auto stream = postUrl.createInputStream(options);

            auto requestTime = juce::Time::getCurrentTime() - startTime;
            DBG("Jerry HTTP connection established in " + juce::String(requestTime.inMilliseconds()) + "ms");

            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();

                auto totalTime = juce::Time::getCurrentTime() - startTime;
                DBG("Jerry HTTP request completed in " + juce::String(totalTime.inMilliseconds()) + "ms");
                DBG("Jerry response length: " + juce::String(responseText.length()) + " characters");

                statusCode = 200;
            }
            else
            {
                DBG("Failed to create input stream for Jerry request");
                responseText = "";
                statusCode = 0;
            }
        }
        catch (const std::exception& e)
        {
            DBG("Jerry HTTP request exception: " + juce::String(e.what()));
            responseText = "";
            statusCode = 0;
        }
        catch (...)
        {
            DBG("Unknown Jerry HTTP request exception");
            responseText = "";
            statusCode = 0;
        }

        juce::MessageManager::callAsync([asyncAlive, editor, responseText, statusCode, startTime,
                                         useLocalAsyncPolling, requestId, cancelJerryOperation]() {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;
            auto totalTime = juce::Time::getCurrentTime() - startTime;
            DBG("Total Jerry request time: " + juce::String(totalTime.inMilliseconds()) + "ms");

            if (statusCode == 200 && responseText.isNotEmpty())
            {
                DBG("Jerry response: " + responseText.substring(0, 200) + "...");

                auto responseVar = juce::JSON::parse(responseText);
                if (auto* responseObj = responseVar.getDynamicObject())
                {
                    bool success = responseObj->getProperty("success");
                    if (success)
                    {
                        if (useLocalAsyncPolling)
                        {
                            juce::String sessionId = responseObj->getProperty("session_id").toString();
                            if (sessionId.isEmpty())
                                sessionId = requestId;

                            if (sessionId.isEmpty())
                            {
                                editor->showStatusMessage("jerry async request missing session id", 5000);
                                cancelJerryOperation();
                                return;
                            }

                            editor->showStatusMessage("sent to jerry. processing...", 2000);
                            editor->startPollingForResults(sessionId);
                            return;
                        }

                        auto audioBase64 = responseObj->getProperty("audio_base64").toString();

                        if (audioBase64.isNotEmpty())
                        {
                            if (editor->jerryUI)
                                editor->jerryUI->setGenerateButtonText("generate with jerry");

                            editor->saveGeneratedAudio(audioBase64);

                            editor->audioProcessor.clearCurrentSessionId();
                            editor->audioProcessor.setUndoTransformAvailable(false);
                            editor->audioProcessor.setRetryAvailable(false);

                            if (auto* metadata = responseObj->getProperty("metadata").getDynamicObject())
                            {
                                auto genTime = metadata->getProperty("generation_time").toString();
                                auto rtFactor = metadata->getProperty("realtime_factor").toString();

                                if (editor->generateAsLoop) {
                                    auto bars = (int)metadata->getProperty("bars");
                                    auto loopDuration = (double)metadata->getProperty("loop_duration_seconds");
                                    editor->showStatusMessage("smart loop rdy " + juce::String(bars) + " bars (" +
                                        juce::String(loopDuration, 1) + "s) " + genTime + "s", 5000);
                                    DBG("Jerry loop metadata - Bars: " + juce::String(bars) +
                                        ", Duration: " + juce::String(loopDuration, 1) + "s");
                                }
                                else {
                                    editor->showStatusMessage("jerry's done already " + genTime + "s (" + rtFactor + "x RT)", 4000);
                                }

                                DBG("Jerry metadata - Generation time: " + genTime + "s, RT factor: " + rtFactor + "x");
                            }
                            else
                            {
                                juce::String successMsg = editor->generateAsLoop ?
                                    "smart loop rdy" : "jerry's done already";
                                editor->showStatusMessage(successMsg, 3000);
                            }
                        }
                        else
                        {
                            editor->showStatusMessage("jerry finished but no audio received", 3000);
                            DBG("Jerry success but missing audio_base64");
                        }
                    }
                    else
                    {
                        auto error = responseObj->getProperty("error").toString();
                        editor->showStatusMessage("jerry error: " + error, 5000);
                        DBG("Jerry server error: " + error);
                        if (useLocalAsyncPolling)
                            cancelJerryOperation();
                    }
                }
                else
                {
                    editor->showStatusMessage("invalid JSON response from jerry", 4000);
                    DBG("Failed to parse Jerry JSON response");

                    DBG("Jerry JSON parsing failed - checking backend health");
                    editor->audioProcessor.checkBackendHealth();

                    juce::Timer::callAfterDelay(6000, [asyncAlive, editor]() {
                        const auto alive = asyncAlive.lock();
                        if (alive == nullptr || !alive->load(std::memory_order_acquire))
                            return;

                        if (!editor->audioProcessor.isBackendConnected())
                            editor->handleBackendDisconnection();
                    });

                    if (useLocalAsyncPolling)
                        cancelJerryOperation();
                }
            }
            else
            {
                juce::String errorMsg;
                bool shouldCheckHealth = false;

                if (statusCode == 0 && editor->audioProcessor.getIsUsingLocalhost())
                {
                    errorMsg = "cannot connect to jerry on localhost - ensure jerry is running in gary4local";
                    editor->markBackendDisconnectedFromRequestFailure("jerry request");
                }
                else if (statusCode == 0)
                {
                    errorMsg = "failed to connect to jerry on remote backend";
                    shouldCheckHealth = true;
                }
                else if (statusCode >= 400)
                {
                    errorMsg = "jerry server error (HTTP " + juce::String(statusCode) + ")";
                    shouldCheckHealth = true;
                }
                else
                    errorMsg = "empty response from jerry";

                editor->showStatusMessage(errorMsg, 4000);
                DBG("Jerry request failed: " + errorMsg);

                if (useLocalAsyncPolling)
                    cancelJerryOperation();

                if (shouldCheckHealth)
                {
                    DBG("Jerry failed - checking backend health");
                    editor->audioProcessor.checkBackendHealth();

                    juce::Timer::callAfterDelay(6000, [asyncAlive, editor]() {
                        const auto alive = asyncAlive.lock();
                        if (alive == nullptr || !alive->load(std::memory_order_acquire))
                            return;

                        if (!editor->audioProcessor.isBackendConnected())
                        {
                            editor->handleBackendDisconnection();
                            editor->lastBackendDisconnectionPopupTime = juce::Time::getCurrentTime();
                        }
                    });
                }
            }

            if (!useLocalAsyncPolling && editor->jerryUI)
                editor->jerryUI->setGenerateButtonText("generate with jerry");
            });
        });
}
