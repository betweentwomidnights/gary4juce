/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class Gary4juceAudioProcessorEditor : public juce::AudioProcessorEditor,
    public juce::Timer
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

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Gary4juceAudioProcessor& audioProcessor;

    // Connection status
    bool isConnected = false;

    // Recording status (cached for UI)
    bool isRecording = false;
    float recordingProgress = 0.0f;
    int recordedSamples = 0;
    int savedSamples = 0;  // NEW: Track which samples have been saved

    // Status message system
    juce::String statusMessage = "";
    juce::int64 statusMessageTime = 0;
    bool hasStatusMessage = false;

    // Waveform display components  
    bool needsWaveformUpdate = false;

    // Waveform display area
    juce::Rectangle<int> waveformArea;

    // UI Components
    juce::TextButton checkConnectionButton;
    juce::TextButton saveBufferButton;
    juce::TextButton clearBufferButton;

    // Gary (MusicGen) controls - ADD THESE BACK STEP BY STEP
    juce::Label garyLabel;
    juce::Slider promptDurationSlider;
    juce::Label promptDurationLabel;
    juce::ComboBox modelComboBox;
    juce::Label modelLabel;
    juce::TextButton sendToGaryButton;

    // Current Gary settings
    float currentPromptDuration = 6.0f;
    int currentModelIndex = 0;

    // UI Helper methods
    void updateRecordingStatus();
    void saveRecordingBuffer();
    void clearRecordingBuffer();
    void drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& area);
    void showStatusMessage(const juce::String& message, int durationMs = 3000);
    void sendToGary();

    // Add these to PluginEditor.h private section:
    juce::String currentSessionId = "";
    bool isPolling = false;

    // Output audio management
    juce::AudioBuffer<float> outputAudioBuffer;
    juce::File outputAudioFile;
    bool hasOutputAudio = false;
    int generationProgress = 0;  // 0-100 for progress visualization
    bool isGenerating = false;

    // UI areas
    juce::Rectangle<int> outputWaveformArea;

    // UI controls for output
    juce::TextButton playOutputButton;
    juce::TextButton clearOutputButton;
    juce::Label outputLabel;
    juce::TextButton stopOutputButton;

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
    

    // Also add this method declaration:
    void fullStopOutputPlayback();        // Complete stop (different from pause)

    // Smooth progress animation
    int lastKnownProgress = 0;           // Last real progress from server
    int targetProgress = 0;              // Target progress we're animating towards  
    juce::int64 lastProgressUpdateTime = 0;  // When we last got a server update
    bool smoothProgressAnimation = false;     // Whether we're currently animating

    // Add this method declaration:
    void updateSmoothProgress();             // Smooth progress animation helper

    void seekToPosition(double timeInSeconds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Gary4juceAudioProcessorEditor)
};