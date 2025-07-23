/*
  ==============================================================================

    NetworkManager.cpp
    Created: 23 Jul 2025 3:16:10am
    Author:  Kevin

  ==============================================================================
*/


#include "NetworkManager.h"

// ========== POLLING TIMER CLASS ==========
class NetworkManager::PollingTimer : public juce::Timer
{
public:
    PollingTimer(NetworkManager& nm) : networkManager(nm) {}

    void timerCallback() override
    {
        networkManager.pollForResults();
    }

private:
    NetworkManager& networkManager;
};

// ========== CONSTRUCTOR / DESTRUCTOR ==========
NetworkManager::NetworkManager()
{
    pollingTimer = std::make_unique<PollingTimer>(*this);
    DBG("NetworkManager created");
}

NetworkManager::~NetworkManager()
{
    stopPolling();
    DBG("NetworkManager destroyed");
}

// ========== GARY (MUSICGEN) IMPLEMENTATION ==========
void NetworkManager::sendToGary(const GaryParams& params, NetworkCallbacks callbacks)
{
    if (!connectionStatus)
    {
        if (callbacks.onError)
            callbacks.onError("Backend not connected - check connection first");
        return;
    }

    DBG("NetworkManager: Sending to Gary with " + juce::String(params.promptDuration) + "s duration");

    // Store callbacks for polling
    currentCallbacks = callbacks;

    if (callbacks.onStatusUpdate)
        callbacks.onStatusUpdate("Sending audio to Gary...", 2000);

    // Gary model names (matches your original code)
    const juce::StringArray modelNames = {
        "thepatch/vanya_ai_dnb_0.1",
        "thepatch/bleeps-medium",
        "thepatch/gary_orchestra_2",
        "thepatch/hoenn_lofi"
    };

    auto selectedModel = modelNames[juce::jlimit(0, modelNames.size() - 1, params.modelIndex)];

    // Create JSON payload
    juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
    jsonRequest->setProperty("model_name", selectedModel);
    jsonRequest->setProperty("prompt_duration", params.promptDuration);
    jsonRequest->setProperty("audio_data", params.audioData);
    jsonRequest->setProperty("top_k", 250);
    jsonRequest->setProperty("temperature", 1.0);
    jsonRequest->setProperty("cfg_coef", 3.0);
    jsonRequest->setProperty("description", params.description);

    auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));
    juce::URL url("https://g4l.thecollabagepatch.com/api/juce/process_audio");

    performHttpRequest(url, jsonString, [this](const juce::String& response, int statusCode) {
        handleGaryResponse(response, statusCode);
        });
}

void NetworkManager::handleGaryResponse(const juce::String& responseText, int statusCode)
{
    if (statusCode == 200 && !responseText.isEmpty())
    {
        auto responseVar = juce::JSON::parse(responseText);
        if (auto* responseObj = responseVar.getDynamicObject())
        {
            bool success = responseObj->getProperty("success");
            if (success)
            {
                juce::String sessionId = responseObj->getProperty("session_id").toString();

                if (currentCallbacks.onStatusUpdate)
                    currentCallbacks.onStatusUpdate("Sent to Gary! Processing...", 2000);

                DBG("Gary session ID: " + sessionId);
                startPollingForResults(sessionId, currentCallbacks);
            }
            else
            {
                juce::String error = responseObj->getProperty("error").toString();
                if (currentCallbacks.onError)
                    currentCallbacks.onError("Gary error: " + error);
                DBG("Gary server error: " + error);
            }
        }
        else
        {
            if (currentCallbacks.onError)
                currentCallbacks.onError("Invalid response from Gary");
            DBG("Failed to parse Gary JSON response");
        }
    }
    else
    {
        juce::String errorMsg = "Gary request failed (HTTP " + juce::String(statusCode) + ")";
        if (currentCallbacks.onError)
            currentCallbacks.onError(errorMsg);
        DBG(errorMsg);
    }
}

// ========== JERRY (STABLE AUDIO) IMPLEMENTATION ==========
void NetworkManager::sendToJerry(const JerryParams& params, NetworkCallbacks callbacks)
{
    if (!connectionStatus)
    {
        if (callbacks.onError)
            callbacks.onError("Backend not connected - check connection first");
        return;
    }

    DBG("NetworkManager: Sending to Jerry with prompt: " + params.prompt);

    // Store callbacks
    currentCallbacks = callbacks;

    // Determine endpoint based on smart loop toggle
    juce::String endpoint = params.generateAsLoop ? "/audio/generate/loop" : "/audio/generate";
    juce::String statusText = params.generateAsLoop ?
        "Generating smart loop with Jerry..." : "Generating with Jerry...";

    if (callbacks.onStatusUpdate)
        callbacks.onStatusUpdate(statusText, 2000);

    // Create JSON payload
    juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
    jsonRequest->setProperty("prompt", params.prompt);
    jsonRequest->setProperty("steps", params.steps);
    jsonRequest->setProperty("cfg_scale", params.cfgScale);
    jsonRequest->setProperty("return_format", "base64");
    jsonRequest->setProperty("seed", -1);  // Random seed

    // Add loop-specific parameters if using smart loop
    if (params.generateAsLoop) {
        jsonRequest->setProperty("loop_type", params.loopType);
    }

    auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));
    juce::URL url("https://g4l.thecollabagepatch.com" + endpoint);

    performHttpRequest(url, jsonString, [this](const juce::String& response, int statusCode) {
        handleJerryResponse(response, statusCode);
        });
}

void NetworkManager::handleJerryResponse(const juce::String& responseText, int statusCode)
{
    if (statusCode == 200 && !responseText.isEmpty())
    {
        auto responseVar = juce::JSON::parse(responseText);
        if (auto* responseObj = responseVar.getDynamicObject())
        {
            bool success = responseObj->getProperty("success");
            if (success)
            {
                // Jerry returns audio directly (not via polling like Gary)
                auto audioBase64 = responseObj->getProperty("audio_base64").toString();

                if (!audioBase64.isEmpty())
                {
                    if (currentCallbacks.onAudioReceived)
                        currentCallbacks.onAudioReceived(audioBase64, ""); // No session ID for Jerry

                    // Handle metadata if available
                    if (auto* metadata = responseObj->getProperty("metadata").getDynamicObject())
                    {
                        auto genTime = metadata->getProperty("generation_time").toString();
                        if (currentCallbacks.onStatusUpdate)
                            currentCallbacks.onStatusUpdate("Jerry complete! " + genTime + "s", 3000);
                    }
                    else
                    {
                        if (currentCallbacks.onStatusUpdate)
                            currentCallbacks.onStatusUpdate("Jerry generation complete!", 3000);
                    }
                }
                else
                {
                    if (currentCallbacks.onError)
                        currentCallbacks.onError("Jerry completed but no audio received");
                }
            }
            else
            {
                auto error = responseObj->getProperty("error").toString();
                if (currentCallbacks.onError)
                    currentCallbacks.onError("Jerry error: " + error);
                DBG("Jerry server error: " + error);
            }
        }
        else
        {
            if (currentCallbacks.onError)
                currentCallbacks.onError("Invalid JSON response from Jerry");
            DBG("Failed to parse Jerry JSON response");
        }
    }
    else
    {
        juce::String errorMsg = "Jerry request failed (HTTP " + juce::String(statusCode) + ")";
        if (currentCallbacks.onError)
            currentCallbacks.onError(errorMsg);
        DBG(errorMsg);
    }
}

// ========== TERRY (MELODYFLOW) IMPLEMENTATION ==========
void NetworkManager::sendToTerry(const TerryParams& params, NetworkCallbacks callbacks)
{
    if (!connectionStatus)
    {
        if (callbacks.onError)
            callbacks.onError("Backend not connected - check connection first");
        return;
    }

    DBG("NetworkManager: Sending to Terry");

    // Store callbacks for polling
    currentCallbacks = callbacks;

    if (callbacks.onStatusUpdate)
        callbacks.onStatusUpdate("Sending audio to Terry for transformation...", 2000);

    // Terry variation names (matches your backend list)
    const juce::StringArray variationNames = {
        "accordion_folk", "banjo_bluegrass", "piano_classical", "celtic",
        "strings_quartet", "synth_retro", "synth_modern", "synth_edm",
        "lofi_chill", "synth_bass", "rock_band", "cinematic_epic",
        "retro_rpg", "chiptune", "steel_drums", "gamelan_fusion",
        "music_box", "trap_808", "lo_fi_drums", "boom_bap",
        "percussion_ensemble", "future_bass", "synthwave_retro", "melodic_techno",
        "dubstep_wobble", "glitch_hop", "digital_disruption", "circuit_bent",
        "orchestral_glitch", "vapor_drums", "industrial_textures", "jungle_breaks"
    };

    // Create JSON payload
    juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
    jsonRequest->setProperty("audio_data", params.audioData);
    jsonRequest->setProperty("flowstep", params.flowstep);
    jsonRequest->setProperty("solver", params.useMidpointSolver ? "midpoint" : "euler");

    // Add either variation or custom_prompt (not both)
    if (params.variationIndex >= 0 && params.variationIndex < variationNames.size())
    {
        jsonRequest->setProperty("variation", variationNames[params.variationIndex]);
        DBG("Terry using variation: " + variationNames[params.variationIndex]);
    }
    else if (!params.customPrompt.trim().isEmpty())
    {
        jsonRequest->setProperty("custom_prompt", params.customPrompt);
        DBG("Terry using custom prompt: " + params.customPrompt);
    }

    auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));
    juce::URL url("https://g4l.thecollabagepatch.com/api/juce/transform_audio");

    performHttpRequest(url, jsonString, [this](const juce::String& response, int statusCode) {
        handleTerryResponse(response, statusCode);
        });
}

void NetworkManager::handleTerryResponse(const juce::String& responseText, int statusCode)
{
    if (statusCode == 200 && !responseText.isEmpty())
    {
        auto responseVar = juce::JSON::parse(responseText);
        if (auto* responseObj = responseVar.getDynamicObject())
        {
            bool success = responseObj->getProperty("success");
            if (success)
            {
                juce::String sessionId = responseObj->getProperty("session_id").toString();

                if (currentCallbacks.onStatusUpdate)
                    currentCallbacks.onStatusUpdate("Sent to Terry! Processing...", 2000);

                DBG("Terry session ID: " + sessionId);
                startPollingForResults(sessionId, currentCallbacks);
            }
            else
            {
                juce::String error = responseObj->getProperty("error").toString();
                if (currentCallbacks.onError)
                    currentCallbacks.onError("Terry error: " + error);
                DBG("Terry server error: " + error);
            }
        }
        else
        {
            if (currentCallbacks.onError)
                currentCallbacks.onError("Invalid JSON response from Terry");
            DBG("Failed to parse Terry JSON response");
        }
    }
    else
    {
        juce::String errorMsg = "Terry request failed (HTTP " + juce::String(statusCode) + ")";
        if (currentCallbacks.onError)
            currentCallbacks.onError(errorMsg);
        DBG(errorMsg);
    }
}

// ========== CONTINUE MUSIC IMPLEMENTATION ==========
void NetworkManager::continueMusic(const juce::String& audioData, int promptDuration, NetworkCallbacks callbacks)
{
    if (!connectionStatus)
    {
        if (callbacks.onError)
            callbacks.onError("Backend not connected - check connection first");
        return;
    }

    DBG("NetworkManager: Continuing music");

    // Store callbacks for polling
    currentCallbacks = callbacks;

    if (callbacks.onStatusUpdate)
        callbacks.onStatusUpdate("Requesting continuation...", 3000);

    // Create JSON payload - same structure as Gary
    juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
    jsonRequest->setProperty("audio_data", audioData);
    jsonRequest->setProperty("prompt_duration", promptDuration);
    jsonRequest->setProperty("model_name", "thepatch/vanya_ai_dnb_0.1"); // Use same model as Gary
    jsonRequest->setProperty("top_k", 250);
    jsonRequest->setProperty("temperature", 1.0);
    jsonRequest->setProperty("cfg_coef", 3.0);
    jsonRequest->setProperty("description", "");

    auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));
    juce::URL url("https://g4l.thecollabagepatch.com/api/juce/continue_music");

    performHttpRequest(url, jsonString, [this](const juce::String& response, int statusCode) {
        handleContinueResponse(response, statusCode);
        });
}

void NetworkManager::handleContinueResponse(const juce::String& responseText, int statusCode)
{
    if (statusCode == 200 && !responseText.isEmpty())
    {
        auto responseVar = juce::JSON::parse(responseText);
        if (auto* responseObj = responseVar.getDynamicObject())
        {
            bool success = responseObj->getProperty("success");
            if (success)
            {
                auto sessionId = responseObj->getProperty("session_id").toString();

                if (currentCallbacks.onStatusUpdate)
                    currentCallbacks.onStatusUpdate("Continuation queued...", 2000);

                DBG("Continue session ID: " + sessionId);
                startPollingForResults(sessionId, currentCallbacks);
            }
            else
            {
                auto error = responseObj->getProperty("error").toString();
                if (currentCallbacks.onError)
                    currentCallbacks.onError("Continue failed: " + error);
                DBG("Continue server error: " + error);
            }
        }
        else
        {
            if (currentCallbacks.onError)
                currentCallbacks.onError("Invalid response format");
            DBG("Failed to parse continue JSON response");
        }
    }
    else
    {
        juce::String errorMsg = "Continue request failed (HTTP " + juce::String(statusCode) + ")";
        if (currentCallbacks.onError)
            currentCallbacks.onError(errorMsg);
        DBG(errorMsg);
    }
}

// ========== UNDO TERRY TRANSFORM ==========
void NetworkManager::undoTerryTransform(const juce::String& sessionId, NetworkCallbacks callbacks)
{
    if (!connectionStatus)
    {
        if (callbacks.onError)
            callbacks.onError("Backend not connected - check connection first");
        return;
    }

    if (sessionId.isEmpty())
    {
        if (callbacks.onError)
            callbacks.onError("No transform session to undo");
        return;
    }

    DBG("NetworkManager: Undoing Terry transform for session: " + sessionId);

    if (callbacks.onStatusUpdate)
        callbacks.onStatusUpdate("Undoing transform...", 2000);

    // Create JSON payload
    juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
    jsonRequest->setProperty("session_id", sessionId);

    auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));
    juce::URL url("https://g4l.thecollabagepatch.com/api/juce/undo_transform");

    performHttpRequest(url, jsonString, [this, callbacks](const juce::String& response, int statusCode) {
        handleUndoResponse(response, statusCode, callbacks);
        });
}

void NetworkManager::handleUndoResponse(const juce::String& responseText, int statusCode, NetworkCallbacks callbacks)
{
    if (statusCode == 200 && !responseText.isEmpty())
    {
        auto responseVar = juce::JSON::parse(responseText);
        if (auto* responseObj = responseVar.getDynamicObject())
        {
            bool success = responseObj->getProperty("success");
            if (success)
            {
                // Get the restored audio data
                auto audioData = responseObj->getProperty("audio_data").toString();
                if (!audioData.isEmpty())
                {
                    if (callbacks.onAudioReceived)
                        callbacks.onAudioReceived(audioData, ""); // No session ID after undo

                    if (callbacks.onStatusUpdate)
                        callbacks.onStatusUpdate("Transform undone - audio restored!", 3000);

                    DBG("Terry undo successful - audio restored");
                }
                else
                {
                    if (callbacks.onError)
                        callbacks.onError("Undo completed but no audio data received");
                    DBG("Terry undo success but missing audio data");
                }
            }
            else
            {
                auto error = responseObj->getProperty("error").toString();
                if (callbacks.onError)
                    callbacks.onError("Undo failed: " + error);
                DBG("Terry undo server error: " + error);
            }
        }
        else
        {
            if (callbacks.onError)
                callbacks.onError("Invalid undo response format");
            DBG("Failed to parse Terry undo JSON response");
        }
    }
    else
    {
        juce::String errorMsg = "Undo request failed (HTTP " + juce::String(statusCode) + ")";
        if (callbacks.onError)
            callbacks.onError(errorMsg);
        DBG(errorMsg);
    }
}

// ========== POLLING SYSTEM ==========
void NetworkManager::startPollingForResults(const juce::String& sessionId, NetworkCallbacks callbacks)
{
    currentSessionId = sessionId;
    currentCallbacks = callbacks;
    isPolling = true;

    DBG("Started polling for session: " + sessionId);

    // Start polling timer (every 3 seconds)
    pollingTimer->startTimer(3000);
}

void NetworkManager::stopPolling()
{
    isPolling = false;
    pollingTimer->stopTimer();
    currentSessionId = "";
    DBG("Stopped polling");
}

void NetworkManager::pollForResults()
{
    if (!isPolling || currentSessionId.isEmpty())
        return;

    // Create polling request in background thread
    juce::Thread::launch([this]() {
        juce::URL pollUrl("https://g4l.thecollabagepatch.com/api/juce/poll_status/" + currentSessionId);

        try
        {
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(10000)
                .withExtraHeaders("Content-Type: application/json");

            auto stream = pollUrl.createInputStream(options);

            if (stream != nullptr)
            {
                auto responseText = stream->readEntireStreamAsString();

                // Handle response on main thread
                juce::MessageManager::callAsync([this, responseText]() {
                    handlePollingResponse(responseText);
                    });
            }
            else
            {
                DBG("Failed to create polling stream");
            }
        }
        catch (...)
        {
            DBG("Polling exception occurred");
        }
        });
}

void NetworkManager::handlePollingResponse(const juce::String& responseText)
{
    if (responseText.isEmpty())
    {
        DBG("Empty polling response");
        return;
    }

    try
    {
        auto responseVar = juce::JSON::parse(responseText);
        if (auto* responseObj = responseVar.getDynamicObject())
        {
            bool success = responseObj->getProperty("success");
            if (!success)
            {
                DBG("Polling error: " + responseObj->getProperty("error").toString());
                stopPolling();
                if (currentCallbacks.onError)
                    currentCallbacks.onError("Processing failed");
                return;
            }

            // Check if still in progress
            bool generationInProgress = responseObj->getProperty("generation_in_progress");
            bool transformInProgress = responseObj->getProperty("transform_in_progress");

            if (generationInProgress || transformInProgress)
            {
                // Get progress from server
                int serverProgress = responseObj->getProperty("progress");
                serverProgress = juce::jlimit(0, 100, serverProgress);

                if (currentCallbacks.onProgress)
                    currentCallbacks.onProgress(serverProgress);

                juce::String progressType = currentCallbacks.isTransformOperation ? "Transforming" : "Generating";
                if (currentCallbacks.onStatusUpdate)
                    currentCallbacks.onStatusUpdate(progressType + ": " + juce::String(serverProgress) + "%", 1000);

                return;
            }

            // COMPLETED - Check for audio data
            auto audioData = responseObj->getProperty("audio_data").toString();
            auto status = responseObj->getProperty("status").toString();

            if (!audioData.isEmpty())
            {
                stopPolling();

                if (currentCallbacks.onAudioReceived)
                    currentCallbacks.onAudioReceived(audioData, currentSessionId);

                if (currentCallbacks.onStatusUpdate)
                {
                    juce::String completionMsg = currentCallbacks.isTransformOperation ? "Transform complete!" : "Audio generation complete!";
                    currentCallbacks.onStatusUpdate(completionMsg, 3000);
                }

                // ADD THIS LINE:
                if (currentCallbacks.onOperationComplete)
                    currentCallbacks.onOperationComplete();

                DBG("Successfully received audio: " + juce::String(audioData.length()) + " chars");
            }
            else if (status == "failed")
            {
                auto error = responseObj->getProperty("error").toString();
                stopPolling();

                if (currentCallbacks.onError)
                {
                    juce::String errorMsg = currentCallbacks.isTransformOperation ? "Transform failed: " : "Generation failed: ";
                    currentCallbacks.onError(errorMsg + error);
                }

                // ADD THIS LINE:
                if (currentCallbacks.onOperationComplete)
                    currentCallbacks.onOperationComplete();
            }
            else if (status == "completed")
            {
                stopPolling();

                if (currentCallbacks.onError)
                {
                    // FIX: Use operation type instead of server flag
                    juce::String errorMsg = currentCallbacks.isTransformOperation ? "Transform completed but no audio received" : "Generation completed but no audio received";
                    currentCallbacks.onError(errorMsg);
                }

                // ADD THIS LINE:
                if (currentCallbacks.onOperationComplete)
                    currentCallbacks.onOperationComplete();
            }
        }
    }
    catch (...)
    {
        DBG("Failed to parse polling response");
    }
}

// ========== HTTP UTILITIES ==========
void NetworkManager::performHttpRequest(const juce::URL& url, const juce::String& jsonPayload,
    std::function<void(const juce::String&, int)> callback)
{
    // Perform HTTP request in background thread
    juce::Thread::launch([url, jsonPayload, callback]() {

        auto startTime = juce::Time::getCurrentTime();

        juce::String responseText;
        int statusCode = 0;

        try
        {
            // JUCE 8.0.8 CORRECT APPROACH: withPOSTData + InputStreamOptions
            juce::URL postUrl = url.withPOSTData(jsonPayload);

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(30000)
                .withExtraHeaders("Content-Type: application/json");

            auto stream = postUrl.createInputStream(options);

            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();
                statusCode = 200; // Assume success if we got data

                auto totalTime = juce::Time::getCurrentTime() - startTime;
                DBG("HTTP request completed in " + juce::String(totalTime.inMilliseconds()) + "ms");
            }
            else
            {
                DBG("Failed to create input stream for HTTP request");
                statusCode = 0;
            }
        }
        catch (const std::exception& e)
        {
            DBG("HTTP request exception: " + juce::String(e.what()));
            statusCode = 0;
        }
        catch (...)
        {
            DBG("Unknown HTTP request exception");
            statusCode = 0;
        }

        // Handle response on main thread
        juce::MessageManager::callAsync([callback, responseText, statusCode]() {
            callback(responseText, statusCode);
            });
        });
}

// ========== SESSION MANAGEMENT ==========
void NetworkManager::cancelCurrentOperation()
{
    stopPolling();

    if (currentCallbacks.onStatusUpdate)
        currentCallbacks.onStatusUpdate("Operation cancelled", 2000);

    DBG("Current operation cancelled");
}

void NetworkManager::setConnectionStatus(bool connected)
{
    connectionStatus = connected;
    DBG("NetworkManager connection status set to: " + juce::String(connected ? "Connected" : "Disconnected"));
}
