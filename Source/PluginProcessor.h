/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin processor.
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
#include <atomic>  // ADD THIS FOR ATOMIC TYPES

//==============================================================================
/**
*/
class Gary4juceAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    Gary4juceAudioProcessor();
    ~Gary4juceAudioProcessor() override;

    // Backend connection methods
    void checkBackendHealth();
    bool isBackendConnected() const { return backendConnected; }
    void setBackendConnectionStatus(bool connected);
    void stopHealthChecks();

    // Service type enum for different ports
    enum class ServiceType { Gary, Jerry, Terry };
    
    // Backend URL management methods
    void setUsingLocalhost(bool useLocalhost);
    bool getIsUsingLocalhost() const { return isUsingLocalhost; }
    juce::String getCurrentBackendType() const { return isUsingLocalhost ? "local" : "remote"; }
    juce::String getServiceUrl(ServiceType service, const juce::String& endpoint) const;

    // Session ID management for undo functionality
    void setCurrentSessionId(const juce::String& sessionId);
    juce::String getCurrentSessionId() const;
    void clearCurrentSessionId();

    // State management methods
    void setSavedSamples(int samples) { savedSamples = samples; }
    int getSavedSamples() const { return savedSamples.load(); }
    
    void setTransformRecording(bool useRecording) { transformRecording = useRecording; }
    bool getTransformRecording() const { return transformRecording.load(); }

    // Recording buffer methods - REMOVE INLINE IMPLEMENTATIONS
    bool isRecording() const;  // Declaration only - implementation in .cpp
    float getRecordingProgress() const;  // Declaration only - implementation in .cpp
    void saveRecordingToFile(const juce::File& file);
    void clearRecordingBuffer();
    const juce::AudioBuffer<float>& getRecordingBuffer() const { return recordingBuffer; }
    int getRecordedSamples() const;  // Declaration only - implementation in .cpp
    int getMaxRecordingSamples() const { return maxRecordingSamples; }  // ADD THIS

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    double getCurrentBPM() const { return currentBPM.load(); }

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Gary4juceAudioProcessor)

        // Backend connection state
        bool backendConnected = false;
    std::atomic<bool> shouldStopBackgroundOperations{ false };
    
    // Backend URL management
    bool isUsingLocalhost = false;
    juce::String backendBaseUrl = "https://g4l.thecollabagepatch.com"; // Default to remote

    // Thread safety - PUT THESE FIRST
    juce::CriticalSection bufferLock;
    std::atomic<int> atomicRecordedSamples{ 0 };
    std::atomic<bool> atomicRecording{ false };

    // Recording buffer state
    juce::AudioBuffer<float> recordingBuffer;
    int bufferWritePosition = 0;
    int recordedSamples = 0;
    bool recording = false;
    bool wasPlaying = false;  // To detect transport state changes
    double currentSampleRate = 44100.0;

    // Recording settings
    static constexpr double recordingLengthSeconds = 30.0;
    int maxRecordingSamples = 0;  // Will be calculated based on sample rate

    std::atomic<double> currentBPM{ 120.0 };  // Thread-safe BPM storage

    // Session ID for undo functionality (persists even when UI is destroyed)
    juce::String currentSessionId;

    // State that needs to survive editor destruction
    std::atomic<int> savedSamples{0};
    std::atomic<bool> transformRecording{ false };  // Default to output (to match UI default)

    // Private methods
    void startRecording();
    void stopRecording();
};