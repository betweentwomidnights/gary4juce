#pragma once

#include <JuceHeader.h>

#include "../Base/CustomButton.h"
#include "../Base/CustomSlider.h"
#include "../Base/CustomTextEditor.h"
#include "../Base/CustomComboBox.h"

class TerryUI : public juce::Component
{
public:
    TerryUI();

    void paint(juce::Graphics&) override;
    void resized() override;

    void setVariations(const juce::StringArray& items, int selectedIndex);
    void setCustomPrompt(const juce::String& text);
    void setFlowstep(float v);
    void setUseMidpointSolver(bool useMidpoint);
    void setAudioSourceRecording(bool useRecording);
    void setAudioSourceAvailability(bool recordingAvailable, bool outputAvailable);
    void setButtonsEnabled(bool canTransform, bool isGenerating, bool undoAvailable);
    void setTransformButtonText(const juce::String& text);
    void setUndoButtonText(const juce::String& text);
    void setVisibleForTab(bool visible);
    void setBpm(double bpm);

    int getSelectedVariationIndex() const;
    juce::String getCustomPrompt() const;
    float getFlowstep() const;
    bool getUseMidpointSolver() const;
    bool getAudioSourceRecording() const;

    juce::Rectangle<int> getTitleBounds() const;

    std::function<void(int)> onVariationChanged;
    std::function<void(const juce::String&)> onCustomPromptChanged;
    std::function<void(float)> onFlowstepChanged;
    std::function<void(bool)> onSolverChanged;
    std::function<void(bool)> onAudioSourceChanged; // true=recording, false=output
    std::function<void()> onTransform;
    std::function<void()> onUndo;

private:
    void applyEnablement(bool canTransform, bool isGenerating, bool undoAvailable);

    juce::Label terryLabel;
    juce::Label terryVariationLabel;
    CustomComboBox terryVariationComboBox;
    juce::Label terryCustomPromptLabel;
    CustomTextEditor terryCustomPromptEditor;
    juce::Label terryFlowstepLabel;
    CustomSlider terryFlowstepSlider;
    juce::Label terrySolverLabel;
    juce::ToggleButton terrySolverToggle;
    juce::Label terrySourceLabel;
    juce::ToggleButton transformRecordingButton;
    juce::ToggleButton transformOutputButton;
    CustomButton transformWithTerryButton;
    CustomButton undoTransformButton;
    juce::Label bpmLabel;

    int variationIndex { -1 }; // -1 indicates custom prompt
    juce::String customPrompt;
    float flowstep { 0.130f };
    bool useMidpoint { false };
    bool audioSourceRecording { false };
    bool recordingSourceAvailable { false };
    bool outputSourceAvailable { false };

    bool lastCanTransform { false };
    bool lastIsGenerating { false };
    bool lastUndoAvailable { false };

    double bpmValue { 0.0 };

    juce::Rectangle<int> titleBounds;
};

