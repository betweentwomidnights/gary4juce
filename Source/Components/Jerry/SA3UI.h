// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <JuceHeader.h>

#include "../Base/CustomButton.h"
#include "../Base/CustomComboBox.h"
#include "../Base/CustomSlider.h"
#include "../Base/CustomTextEditor.h"
#include "../../Utils/CustomLookAndFeel.h"
#include "../../Utils/Theme.h"

#include <functional>
#include <vector>

class SA3UI : public juce::Component
{
public:
    enum class SubTab
    {
        Generate = 0,
        Transform,
        Continue
    };

    struct LoraSelection
    {
        juce::String name;
        double strength = 0.0;
    };

    SA3UI();
    ~SA3UI() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void setVisibleForTab(bool visible);
    void setTitleVisible(bool visible);
    juce::Rectangle<int> getTitleBounds() const { return titleBounds; }

    void setBpm(double bpm);
    void setRemoteAvailable(bool available);
    void setGenerateButtonEnabled(bool enabled, bool isGenerating);
    void setDiceButtonsEnabled(bool enabled);
    void setGenerateButtonText(const juce::String& text);

    juce::String getPromptText() const { return promptEditor.getText().trim(); }
    void setPromptText(const juce::String& text);

    juce::String getNegativePromptText() const { return negativePromptEditor.getText().trim(); }
    void setNegativePromptText(const juce::String& text);

    juce::String getTransformPromptText() const { return transformPromptEditor.getText().trim(); }
    void setTransformPromptText(const juce::String& text);
    double getTransformStrength() const { return transformStrengthSlider.getValue(); }
    void setTransformStrength(double value);
    bool getTransformAudioSourceRecording() const { return transformAudioSourceRecording; }
    void setTransformAudioSourceRecording(bool useRecording);
    void setTransformAudioSourceAvailability(bool recordingAvailable, bool outputAvailable);

    juce::String getContinuePromptText() const { return continuePromptEditor.getText().trim(); }
    void setContinuePromptText(const juce::String& text);
    int getContinueTotalSeconds() const { return juce::roundToInt(continuationSlider.getValue()); }
    void setContinueTotalSeconds(int seconds);
    bool getContinueLatentPrefixEnabled() const { return continueModeLatentPrefixButton.getToggleState(); }
    void setContinueLatentPrefixEnabled(bool enabled);
    bool getContinueAudioSourceRecording() const { return transformAudioSourceRecording; }
    void setContinueAudioSourceRecording(bool useRecording);
    void setContinueAudioSourceAvailability(bool recordingAvailable, bool outputAvailable);

    int getDurationSeconds() const { return juce::roundToInt(durationSlider.getValue()); }
    void setDurationSeconds(int seconds);

    bool getLoopEnabled() const { return loopToggle.getToggleState(); }
    void setLoopEnabled(bool enabled);
    int getBars() const { return selectedBars; }
    void setBars(int bars);

    int getSteps() const { return juce::roundToInt(stepsSlider.getValue()); }
    void setSteps(int steps);
    double getCfgScale() const { return cfgSlider.getValue(); }
    void setCfgScale(double value);
    juce::int64 getSeed() const;
    bool getUseSeedEnabled() const { return useSeedToggle.getToggleState(); }
    juce::String getSeedText() const { return seedEditor.getText().trim(); }
    void setSeedState(bool enabled, const juce::String& seedText);
    juce::String getLastSeed() const { return lastSeed; }
    void setLastSeed(const juce::String& seed);
    std::vector<LoraSelection> getActiveLoras() const;
    std::vector<LoraSelection> getLoraSelections() const;
    bool getUseLoraEnabled() const { return useLoraRequested; }
    void setLoraState(bool enabled, const std::vector<LoraSelection>& selections);
    void setAvailableLoras(const juce::StringArray& loraNames);
    juce::String getShift() const;
    void setShift(const juce::String& shift);

    juce::String getKeyScale() const;
    void setKeyScale(const juce::String& keyScale);

    SubTab getCurrentSubTab() const { return currentSubTab; }
    void setCurrentSubTab(SubTab tab);
    bool getAdvancedOpen() const { return advancedOpen; }
    void setAdvancedOpen(bool open);

    std::function<void(const juce::String&)> onPromptChanged;
    std::function<void(const juce::String&)> onNegativePromptChanged;
    std::function<void(int)> onDurationChanged;
    std::function<void(bool)> onLoopChanged;
    std::function<void(int)> onBarsChanged;
    std::function<void(int)> onStepsChanged;
    std::function<void(double)> onCfgChanged;
    std::function<void(const juce::String&)> onShiftChanged;
    std::function<void(const juce::String&)> onKeyScaleChanged;
    std::function<void(SubTab)> onSubTabChanged;
    std::function<void()> onGenerate;
    std::function<void()> onGenerateDiceRequested;
    std::function<void(const juce::String&)> onTransformPromptChanged;
    std::function<void(double)> onTransformStrengthChanged;
    std::function<void(bool)> onTransformAudioSourceChanged; // true=recording, false=output
    std::function<void()> onTransform;
    std::function<void()> onTransformDiceRequested;
    std::function<void(const juce::String&)> onContinuePromptChanged;
    std::function<void(int)> onContinueTotalSecondsChanged;
    std::function<void(bool)> onContinueLatentPrefixChanged;
    std::function<void(bool)> onContinueAudioSourceChanged; // true=recording, false=output
    std::function<void()> onContinue;
    std::function<void()> onContinueDiceRequested;
    std::function<void()> onLoraSelectionChanged;

private:
    enum class PromptPopoutTarget
    {
        Generate = 0,
        Transform,
        Continue
    };

    void addToContent(juce::Component& component);
    void setCurrentSubTabInternal(SubTab tab, bool notify);
    void updateSubTabButtonStyles();
    void updateLoopControls();
    void updateBarsButtonStyles();
    void updateAdvancedToggleText();
    void updateKeyScaleVisibility(bool notify);
    void updateLoraControls();
    void updateSavedLoraSelection(const juce::String& name, double strength);
    void clearLoraRows();
    void updateContentLayout();
    void drawDiceIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isHovered, bool isPressed);
    void drawPopoutIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isHovered, bool isPressed);
    void openPromptPopout(PromptPopoutTarget target,
                          CustomTextEditor& sourceEditor,
                          const juce::String& titleText,
                          const juce::String& placeholderText,
                          std::function<void()> diceCallback);
    void syncActivePromptPopout(PromptPopoutTarget target, const juce::String& text);

    juce::Label titleLabel;
    juce::Rectangle<int> titleBounds;
    bool titleHidden = false;

    CustomButton generateSubTabButton;
    CustomButton transformSubTabButton;
    CustomButton continueSubTabButton;
    SubTab currentSubTab = SubTab::Generate;

    juce::Label keyScaleLabel;
    CustomComboBox keyRootComboBox;
    CustomComboBox keyModeComboBox;
    juce::Label bpmLabel;
    double bpmValue = 120.0;

    std::unique_ptr<juce::Component> contentComponent;
    std::unique_ptr<juce::Viewport> contentViewport;
    CustomLookAndFeel customLookAndFeel;

    juce::Label promptLabel;
    CustomTextEditor promptEditor;
    CustomButton promptPopoutButton;
    CustomButton diceButton;

    juce::Label transformPromptLabel;
    CustomTextEditor transformPromptEditor;
    CustomButton transformPromptPopoutButton;
    CustomButton transformDiceButton;
    juce::Label transformSourceLabel;
    juce::ToggleButton transformRecordingButton;
    juce::ToggleButton transformOutputButton;
    juce::Label transformStrengthLabel;
    CustomSlider transformStrengthSlider;
    bool transformAudioSourceRecording = false;
    bool transformRecordingSourceAvailable = false;
    bool transformOutputSourceAvailable = false;

    juce::Label continuePromptLabel;
    CustomTextEditor continuePromptEditor;
    CustomButton continuePromptPopoutButton;
    CustomButton continueDiceButton;
    juce::Label continueSourceLabel;
    juce::ToggleButton continueRecordingButton;
    juce::ToggleButton continueOutputButton;
    juce::Label continuationLabel;
    CustomSlider continuationSlider;

    juce::Label durationLabel;
    CustomSlider durationSlider;

    CustomButton loopToggle;
    CustomButton bars4Button;
    CustomButton bars8Button;
    CustomButton bars16Button;
    int selectedBars = 8;
    bool loopEnabled = false;

    CustomButton advancedToggle;
    bool advancedOpen = false;
    juce::Label stepsLabel;
    CustomSlider stepsSlider;
    juce::Label cfgLabel;
    CustomSlider cfgSlider;
    juce::Label negativePromptLabel;
    CustomTextEditor negativePromptEditor;
    juce::ToggleButton useSeedToggle;
    CustomTextEditor seedEditor;
    juce::Label lastSeedLabel;
    juce::String lastSeed;
    juce::Label continueModeLabel;
    juce::ToggleButton continueModeStandardButton;
    juce::ToggleButton continueModeLatentPrefixButton;
    juce::ToggleButton useLoraToggle;
    juce::Label loraStatusLabel;
    bool useLoraRequested = false;
    std::vector<LoraSelection> savedLoraSelections;
    struct LoraRow
    {
        juce::String name;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<CustomSlider> slider;
    };
    std::vector<LoraRow> loraRows;
    juce::Label shiftLabel;
    CustomComboBox shiftComboBox;

    CustomButton generateButton;
    juce::Label infoLabel;

    bool remoteAvailable = true;
    bool lastCanGenerate = false;
    bool lastIsGenerating = false;
    bool lastDiceButtonsEnabled = false;
    PromptPopoutTarget activePromptPopoutTarget = PromptPopoutTarget::Generate;
    CustomTextEditor* activePromptPopoutEditor = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SA3UI)
};
