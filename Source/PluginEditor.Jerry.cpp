// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

/*
  ==============================================================================
    Jerry-specific PluginEditor method definitions.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Utils/GenerationMetadata.h"

namespace
{
    juce::String getSA3PromptErrorMessage(const juce::var& responseVar)
    {
        if (auto* responseObj = responseVar.getDynamicObject())
        {
            for (const auto& key : { "detail", "error", "message" })
            {
                const juce::String value = responseObj->getProperty(key).toString().trim();
                if (value.isNotEmpty())
                    return value;
            }

            if (auto* errors = responseObj->getProperty("errors").getArray())
            {
                juce::StringArray parts;
                for (const auto& item : *errors)
                    if (item.toString().trim().isNotEmpty())
                        parts.add(item.toString().trim());

                if (!parts.isEmpty())
                    return parts.joinIntoString(", ");
            }
        }

        return {};
    }

    void addSA3PromptBucket(const juce::DynamicObject& diceObj,
                            const juce::String& bucketName,
                            juce::StringArray& promptPool)
    {
        if (bucketName.trim().isEmpty())
            return;

        const auto bucketVar = diceObj.getProperty(juce::Identifier(bucketName));
        if (auto* prompts = bucketVar.getArray())
        {
            for (const auto& item : *prompts)
            {
                const juce::String prompt = item.toString().trim();
                if (prompt.isNotEmpty() && !promptPool.contains(prompt))
                    promptPool.add(prompt);
            }
        }
    }

    juce::String pickSA3DicePrompt(const juce::String& responseText,
                                   juce::StringArray& missingLoras,
                                   juce::String& errorMessage)
    {
        juce::var responseVar;
        try
        {
            responseVar = juce::JSON::parse(responseText);
        }
        catch (...)
        {
            errorMessage = "invalid sa3 prompt response";
            return {};
        }

        auto* responseObj = responseVar.getDynamicObject();
        if (responseObj == nullptr)
        {
            errorMessage = "invalid sa3 prompt response";
            return {};
        }

        if (!responseObj->getProperty("success"))
        {
            errorMessage = getSA3PromptErrorMessage(responseVar);
            if (errorMessage.isEmpty())
                errorMessage = "sa3 prompt request failed";
            return {};
        }

        if (auto* missingArray = responseObj->getProperty("missing_loras").getArray())
            for (const auto& item : *missingArray)
                if (item.toString().trim().isNotEmpty())
                    missingLoras.addIfNotAlreadyThere(item.toString().trim());

        auto promptsVar = responseObj->getProperty("prompts");
        auto* promptsObj = promptsVar.getDynamicObject();
        if (promptsObj == nullptr)
        {
            errorMessage = "sa3 prompt response missing prompts";
            return {};
        }

        auto diceVar = promptsObj->getProperty("dice");
        auto* diceObj = diceVar.getDynamicObject();
        if (diceObj == nullptr)
        {
            errorMessage = "sa3 prompt response missing dice pool";
            return {};
        }

        juce::StringArray promptPool;
        juce::StringArray preferredBuckets;
        preferredBuckets.add("generic");
        preferredBuckets.add("instrumental");
        preferredBuckets.add("drums");

        for (const auto& bucket : preferredBuckets)
            addSA3PromptBucket(*diceObj, bucket, promptPool);

        for (const auto& entry : diceObj->getProperties())
        {
            const juce::String bucket = entry.name.toString();
            if (!preferredBuckets.contains(bucket))
                addSA3PromptBucket(*diceObj, bucket, promptPool);
        }

        if (promptPool.isEmpty())
        {
            errorMessage = "sa3 dice returned no prompts";
            return {};
        }

        auto& random = juce::Random::getSystemRandom();
        return promptPool[random.nextInt(promptPool.size())];
    }
}

void Gary4juceAudioProcessorEditor::switchJerrySubTab(JerrySubTab sub)
{
    if (jerrySubTab == sub && currentTab == ModelTab::Jerry) return;

    jerrySubTab = sub;

    // If we're not on the Jerry tab, switch to it first
    if (currentTab != ModelTab::Jerry)
    {
        switchToTab(ModelTab::Jerry);
        return; // switchToTab will call resized() which handles everything
    }

    updateJerrySubTabStates();

    bool showSA3 = (sub == JerrySubTab::SA3);
    bool showSAOS = (sub == JerrySubTab::SAOS);
    if (sa3UI)
    {
        sa3UI->setRemoteAvailable(isServiceReachable(ServiceType::SA3));
        sa3UI->setVisibleForTab(showSA3);
        if (showSA3)
        {
            refreshSA3AvailableLoras(true);
            updateSA3EnablementSnapshot();
        }
    }
    if (jerryUI)
    {
        jerryUI->setVisibleForTab(showSAOS);
        if (showSAOS)
        {
            // The sub-tab row is the one route onto SAOS that doesn't go through
            // switchToTab, so without this the combo box sits on "loading
            // models..." until you leave the tab and come back.
            fetchJerryAvailableModels();

            maybeFetchJerryPrompts();
        }
    }
    if (foundationUI)
    {
        foundationUI->setVisibleForTab(sub == JerrySubTab::Foundation);
        if (sub == JerrySubTab::Foundation) updateFoundationEnablementSnapshot();
    }

    // Update help button visibility
    if (helpIcon)
    {
        sa3HelpButton.setVisible(showSA3);
        jerryHelpButton.setVisible(showSAOS);
        foundationHelpButton.setVisible(sub == JerrySubTab::Foundation);
    }

    updateSA3LanePanel();   // lays out too: the lanes panel shows only on SA3
    repaint();
    persistEditorState();
}

void Gary4juceAudioProcessorEditor::updateJerrySubTabStates()
{
    const bool sa3Selected = jerrySubTab == JerrySubTab::SA3;
    jerrySubTabSA3.setEnabled(true);
    jerrySubTabSA3.setButtonStyle(sa3Selected
        ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
    jerrySubTabSA3.setTooltip(audioProcessor.getIsUsingLocalhost() && !isLocalServiceOnline(ServiceType::SA3)
        ? "sa3 localhost:8006 offline" : "");

    jerrySubTabSAOS.setButtonStyle(jerrySubTab == JerrySubTab::SAOS
        ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);

    jerrySubTabFoundation.setEnabled(true);
    {
        jerrySubTabFoundation.setButtonStyle(jerrySubTab == JerrySubTab::Foundation
            ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
        jerrySubTabFoundation.setTooltip("");
    }
}

void Gary4juceAudioProcessorEditor::fetchJerryAvailableModels(bool force)
{
    if (!force && !isServiceReachable(ServiceType::Jerry))
    {
        DBG("Jerry not reachable - skipping model fetch");
        return;
    }

    if (force && !isServiceReachable(ServiceType::Jerry))
    {
        DBG("Forcing Jerry model fetch after host preset restore");
    }

    const juce::String endpoint = audioProcessor.getIsUsingLocalhost()
        ? "/models/status"
        : "/audio/models/status";
    const auto modelsUrl = getServiceUrl(ServiceType::Jerry, endpoint);
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, modelsUrl]()
        {
            juce::URL url(modelsUrl);

            std::unique_ptr<juce::InputStream> stream(url.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(10000)
            ));

            juce::String responseText;
            if (stream != nullptr)
                responseText = stream->readEntireStreamAsString();

            juce::MessageManager::callAsync([asyncAlive, editor, responseText]()
                {
                    const auto alive = asyncAlive.lock();
                    if (alive == nullptr || !alive->load(std::memory_order_acquire))
                        return;

                    editor->handleJerryModelsResponse(responseText);
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

        // Which model the backend actually has live. usage_order is LRU, so the
        // last entry is the current one - the same rule /models/prompts uses to
        // resolve "active".
        juce::String activeKey;
        if (auto* cacheStatus = obj->getProperty("cache_status").getDynamicObject())
            if (auto* order = cacheStatus->getProperty("usage_order").getArray())
                if (!order->isEmpty())
                    activeKey = order->getLast().toString();

        if (jerryUI && modelNames.size() > 0)
        {
            const juce::String desiredKey = preferredJerryModelKey;
            const juce::String desiredRepo = preferredJerryFinetuneRepo;
            const juce::String desiredCheckpoint = preferredJerryFinetuneCheckpoint;
            const juce::String desiredSampler = currentJerrySamplerType;
            const float desiredCfg = currentJerryCfg;
            const int desiredSteps = currentJerrySteps;
            const bool hadStoredSelection = desiredKey.isNotEmpty() || desiredRepo.isNotEmpty();

            jerryUI->setAvailableModels(modelNames, isFinetune, modelKeys,
                modelTypes, modelRepos, modelCheckpoints);

            int desiredIndex = -1;
            for (int i = 0; i < modelNames.size(); ++i)
            {
                const bool keyMatches = desiredKey.isNotEmpty() && modelKeys[i] == desiredKey;
                const bool finetuneMatches = desiredRepo.isNotEmpty()
                    && modelRepos[i] == desiredRepo
                    && modelCheckpoints[i] == desiredCheckpoint;
                if (keyMatches || finetuneMatches)
                {
                    desiredIndex = i;
                    break;
                }
            }

            // Nothing stored yet (fresh instance): follow whatever the backend
            // has loaded rather than leaving the state blank while the combo box
            // sits on entry 0, which is how a finetune ended up being driven with
            // the standard model's cfg and steps.
            if (desiredIndex < 0 && activeKey.isNotEmpty())
                desiredIndex = modelKeys.indexOf(activeKey);

            // setAvailableModels already selected entry 0; adopt it explicitly so
            // the model type is recorded either way.
            if (desiredIndex < 0)
                desiredIndex = 0;

            jerryUI->setSelectedModel(desiredIndex);
            if (desiredSampler.isNotEmpty())
                jerryUI->setSelectedSamplerType(desiredSampler);
            currentJerryModelIndex = desiredIndex;
            currentJerryModelKey = modelKeys[desiredIndex];
            currentJerryModelType = modelTypes[desiredIndex];
            currentJerryFinetuneRepo = modelRepos[desiredIndex];
            currentJerryFinetuneCheckpoint = modelCheckpoints[desiredIndex];
            currentJerryIsFinetune = isFinetune[desiredIndex];
            currentJerrySamplerType = jerryUI->getSelectedSamplerType();
            preferredJerryModelKey = currentJerryModelKey;
            preferredJerryFinetuneRepo = currentJerryFinetuneRepo;
            preferredJerryFinetuneCheckpoint = currentJerryFinetuneCheckpoint;

            // Selecting a model adjusts its slider ranges and defaults. Restore
            // the host-loaded values after that adjustment - but only when there
            // were any. With nothing stored, those "values" are just this class's
            // initialisers, which happen to be the standard model's 1.0 / 8 and
            // would silently overwrite a finetune's 4.0 / 30.
            if (hadStoredSelection)
            {
                jerryUI->setCfg(desiredCfg);
                jerryUI->setSteps(desiredSteps);
            }
            currentJerryCfg = jerryUI->getCfg();
            currentJerrySteps = jerryUI->getSteps();

            DBG("=== SUCCESS: Updated Jerry UI with " + juce::String(modelNames.size()) + " models ===");
        }
    }
    catch (...)
    {
        DBG("Exception parsing models response");
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

void Gary4juceAudioProcessorEditor::maybeFetchJerryPrompts()
{
    DBG("[prompts] maybeFetchJerryPrompts called");
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

    juce::URL promptsUrl(getServiceUrl(ServiceType::Jerry,
        audioProcessor.getIsUsingLocalhost() ? "/models/prompts" : "/audio/models/prompts"));
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
    const auto generationToken = beginGenerationAsyncWork();

    juce::Thread::launch([asyncAlive, editor, generationToken, fullPrompt, requestUrl, jerrySteps, jerryCfg, modelType,
                          isFinetune, finetuneRepo, finetuneCheckpoint, samplerType,
                          requestGenerateAsLoop, requestLoopType]() {
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

        const auto metadataRequest = generation_metadata::snapshot("jerry", "generate", juce::var(jsonRequest.get()));
        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        DBG("Jerry JSON payload: " + jsonString);

        juce::URL url(requestUrl);

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

                if (!editor->isGenerationAsyncWorkCurrent(generationToken)) return;
            editor->acceptGenerationSnapshot(generationToken, metadataRequest);
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

        juce::MessageManager::callAsync([asyncAlive, editor, generationToken, metadataRequest, responseText, statusCode, startTime]() {
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
                        auto audioBase64 = responseObj->getProperty("audio_base64").toString();

                        if (audioBase64.isNotEmpty())
                        {
                            if (editor->jerryUI)
                                editor->jerryUI->setGenerateButtonText("generate with jerry");

                            editor->saveGeneratedAudio(audioBase64, responseVar);

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
                }
            }
            else
            {
                juce::String errorMsg;
                bool shouldCheckHealth = false;

                if (statusCode == 0 && editor->audioProcessor.getIsUsingLocalhost())
                {
                    errorMsg = "cannot connect to jerry on localhost - ensure docker compose is running";
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

            if (editor->jerryUI)
                editor->jerryUI->setGenerateButtonText("generate with jerry");
            });
        });
}

void Gary4juceAudioProcessorEditor::updateSA3EnablementSnapshot()
{
    if (!sa3UI)
        return;

    const bool serviceReady = isServiceReachable(ServiceType::SA3);
    sa3UI->setMaximumDurationSeconds(getSA3MaximumDurationSeconds());
    sa3UI->setRemoteAvailable(serviceReady);
    sa3UI->setTransformAudioSourceRecording(transformRecording);
    sa3UI->setTransformAudioSourceAvailability(savedSamples > 0, hasOutputAudio);
    sa3UI->setContinueAudioSourceRecording(transformRecording);
    sa3UI->setContinueAudioSourceAvailability(savedSamples > 0, hasOutputAudio);

    const double recordingDuration = savedSamples > 0
        ? (double)savedSamples / juce::jmax(1.0, audioProcessor.getCurrentSampleRate())
        : 0.0;
    const bool selectedAudioAvailable = transformRecording ? savedSamples > 0 : hasOutputAudio;
    sa3UI->setContinueSourceDuration(
        transformRecording ? recordingDuration : totalAudioDuration,
        selectedAudioAvailable);

    bool canSubmit = false;
    if (sa3UI->getCurrentSubTab() == SA3UI::SubTab::Generate)
    {
        canSubmit = serviceReady;
    }
    else if (sa3UI->getCurrentSubTab() == SA3UI::SubTab::Transform)
    {
        canSubmit = serviceReady
            && selectedAudioAvailable;
    }
    else if (sa3UI->getCurrentSubTab() == SA3UI::SubTab::Continue)
    {
        canSubmit = serviceReady
            && selectedAudioAvailable
            && sa3UI->getContinueMaximumAddSeconds() >= 1;
    }

    sa3UI->setGenerateButtonEnabled(canSubmit, isGenerating);
    sa3UI->setDiceButtonsEnabled(serviceReady && !isGenerating);

    if (!isGenerating)
    {
        if (sa3UI->getCurrentSubTab() == SA3UI::SubTab::Continue)
            sa3UI->setGenerateButtonText("continue with sa3");
        else
            sa3UI->setGenerateButtonText(sa3UI->getCurrentSubTab() == SA3UI::SubTab::Transform
                ? "transform with sa3" : "generate with sa3");
    }

    if (!juce::JUCEApplicationBase::isStandaloneApp())
    {
        const double bpm = audioProcessor.getCurrentBPM();
        if (bpm > 0.0)
            sa3UI->setBpm(bpm);
    }
}

void Gary4juceAudioProcessorEditor::syncSA3LoraUi()
{
    if (!sa3UI)
        return;

    sa3UI->setAvailableLoras(availableSA3Loras);
    // The rows are new, and lanes restored before the catalog arrived had no LoRAs to match.
    syncSA3Lanes();
}

void Gary4juceAudioProcessorEditor::refreshSA3AvailableLoras(bool force)
{
    if (!sa3UI)
        return;

    if (!isServiceReachable(ServiceType::SA3))
    {
        if (audioProcessor.getIsUsingLocalhost())
        {
            availableSA3Loras.clear();
            syncSA3LoraUi();
        }
        return;
    }

    const juce::String loraUrl = getServiceUrl(ServiceType::SA3, "/loras");
    const auto now = juce::Time::getCurrentTime().toMilliseconds();
    constexpr juce::int64 remoteLoraTtlMs = 60 * 60 * 1000;
    constexpr juce::int64 localLoraTtlMs = 5 * 1000;
    const auto ttlMs = audioProcessor.getIsUsingLocalhost() ? localLoraTtlMs : remoteLoraTtlMs;

    if (!force
        && loraUrl == sa3LoraFetchBackendUrl
        && sa3LoraLastFetchMs > 0
        && now - sa3LoraLastFetchMs < ttlMs)
    {
        syncSA3LoraUi();
        return;
    }

    if (sa3LoraFetchInFlight.exchange(true))
        return;

    const int nonce = ++sa3LoraFetchNonce;
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, loraUrl, nonce]()
    {
        juce::String responseText;
        int statusCode = 0;

        try
        {
            juce::URL url(loraUrl);
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(8000)
                .withStatusCode(&statusCode)
                .withExtraHeaders("Accept: application/json");

            auto stream = url.createInputStream(options);
            if (stream)
                responseText = stream->readEntireStreamAsString();
        }
        catch (...) {}

        juce::MessageManager::callAsync([asyncAlive, editor, loraUrl, nonce, responseText, statusCode]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            editor->sa3LoraFetchInFlight.store(false);

            if (nonce != editor->sa3LoraFetchNonce.load())
                return;

            if (editor->getServiceUrl(ServiceType::SA3, "/loras") != loraUrl)
                return;

            bool responseOk = false;
            juce::StringArray loraNames;
            if (statusCode >= 200 && statusCode < 300 && responseText.isNotEmpty())
            {
                auto parsed = juce::JSON::parse(responseText);
                if (auto* obj = parsed.getDynamicObject())
                {
                    if (auto* loras = obj->getProperty("loras").getArray())
                    {
                        for (auto& item : *loras)
                        {
                            juce::String name;
                            if (auto* loraObj = item.getDynamicObject())
                                name = loraObj->getProperty("name").toString().trim();
                            else
                                name = item.toString().trim();

                            if (name.isNotEmpty())
                                loraNames.addIfNotAlreadyThere(name);
                        }
                        responseOk = true;
                    }
                }
            }
            else
            {
                DBG("[sa3] lora fetch failed status=" + juce::String(statusCode)
                    + " body=" + responseText.substring(0, 256));
            }

            if (responseOk)
            {
                editor->availableSA3Loras = loraNames;
                editor->sa3LoraFetchBackendUrl = loraUrl;
                editor->sa3LoraLastFetchMs = juce::Time::getCurrentTime().toMilliseconds();
            }
            editor->syncSA3LoraUi();
        });
    });
}

void Gary4juceAudioProcessorEditor::requestSA3DicePrompt(SA3UI::SubTab targetTab)
{
    if (!isServiceReachable(ServiceType::SA3))
    {
        showStatusMessage("sa3 not reachable - check connection first", 3000);
        return;
    }

    if (!sa3UI)
        return;

    juce::StringArray activeLoraNames;
    for (const auto& lora : sa3UI->getActiveLoras())
    {
        const auto name = lora.name.trim();
        if (name.isNotEmpty())
            activeLoraNames.addIfNotAlreadyThere(name);
    }

    const juce::String promptsBaseUrl = getServiceUrl(ServiceType::SA3, "/prompts");
    juce::URL promptsUrl(promptsBaseUrl);
    if (!activeLoraNames.isEmpty())
        promptsUrl = promptsUrl.withParameter("lora", activeLoraNames.joinIntoString(","));

    const int requestNonce = sa3PromptRequestNonce.fetch_add(1) + 1;
    const int activeLoraCount = activeLoraNames.size();

    showStatusMessage(activeLoraCount > 0 ? "rolling sa3 lora prompt..." : "rolling sa3 prompt...", 1500);

    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, requestNonce, targetTab, promptsBaseUrl, promptsUrl, activeLoraCount]()
    {
        juce::String responseText;
        int statusCode = 0;

        try
        {
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withHttpRequestCmd("GET")
                .withConnectionTimeoutMs(15000)
                .withStatusCode(&statusCode)
                .withExtraHeaders("Accept: application/json");

            auto stream = promptsUrl.createInputStream(options);
            if (stream)
                responseText = stream->readEntireStreamAsString();
        }
        catch (const std::exception& e)
        {
            DBG("[sa3] prompt roll failed: " + juce::String(e.what()));
        }
        catch (...)
        {
            DBG("[sa3] prompt roll failed");
        }

        juce::MessageManager::callAsync([asyncAlive, editor, requestNonce, targetTab,
                                         promptsBaseUrl, responseText, statusCode, activeLoraCount]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            if (editor->sa3PromptRequestNonce.load() != requestNonce)
                return;

            if (editor->getServiceUrl(ServiceType::SA3, "/prompts") != promptsBaseUrl)
                return;

            if (statusCode < 200 || statusCode >= 300 || responseText.isEmpty())
            {
                juce::String error = "failed to roll sa3 prompt";
                if (responseText.isNotEmpty())
                {
                    try
                    {
                        const auto responseVar = juce::JSON::parse(responseText);
                        const auto serverMessage = getSA3PromptErrorMessage(responseVar);
                        if (serverMessage.isNotEmpty())
                            error = serverMessage;
                    }
                    catch (...) {}
                }

                editor->showStatusMessage(error, 3500);
                DBG("[sa3] prompt roll failed status=" + juce::String(statusCode)
                    + " body=" + responseText.substring(0, 512));
                return;
            }

            juce::StringArray missingLoras;
            juce::String errorMessage;
            const auto prompt = pickSA3DicePrompt(responseText, missingLoras, errorMessage);
            if (prompt.isEmpty())
            {
                editor->showStatusMessage(errorMessage.isNotEmpty() ? errorMessage : "failed to roll sa3 prompt", 3500);
                DBG("[sa3] prompt roll parse failed body=" + responseText.substring(0, 512));
                return;
            }

            if (editor->sa3UI)
            {
                if (targetTab == SA3UI::SubTab::Transform)
                {
                    editor->currentSA3TransformPrompt = prompt;
                    editor->sa3UI->setTransformPromptText(prompt);
                }
                else if (targetTab == SA3UI::SubTab::Continue)
                {
                    editor->currentSA3ContinuePrompt = prompt;
                    editor->sa3UI->setContinuePromptText(prompt);
                }
                else
                {
                    editor->currentSA3Prompt = prompt;
                    editor->sa3UI->setPromptText(prompt);
                }
            }

            if (!missingLoras.isEmpty())
                editor->showStatusMessage("rolled sa3 prompt (missing " + juce::String(missingLoras.size()) + " lora pool)", 2500);
            else
                editor->showStatusMessage(activeLoraCount > 0 ? "rolled sa3 prompt from loras" : "rolled sa3 prompt", 1500);

            editor->updateSA3EnablementSnapshot();
        });
    });
}

void Gary4juceAudioProcessorEditor::sendToSA3()
{
    if (!isServiceReachable(ServiceType::SA3))
    {
        showStatusMessage("sa3 not reachable - check connection first", 3000);
        return;
    }

    if (!sa3UI)
        return;

    const juce::String prompt = currentSA3Prompt.trim();

    currentSA3DurationSeconds = juce::jlimit(
        1, getSA3MaximumDurationSeconds(), sa3UI->getDurationSeconds());
    currentSA3LoopEnabled = sa3UI->getLoopEnabled();
    currentSA3GenerateKeepsGoing = sa3UI->getGenerateKeepsGoing();
    currentSA3Bars = sa3UI->getBars();
    currentSA3Steps = sa3UI->getSteps();
    currentSA3Cfg = sa3UI->getCfgScale();
    currentSA3Shift = sa3UI->getShift();
    currentSA3KeyScale = sa3UI->getKeyScale();
    currentSA3NegativePrompt = sa3UI->getNegativePromptText();
    const juce::int64 requestSeed = sa3UI->getSeed();

    auto cancelOp = [this]()
    {
        isGenerating = false;
        isCurrentlyQueued = false;
        generationProgress = 0;
        smoothProgressAnimation = false;
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
        repaint();
    };

    double bpm = juce::JUCEApplicationBase::isStandaloneApp() && sa3UI
        ? sa3UI->getBpm() : audioProcessor.getCurrentBPM();
    if (bpm <= 0.0)
        bpm = 120.0;

    juce::String fullPrompt = prompt;
    if (fullPrompt.isNotEmpty())
        fullPrompt += ", ";
    fullPrompt += juce::String(juce::roundToInt(bpm)) + " bpm";
    if (currentSA3KeyScale.trim().isNotEmpty())
        fullPrompt += ", " + currentSA3KeyScale.trim();

    const bool requestLoop = currentSA3LoopEnabled;
    const juce::String endpoint = requestLoop ? "/generate/loop" : "/generate";
    const juce::String requestUrl = getServiceUrl(ServiceType::SA3, endpoint);

    juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
    jsonRequest->setProperty("prompt", fullPrompt);
    if (currentSA3NegativePrompt.isNotEmpty())
        jsonRequest->setProperty("negative_prompt", currentSA3NegativePrompt);
    jsonRequest->setProperty("steps", currentSA3Steps);
    jsonRequest->setProperty("cfg_scale", currentSA3Cfg);
    jsonRequest->setProperty("shift", currentSA3Shift.isNotEmpty() ? currentSA3Shift : "logsnr");
    jsonRequest->setProperty("seed", requestSeed);

    juce::Array<juce::var> loraEntries;
    for (const auto& lora : sa3UI->getActiveLoras())
    {
        juce::DynamicObject::Ptr loraObj = new juce::DynamicObject();
        loraObj->setProperty("name", lora.name);
        loraObj->setProperty("strength", lora.strength);
        loraObj->setProperty("interval_min", 0.0);
        loraObj->setProperty("interval_max", 1.0);
        loraEntries.add(juce::var(loraObj.get()));
    }
    jsonRequest->setProperty("loras", loraEntries);

    if (requestLoop)
        jsonRequest->setProperty("bars", currentSA3Bars);
    else
    {
        jsonRequest->setProperty("duration", currentSA3DurationSeconds);
        jsonRequest->setProperty("tail_pad", currentSA3GenerateKeepsGoing
            ? SA3UI::kKeepsGoingTailPadSeconds : SA3UI::kEndsHereTailPadSeconds);
    }

    const auto decorateLanePrompt = [bpm, keyScale = currentSA3KeyScale.trim()](const juce::String& lanePrompt)
    {
        auto text = lanePrompt + ", " + juce::String(juce::roundToInt(bpm)) + " bpm";
        return keyScale.isNotEmpty() ? text + ", " + keyScale : text;
    };
    // A loop's length comes from the tempo in its prompt, which is rounded there.
    const double laneSeconds = requestLoop
        ? 240.0 / juce::jmax(1, juce::roundToInt(bpm)) * currentSA3Bars
        : (double)currentSA3DurationSeconds;
    if (!addSA3LanesToRequest(*jsonRequest, 0.0, laneSeconds, decorateLanePrompt))
        return;
    sa3LaneTimelineStart = 0.0;

    const juce::String jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

    DBG("[sa3] submit " + endpoint + ": " + jsonString);

    setActiveOp(ActiveOp::SA3Generate);
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
    if (sa3UI)
        sa3UI->setGenerateButtonText("generating");
    repaint();

    showStatusMessage(requestLoop ? "submitting sa3 loop..." : "submitting sa3 request...", 2500);

    const auto generationToken = beginGenerationAsyncWork();
    captureGenerationRequest(generationToken, "sa3", "generate", juce::var(jsonRequest.get()));
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, generationToken, requestUrl, jsonString, requestLoop]()
    {
        juce::String responseText;
        int statusCode = 0;
        bool ok = false;

        try
        {
            juce::URL url(requestUrl);
            juce::URL postUrl = url.withPOSTData(jsonString);
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(15000)
                .withStatusCode(&statusCode)
                .withExtraHeaders("Content-Type: application/json");

            auto stream = postUrl.createInputStream(options);
            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();
                ok = statusCode >= 200 && statusCode < 300;
            }
        }
        catch (...) {}

        juce::MessageManager::callAsync([asyncAlive, editor, generationToken, responseText, statusCode, ok, requestLoop]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            if (!editor->isGenerationAsyncWorkCurrent(generationToken) || !editor->isGenerating)
                return;

            auto cancelSA3Operation = [editor]()
            {
                editor->isGenerating = false;
                editor->isCurrentlyQueued = false;
                editor->generationProgress = 0;
                editor->smoothProgressAnimation = false;
                editor->setActiveOp(ActiveOp::None);
                editor->updateAllGenerationButtonStates();
                editor->repaint();
            };

            if (!ok || responseText.isEmpty())
            {
                juce::String message = statusCode > 0
                    ? "sa3 submit failed (HTTP " + juce::String(statusCode) + ")"
                    : "sa3 backend not responding";
                editor->showStatusMessage(message, 4000);
                DBG("[sa3] submit failed: " + message + " body=" + responseText.substring(0, 512));
                cancelSA3Operation();
                return;
            }

            auto responseVar = juce::JSON::parse(responseText);
            auto* responseObj = responseVar.getDynamicObject();
            if (!responseObj)
            {
                editor->showStatusMessage("invalid response from sa3", 4000);
                cancelSA3Operation();
                return;
            }

            const bool success = responseObj->getProperty("success");
            if (!success)
            {
                juce::String error = responseObj->getProperty("error").toString();
                if (error.isEmpty())
                {
                    if (auto* errors = responseObj->getProperty("errors").getArray())
                    {
                        juce::StringArray parts;
                        for (auto& item : *errors)
                            parts.add(item.toString());
                        error = parts.joinIntoString(", ");
                    }
                }
                editor->showStatusMessage("sa3 error: " + (error.isEmpty() ? "unknown error" : error), 5000);
                DBG("[sa3] submit error: " + responseText.substring(0, 512));
                cancelSA3Operation();
                return;
            }

            const juce::String sessionId = responseObj->getProperty("session_id").toString();
            if (sessionId.isEmpty())
            {
                editor->showStatusMessage("sa3 response missing session id", 4000);
                cancelSA3Operation();
                return;
            }

            const juce::String usedSeed = responseObj->getProperty("seed").toString();
            editor->noteGenerationSeed("sa3", usedSeed);
            if (usedSeed.isNotEmpty() && editor->sa3UI)
                editor->sa3UI->setLastSeed(usedSeed);

            DBG("[sa3] session=" + sessionId);
            editor->showStatusMessage(requestLoop ? "sa3 loop generating..." : "sa3 generating...", 2000);
            editor->startPollingForResults(sessionId);
        });
    });
}

void Gary4juceAudioProcessorEditor::sendSA3Transform()
{
    if (!isServiceReachable(ServiceType::SA3))
    {
        showStatusMessage("sa3 not reachable - check connection first", 3000);
        return;
    }

    if (!sa3UI)
        return;

    const juce::String prompt = sa3UI->getTransformPromptText().trim();

    currentSA3TransformPrompt = prompt;
    currentSA3TransformStrength = juce::jlimit(0.01, 1.0, sa3UI->getTransformStrength());
    transformRecording = sa3UI->getTransformAudioSourceRecording();
    if (!validateAudioDurationForModel(transformRecording ? getGaryBufferFile() : getGaryOutputFile(),
                                      getSA3MaximumDurationSeconds(), "sa3"))
        return;
    audioProcessor.setTransformRecording(transformRecording);
    currentSA3Steps = sa3UI->getSteps();
    currentSA3Cfg = sa3UI->getCfgScale();
    currentSA3Shift = sa3UI->getShift();
    currentSA3KeyScale = sa3UI->getKeyScale();
    currentSA3NegativePrompt = sa3UI->getNegativePromptText();
    const juce::int64 requestSeed = sa3UI->getSeed();

    if (transformRecording)
    {
        if (savedSamples <= 0)
        {
            showStatusMessage("no recording available - save your recording first", 3000);
            updateSA3EnablementSnapshot();
            return;
        }
    }
    else if (!hasOutputAudio)
    {
        showStatusMessage("no output audio available - generate audio first", 3000);
        updateSA3EnablementSnapshot();
        return;
    }

    if (!ensureGaryDataDirectoryAvailable())
        return;
    const juce::File audioFile = transformRecording
        ? getGaryBufferFile()
        : getGaryOutputFile();

    if (!audioFile.existsAsFile())
    {
        showStatusMessage("audio file not found - " + audioFile.getFileName(), 3000);
        updateSA3EnablementSnapshot();
        return;
    }

    juce::MemoryBlock audioData;
    if (!audioFile.loadFileAsData(audioData) || audioData.getSize() == 0)
    {
        showStatusMessage("failed to read audio file", 3000);
        updateSA3EnablementSnapshot();
        return;
    }

    const auto base64Audio = juce::Base64::toBase64(audioData.getData(), audioData.getSize());

    double bpm = juce::JUCEApplicationBase::isStandaloneApp() && sa3UI
        ? sa3UI->getBpm() : audioProcessor.getCurrentBPM();
    if (bpm <= 0.0)
        bpm = 120.0;

    juce::String fullPrompt = prompt;
    if (fullPrompt.isNotEmpty())
        fullPrompt += ", ";
    fullPrompt += juce::String(juce::roundToInt(bpm)) + " bpm";
    if (currentSA3KeyScale.trim().isNotEmpty())
        fullPrompt += ", " + currentSA3KeyScale.trim();

    juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
    jsonRequest->setProperty("prompt", fullPrompt);
    if (currentSA3NegativePrompt.isNotEmpty())
        jsonRequest->setProperty("negative_prompt", currentSA3NegativePrompt);
    jsonRequest->setProperty("steps", currentSA3Steps);
    jsonRequest->setProperty("cfg_scale", currentSA3Cfg);
    jsonRequest->setProperty("shift", currentSA3Shift.isNotEmpty() ? currentSA3Shift : "logsnr");
    jsonRequest->setProperty("seed", requestSeed);
    jsonRequest->setProperty("audio_data", base64Audio);
    jsonRequest->setProperty("strength", currentSA3TransformStrength);

    juce::Array<juce::var> loraEntries;
    for (const auto& lora : sa3UI->getActiveLoras())
    {
        juce::DynamicObject::Ptr loraObj = new juce::DynamicObject();
        loraObj->setProperty("name", lora.name);
        loraObj->setProperty("strength", lora.strength);
        loraObj->setProperty("interval_min", 0.0);
        loraObj->setProperty("interval_max", 1.0);
        loraEntries.add(juce::var(loraObj.get()));
    }
    jsonRequest->setProperty("loras", loraEntries);

    const auto decorateLanePrompt = [bpm, keyScale = currentSA3KeyScale.trim()](const juce::String& lanePrompt)
    {
        auto text = lanePrompt + ", " + juce::String(juce::roundToInt(bpm)) + " bpm";
        return keyScale.isNotEmpty() ? text + ", " + keyScale : text;
    };
    const double laneSeconds = transformRecording
        ? (double)savedSamples / juce::jmax(1.0, audioProcessor.getCurrentSampleRate())
        : totalAudioDuration;
    if (!addSA3LanesToRequest(*jsonRequest, 0.0, laneSeconds, decorateLanePrompt))
        return;
    sa3LaneTimelineStart = 0.0;

    const juce::String jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));
    const juce::String requestUrl = getServiceUrl(ServiceType::SA3, "/transform");

    DBG("[sa3] submit /transform from " + juce::String(transformRecording ? "recording" : "output")
        + " bytes=" + juce::String((int)audioData.getSize()));

    setActiveOp(ActiveOp::SA3Transform);
    isGenerating = true;
    isCurrentlyQueued = true;
    generationProgress = 0;
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;
    audioProcessor.clearCurrentSessionId();
    audioProcessor.setUndoTransformAvailable(false);
    audioProcessor.setRetryAvailable(false);
    updateRetryButtonState();
    updateContinueButtonState();
    updateAllGenerationButtonStates();
    if (sa3UI)
        sa3UI->setGenerateButtonText("transforming");
    repaint();

    showStatusMessage("submitting sa3 transform...", 2500);

    const auto generationToken = beginGenerationAsyncWork();
    captureGenerationRequest(generationToken, "sa3", "transform", juce::var(jsonRequest.get()));
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, generationToken, requestUrl, jsonString]()
    {
        juce::String responseText;
        int statusCode = 0;
        bool ok = false;

        try
        {
            juce::URL url(requestUrl);
            juce::URL postUrl = url.withPOSTData(jsonString);
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(15000)
                .withStatusCode(&statusCode)
                .withExtraHeaders("Content-Type: application/json");

            auto stream = postUrl.createInputStream(options);
            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();
                ok = statusCode >= 200 && statusCode < 300;
            }
        }
        catch (...) {}

        juce::MessageManager::callAsync([asyncAlive, editor, generationToken, responseText, statusCode, ok]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            if (!editor->isGenerationAsyncWorkCurrent(generationToken) || !editor->isGenerating)
                return;

            auto cancelSA3Operation = [editor]()
            {
                editor->isGenerating = false;
                editor->isCurrentlyQueued = false;
                editor->generationProgress = 0;
                editor->smoothProgressAnimation = false;
                editor->setActiveOp(ActiveOp::None);
                editor->updateAllGenerationButtonStates();
                editor->repaint();
            };

            if (!ok || responseText.isEmpty())
            {
                juce::String message = statusCode > 0
                    ? "sa3 transform failed (HTTP " + juce::String(statusCode) + ")"
                    : "sa3 backend not responding";
                editor->showStatusMessage(message, 4000);
                DBG("[sa3] transform submit failed: " + message + " body=" + responseText.substring(0, 512));
                cancelSA3Operation();
                return;
            }

            auto responseVar = juce::JSON::parse(responseText);
            auto* responseObj = responseVar.getDynamicObject();
            if (!responseObj)
            {
                editor->showStatusMessage("invalid response from sa3 transform", 4000);
                cancelSA3Operation();
                return;
            }

            const bool success = responseObj->getProperty("success");
            if (!success)
            {
                juce::String error = responseObj->getProperty("error").toString();
                if (error.isEmpty())
                {
                    if (auto* errors = responseObj->getProperty("errors").getArray())
                    {
                        juce::StringArray parts;
                        for (auto& item : *errors)
                            parts.add(item.toString());
                        error = parts.joinIntoString(", ");
                    }
                }
                editor->showStatusMessage("sa3 transform error: " + (error.isEmpty() ? "unknown error" : error), 5000);
                DBG("[sa3] transform submit error: " + responseText.substring(0, 512));
                cancelSA3Operation();
                return;
            }

            const juce::String sessionId = responseObj->getProperty("session_id").toString();
            if (sessionId.isEmpty())
            {
                editor->showStatusMessage("sa3 transform response missing session id", 4000);
                cancelSA3Operation();
                return;
            }

            const juce::String usedSeed = responseObj->getProperty("seed").toString();
            editor->noteGenerationSeed("sa3", usedSeed);
            if (usedSeed.isNotEmpty() && editor->sa3UI)
                editor->sa3UI->setLastSeed(usedSeed);

            DBG("[sa3] transform session=" + sessionId);
            editor->showStatusMessage("sa3 transforming...", 2000);
            editor->startPollingForResults(sessionId);
        });
    });
}

void Gary4juceAudioProcessorEditor::sendSA3Continue()
{
    if (!isServiceReachable(ServiceType::SA3))
    {
        showStatusMessage("sa3 not reachable - check connection first", 3000);
        return;
    }

    if (!sa3UI)
        return;

    const juce::String prompt = sa3UI->getContinuePromptText().trim();

    currentSA3ContinuePrompt = prompt;
    currentSA3ContinueAddSeconds = juce::jlimit(
        0, getSA3MaximumDurationSeconds(), sa3UI->getContinueAddSeconds());
    transformRecording = sa3UI->getContinueAudioSourceRecording();
    audioProcessor.setTransformRecording(transformRecording);
    currentSA3Steps = sa3UI->getSteps();
    currentSA3Cfg = sa3UI->getCfgScale();
    currentSA3Shift = sa3UI->getShift();
    currentSA3KeyScale = sa3UI->getKeyScale();
    currentSA3NegativePrompt = sa3UI->getNegativePromptText();
    currentSA3ContinueLatentPrefix = sa3UI->getContinueLatentPrefixEnabled();
    currentSA3ContinueKeepsGoing = sa3UI->getContinueKeepsGoing();
    const juce::int64 requestSeed = sa3UI->getSeed();

    if (transformRecording)
    {
        if (savedSamples <= 0)
        {
            showStatusMessage("no recording available - save your recording first", 3000);
            updateSA3EnablementSnapshot();
            return;
        }
    }
    else if (!hasOutputAudio)
    {
        showStatusMessage("no output audio available - generate audio first", 3000);
        updateSA3EnablementSnapshot();
        return;
    }

    const double sourceSeconds = transformRecording
        ? (double)savedSamples / juce::jmax(1.0, audioProcessor.getCurrentSampleRate())
        : totalAudioDuration;
    const double continuationSecondsForRequest = (double)currentSA3ContinueAddSeconds;
    const double requestedTotalSeconds = sourceSeconds + continuationSecondsForRequest;
    if (sourceSeconds <= 0.0 || continuationSecondsForRequest <= 0.0)
    {
        showStatusMessage("choose at least one second of audio to add", 4000);
        updateSA3EnablementSnapshot();
        return;
    }
    if (requestedTotalSeconds > (double)getSA3MaximumDurationSeconds())
    {
        showStatusMessage("total continuation output must stay at or below "
            + juce::String(getSA3MaximumDurationSeconds()) + " seconds", 4000);
        updateSA3EnablementSnapshot();
        return;
    }

    if (!ensureGaryDataDirectoryAvailable())
        return;
    const juce::File audioFile = transformRecording
        ? getGaryBufferFile()
        : getGaryOutputFile();

    if (!audioFile.existsAsFile())
    {
        showStatusMessage("audio file not found - " + audioFile.getFileName(), 3000);
        updateSA3EnablementSnapshot();
        return;
    }

    juce::MemoryBlock audioData;
    if (!audioFile.loadFileAsData(audioData) || audioData.getSize() == 0)
    {
        showStatusMessage("failed to read audio file", 3000);
        updateSA3EnablementSnapshot();
        return;
    }

    const auto base64Audio = juce::Base64::toBase64(audioData.getData(), audioData.getSize());
    const juce::String continuationMode = currentSA3ContinueLatentPrefix ? "latent_prefix" : "inpaint";

    double bpm = juce::JUCEApplicationBase::isStandaloneApp() && sa3UI
        ? sa3UI->getBpm() : audioProcessor.getCurrentBPM();
    if (bpm <= 0.0)
        bpm = 120.0;

    juce::String fullPrompt = prompt;
    if (fullPrompt.isNotEmpty())
        fullPrompt += ", ";
    fullPrompt += juce::String(juce::roundToInt(bpm)) + " bpm";
    if (currentSA3KeyScale.trim().isNotEmpty())
        fullPrompt += ", " + currentSA3KeyScale.trim();

    juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
    jsonRequest->setProperty("prompt", fullPrompt);
    if (currentSA3NegativePrompt.isNotEmpty())
        jsonRequest->setProperty("negative_prompt", currentSA3NegativePrompt);
    jsonRequest->setProperty("steps", currentSA3Steps);
    jsonRequest->setProperty("cfg_scale", currentSA3Cfg);
    jsonRequest->setProperty("shift", currentSA3Shift.isNotEmpty() ? currentSA3Shift : "logsnr");
    jsonRequest->setProperty("seed", requestSeed);
    jsonRequest->setProperty("audio_data", base64Audio);
    jsonRequest->setProperty("continuation_seconds", continuationSecondsForRequest);
    jsonRequest->setProperty("continuation_mode", continuationMode);
    jsonRequest->setProperty("continuation_tail_pad", currentSA3ContinueKeepsGoing
        ? SA3UI::kKeepsGoingTailPadSeconds : SA3UI::kEndsHereTailPadSeconds);

    juce::Array<juce::var> loraEntries;
    for (const auto& lora : sa3UI->getActiveLoras())
    {
        juce::DynamicObject::Ptr loraObj = new juce::DynamicObject();
        loraObj->setProperty("name", lora.name);
        loraObj->setProperty("strength", lora.strength);
        loraObj->setProperty("interval_min", 0.0);
        loraObj->setProperty("interval_max", 1.0);
        loraEntries.add(juce::var(loraObj.get()));
    }
    jsonRequest->setProperty("loras", loraEntries);

    const auto decorateLanePrompt = [bpm, keyScale = currentSA3KeyScale.trim()](const juce::String& lanePrompt)
    {
        auto text = lanePrompt + ", " + juce::String(juce::roundToInt(bpm)) + " bpm";
        return keyScale.isNotEmpty() ? text + ", " + keyScale : text;
    };
    // Lanes cover what's added. Their times count from the start of the source, which the
    // backend keeps whole, so they begin where it ends.
    if (!addSA3LanesToRequest(*jsonRequest, sourceSeconds, continuationSecondsForRequest, decorateLanePrompt))
        return;
    sa3LaneTimelineStart = sourceSeconds;

    const juce::String jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));
    const juce::String requestUrl = getServiceUrl(ServiceType::SA3, "/continue");

    DBG("[sa3] submit /continue from " + juce::String(transformRecording ? "recording" : "output")
        + " bytes=" + juce::String((int)audioData.getSize())
        + " target=" + juce::String(requestedTotalSeconds, 2) + "s"
        + " continuation=" + juce::String(continuationSecondsForRequest, 2) + "s"
        + " mode=" + continuationMode);

    setActiveOp(ActiveOp::SA3Continue);
    isGenerating = true;
    isCurrentlyQueued = true;
    generationProgress = 0;
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;
    audioProcessor.clearCurrentSessionId();
    audioProcessor.setUndoTransformAvailable(false);
    audioProcessor.setRetryAvailable(false);
    updateRetryButtonState();
    updateContinueButtonState();
    updateAllGenerationButtonStates();
    if (sa3UI)
        sa3UI->setGenerateButtonText("continuing");
    repaint();

    showStatusMessage("submitting sa3 continuation...", 2500);

    const auto generationToken = beginGenerationAsyncWork();
    captureGenerationRequest(generationToken, "sa3", "continue", juce::var(jsonRequest.get()));
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, generationToken, requestUrl, jsonString]()
    {
        juce::String responseText;
        int statusCode = 0;
        bool ok = false;

        try
        {
            juce::URL url(requestUrl);
            juce::URL postUrl = url.withPOSTData(jsonString);
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(15000)
                .withStatusCode(&statusCode)
                .withExtraHeaders("Content-Type: application/json");

            auto stream = postUrl.createInputStream(options);
            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();
                ok = statusCode >= 200 && statusCode < 300;
            }
        }
        catch (...) {}

        juce::MessageManager::callAsync([asyncAlive, editor, generationToken, responseText, statusCode, ok]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            if (!editor->isGenerationAsyncWorkCurrent(generationToken) || !editor->isGenerating)
                return;

            auto cancelSA3Operation = [editor]()
            {
                editor->isGenerating = false;
                editor->isCurrentlyQueued = false;
                editor->generationProgress = 0;
                editor->smoothProgressAnimation = false;
                editor->setActiveOp(ActiveOp::None);
                editor->updateAllGenerationButtonStates();
                editor->repaint();
            };

            if (!ok || responseText.isEmpty())
            {
                juce::String message = statusCode > 0
                    ? "sa3 continue failed (HTTP " + juce::String(statusCode) + ")"
                    : "sa3 backend not responding";
                editor->showStatusMessage(message, 4000);
                DBG("[sa3] continue submit failed: " + message + " body=" + responseText.substring(0, 512));
                cancelSA3Operation();
                return;
            }

            auto responseVar = juce::JSON::parse(responseText);
            auto* responseObj = responseVar.getDynamicObject();
            if (!responseObj)
            {
                editor->showStatusMessage("invalid response from sa3 continue", 4000);
                cancelSA3Operation();
                return;
            }

            const bool success = responseObj->getProperty("success");
            if (!success)
            {
                juce::String error = responseObj->getProperty("error").toString();
                if (error.isEmpty())
                {
                    if (auto* errors = responseObj->getProperty("errors").getArray())
                    {
                        juce::StringArray parts;
                        for (auto& item : *errors)
                            parts.add(item.toString());
                        error = parts.joinIntoString(", ");
                    }
                }
                editor->showStatusMessage("sa3 continue error: " + (error.isEmpty() ? "unknown error" : error), 5000);
                DBG("[sa3] continue submit error: " + responseText.substring(0, 512));
                cancelSA3Operation();
                return;
            }

            const juce::String sessionId = responseObj->getProperty("session_id").toString();
            if (sessionId.isEmpty())
            {
                editor->showStatusMessage("sa3 continue response missing session id", 4000);
                cancelSA3Operation();
                return;
            }

            const juce::String usedSeed = responseObj->getProperty("seed").toString();
            editor->noteGenerationSeed("sa3", usedSeed);
            if (usedSeed.isNotEmpty() && editor->sa3UI)
                editor->sa3UI->setLastSeed(usedSeed);

            DBG("[sa3] continue session=" + sessionId);
            editor->showStatusMessage("sa3 continuing...", 2000);
            editor->startPollingForResults(sessionId);
        });
    });
}

//==============================================================================
// SA3 LoRA automation lanes

bool Gary4juceAudioProcessorEditor::sa3LanePanelShown() const
{
    return sa3Lanes.open && sa3LaneEditor != nullptr
        && currentTab == ModelTab::Jerry && jerrySubTab == JerrySubTab::SA3;
}

void Gary4juceAudioProcessorEditor::syncSA3Lanes()
{
    // Only while automation is open: a LoRA turned on with it closed gets a flat lane when it opens.
    if (sa3Lanes.open && sa3UI != nullptr && sa3LaneEditor != nullptr)
    {
        sa3lanes::sync(sa3Lanes.lanes, sa3UI->getActiveLoras());
        sa3LaneEditor->repaint();
    }
    updateSA3LoraAutomation();
}

void Gary4juceAudioProcessorEditor::updateSA3LoraAutomation()
{
    if (sa3UI == nullptr)
        return;
    std::vector<SA3UI::LoraAutomation> automated;
    if (sa3Lanes.open)
    {
        const auto active = sa3UI->getActiveLoras();
        for (size_t i = 0; i < sa3Lanes.lanes.size(); ++i)
        {
            auto& lane = sa3Lanes.lanes[i];
            if (!sa3lanes::isActive(lane, active))
                continue;
            if (!sa3lanes::isDrawn(lane))
            {
                float amount = 0.f;
                sa3lanes::flatAmount(lane, amount);
                const auto landed = sa3UI->setLoraStrength(juce::String(lane.path), amount);
                if (landed > 0.0)
                    for (auto& point : lane.points)
                        point.amount = (float)landed;
                continue;
            }
            SA3UI::LoraAutomation entry;
            entry.name = juce::String(lane.path);
            entry.colour = SA3LaneEditor::laneColour((int)i);
            for (const auto& point : lane.points)
                entry.curve.push_back({ point.position, point.amount / gary::kSA3MaxLaneAmount });
            automated.push_back(std::move(entry));
        }
    }
    sa3UI->setLoraAutomation(std::move(automated));
}

// What the next render covers, as its request will say: a continuation's added seconds, a
// transform's source, or a generation's duration or loop.
double Gary4juceAudioProcessorEditor::sa3LaneRangeSeconds() const
{
    if (sa3UI == nullptr)
        return 0.0;
    switch (sa3UI->getCurrentSubTab())
    {
        case SA3UI::SubTab::Continue:
            return (double)sa3UI->getContinueAddSeconds();
        case SA3UI::SubTab::Transform:
            return sa3UI->getTransformAudioSourceRecording()
                ? (double)savedSamples / juce::jmax(1.0, audioProcessor.getCurrentSampleRate())
                : (hasOutputAudio ? totalAudioDuration : 0.0);
        case SA3UI::SubTab::Generate:
            break;
    }
    if (sa3UI->getLoopEnabled())
    {
        double bpm = juce::JUCEApplicationBase::isStandaloneApp() ? sa3UI->getBpm() : audioProcessor.getCurrentBPM();
        if (bpm <= 0.0)
            bpm = 120.0;
        return 240.0 / juce::jmax(1, juce::roundToInt(bpm)) * sa3UI->getBars();
    }
    return (double)sa3UI->getDurationSeconds();
}

void Gary4juceAudioProcessorEditor::updateSA3LaneTimeline()
{
    if (sa3LaneEditor == nullptr || sa3UI == nullptr)
        return;
    double bpm = juce::JUCEApplicationBase::isStandaloneApp() ? sa3UI->getBpm() : audioProcessor.getCurrentBPM();
    if (bpm <= 0.0)
        bpm = 120.0;
    // SA3's loops are 4/4, and its prompts carry no other meter.
    sa3LaneEditor->setTimeline(sa3LaneRangeSeconds(), bpm, 4.0);
    sa3LaneEditor->setSteps(sa3UI->getSteps());
    // Mirrors the output waveform's cursor. The curves start where the last SA3 render's did,
    // which in a continuation is after the source it plays first.
    const bool playing = isPlayingOutput && activePlaybackSource == PlaybackSource::Output;
    sa3LaneEditor->setPlayhead(playing ? currentPlaybackPosition - sa3LaneTimelineStart : -1.0);
}

void Gary4juceAudioProcessorEditor::updateSA3LaneBackdrop()
{
    if (sa3LaneEditor == nullptr)
        return;
    sa3LaneEditor->setBackdrop(hasOutputAudio
        ? sa3lanes::peaks(outputAudioBuffer, currentAudioSampleRate, sa3LaneTimelineStart)
        : std::vector<float>());
}

void Gary4juceAudioProcessorEditor::rollSA3LanePrompt(size_t laneIndex)
{
    if (laneIndex >= sa3Lanes.lanes.size())
        return;
    if (!isServiceReachable(ServiceType::SA3))
    {
        showStatusMessage("sa3 not reachable - check connection first", 3000);
        return;
    }

    const juce::String loraName(sa3Lanes.lanes[laneIndex].path);
    const juce::String promptsBaseUrl = getServiceUrl(ServiceType::SA3, "/prompts");
    const juce::URL promptsUrl = juce::URL(promptsBaseUrl).withParameter("lora", loraName);
    showStatusMessage("rolling a prompt for " + loraName + "...", 1500);

    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;
    juce::Thread::launch([asyncAlive, editor, promptsBaseUrl, promptsUrl, loraName]()
    {
        juce::String responseText;
        int statusCode = 0;
        try
        {
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withHttpRequestCmd("GET")
                .withConnectionTimeoutMs(15000)
                .withStatusCode(&statusCode)
                .withExtraHeaders("Accept: application/json");
            if (auto stream = promptsUrl.createInputStream(options))
                responseText = stream->readEntireStreamAsString();
        }
        catch (...) {}

        juce::MessageManager::callAsync([asyncAlive, editor, promptsBaseUrl, loraName, responseText, statusCode]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;
            if (editor->getServiceUrl(ServiceType::SA3, "/prompts") != promptsBaseUrl)
                return;

            juce::StringArray missingLoras;
            juce::String errorMessage;
            const auto prompt = statusCode >= 200 && statusCode < 300
                ? pickSA3DicePrompt(responseText, missingLoras, errorMessage) : juce::String();
            if (prompt.isEmpty() || missingLoras.contains(loraName))
            {
                editor->showStatusMessage(missingLoras.contains(loraName)
                    ? "no prompt pool for " + loraName
                    : (errorMessage.isNotEmpty() ? errorMessage : juce::String("failed to roll a prompt")), 3000);
                return;
            }
            // Found by name: lanes can be removed or reordered while this was out.
            for (auto& lane : editor->sa3Lanes.lanes)
            {
                if (juce::String(lane.path) == loraName)
                {
                    lane.prompt = prompt.toStdString();
                    if (editor->sa3LaneEditor != nullptr)
                        editor->sa3LaneEditor->repaint();
                    editor->persistEditorState();
                    break;
                }
            }
        });
    });
}

// Asks the SA3 backend at healthUrl whether it runs lanes. Localhost goes through gary4local's
// adapter, whose 0.7.0 release passed sa3.cpp's flag through but dropped the lanes themselves,
// so it must also say it forwards them.
void Gary4juceAudioProcessorEditor::checkSA3LaneSupport(const juce::String& healthUrl)
{
    if (healthUrl == sa3LanesCheckedUrl || sa3LaneCheckInFlight.exchange(true))
        return;
    const bool viaGary4local = audioProcessor.getIsUsingLocalhost();
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;
    juce::Thread::launch([asyncAlive, editor, healthUrl, viaGary4local]()
    {
        juce::String responseText;
        int statusCode = 0;
        try
        {
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(8000)
                .withStatusCode(&statusCode)
                .withExtraHeaders("Accept: application/json");
            if (auto stream = juce::URL(healthUrl).createInputStream(options))
                responseText = stream->readEntireStreamAsString();
        }
        catch (...) {}

        juce::MessageManager::callAsync([asyncAlive, editor, healthUrl, viaGary4local, responseText, statusCode]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;
            editor->sa3LaneCheckInFlight.store(false);
            if (statusCode < 200 || statusCode >= 300)
                return;   // asked again when the panel next opens or a render needs the answer
            const auto health = juce::JSON::parse(responseText);
            bool supported = false;
            if (auto* object = health.getDynamicObject())
            {
                auto* capabilities = object->getProperty("capabilities").getDynamicObject();
                supported = capabilities != nullptr && (bool)capabilities->getProperty("lora_lanes");
                if (viaGary4local)
                    supported = supported && (bool)object->getProperty("lora_lanes_forwarded");
            }
            editor->sa3LanesSupported = supported;
            editor->sa3LanesCheckedUrl = healthUrl;
        });
    });
}

bool Gary4juceAudioProcessorEditor::addSA3LanesToRequest(
    juce::DynamicObject& request, double startSeconds, double seconds,
    const std::function<juce::String(const juce::String&)>& decoratePrompt)
{
    if (!sa3Lanes.open || sa3UI == nullptr)
        return true;
    syncSA3Lanes();
    const auto active = sa3UI->getActiveLoras();
    const bool anyLane = std::any_of(sa3Lanes.lanes.begin(), sa3Lanes.lanes.end(),
        [&active](const gary::SA3Lane& lane) { return sa3lanes::isActive(lane, active); });
    if (!anyLane)
        return true;

    const auto healthUrl = getServiceUrl(ServiceType::SA3, "/health");
    if (healthUrl != sa3LanesCheckedUrl)
    {
        checkSA3LaneSupport(healthUrl);
        showStatusMessage("checking whether sa3 can run automation lanes - try again in a moment", 3000);
        return false;
    }
    if (!sa3LanesSupported)
    {
        showStatusMessage(audioProcessor.getIsUsingLocalhost()
            ? "automation lanes need a newer gary4local - update it, or close automation to render"
            : "this sa3 backend can't run automation lanes - close automation to render", 4500);
        return false;
    }
    if (seconds <= 0.0)
    {
        showStatusMessage("automation needs a range to draw over", 3000);
        return false;
    }
    sa3lanes::addToRequest(request, sa3Lanes, active, decoratePrompt, startSeconds, seconds);
    return true;
}
