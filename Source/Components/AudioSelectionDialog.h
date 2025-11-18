/*
  ==============================================================================
    AudioSelectionDialog.h

    Modal dialog for selecting a 30-second segment from long audio files.
    Stage 2A: Basic waveform display and playback controls
    Stage 2B: Will add 30-second selection window overlay
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
    std::function<void(const juce::AudioBuffer<float>&)> onConfirm;  // Called with selected 30s segment

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
    double selectionStartTime = 0.0;  // Start time of 30s window (in seconds)
    const double selectionDuration = 30.0;  // Always 30 seconds
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
