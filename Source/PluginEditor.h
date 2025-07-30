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
        Terry  // For future implementation
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

    // Discord and X icons
    std::unique_ptr<juce::Drawable> discordIcon;
    std::unique_ptr<juce::Drawable> xIcon;

    void debugModelSelection(const juce::String& functionName);
    void updateModelAvailability();

    void stopAllBackgroundOperations();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Gary4juceAudioProcessorEditor)
};