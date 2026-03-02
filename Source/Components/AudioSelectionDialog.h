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
#include <utility>

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

    // Configure selection window behavior in seconds.
    // If min/max/preferred are equal, the selection window is fixed-length.
    void setSelectionWindowConstraints(double minDurationSeconds,
                                       double maxDurationSeconds,
                                       double preferredDurationSeconds);

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Timer callback for playback cursor updates
    void timerCallback() override;

    // Set initial selection start time (for reopening dialog at previous position)
    void setInitialSelectionStartTime(double startTime);

    // Callbacks (set from parent)
    std::function<void()> onCancel;
    std::function<void(const juce::AudioBuffer<float>&, double, double)> onConfirm;  // Called with selected segment, sample rate, and selection start time

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
    double selectionMinDuration = 10.0;
    double selectionMaxDuration = 30.0;
    double selectionPreferredDuration = 30.0;
    double selectionStartTime = 0.0;  // Start time of selection window (in seconds)
    double selectionDuration = 30.0;  // Active selection duration in seconds
    bool isDraggingSelection = false;
    int dragStartX = 0;
    double dragStartSelectionTime = 0.0;

    // Mouse event handlers
    void mouseMove(const juce::MouseEvent& event) override;
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
    void updateSelectionFromResizeDrag(int mouseX);
    void confirmSelection();
    void updateInstructionText();
    bool canResizeSelection() const;
    bool isMouseNearLeftHandle(int mouseX) const;
    bool isMouseNearRightHandle(int mouseX) const;
    std::pair<double, double> getEffectiveDurationRange() const;
    double mouseXToTime(int mouseX) const;

    enum class SelectionDragMode
    {
        None = 0,
        Move,
        ResizeLeft,
        ResizeRight
    };
    SelectionDragMode selectionDragMode = SelectionDragMode::None;
    double dragStartSelectionDuration = 0.0;
    bool userResizedSelection = false;

    // Drawing methods
    void drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& area);
    void drawSelectionWindow(juce::Graphics& g, const juce::Rectangle<int>& area);

    // Audio device management
    juce::AudioDeviceManager deviceManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSelectionDialog)
};
