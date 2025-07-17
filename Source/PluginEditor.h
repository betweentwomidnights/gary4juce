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

    // UI Components
    juce::TextButton checkConnectionButton;
    juce::TextButton saveBufferButton;
    juce::TextButton clearBufferButton;

    // UI Helper methods
    void updateRecordingStatus();
    void saveRecordingBuffer();
    void clearRecordingBuffer();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Gary4juceAudioProcessorEditor)
};