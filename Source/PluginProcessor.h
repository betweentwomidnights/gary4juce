/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin processor.
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
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

    // Recording buffer methods
    bool isRecording() const { return recording; }
    float getRecordingProgress() const; // Returns 0.0 to 1.0
    void saveRecordingToFile(const juce::File& file);
    void clearRecordingBuffer();
    const juce::AudioBuffer<float>& getRecordingBuffer() const { return recordingBuffer; }
    int getRecordedSamples() const { return recordedSamples; }

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

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Gary4juceAudioProcessor)

        // Backend connection state
        bool backendConnected = false;
    const juce::String backendBaseUrl = "https://g4l.thecollabagepatch.com";

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

    void startRecording();
    void stopRecording();
};