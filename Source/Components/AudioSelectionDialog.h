/*
  ==============================================================================
    AudioSelectionDialog.h

    Modal dialog for selecting a segment from long audio files.
    Selection window is 30s by default, auto-shrinks to 10s minimum when
    dragged near the end of the audio file.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Base/CustomButton.h"
#include "../Utils/IconFactory.h"

class AudioSelectionDialog : public juce::Component,
                              public juce::Timer
{
public:
    AudioSelectionDialog();
    ~AudioSelectionDialog() override;

    // Load audio file into the dialog
    bool loadAudioFile(const juce::File& audioFile);

    // Get the audio duration
    double getAudioDuration() const { return totalAudioDuration; }

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Timer callback for playback cursor updates
    void timerCallback() override;

    // Callbacks (set from parent)
    std::function<void()> onCancel;
    std::function<void(const juce::AudioBuffer<float>&, double)> onConfirm;  // Called with selected segment (10-30s) and sample rate

private:
    // Audio data
    juce::AudioBuffer<float> audioBuffer;
    double audioSampleRate = 44100.0;
    double totalAudioDuration = 0.0;

    // Playback state
    juce::AudioTransportSource transportSource;
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioSourcePlayer sourcePlayer;

    bool isPlaying = false;
    bool isPaused = false;
    double currentPlaybackPosition = 0.0;
    double pausedPosition = 0.0;

    // UI Components
    CustomButton playButton;
    CustomButton stopButton;
    CustomButton confirmButton;
    CustomButton cancelButton;

    juce::Label titleLabel;
    juce::Label durationLabel;
    juce::Label instructionLabel;

    // Play/Pause icons
    std::unique_ptr<juce::Drawable> playIcon;
    std::unique_ptr<juce::Drawable> pauseIcon;
    std::unique_ptr<juce::Drawable> stopIcon;

    // UI areas
    juce::Rectangle<int> waveformArea;

    // Selection window state (Stage 2B)
    double selectionStartTime = 0.0;  // Start time of selection window (in seconds)
    double selectionDuration = 30.0;  // Selection duration (30s max, 10s min)
    bool isDraggingSelection = false;
    int dragStartX = 0;
    double dragStartSelectionTime = 0.0;

    // Mouse event handlers
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    // Playback control methods
    void playAudio();
    void stopAudio();
    void updatePlayButtonIcon();

    // Selection window methods
    juce::Rectangle<int> getSelectionRectangle() const;
    bool isMouseOverSelection(const juce::Point<int>& pos) const;
    void updateSelectionFromMouseDrag(int mouseX);
    void confirmSelection();

    // Drawing methods
    void drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& area);
    void drawSelectionWindow(juce::Graphics& g, const juce::Rectangle<int>& area);

    // Audio device management
    juce::AudioDeviceManager deviceManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSelectionDialog)
};
