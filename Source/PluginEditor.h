/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Components/Base/CustomButton.h"
#include "Components/Terry/TerryUI.h"
#include "Components/Darius/DariusUI.h"
#include "Components/Gary/GaryUI.h"
#include "Components/Jerry/JerryUI.h"
#include "Utils/Theme.h"
#include "Utils/IconFactory.h"

#include <atomic>
#include <future>
#include <memory>
#include <vector>
#include <cstdint>

//==============================================================================
/**
*/
class Gary4juceAudioProcessorEditor : public juce::AudioProcessorEditor,
    public juce::Timer,
    public juce::DragAndDropContainer
{
public:
    Gary4juceAudioProcessorEditor(Gary4juceAudioProcessor&);
    ~Gary4juceAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // Backend connection status
    void updateConnectionStatus(bool connected);

    void startPollingForResults(const juce::String& sessionId);
    void stopPolling();
    void pollForResults();
    void handlePollingResponse(const juce::String& responseText);
    void saveGeneratedAudio(const juce::String& base64Audio);

    void loadOutputAudioFile();
    void drawOutputWaveform(juce::Graphics& g, const juce::Rectangle<int>& area);
    void drawExistingOutput(juce::Graphics& g, const juce::Rectangle<int>& area, float opacity);
    void playOutputAudio();
    void clearOutputAudio();

    void stopOutputPlayback();
    void checkPlaybackStatus();

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    void updateAllGenerationButtonStates();
    void updateGaryButtonStates(bool resetTexts = false);

    

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Gary4juceAudioProcessor& audioProcessor;

    // Connection status
    bool isConnected = false;
    bool connectionFlashState = false;  // For flashing animation when connected

    // Backend toggle system
    bool isUsingLocalhost = false; // Local cache synced with processor
    CustomButton backendToggleButton;

    // Service type enum for URL construction (maps to processor enum)
    enum class ServiceType { Gary, Jerry, Terry };

    // Recording status (cached for UI)
    bool isRecording = false;
    float recordingProgress = 0.0f;
    int recordedSamples = 0;
    int savedSamples = 0;  // Track which samples have been saved

    // Status message system
    juce::String statusMessage = "";
    juce::int64 statusMessageTime = 0;
    int statusMessageDuration = 3000;  // Custom duration for current message
    bool hasStatusMessage = false;

    // Waveform display components  
    bool needsWaveformUpdate = false;

    // Waveform display area
    juce::Rectangle<int> waveformArea;

    // UI Components
    CustomButton checkConnectionButton;
    CustomButton saveBufferButton;
    CustomButton clearBufferButton;

    juce::Rectangle<int> titleArea;
    juce::Rectangle<int> connectionStatusArea;
    juce::Rectangle<int> recordingLabelArea;
    juce::Rectangle<int> inputStatusArea;      // For status messages
    juce::Rectangle<int> inputInfoArea;        // For sample count info

    // ========== TAB SYSTEM ==========
    enum class ModelTab
    {
        Gary = 0,
        Jerry,
        Terry,
        Darius // magenta

    };

    // Tracks which operation (if any) is in-flight, independent of the visible tab.
    enum class ActiveOp
    {
        None = 0,
        GaryGenerate,
        GaryContinue,
        GaryRetry,
        TerryTransform,
        JerryGenerate,
        DariusGenerate
    };

    ModelTab currentTab = ModelTab::Terry;  // Initialize to different tab so first switchToTab() works
    CustomButton garyTabButton;
    CustomButton jerryTabButton;
    CustomButton terryTabButton;  // For future

    void switchToTab(ModelTab tab);
    void updateTabButtonStates();
    juce::Rectangle<int> fullTabAreaRect;  // Store the calculated tab area

    void setActiveOp(ActiveOp op) { activeOp = op; }
    ActiveOp getActiveOp() const { return activeOp; }
    juce::String currentOperationVerb() const;

    std::unique_ptr<GaryUI> garyUI;
    juce::StringArray garyModelItems;

    // Current Gary settings
    float currentPromptDuration = 6.0f;
    int currentModelIndex = 0;

    std::unique_ptr<JerryUI> jerryUI;

    // Add Darius tab button (with other tab buttons)
    CustomButton dariusTabButton;

    std::unique_ptr<DariusUI> dariusUI;

    // Backend state retained in editor
    juce::String dariusBackendUrl = "https://thecollabagepatch-magenta-retry.hf.space";
    bool dariusConnected = false;

    // --- Darius model/config helper (first network helper)
    void fetchDariusConfig();
    void handleDariusConfigResponse(const juce::String& responseText, int statusCode);

    juce::var lastDariusConfig;

    void checkDariusHealth();
    void handleDariusHealthResponse(const juce::String& response, bool connectionSucceeded);

    // --- Darius: checkpoints & select ---
    void fetchDariusCheckpoints(const juce::String& repo, const juce::String& revision);
    void handleDariusCheckpointsResponse(const juce::String& responseText, int statusCode);
    void postDariusSelect(const juce::var& requestObj);
    void handleDariusSelectResponse(const juce::String& responseText, int statusCode);

    juce::Array<int> dariusCheckpointSteps;
    int dariusLatestCheckpoint = -1;
    juce::var lastDariusSelectResp;

    void updateDariusModelConfigUI();
    void syncDariusRepoFromField();
    void updateDariusModelControlsEnabled();
    void startWarmDots();
    void stopWarmDots();

    bool dariusUseBaseModel = true;
    bool dariusIsFetchingCheckpoints = false;
    juce::String dariusSelectedStepStr = "latest";
    juce::String dariusFinetuneRepo = "thepatch/magenta-ft";
    juce::String dariusFinetuneRevision = "main";

    bool dariusIsApplying = false;
    bool dariusIsWarming = false;

    juce::var makeDariusSelectApplyRequest();
    void beginDariusApplyAndWarm();
    void onDariusApplyFinished(bool ok, bool warmAlready);
    void startDariusWarmPolling(int attempt = 0);

    void fetchDariusAssetsStatus();
    void handleDariusAssetsStatusResponse(const juce::String& responseText, int statusCode);
    void clearDariusSteeringAssets();

    bool dariusAssetsMeanAvailable = false;
    int  dariusAssetsCentroidCount = 0;
    std::vector<double> dariusCentroidWeights;

    bool  genAssetsAvailable() const { return dariusAssetsMeanAvailable || (dariusAssetsCentroidCount > 0); }
    juce::String centroidWeightsCSV() const;

    bool genIsGenerating = false;

    juce::String getGenAudioFilePath() const;
    void onClickGenerate();
    void postDariusGenerate();
    juce::URL makeGenerateURL(const juce::String& requestId) const;  // NEW
    void handleDariusGenerateResponse(const juce::String& responseText, int statusCode);

    bool        dariusIsPollingProgress = false;
    juce::String dariusProgressRequestId;
    int         dariusProgressPollTick = 0;   // ticks within timerCallback
    int         dariusLastKnownPercent = 0;   // for local smoothing if you prefer
    int         dariusTargetPercent = 0;
    juce::int64 dariusLastUpdateMs = 0;

    void startDariusProgressPoll(const juce::String& requestId);
    void stopDariusProgressPoll();
    void pollDariusProgress();                       // GET /progress?request_id=...
    juce::URL makeDariusProgressURL(const juce::String& reqId) const;






    // Current Jerry settings
    juce::String currentJerryPrompt = "";
    float currentJerryCfg = 1.0f;
    int currentJerrySteps = 8;
    bool generateAsLoop = false;
    juce::String currentLoopType = "auto";

    // Model selection state
    int currentJerryModelIndex = 0;
    juce::String currentJerryModelKey = "standard_saos";  // Keep for reference
    juce::String currentJerryModelType = "standard";      // NEW: 'standard' or 'finetune'
    juce::String currentJerryFinetuneRepo = "";           // NEW: e.g., 'thepatch/jerry_grunge'
    juce::String currentJerryFinetuneCheckpoint = "";     // NEW: e.g., 'jerry_encoded_epoch=33-step=100.ckpt'
    bool currentJerryIsFinetune = false;
    juce::String currentJerrySamplerType = "pingpong";

    void fetchJerryAvailableModels();
    void handleJerryModelsResponse(const juce::String& responseText);

    // Custom finetune methods
    void fetchJerryCheckpoints(const juce::String& repo);
    void handleJerryCheckpointsResponse(const juce::String& responseText);
    void addCustomJerryModel(const juce::String& repo, const juce::String& checkpoint);
    void handleAddCustomModelResponse(const juce::String& responseText,
                                       const juce::String& repo,
                                       const juce::String& checkpoint);
    juce::String extractCheckpointInfo(const juce::String& checkpoint);

    void fetchJerryPrompts(const juce::String& repo, const juce::String& checkpoint);
    void applyJerryPromptsToUI(const juce::String& repo,
        const juce::String& checkpoint,
        const juce::String& jsonText,
        int statusCode = 200);

    void maybeFetchRemoteJerryPrompts(); // gatekeeper for remote mode, avoids hammering

    // Optional local cache to avoid re-fetching
    juce::HashMap<juce::String, juce::String> promptsCache; // key: repo + "|" + checkpoint

    // Debounce/TTL state
    std::atomic<bool> promptsFetchInFlight{ false };
    std::int64_t lastPromptsFetchMs = 0;
    static constexpr std::int64_t kPromptsTTLms = 5 * 60 * 1000; // 5 minutes

    juce::URL buildPromptsUrl(const juce::String& repo,
        const juce::String& checkpoint) const;
    
    
    
    
    std::unique_ptr<TerryUI> terryUI;
    juce::StringArray terryVariationNames;

    // Current Terry settings
    int currentTerryVariation = 0;  // Index into variations array
    juce::String currentTerryCustomPrompt = "";
    float currentTerryFlowstep = 0.130f;
    bool useMidpointSolver = false;  // false = euler, true = midpoint
    bool transformRecording = false; // false = transform output, true = transform recording

    // Terry helper methods
    void updateTerryEnablementSnapshot();
    void setTerryAudioSource(bool useRecording);
    void sendToTerry();
    void undoTerryTransform();

    // Backend toggle methods
    juce::String getServiceUrl(ServiceType service, const juce::String& endpoint) const;
    void toggleBackend();
    void updateBackendToggleButton();

    // UI Helper methods
    void updateRecordingStatus();
    void saveRecordingBuffer();
    void clearRecordingBuffer();
    void drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& area);
    void showStatusMessage(const juce::String& message, int durationMs = 3000);
    void sendToGary();
    void sendToJerry();  // New method for Jerry

    // Polling system
    bool isPolling = false;

    // Output audio management
    juce::AudioBuffer<float> outputAudioBuffer;
    juce::File outputAudioFile;
    double currentAudioSampleRate = 44100.0;  // Store the actual sample rate of loaded audio
    bool hasOutputAudio = false;
    int generationProgress = 0;  // 0-100 for progress visualization
    bool isGenerating = false;
    bool continueInProgress = false;  // Track if current generation is a continue operation
    ActiveOp activeOp = ActiveOp::None;

    // UI areas
    juce::Rectangle<int> outputWaveformArea;
    juce::Rectangle<int> outputInfoArea;
    juce::Rectangle<int> tabArea;
    juce::Rectangle<int> modelControlsArea;

    // UI controls for output
    CustomButton playOutputButton;
    CustomButton clearOutputButton;
    juce::Label outputLabel;
    CustomButton stopOutputButton;
    juce::DrawableButton cropButton;

    bool isPlayingOutput = false;

    // Playback cursor tracking
    double currentPlaybackPosition = 0.0;  // Current position in seconds
    double totalAudioDuration = 0.0;      // Total duration of loaded audio in seconds
    bool needsPlaybackUpdate = false;     // Flag to trigger repaints during playback

    bool isPausedOutput = false;       // True when paused (different from stopped)
    double pausedPosition = 0.0;       // Position where we paused

    void fullStopOutputPlayback();     // Complete stop (different from pause)

    // Smooth progress animation
    int lastKnownProgress = 0;           // Last real progress from server
    int targetProgress = 0;              // Target progress we're animating towards  
    juce::int64 lastProgressUpdateTime = 0;  // When we last got a server update
    bool smoothProgressAnimation = false;     // Whether we're currently animating

    void updateSmoothProgress();             // Smooth progress animation helper

    // Stall detection for backend disconnection handling
    int lastKnownServerProgress = 0;
    bool hasDetectedStall = false;
    static const int STALL_TIMEOUT_SECONDS = 15;  // 15 seconds without progress = stall
    juce::int64 lastHealthCheckTime = 0;
    static const int MIN_HEALTH_CHECK_INTERVAL_MS = 30000; // 30 seconds minimum between checks
    juce::Time lastBackendDisconnectionPopupTime;  // For throttling manual check popup

    const int STARTUP_TIMEOUT_SECONDS = 90;  // Generous timeout for model loading (0% progress)
    const int GENERATION_TIMEOUT_SECONDS = 9; // Tighter timeout once generation starts (>0% progress)



    // Backend disconnection and stall handling methods
    bool checkForGenerationStall();
    void handleGenerationStall();
    void handleBackendDisconnection();
    void handleGenerationFailure(const juce::String& reason);
    void resetStallDetection();
    void performSmartHealthCheck();
    void showBackendDisconnectionDialog();

    bool isCurrentlyQueued = false;

    void seekToPosition(double timeInSeconds);

    // Crop and continue functionality
    void cropAudioAtCurrentPosition();
    void continueMusic();
    void sendContinueRequest(const juce::String& audioData);
    void retryLastContinuation();
    void updateRetryButtonState();
    void updateContinueButtonState();

    // Drag and drop functionality
    bool isDragging = false;
    bool dragStarted = false;
    juce::Point<int> dragStartPosition;
    void startAudioDrag();
    bool isMouseOverOutputWaveform(const juce::Point<int>& position) const;

    // Crop icon
    std::unique_ptr<juce::Drawable> cropIcon;

    // Check connection icon
    std::unique_ptr<juce::Drawable> checkConnectionIcon;

    // Trash icon (for clear buttons)
    std::unique_ptr<juce::Drawable> trashIcon;

    // Play/Pause icons
    std::unique_ptr<juce::Drawable> playIcon;
    std::unique_ptr<juce::Drawable> pauseIcon;
    void updatePlayButtonIcon();

    // Stop icon
    std::unique_ptr<juce::Drawable> stopIcon;

    // Logo image
    juce::Image logoImage;

    // Tooltip system
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;

    // Help icons
    std::unique_ptr<juce::Drawable> helpIcon;
    juce::DrawableButton garyHelpButton, jerryHelpButton, terryHelpButton, dariusHelpButton;

    // Allow an extended grace period when we're at 0% but still receiving polls
    const int HF_WARMUP_GRACE_SECONDS = 480; // 8 minutes
    juce::int64 pollingStartTimeMs = 0;      // set when polling begins
    bool withinWarmup = false;

    // Discord and X icons
    std::unique_ptr<juce::Drawable> discordIcon;
    std::unique_ptr<juce::Drawable> xIcon;

    void debugModelSelection(const juce::String& functionName);
    void updateModelAvailability();

    void stopAllBackgroundOperations();

    juce::CriticalSection fileLock;  // Protects file operations
    std::atomic<bool> isDragInProgress{ false };
    std::atomic<bool> isEditorValid{ true };  // Add this alongside your existing atomics

    std::pair<bool, juce::File> prepareFileForDrag();
    bool performDragOperation(const juce::File& dragFile);

    std::atomic<bool> pollInFlight{ false };   // prevent overlapping polls
    juce::int64 lastGoodPollMs = 0;             // for diagnostics / backoff (optional)

    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Gary4juceAudioProcessorEditor)
};
