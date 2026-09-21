// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <JuceHeader.h>

#include "../Base/BpmControl.h"
#include "../Base/CustomButton.h"
#include "../Base/CustomComboBox.h"
#include "../Base/CustomTextEditor.h"
#include "../../Utils/CustomLookAndFeel.h"
#include "../../Utils/Theme.h"

#include <functional>
#include <memory>
#include <vector>

class YueyUI final : public juce::Component
{
public:
    enum class SubTab { Create = 0, Remix, Continue };
    enum class ContinuationMethod { Score = 0, Audio };

    YueyUI();
    ~YueyUI() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void setVisibleForTab(bool visible) { setVisible(visible); }
    juce::Rectangle<int> getTitleBounds() const { return titleBounds; }

    SubTab getCurrentSubTab() const { return currentSubTab; }
    void setCurrentSubTab(SubTab tab);
    ContinuationMethod getContinuationMethod() const { return continuationMethod; }
    void setContinuationMethod(ContinuationMethod method);

    juce::String getCreatePrompt() const { return createPromptEditor.getText().trim(); }
    juce::String getRemixPrompt() const { return remixPromptEditor.getText().trim(); }
    juce::String getContinuePrompt() const { return continuePromptEditor.getText().trim(); }
    void setCreatePrompt(const juce::String& text);
    void setRemixPrompt(const juce::String& text);
    void setContinuePrompt(const juce::String& text);

    juce::String getLyricsText() const { return lyricsText; }
    void setLyricsText(const juce::String& text);

    bool getCreateInstrumental() const { return createInstrumental; }
    // Off means we write the score ourselves and the model never plans one.
    bool getCreateLetYueyPlan() const { return createLetYueyPlan; }
    void setCreateLetYueyPlan(bool enabled);
    bool getRemixInstrumental() const { return remixInstrumental; }
    void setCreateInstrumental(bool enabled);
    void setRemixInstrumental(bool enabled);

    double getBpm() const { return bpmControl.getValue(); }
    void setBpm(double bpm);
    juce::String getKey() const;
    void setKey(const juce::String& key);
    juce::String getMeter() const;
    void setMeter(const juce::String& meter);

    bool getCreateFixedBars() const { return createFixedBars; }
    int getCreateBars() const;
    void setCreateLength(bool fixedBars, int bars);

    bool getContinueFixedBars() const { return continueFixedBars; }
    int getContinueBars() const;
    void setContinueLength(bool fixedBars, int bars);

    juce::String getTranscriptionMode() const;
    void setTranscriptionMode(const juce::String& mode);

    bool getAudioSourceRecording() const { return audioSourceRecording; }
    void setAudioSourceRecording(bool recording);
    void setAudioSourceAvailability(bool recordingAvailable, bool outputAvailable);

    void setGenerateButtonEnabled(bool canCreate, bool canRemix, bool canContinue,
                                  bool generating);
    // adoptTempo is false inside a host: the project owns the tempo, and the
    // score is retimed to it rather than the other way round.
    void applyPlanMetadata(const juce::String& abc, bool adoptTempo);

    // The backend holds a planner-chosen score to a ceiling. Seconds, where 0
    // means that backend is unbounded and a negative value means we have not
    // heard back yet. It only reaches the tooltips: the buttons are 128px and
    // "let yuey choose" already fills them.
    void setNaturalLengthCeiling(double seconds);

    std::function<void(SubTab)> onSubTabChanged;
    std::function<void(ContinuationMethod)> onContinuationMethodChanged;
    std::function<void(SubTab, const juce::String&)> onPromptChanged;
    std::function<void(const juce::String&)> onLyricsChanged;
    std::function<void(bool)> onAudioSourceChanged;
    std::function<void()> onPlanningChanged;
    std::function<void()> onCreate;
    std::function<void()> onRemix;
    std::function<void()> onContinue;

private:
    enum class PromptTarget { Create = 0, Remix, Continue };

    void addToContent(juce::Component& component);
    void updateSubTabState();
    void updateLengthState();
    void updateInstrumentalState();
    void updateSourceState();
    void openPromptPopout(PromptTarget target);
    void openLyricsPopout();
    void updateLyricsButton();
    void closeAuxiliaryWindows();
    void drawDiceIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                      bool isHovered, bool isPressed);
    void drawPopoutIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                        bool isHovered, bool isPressed);
    void selectCreateLength(bool fixedBars, bool notify);
    void selectContinueLength(bool fixedBars, bool notify);
    static int selectedNumber(const CustomComboBox& combo, int fallback);

    std::unique_ptr<juce::Component> contentComponent;
    std::unique_ptr<juce::Viewport> contentViewport;
    CustomLookAndFeel customLookAndFeel;

    juce::Label titleLabel;
    juce::Rectangle<int> titleBounds;
    CustomButton createSubTabButton;
    CustomButton remixSubTabButton;
    CustomButton continueSubTabButton;
    SubTab currentSubTab = SubTab::Create;

    juce::Label promptLabel;
    CustomTextEditor createPromptEditor;
    CustomTextEditor remixPromptEditor;
    CustomTextEditor continuePromptEditor;
    CustomButton createPromptPopoutButton;
    CustomButton remixPromptPopoutButton;
    CustomButton continuePromptPopoutButton;
    CustomButton createDiceButton;
    CustomButton remixDiceButton;
    CustomButton continueDiceButton;

    CustomButton lyricsButton;
    juce::ToggleButton instrumentalToggle;
    juce::ToggleButton letYueyPlanToggle;
    juce::String lyricsText;
    bool createInstrumental = false;
    bool createLetYueyPlan = false;
    bool remixInstrumental = true;

    juce::Label planningLabel;
    CustomComboBox keyRootComboBox;
    CustomComboBox keyModeComboBox;
    CustomComboBox meterComboBox;
    BpmControl bpmControl;

    juce::Label lengthLabel;
    CustomButton naturalLengthButton;
    CustomButton fixedLengthButton;
    CustomComboBox createBarsComboBox;
    bool createFixedBars = false;

    juce::Label sourceLabel;
    juce::ToggleButton recordingSourceButton;
    juce::ToggleButton outputSourceButton;
    bool audioSourceRecording = false;
    bool recordingSourceAvailable = false;
    bool outputSourceAvailable = false;

    juce::Label continuationMethodLabel;
    CustomButton scoreContinuationButton;
    CustomButton audioContinuationButton;
    ContinuationMethod continuationMethod = ContinuationMethod::Score;

    juce::Label transcriptionLabel;
    CustomComboBox transcriptionModeComboBox;

    juce::Label continueLengthLabel;
    CustomButton continueNaturalButton;
    CustomButton continueFixedButton;
    CustomComboBox continueBarsComboBox;
    bool continueFixedBars = false;
    double naturalLengthCeiling = -1.0;

    CustomButton actionButton;
    juce::Label infoLabel;
    bool canCreate = false;
    bool canRemix = false;
    bool canContinue = false;
    bool generating = false;

    std::vector<juce::Component::SafePointer<juce::DialogWindow>> auxiliaryWindows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YueyUI)
};
