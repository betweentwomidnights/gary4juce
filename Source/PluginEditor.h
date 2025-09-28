/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Components/Base/CustomButton.h"
#include "Components/Base/CustomSlider.h"
#include "Components/Base/CustomTextEditor.h"
#include "Components/Base/CustomComboBox.h"
#include "Utils/Theme.h"
#include "Utils/IconFactory.h"

#include <atomic>
#include <future>

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

    void initializeAudioPlayback();
    void stopOutputPlayback();
    void checkPlaybackStatus();

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    void updateAllGenerationButtonStates();

    

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

    ModelTab currentTab = ModelTab::Terry;  // Initialize to different tab so first switchToTab() works
    CustomButton garyTabButton;
    CustomButton jerryTabButton;
    CustomButton terryTabButton;  // For future

    void switchToTab(ModelTab tab);
    void updateTabButtonStates();
    juce::Rectangle<int> fullTabAreaRect;  // Store the calculated tab area

    // ========== GARY CONTROLS ==========
    juce::Label garyLabel;
    CustomSlider promptDurationSlider;
    juce::Label promptDurationLabel;
    CustomComboBox modelComboBox;
    juce::Label modelLabel;
    CustomButton sendToGaryButton;
    CustomButton continueButton;
    CustomButton retryButton;

    // Current Gary settings
    float currentPromptDuration = 6.0f;
    int currentModelIndex = 0;

    // ========== JERRY CONTROLS ==========
    juce::Label jerryLabel;
    CustomTextEditor jerryPromptEditor;
    juce::Label jerryPromptLabel;
    CustomSlider jerryCfgSlider;
    juce::Label jerryCfgLabel;
    CustomSlider jerryStepsSlider;
    juce::Label jerryStepsLabel;
    CustomButton generateWithJerryButton;
    juce::Label jerryBpmLabel;




    // Add Darius tab button (with other tab buttons)
    CustomButton dariusTabButton;

    // Add Darius-specific components
    juce::Label dariusLabel;
    juce::TextEditor dariusUrlEditor;
    juce::Label dariusUrlLabel;
    CustomButton dariusHealthCheckButton;
    juce::Label dariusStatusLabel;

    // Add Darius backend state variables
    juce::String dariusBackendUrl = "https://thecollabagepatch-magenta-retry.hf.space";
    bool dariusConnected = false;

    // Darius subtab system
    enum class DariusSubTab
    {
        Backend = 0,
        Model,
        Generation
    };

    DariusSubTab currentDariusSubTab = DariusSubTab::Backend;
    CustomButton dariusBackendTabButton;
    CustomButton dariusModelTabButton;
    CustomButton dariusGenerationTabButton;

    // Model subtab components (empty for now)
    std::unique_ptr<juce::Viewport> dariusModelViewport;
    std::unique_ptr<juce::Component> dariusModelContent;

    // Generation subtab components (empty for now)
    std::unique_ptr<juce::Viewport> dariusGenerationViewport;
    std::unique_ptr<juce::Component> dariusGenerationContent;

    // --- Darius model/config helper (first network helper)
    void fetchDariusConfig();
    void handleDariusConfigResponse(const juce::String& responseText, int statusCode);

    // Optional: stash last-seen config for future UI (e.g., a readout)
    juce::var lastDariusConfig;

    void checkDariusHealth();
    void handleDariusHealthResponse(const juce::String& response, bool connectionSucceeded);
    void switchToDariusSubTab(DariusSubTab subTab);
    void updateDariusSubTabStates();

    // --- Darius: checkpoints & select ---
    void fetchDariusCheckpoints(const juce::String& repo, const juce::String& revision);
    void handleDariusCheckpointsResponse(const juce::String& responseText, int statusCode);
    void postDariusSelect(const juce::var& requestObj); // requestObj is a JSON object
    void handleDariusSelectResponse(const juce::String& responseText, int statusCode);

    // Stashed results for upcoming UI binding
    juce::Array<int> dariusCheckpointSteps;
    int dariusLatestCheckpoint = -1;
    juce::var lastDariusSelectResp;

    // --- Darius: simple "Current model" UI in Model subtab
    juce::Label dariusModelHeaderLabel;
    juce::Label dariusModelGuardLabel;       // shown if backend unhealthy
    CustomButton dariusRefreshConfigButton;

    juce::Label dariusActiveSizeLabel;
    juce::Label dariusRepoLabel;
    juce::Label dariusStepLabel;
    juce::Label dariusLoadedLabel;
    juce::Label dariusWarmupLabel;

    // Small helper to (re)populate these from lastDariusConfig
    void updateDariusModelConfigUI();

    // --- Darius: Model selection controls
    juce::ToggleButton dariusUseBaseModelToggle;
    bool dariusUseBaseModel = true; // default ON
    void updateDariusModelControlsEnabled(); // enables/disables finetune UI (soon)

    // Finetune repo field (compact)
    juce::Label dariusRepoFieldLabel;
    juce::TextEditor dariusRepoField;
    void syncDariusRepoFromField();

    // --- Darius: checkpoint selector (compact)
    CustomButton dariusCheckpointButton;
    bool dariusIsFetchingCheckpoints = false;
    bool dariusOpenMenuAfterFetch = false;

    // Selected step string used by /model/select:
    // "latest" | "none" | "<int>"
    juce::String dariusSelectedStepStr = "latest";

    // Finetune defaults used for fetching checkpoints (UI later)
    juce::String dariusFinetuneRepo = "thepatch/magenta-ft";
    juce::String dariusFinetuneRevision = "main";

    // --- Darius: Apply & Warm
    CustomButton dariusApplyWarmButton;
    juce::Label dariusWarmStatusLabel; // animated “warming…” label

    bool dariusIsApplying = false; // covers POST /model/select
    bool dariusIsWarming = false; // covers polling /model/config

    // request building + warm polling
    juce::var makeDariusSelectApplyRequest();   // prewarm=true, stop_active=true
    void beginDariusApplyAndWarm();             // click handler
    void onDariusApplyFinished(bool ok, bool warmAlready);
    void startDariusWarmPolling(int attempt = 0); // recursive callAfterDelay polling

    // tiny ellipsis animation
    int  dariusWarmDots = 0;
    bool dariusDotsTicking = false;
    void startWarmDots();
    void stopWarmDots();

    // Helpers
    void showDariusCheckpointMenu();
    void updateDariusCheckpointButtonLabel();

    // ===== Generation: Styles UI =====
    juce::Label genStylesHeaderLabel;
    CustomButton genAddStyleButton;

    // A single row = text + weight + remove
    struct GenStyleRow {
        std::unique_ptr<juce::TextEditor> text;
        std::unique_ptr<juce::Slider>     weight;
        std::unique_ptr<CustomButton>     remove;
    };
    std::vector<GenStyleRow> genStyleRows;

    // Limits & defaults (mirrors Swift cap at ~4 entries)
    int  genStylesMax = 4;
    bool genStylesDirty = false;

    void addGenStyleRow(const juce::String& text = {}, double weight = 1.0);
    void removeGenStyleRow(int index);
    void rebuildGenStylesUI();              // rebuild row widgets (add/remove)
    void layoutGenStylesUI(juce::Rectangle<int>& area); // place rows in resized()
    juce::String genStylesCSV() const;      // "acid house, trumpet, lofi"
    juce::String genStyleWeightsCSV() const;// "1.00,0.75,0.50"

    // ===== Generation: Loop Influence =====
    juce::Label  genLoopLabel;
    juce::Slider genLoopSlider;        // 0.00 … 1.00
    double       genLoopInfluence = 0.50; // default midpoint

    // Helper to keep the label text in sync with slider
    void updateGenLoopLabel();
    double getGenLoopInfluence() const { return genLoopInfluence; }

    // ===== Generation: Advanced (dropdown) =====
    CustomButton genAdvancedToggle;   // "advanced ?"/"advanced ?"
    bool         genAdvancedOpen = false;

    // Temperature
    juce::Label  genTempLabel;
    juce::Slider genTempSlider;       // 0.00 … 10.00
    double       genTemperature = 1.20;

    // Top-K
    juce::Label  genTopKLabel;
    juce::Slider genTopKSlider;       // 1 … 300 (int)
    int          genTopK = 40;

    // Guidance weight
    juce::Label  genGuidanceLabel;
    juce::Slider genGuidanceSlider;   // 0.00 … 10.00
    double       genGuidance = 5.00;

    // Helpers
    void updateGenAdvancedToggleText();
    void layoutGenAdvancedUI(juce::Rectangle<int>& area); // lays out when open
    // Getters for /generate
    double getGenTemperature() const { return genTemperature; }
    int    getGenTopK()        const { return genTopK; }
    double getGenGuidance()    const { return genGuidance; }

    // ===== Generation: Bars + BPM =====
    juce::Label  genBarsLabel;
    CustomButton genBars4Button;
    CustomButton genBars8Button;
    CustomButton genBars16Button;
    int          genBars = 4;

    juce::Label  genBpmLabel;        // "bpm"
    juce::Label  genBpmValueLabel;   // "120.0"
    double       genBPM = 120.0;     // default until host updates

    // Helpers
    void updateGenBarsButtons();
    void setDAWBPM(double bpm);      // call this from your host/BPM bridge
    int    getGenBars() const { return genBars; }
    double getGenBPM()  const { return genBPM; }

    // ===== Generation: Input Source (Recording vs Output) =====
    juce::Label  genSourceLabel;
    CustomButton genRecordingButton;
    CustomButton genOutputButton;
    juce::Label  genSourceGuardLabel;  // shows when recording isn't available

    enum class GenAudioSource { Recording, Output };
    GenAudioSource genAudioSource = GenAudioSource::Output;

    // Helpers
    void updateGenSourceButtons();
    void updateGenSourceEnabled();   // enable/disable based on recording availability

    juce::String getGenAudioFilePath() const;  // path used for /generate upload

    // ===== Generation: Generate action =====
    CustomButton genGenerateButton;
    bool genIsGenerating = false;

    void onClickGenerate();
    void postDariusGenerate();                      // does the multipart upload
    juce::URL makeGenerateURL() const;              // builds /generate with form fields
    void handleDariusGenerateResponse(const juce::String& responseText, int statusCode);

    // ===== Generation: Steering (finetune assets) =====
    CustomButton genSteeringToggle;   // "steering ?/?"
    bool         genSteeringOpen = false;

    juce::Label  genMeanLabel;
    juce::Slider genMeanSlider;       // 0.00…2.00
    double       genMean = 1.0;

    static constexpr int kMaxCentroidsUI = 5;  // show up to 5 compact sliders
    juce::Label  genCentroidsHeaderLabel;
    juce::OwnedArray<juce::Label>  genCentroidLabels;
    juce::OwnedArray<juce::Slider> genCentroidSliders;
    std::vector<double> genCentroidWeights;    // full vector from backend size

    // Backend-reported assets status
    bool dariusAssetsMeanAvailable = false;
    int  dariusAssetsCentroidCount = 0;        // how many the backend has (we’ll show up to 5)

    // Networking
    void fetchDariusAssetsStatus();
    void handleDariusAssetsStatusResponse(const juce::String& responseText, int statusCode);

    // UI
    void updateGenSteeringToggleText();
    void rebuildGenCentroidRows();             // (re)create up to 5 sliders to match count
    void layoutGenSteeringUI(juce::Rectangle<int>& area); // place controls if open

    // For /generate
    bool  genAssetsAvailable() const { return dariusAssetsMeanAvailable || (dariusAssetsCentroidCount > 0); }
    juce::String centroidWeightsCSV() const;   // first N (or all, if you prefer)








    // Smart loop controls
    CustomButton generateAsLoopButton;
    void styleSmartLoopButton();

    // Loop type radio buttons (only visible when smart loop is enabled)
    CustomButton loopTypeAutoButton;
    CustomButton loopTypeDrumsButton;
    CustomButton loopTypeInstrumentsButton;

    // Add to header private section:
    void setLoopType(const juce::String& type);
    void styleLoopTypeButton(CustomButton& button, bool selected);

    // Current Jerry settings
    juce::String currentJerryPrompt = "";
    float currentJerryCfg = 1.0f;
    int currentJerrySteps = 8;
    bool generateAsLoop = false;          // New: tracks smart loop toggle
    juce::String currentLoopType = "auto"; // New: tracks selected loop type

    void updateLoopTypeVisibility();

    // ========== TERRY CONTROLS ==========
    juce::Label terryLabel;
    CustomComboBox terryVariationComboBox;
    juce::Label terryVariationLabel;
    CustomTextEditor terryCustomPromptEditor;
    juce::Label terryCustomPromptLabel;
    CustomSlider terryFlowstepSlider;
    juce::Label terryFlowstepLabel;
    juce::ToggleButton terrySolverToggle;  // false = euler, true = midpoint
    juce::Label terrySolverLabel;

    // Audio source selection
    juce::ToggleButton transformRecordingButton;
    juce::ToggleButton transformOutputButton;
    juce::Label terrySourceLabel;

    // Transform and undo buttons
    CustomButton transformWithTerryButton;
    CustomButton undoTransformButton;

    // Current Terry settings
    int currentTerryVariation = 0;  // Index into variations array
    juce::String currentTerryCustomPrompt = "";
    float currentTerryFlowstep = 0.130f;
    bool useMidpointSolver = false;  // false = euler, true = midpoint
    bool transformRecording = false; // false = transform output, true = transform recording

    // Terry helper methods
    void updateTerrySourceButtons();
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

    std::unique_ptr<juce::AudioFormatManager> audioFormatManager;
    std::unique_ptr<juce::AudioTransportSource> transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::unique_ptr<juce::AudioSourcePlayer> audioSourcePlayer;
    bool isPlayingOutput = false;

    // Playback cursor tracking
    double currentPlaybackPosition = 0.0;  // Current position in seconds
    double totalAudioDuration = 0.0;      // Total duration of loaded audio in seconds
    bool needsPlaybackUpdate = false;     // Flag to trigger repaints during playback

    bool isPausedOutput = false;       // True when paused (different from stopped)
    double pausedPosition = 0.0;       // Position where we paused

    void startOutputPlayback();        // Start fresh from beginning
    void pauseOutputPlayback();        // Pause at current position
    void resumeOutputPlayback();       // Resume from paused position
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
    juce::DrawableButton garyHelpButton, jerryHelpButton, terryHelpButton;

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

    void showAsioDriverWarning(const juce::String& driverName);

    std::atomic<bool> pollInFlight{ false };   // prevent overlapping polls
    juce::int64 lastGoodPollMs = 0;             // for diagnostics / backoff (optional)

    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Gary4juceAudioProcessorEditor)
};