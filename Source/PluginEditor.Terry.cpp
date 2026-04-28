/*
  ==============================================================================
    Terry-specific PluginEditor method definitions.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginEditorTerryHelpers.h"

using plugin_editor_detail::getTerryVariationNames;

void Gary4juceAudioProcessorEditor::setTerryAudioSource(bool useRecording)
{
    transformRecording = useRecording;

    // Persist this state in processor so rest of app is aware
    audioProcessor.setTransformRecording(useRecording);

    DBG("Terry audio source set to: " + juce::String(useRecording ? "Recording" : "Output"));
    if (terryUI)
        terryUI->setAudioSourceRecording(useRecording);

    updateTerryEnablementSnapshot();
}

void Gary4juceAudioProcessorEditor::updateTerryEnablementSnapshot()
{
    if (!terryUI)
        return;

    const bool recordingAvailable = savedSamples > 0;
    const bool outputAvailable = hasOutputAudio;
    terryUI->setAudioSourceAvailability(recordingAvailable, outputAvailable);

    const bool hasVariation = currentTerryVariation >= 0;
    const bool hasCustomPrompt = !currentTerryCustomPrompt.trim().isEmpty();

    bool canTransform = false;
    if (transformRecording)
        canTransform = recordingAvailable;
    else
        canTransform = outputAvailable;

    canTransform = canTransform && isServiceReachable(ServiceType::Terry) && (hasVariation || hasCustomPrompt);

    const bool undoAvailable = audioProcessor.getUndoTransformAvailable() &&
        !audioProcessor.getCurrentSessionId().isEmpty();

    terryUI->setButtonsEnabled(canTransform, isGenerating, undoAvailable);

    if (!isGenerating)
    {
        terryUI->setTransformButtonText("transform with terry");
        terryUI->setUndoButtonText("undo transform");
    }
}

void Gary4juceAudioProcessorEditor::sendToTerry()
{
    setActiveOp(ActiveOp::TerryTransform);

    auto cancelTerryOperation = [this]() {
        isGenerating = false;
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
    };

    // Reset generation state immediately (like Gary)
    isGenerating = true;
    generationProgress = 0;
    lastKnownProgress = 0;
    resetStallDetection();
    targetProgress = 0;
    smoothProgressAnimation = false;


    // Clear any previous session ID since we're starting fresh transform
    audioProcessor.clearCurrentSessionId();
    audioProcessor.setRetryAvailable(false);  // ADD THIS - clear retry for new transform
    updateRetryButtonState();

    updateAllGenerationButtonStates();
    repaint(); // Force immediate UI update

    // Basic validation checks (like Gary and Jerry)
    if (!isServiceReachable(ServiceType::Terry))
    {
        showStatusMessage("terry not reachable - check connection first");
        cancelTerryOperation();
        return;
    }

    // Check that we have either a variation selected OR a custom prompt
    bool hasVariation = (currentTerryVariation >= 0);
    bool hasCustomPrompt = !currentTerryCustomPrompt.trim().isEmpty();

    if (!hasVariation && !hasCustomPrompt)
    {
        showStatusMessage("please select a variation OR enter a custom prompt");
        cancelTerryOperation();
        updateAllGenerationButtonStates();
        return;
    }

    // Check that we have the selected audio source
    if (transformRecording)
    {
        if (savedSamples <= 0)
        {
            showStatusMessage("no recording available - save your recording first");
            cancelTerryOperation();
            updateAllGenerationButtonStates();
            return;
        }
    }
    else // transforming output
    {
        if (!hasOutputAudio)
        {
            showStatusMessage("no output audio available - generate with gary or jerry first");
            cancelTerryOperation();
            updateAllGenerationButtonStates();
            return;
        }
    }

    // Get the variation names array (matches your backend list)
    const auto& variationNames = terryVariationNames.isEmpty() ? getTerryVariationNames() : terryVariationNames;

    // Determine which audio file to read
    juce::File audioFile;
    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");

    if (transformRecording)
    {
        audioFile = garyDir.getChildFile("myBuffer.wav");
        DBG("Terry transforming recording: myBuffer.wav");
    }
    else
    {
        audioFile = garyDir.getChildFile("myOutput.wav");
        DBG("Terry transforming output: myOutput.wav");
    }

    if (!audioFile.exists())
    {
        showStatusMessage("audio file not found - " + audioFile.getFileName());
        cancelTerryOperation();
        updateAllGenerationButtonStates();
        return;
    }

    // Read and encode audio file (same as Gary)
    juce::MemoryBlock audioData;
    if (!audioFile.loadFileAsData(audioData))
    {
        showStatusMessage("failed to read audio file");
        cancelTerryOperation();
        updateAllGenerationButtonStates();
        return;
    }

    if (audioData.getSize() == 0)
    {
        showStatusMessage("audio file is empty");
        cancelTerryOperation();
        updateAllGenerationButtonStates();
        return;
    }

    auto base64Audio = juce::Base64::toBase64(audioData.getData(), audioData.getSize());

    // Debug: Log audio data size
    DBG("Terry audio file size: " + juce::String(audioData.getSize()) + " bytes");
    DBG("Terry base64 length: " + juce::String(base64Audio.length()) + " chars");
    DBG("Terry flowstep: " + juce::String(currentTerryFlowstep, 3));
    DBG("Terry solver: " + juce::String(useMidpointSolver ? "midpoint" : "euler"));

    // Button text feedback and status during processing
    if (terryUI)
        terryUI->setTransformButtonText("transforming...");
    showStatusMessage("sending audio to terry for transformation...");
    const float flowstep = currentTerryFlowstep;
    const bool useMidpoint = useMidpointSolver;
    const juce::String customPrompt = currentTerryCustomPrompt;
    const int selectedVariation = currentTerryVariation;
    const juce::URL requestUrl(getServiceUrl(ServiceType::Terry, "/api/juce/transform_audio"));
    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis(this);
    const auto generationToken = beginGenerationAsyncWork();

    // Create HTTP request in background thread (same pattern as Gary and Jerry)
    juce::Thread::launch([safeThis, generationToken, base64Audio, variationNames, hasVariation, hasCustomPrompt, flowstep, useMidpoint, customPrompt, selectedVariation, requestUrl]() {
        if (safeThis == nullptr || !safeThis->isGenerationAsyncWorkCurrent(generationToken)) {
            DBG("Terry request aborted - generation stopped");
            return;
        }

        auto startTime = juce::Time::getCurrentTime();

        // Create JSON payload
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("audio_data", base64Audio);
        jsonRequest->setProperty("flowstep", flowstep);
        jsonRequest->setProperty("solver", useMidpoint ? "midpoint" : "euler");

        // FIXED: Always send a variation (required by backend), custom_prompt overrides it
        if (hasCustomPrompt)
        {
            // Send default variation + custom_prompt (custom_prompt will override)
            jsonRequest->setProperty("variation", "accordion_folk");  // Default/neutral variation
            jsonRequest->setProperty("custom_prompt", customPrompt);
            DBG("Terry using custom prompt: " + customPrompt + " (with default variation)");
        }
        else if (hasVariation && selectedVariation < variationNames.size())
        {
            // Send selected variation only
            jsonRequest->setProperty("variation", variationNames[selectedVariation]);
            DBG("Terry using variation: " + variationNames[selectedVariation]);
        }
        else
        {
            // Fallback - should not happen due to validation, but safety net
            jsonRequest->setProperty("variation", "accordion_folk");
            DBG("Terry fallback to default variation");
        }

        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        DBG("Terry JSON payload size: " + juce::String(jsonString.length()) + " characters");

        juce::String responseText;
        int statusCode = 0;

        try
        {
            // Use JUCE 8.0.8 pattern: withPOSTData + InputStreamOptions (same as Gary/Jerry)
            juce::URL postUrl = requestUrl.withPOSTData(jsonString);

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(30000)
                .withExtraHeaders("Content-Type: application/json");

            auto stream = postUrl.createInputStream(options);

            auto requestTime = juce::Time::getCurrentTime() - startTime;
            DBG("Terry HTTP connection established in " + juce::String(requestTime.inMilliseconds()) + "ms");

            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();

                auto totalTime = juce::Time::getCurrentTime() - startTime;
                DBG("Terry HTTP request completed in " + juce::String(totalTime.inMilliseconds()) + "ms");
                DBG("Terry response length: " + juce::String(responseText.length()) + " characters");

                statusCode = 200; // Assume success if we got data
            }
            else
            {
                DBG("Failed to create input stream for Terry request");
                responseText = "";
                statusCode = 0;
            }
        }
        catch (const std::exception& e)
        {
            DBG("Terry HTTP request exception: " + juce::String(e.what()));
            responseText = "";
            statusCode = 0;
        }
        catch (...)
        {
            DBG("Unknown Terry HTTP request exception");
            responseText = "";
            statusCode = 0;
        }

        // Handle response on main thread (same pattern as Gary)
        juce::MessageManager::callAsync([safeThis, generationToken, responseText, statusCode, startTime]() {
            if (safeThis == nullptr || !safeThis->isGenerationAsyncWorkCurrent(generationToken) || !safeThis->isGenerating) {
                DBG("Terry callback aborted");
                return;
            }

            auto cancelTerryOperation = [safeThis]() {
                safeThis->isGenerating = false;
                safeThis->setActiveOp(ActiveOp::None);
                safeThis->updateAllGenerationButtonStates();
            };

            auto totalTime = juce::Time::getCurrentTime() - startTime;
            DBG("Total Terry request time: " + juce::String(totalTime.inMilliseconds()) + "ms");

            if (statusCode == 200 && responseText.isNotEmpty())
            {
                DBG("Terry response preview: " + responseText.substring(0, 200) + (responseText.length() > 200 ? "..." : ""));

                // Parse response JSON (same as Gary)
                auto responseVar = juce::JSON::parse(responseText);
                if (auto* responseObj = responseVar.getDynamicObject())
                {
                    bool success = responseObj->getProperty("success");
                    if (success)
                    {
                        juce::String sessionId = responseObj->getProperty("session_id").toString();
                        safeThis->showStatusMessage("sent to terry. processing...", 2000);
                        DBG("Terry session ID: " + sessionId);

                        // START POLLING FOR RESULTS (same as Gary)
                        safeThis->startPollingForResults(sessionId);  // This sets currentSessionId = sessionId

                        // Disable undo button until transform completes
                        safeThis->updateTerryEnablementSnapshot();
                    }
                    else
                    {
                        juce::String error = responseObj->getProperty("error").toString();
                        safeThis->showStatusMessage("terry error: " + error, 5000);
                        DBG("Terry server error: " + error);

                        if (safeThis->terryUI)
                            safeThis->terryUI->setTransformButtonText("transform with terry");
                        cancelTerryOperation();
                    }
                }
                else
                {
                    safeThis->showStatusMessage("invalid JSON response from terry", 4000);
                    DBG("Failed to parse Terry JSON response: " + responseText.substring(0, 100));

                    if (safeThis->terryUI)
                        safeThis->terryUI->setTransformButtonText("transform with terry");
                    cancelTerryOperation();
                }
            }
            else
            {
                juce::String errorMsg;
                bool shouldCheckHealth = false;

                if (statusCode == 0 && safeThis->audioProcessor.getIsUsingLocalhost())
                {
                    errorMsg = "cannot connect to terry on localhost - ensure terry is running in gary4local";
                    safeThis->markBackendDisconnectedFromRequestFailure("terry request");
                }
                else if (statusCode == 0)
                {
                    errorMsg = "failed to connect to Terry on remote backend";
                    shouldCheckHealth = true;  // Connection failure on remote - check if VM is down
                }
                else if (statusCode >= 400)
                {
                    errorMsg = "terry server error (HTTP " + juce::String(statusCode) + ")";
                    shouldCheckHealth = true;  // Server error - might be backend issue
                }
                else
                    errorMsg = "empty response from terry";

                safeThis->showStatusMessage(errorMsg, 4000);
                DBG("Terry request failed: " + errorMsg);

                // Check backend health if connection/server failure
                if (shouldCheckHealth)
                {
                    DBG("Terry failed - checking backend health");
                    safeThis->audioProcessor.checkBackendHealth();

                    // Give health check time, then handle result
                    juce::Timer::callAfterDelay(6000, [safeThis, generationToken]() {
                        if (safeThis == nullptr || !safeThis->isGenerationAsyncWorkCurrent(generationToken))
                            return;

                        if (!safeThis->audioProcessor.isBackendConnected())
                        {
                            safeThis->handleBackendDisconnection();
                            safeThis->lastBackendDisconnectionPopupTime = juce::Time::getCurrentTime();
                        }
                    });
                }

                if (safeThis->terryUI)
                    safeThis->terryUI->setTransformButtonText("transform with terry");

                cancelTerryOperation();
            }

            // Re-enable transform button (same pattern as Gary/Jerry)
            safeThis->updateTerryEnablementSnapshot();

            });
        });
}

void Gary4juceAudioProcessorEditor::undoTerryTransform()
{
    // We need a session ID to undo - this should be the current session
    juce::String sessionId = audioProcessor.getCurrentSessionId();
    if (sessionId.isEmpty())
    {
        showStatusMessage("no transform session to undo", 3000);
        return;
    }

    DBG("Attempting to undo Terry transform for session: " + sessionId);
    showStatusMessage("undoing transform...", 2000);

    // Disable undo button during request
    audioProcessor.setUndoTransformAvailable(false);
    updateTerryEnablementSnapshot();
    if (terryUI)
        terryUI->setUndoButtonText("undoing...");

    // Create HTTP request in background thread
    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis(this);
    juce::Thread::launch([safeThis, sessionId]() {
        // REMOVED: Don't check isGenerating for undo operations!
        // Undo operations are not generation operations and should proceed regardless
        // if (!isGenerating) {
        //     DBG("Undo Terry request aborted - generation stopped");
        //     return;
        // }

        auto startTime = juce::Time::getCurrentTime();

        // Create JSON payload - just needs session_id
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("session_id", sessionId);

        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        DBG("Terry undo JSON payload: " + jsonString);

        if (safeThis == nullptr)
        {
            DBG("Undo Terry request aborted - editor destroyed");
            return;
        }

        juce::URL url(safeThis->getServiceUrl(ServiceType::Terry, "/api/juce/undo_transform"));

        juce::String responseText;
        int statusCode = 0;

        try
        {
            // Use JUCE 8.0.8 pattern: withPOSTData + InputStreamOptions
            juce::URL postUrl = url.withPOSTData(jsonString);

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(15000)  // Shorter timeout for undo - should be quick
                .withExtraHeaders("Content-Type: application/json");

            auto stream = postUrl.createInputStream(options);

            auto requestTime = juce::Time::getCurrentTime() - startTime;
            DBG("Terry undo HTTP connection established in " + juce::String(requestTime.inMilliseconds()) + "ms");

            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();

                auto totalTime = juce::Time::getCurrentTime() - startTime;
                DBG("Terry undo HTTP request completed in " + juce::String(totalTime.inMilliseconds()) + "ms");

                statusCode = 200; // Assume success if we got data
            }
            else
            {
                DBG("Failed to create input stream for Terry undo request");
                responseText = "";
                statusCode = 0;
            }
        }
        catch (const std::exception& e)
        {
            DBG("Terry undo HTTP request exception: " + juce::String(e.what()));
            responseText = "";
            statusCode = 0;
        }
        catch (...)
        {
            DBG("Unknown Terry undo HTTP request exception");
            responseText = "";
            statusCode = 0;
        }

        // Handle response on main thread
        juce::MessageManager::callAsync([safeThis, responseText, statusCode]() {
            if (safeThis == nullptr)
            {
                DBG("Undo Terry callback aborted");
                return;
            }

            // REMOVED: Don't check isGenerating for undo callback either!
            // if (!isGenerating) {
            //     DBG("Undo Terry callback aborted");
            //     return;
            // }

            if (statusCode == 200 && responseText.isNotEmpty())
            {
                DBG("Terry undo response: " + responseText);

                // Parse response JSON
                auto responseVar = juce::JSON::parse(responseText);
                if (auto* responseObj = responseVar.getDynamicObject())
                {
                    bool success = responseObj->getProperty("success");
                    if (success)
                    {
                        // Get the restored audio data
                        auto audioData = responseObj->getProperty("audio_data").toString();
                        if (audioData.isNotEmpty())
                        {
                            // Save the restored audio
                            safeThis->saveGeneratedAudio(audioData);
                            safeThis->showStatusMessage("transform undone - audio restored.", 3000);
                            DBG("Terry undo successful - audio restored");

                            // Clear session ID since undo is complete
                            safeThis->audioProcessor.clearCurrentSessionId();
                            safeThis->audioProcessor.setRetryAvailable(false);          // ADD THIS
                            safeThis->updateTerryEnablementSnapshot();
                            if (safeThis->terryUI)
                                safeThis->terryUI->setUndoButtonText("undo transform");
                            safeThis->updateRetryButtonState();
                        }
                        else
                        {
                            safeThis->showStatusMessage("undo completed but no audio data received", 3000);
                            DBG("Terry undo success but missing audio data");
                            // Re-enable undo button since this is an error case
                            safeThis->audioProcessor.setUndoTransformAvailable(true);
                            safeThis->updateTerryEnablementSnapshot();
                            if (safeThis->terryUI)
                                safeThis->terryUI->setUndoButtonText("undo transform");
                        }
                    }
                    else
                    {
                        auto error = responseObj->getProperty("error").toString();
                        safeThis->showStatusMessage("undo failed: " + error, 4000);
                        DBG("Terry undo server error: " + error);
                        safeThis->audioProcessor.setUndoTransformAvailable(true);
                        safeThis->updateTerryEnablementSnapshot();
                        if (safeThis->terryUI)
                            safeThis->terryUI->setUndoButtonText("undo transform");
                    }
                }
                else
                {
                    safeThis->showStatusMessage("invalid undo response format", 3000);
                    DBG("Failed to parse Terry undo JSON response");
                    safeThis->audioProcessor.setUndoTransformAvailable(true);
                    safeThis->updateTerryEnablementSnapshot();
                    if (safeThis->terryUI)
                        safeThis->terryUI->setUndoButtonText("undo transform");
                }
            }
            else
            {
                juce::String errorMsg;
                if (statusCode == 0 && safeThis->audioProcessor.getIsUsingLocalhost())
                {
                    errorMsg = "cannot connect for undo on localhost - ensure terry is running in gary4local";
                    safeThis->markBackendDisconnectedFromRequestFailure("terry undo request");
                }
                else if (statusCode == 0)
                    errorMsg = "failed to connect for undo on remote backend";
                else if (statusCode >= 400)
                    errorMsg = "undo server error (HTTP " + juce::String(statusCode) + ")";
                else
                    errorMsg = "empty undo response";

                safeThis->showStatusMessage(errorMsg, 4000);
                DBG("Terry undo request failed: " + errorMsg);
                safeThis->audioProcessor.setUndoTransformAvailable(true);
                safeThis->updateTerryEnablementSnapshot();
                if (safeThis->terryUI)
                    safeThis->terryUI->setUndoButtonText("undo transform");
            }
            });
        });
}
