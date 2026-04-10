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

    juce::Thread::launch([this]()
        {
            // Determine endpoint based on connection type (same pattern as sendToJerry)
            juce::String endpoint = audioProcessor.getIsUsingLocalhost()
                ? "/models/status"           // Localhost: no /audio prefix
                : "/audio/models/status";    // Remote: requires /audio prefix

            juce::URL url(getServiceUrl(ServiceType::Jerry, endpoint));

            std::unique_ptr<juce::InputStream> stream(url.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(10000)
            ));

            juce::String responseText;
            if (stream != nullptr)
                responseText = stream->readEntireStreamAsString();

            juce::MessageManager::callAsync([this, responseText]()
                {
                    handleJerryModelsResponse(responseText);
                });
        });
}

void Gary4juceAudioProcessorEditor::handleJerryModelsResponse(const juce::String& responseText)
{
    if (responseText.isEmpty())
    {
        DBG("Empty models response");
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
        juce::Array<bool> isFinetune;

        auto* detailsObj = modelDetails.getDynamicObject();
        if (!detailsObj)
        {
            DBG("Failed to get model_details as dynamic object");
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
                isFinetune.add(type == "finetune");

                DBG("Added model: " + displayName);
                DBG("  Type: " + type + ", Repo: " + repo + ", Checkpoint: " + checkpoint);
            }
        }

        if (jerryUI && modelNames.size() > 0)
        {
            jerryUI->setAvailableModels(modelNames, isFinetune, modelKeys,
                modelTypes, modelRepos, modelCheckpoints);
            DBG("=== SUCCESS: Updated Jerry UI with " + juce::String(modelNames.size()) + " models ===");
        }
    }
    catch (...)
    {
        DBG("Exception parsing models response");
    }
}

void Gary4juceAudioProcessorEditor::fetchJerryCheckpoints(const juce::String& repo)
{
    if (jerryUI)
        jerryUI->setFetchingCheckpoints(true);

    juce::Thread::launch([this, repo]() {
        juce::String endpoint = "/models/checkpoints";  // No /audio prefix on localhost
        juce::URL url(getServiceUrl(ServiceType::Jerry, endpoint));

        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("finetune_repo", repo);
        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        juce::URL postUrl = url.withPOSTData(jsonString);

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(10000)
            .withExtraHeaders("Content-Type: application/json");

        std::unique_ptr<juce::InputStream> stream(postUrl.createInputStream(options));

        juce::String responseText;
        if (stream != nullptr)
            responseText = stream->readEntireStreamAsString();

        juce::MessageManager::callAsync([this, responseText]() {
            handleJerryCheckpointsResponse(responseText);
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

    juce::Thread::launch([this, repo, checkpoint]()
        {
            juce::URL url = buildPromptsUrl(repo, checkpoint);

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

            juce::MessageManager::callAsync([this, repo, checkpoint, responseText, statusCode]()
                {
                    applyJerryPromptsToUI(repo, checkpoint, responseText, statusCode);
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

    juce::Thread::launch([this]()
        {
            juce::URL url(getServiceUrl(ServiceType::Jerry, "/audio/models/prompts"));
            url = url.withParameter("prefer", "finetune");

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(8000);

            std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));
            juce::String responseText;
            if (stream) responseText = stream->readEntireStreamAsString();

            juce::MessageManager::callAsync([this, responseText]()
                {
                    promptsFetchInFlight = false;
                    lastPromptsFetchMs = static_cast<std::int64_t>(juce::Time::getCurrentTime().toMilliseconds());
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
                            promptsCache.set(cacheKey, responseText);

                            if (jerryUI)
                                jerryUI->setFinetunePromptBank(repo, ckpt, promptsVar);
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

    juce::Thread::launch([this, repo, checkpoint]() {
        juce::String endpoint = "/models/switch";
        juce::URL url(getServiceUrl(ServiceType::Jerry, endpoint));

        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("model_type", "finetune");
        jsonRequest->setProperty("finetune_repo", repo);
        jsonRequest->setProperty("finetune_checkpoint", checkpoint);
        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        juce::URL postUrl = url.withPOSTData(jsonString);

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(300000)
            .withExtraHeaders("Content-Type: application/json");

        std::unique_ptr<juce::InputStream> stream(postUrl.createInputStream(options));

        juce::String responseText;
        if (stream != nullptr)
            responseText = stream->readEntireStreamAsString();

        juce::MessageManager::callAsync([this, responseText, repo, checkpoint]() {
            handleAddCustomModelResponse(responseText, repo, checkpoint);
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

                juce::Timer::callAfterDelay(500, [this, repo, checkpoint]() {
                    if (jerryUI)
                    {
                        jerryUI->setLoadingModel(false);
                        jerryUI->selectModelByRepo(repo);
                        fetchJerryPrompts(repo, checkpoint);
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

    juce::String endpoint;

    if (audioProcessor.getIsUsingLocalhost())
        endpoint = generateAsLoop ? "/generate/loop" : "/generate";
    else
        endpoint = generateAsLoop ? "/audio/generate/loop" : "/audio/generate";

    juce::String statusText = generateAsLoop ?
        "cooking a smart loop with jerry..." : "baking with jerry...";

    DBG("=== JERRY GENERATION REQUEST ===");
    DBG("Jerry generating with prompt: " + fullPrompt);
    DBG("Endpoint: " + endpoint);
    DBG("Model key: " + currentJerryModelKey);
    DBG("Model is finetune: " + juce::String(currentJerryIsFinetune ? "true" : "false"));
    DBG("Sampler type: " + currentJerrySamplerType);
    DBG("CFG: " + juce::String(currentJerryCfg, 1) + ", Steps: " + juce::String(currentJerrySteps));

    if (generateAsLoop)
        DBG("Loop Type: " + currentLoopType);

    if (jerryUI)
        jerryUI->setGenerateButtonText("generating");
    showStatusMessage(statusText, 2000);

    juce::Thread::launch([this, fullPrompt, endpoint]() {
        auto startTime = juce::Time::getCurrentTime();

        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("prompt", fullPrompt);
        jsonRequest->setProperty("steps", currentJerrySteps);
        jsonRequest->setProperty("cfg_scale", currentJerryCfg);
        jsonRequest->setProperty("return_format", "base64");
        jsonRequest->setProperty("seed", -1);
        jsonRequest->setProperty("model_type", currentJerryModelType);
        if (currentJerryIsFinetune)
        {
            jsonRequest->setProperty("finetune_repo", currentJerryFinetuneRepo);
            jsonRequest->setProperty("finetune_checkpoint", currentJerryFinetuneCheckpoint);
        }
        jsonRequest->setProperty("sampler_type", currentJerrySamplerType);

        if (generateAsLoop)
            jsonRequest->setProperty("loop_type", currentLoopType);

        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        DBG("Jerry JSON payload: " + jsonString);

        juce::URL url(getServiceUrl(ServiceType::Jerry, endpoint));

        juce::String responseText;
        int statusCode = 0;

        try
        {
            juce::URL postUrl = url.withPOSTData(jsonString);

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(30000)
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

        juce::MessageManager::callAsync([this, responseText, statusCode, startTime]() {
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
                        auto audioBase64 = responseObj->getProperty("audio_base64").toString();

                        if (audioBase64.isNotEmpty())
                        {
                            if (jerryUI)
                                jerryUI->setGenerateButtonText("generate with jerry");

                            saveGeneratedAudio(audioBase64);

                            audioProcessor.clearCurrentSessionId();
                            audioProcessor.setUndoTransformAvailable(false);
                            audioProcessor.setRetryAvailable(false);

                            if (auto* metadata = responseObj->getProperty("metadata").getDynamicObject())
                            {
                                auto genTime = metadata->getProperty("generation_time").toString();
                                auto rtFactor = metadata->getProperty("realtime_factor").toString();

                                if (generateAsLoop) {
                                    auto bars = (int)metadata->getProperty("bars");
                                    auto loopDuration = (double)metadata->getProperty("loop_duration_seconds");
                                    showStatusMessage("smart loop rdy " + juce::String(bars) + " bars (" +
                                        juce::String(loopDuration, 1) + "s) " + genTime + "s", 5000);
                                    DBG("Jerry loop metadata - Bars: " + juce::String(bars) +
                                        ", Duration: " + juce::String(loopDuration, 1) + "s");
                                }
                                else {
                                    showStatusMessage("jerry's done already " + genTime + "s (" + rtFactor + "x RT)", 4000);
                                }

                                DBG("Jerry metadata - Generation time: " + genTime + "s, RT factor: " + rtFactor + "x");
                            }
                            else
                            {
                                juce::String successMsg = generateAsLoop ?
                                    "smart loop rdy" : "jerry's done already";
                                showStatusMessage(successMsg, 3000);
                            }
                        }
                        else
                        {
                            showStatusMessage("jerry finished but no audio received", 3000);
                            DBG("Jerry success but missing audio_base64");
                        }
                    }
                    else
                    {
                        auto error = responseObj->getProperty("error").toString();
                        showStatusMessage("jerry error: " + error, 5000);
                        DBG("Jerry server error: " + error);
                    }
                }
                else
                {
                    showStatusMessage("invalid JSON response from jerry", 4000);
                    DBG("Failed to parse Jerry JSON response");

                    DBG("Jerry JSON parsing failed - checking backend health");
                    audioProcessor.checkBackendHealth();

                    juce::Timer::callAfterDelay(6000, [this]() {
                        if (!audioProcessor.isBackendConnected())
                            handleBackendDisconnection();
                    });
                }
            }
            else
            {
                juce::String errorMsg;
                bool shouldCheckHealth = false;

                if (statusCode == 0 && audioProcessor.getIsUsingLocalhost())
                {
                    errorMsg = "cannot connect to jerry on localhost - ensure docker compose is running";
                    markBackendDisconnectedFromRequestFailure("jerry request");
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

                showStatusMessage(errorMsg, 4000);
                DBG("Jerry request failed: " + errorMsg);

                if (shouldCheckHealth)
                {
                    DBG("Jerry failed - checking backend health");
                    audioProcessor.checkBackendHealth();

                    juce::Timer::callAfterDelay(6000, [this]() {
                        if (!audioProcessor.isBackendConnected())
                        {
                            handleBackendDisconnection();
                            lastBackendDisconnectionPopupTime = juce::Time::getCurrentTime();
                        }
                    });
                }
            }

            if (jerryUI)
                jerryUI->setGenerateButtonText("generate with jerry");
            });
        });
}
