#pragma once

#include <JuceHeader.h>
#include <functional>

class NetworkManager
{
public:
    // Callback types for different network operations
    struct NetworkCallbacks
    {
        std::function<void(const juce::String& message, int durationMs)> onStatusUpdate;
        std::function<void(int progress)> onProgress;
        std::function<void(const juce::String& base64Audio, const juce::String& sessionId)> onAudioReceived;
        std::function<void(const juce::String& error)> onError;
        std::function<void()> onOperationComplete;

        // NEW: Add operation type so NetworkManager knows what to call it
        bool isTransformOperation = false;  // true for Terry, false for Gary/Jerry
    };

    // Gary (MusicGen) parameters
    struct GaryParams
    {
        juce::String audioData;          // Base64 encoded audio
        int promptDuration;              // 1-15 seconds
        int modelIndex;                  // 0-3 for model selection
        juce::String description = "";   // Optional description
    };

    // Jerry (Stable Audio) parameters  
    struct JerryParams
    {
        juce::String prompt;             // Text prompt (with BPM appended)
        int steps;                       // 4-16 steps
        float cfgScale;                  // 0.5-2.0 CFG scale
        bool generateAsLoop;             // Smart loop toggle
        juce::String loopType;           // "auto", "drums", "instruments"
    };

    // Terry (MelodyFlow) parameters
    struct TerryParams
    {
        juce::String audioData;          // Base64 encoded audio
        float flowstep;                  // 0.050-0.150
        bool useMidpointSolver;          // true=midpoint, false=euler
        int variationIndex;              // -1 = use custom prompt
        juce::String customPrompt;       // Custom prompt if variationIndex=-1
    };

    NetworkManager();
    ~NetworkManager();

    // Check backend connection status
    bool isConnected() const { return connectionStatus; }

    void setConnectionStatus(bool connected);

    // Main generation methods
    void sendToGary(const GaryParams& params, NetworkCallbacks callbacks);
    void sendToJerry(const JerryParams& params, NetworkCallbacks callbacks);
    void sendToTerry(const TerryParams& params, NetworkCallbacks callbacks);

    // Continuation and undo
    void continueMusic(const juce::String& audioData, int promptDuration, NetworkCallbacks callbacks);
    void undoTerryTransform(const juce::String& sessionId, NetworkCallbacks callbacks);

    // Session management
    void cancelCurrentOperation();
    juce::String getCurrentSessionId() const { return currentSessionId; }

private:
    // Connection state
    bool connectionStatus = false;

    // Session tracking
    juce::String currentSessionId;
    bool isPolling = false;
    NetworkCallbacks currentCallbacks;

    // Polling system
    void startPollingForResults(const juce::String& sessionId, NetworkCallbacks callbacks);
    void stopPolling();
    void pollForResults();
    void handlePollingResponse(const juce::String& responseText);

    // Response handlers for each service
    void handleGaryResponse(const juce::String& responseText, int statusCode);
    void handleJerryResponse(const juce::String& responseText, int statusCode);
    void handleTerryResponse(const juce::String& responseText, int statusCode);
    void handleContinueResponse(const juce::String& responseText, int statusCode);
    void handleUndoResponse(const juce::String& responseText, int statusCode, NetworkCallbacks callbacks);

    // HTTP utilities
    void performHttpRequest(const juce::URL& url, const juce::String& jsonPayload,
        std::function<void(const juce::String&, int)> callback);

    // Timer for polling (we'll use juce::Timer)
    class PollingTimer;
    std::unique_ptr<PollingTimer> pollingTimer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NetworkManager)
};