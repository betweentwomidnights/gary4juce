/*
  ==============================================================================
    Jerry-specific PluginEditor method definitions.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    if (jerryUI) jerryUI->setVisibleForTab(showSAOS);
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

    resized();
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

void Gary4juceAudioProcessorEditor::fetchJerryAvailableModels()
{
    if (!isServiceReachable(ServiceType::Jerry))
    {
        DBG("Jerry not reachable - skipping model fetch");
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

        if (jerryUI && modelNames.size() > 0)
        {
            const juce::String desiredKey = preferredJerryModelKey;
            const juce::String desiredRepo = preferredJerryFinetuneRepo;
            const juce::String desiredCheckpoint = preferredJerryFinetuneCheckpoint;
            const juce::String desiredSampler = currentJerrySamplerType;

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

            if (desiredIndex >= 0)
            {
                jerryUI->setSelectedModel(desiredIndex);
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
            }

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

    juce::Thread::launch([asyncAlive, editor, fullPrompt, requestUrl, jerrySteps, jerryCfg, modelType,
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

        juce::MessageManager::callAsync([asyncAlive, editor, responseText, statusCode, startTime]() {
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
    sa3UI->setRemoteAvailable(serviceReady);
    sa3UI->setTransformAudioSourceRecording(transformRecording);
    sa3UI->setTransformAudioSourceAvailability(savedSamples > 0, hasOutputAudio);
    sa3UI->setContinueAudioSourceRecording(transformRecording);
    sa3UI->setContinueAudioSourceAvailability(savedSamples > 0, hasOutputAudio);

    bool canSubmit = false;
    if (sa3UI->getCurrentSubTab() == SA3UI::SubTab::Generate)
    {
        canSubmit = serviceReady;
    }
    else if (sa3UI->getCurrentSubTab() == SA3UI::SubTab::Transform)
    {
        const bool selectedAudioAvailable = transformRecording ? savedSamples > 0 : hasOutputAudio;
        canSubmit = serviceReady
            && selectedAudioAvailable;
    }
    else if (sa3UI->getCurrentSubTab() == SA3UI::SubTab::Continue)
    {
        const bool selectedAudioAvailable = transformRecording ? savedSamples > 0 : hasOutputAudio;
        canSubmit = serviceReady
            && selectedAudioAvailable;
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

    const double bpm = audioProcessor.getCurrentBPM();
    if (bpm > 0.0)
        sa3UI->setBpm(bpm);
}

void Gary4juceAudioProcessorEditor::syncSA3LoraUi()
{
    if (!sa3UI)
        return;

    sa3UI->setAvailableLoras(availableSA3Loras);
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

    currentSA3DurationSeconds = sa3UI->getDurationSeconds();
    currentSA3LoopEnabled = sa3UI->getLoopEnabled();
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

    double bpm = audioProcessor.getCurrentBPM();
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
    jsonRequest->setProperty("shift", currentSA3Shift.isNotEmpty() ? currentSA3Shift : "full");
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
        jsonRequest->setProperty("duration", currentSA3DurationSeconds);

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

    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");
    const juce::File audioFile = transformRecording
        ? garyDir.getChildFile("myBuffer.wav")
        : garyDir.getChildFile("myOutput.wav");

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

    double bpm = audioProcessor.getCurrentBPM();
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
    jsonRequest->setProperty("shift", currentSA3Shift.isNotEmpty() ? currentSA3Shift : "full");
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
    currentSA3ContinueTotalSeconds = juce::jlimit(1, 300, sa3UI->getContinueTotalSeconds());
    transformRecording = sa3UI->getContinueAudioSourceRecording();
    audioProcessor.setTransformRecording(transformRecording);
    currentSA3Steps = sa3UI->getSteps();
    currentSA3Cfg = sa3UI->getCfgScale();
    currentSA3Shift = sa3UI->getShift();
    currentSA3KeyScale = sa3UI->getKeyScale();
    currentSA3NegativePrompt = sa3UI->getNegativePromptText();
    currentSA3ContinueLatentPrefix = sa3UI->getContinueLatentPrefixEnabled();
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
    const double requestedTotalSeconds = (double)currentSA3ContinueTotalSeconds;
    const double continuationSecondsForRequest = requestedTotalSeconds - sourceSeconds;
    if (sourceSeconds <= 0.0 || continuationSecondsForRequest <= 0.0)
    {
        showStatusMessage("duration must be longer than the source audio", 4000);
        updateSA3EnablementSnapshot();
        return;
    }
    if (requestedTotalSeconds > 300.0)
    {
        showStatusMessage("total continuation output must stay under 300 seconds", 4000);
        updateSA3EnablementSnapshot();
        return;
    }

    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");
    const juce::File audioFile = transformRecording
        ? garyDir.getChildFile("myBuffer.wav")
        : garyDir.getChildFile("myOutput.wav");

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

    double bpm = audioProcessor.getCurrentBPM();
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
    jsonRequest->setProperty("shift", currentSA3Shift.isNotEmpty() ? currentSA3Shift : "full");
    jsonRequest->setProperty("seed", requestSeed);
    jsonRequest->setProperty("audio_data", base64Audio);
    jsonRequest->setProperty("continuation_seconds", continuationSecondsForRequest);
    jsonRequest->setProperty("continuation_mode", continuationMode);

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
            if (usedSeed.isNotEmpty() && editor->sa3UI)
                editor->sa3UI->setLastSeed(usedSeed);

            DBG("[sa3] continue session=" + sessionId);
            editor->showStatusMessage("sa3 continuing...", 2000);
            editor->startPollingForResults(sessionId);
        });
    });
}
