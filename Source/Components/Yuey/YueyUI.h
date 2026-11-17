// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <JuceHeader.h>

#include "../Base/BpmControl.h"
#include "../Base/CustomButton.h"
#include "../Base/CustomComboBox.h"
#include "../Base/CustomTextEditor.h"
#include "YueyMidiSlot.h"
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
    int getPreferredHeight(int width);

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

    double getBpm() const { return isStandalone ? bpmControl.getValue() : hostBpm; }
    void setBpm(double bpm);
    void setIsStandalone(bool standalone);
    juce::String getKey() const;
    bool isKeyNone() const;
    void setKey(const juce::String& key);
    juce::String getMeter() const;
    void setMeter(const juce::String& meter);

    // -1 unless "use seed" is on, which the backend reads as "pick one".
    juce::int64 getSeed() const;
    bool getUseSeedEnabled() const { return useSeedToggle.getToggleState(); }
    juce::String getSeedText() const { return seedEditor.getText().trim(); }
    void setSeedState(bool enabled, const juce::String& seedText);
    juce::String getLastSeed() const { return lastSeed; }
    void setLastSeed(const juce::String& seed);

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

    // MIDI as a third source beside recording and output, for remix and continue. It is yuey's
    // own choice: the recording/output state is shared with the other models and stays theirs.
    // The editor reads the files; this only shows what it found.
    enum class MidiLane { Melody = 0, Chords };
    bool getMidiSourceSelected() const { return midiSourceSelected; }
    void setMidiSourceSelected(bool selected);  // no notification, for restoring state
    void showMidiEmpty(MidiLane lane);
    void showMidiLoaded(MidiLane lane, const juce::File& file, const juce::String& summary);
    void showMidiInvalid(MidiLane lane, const juce::File& file, const juce::String& error);
    // One line under both slots, for what only the pair can say, like different lengths.
    void setMidiNote(const juce::String& text);

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
    std::function<void()> onLayoutHeightChanged;
    std::function<void(ContinuationMethod)> onContinuationMethodChanged;
    std::function<void(SubTab, const juce::String&)> onPromptChanged;
    std::function<void(const juce::String&)> onLyricsChanged;
    std::function<void(bool)> onAudioSourceChanged;
    std::function<void(bool)> onMidiSourceChanged;
    std::function<void(MidiLane, const juce::File&)> onMidiFile;  // dropped or picked
    std::function<void(MidiLane)> onMidiCleared;
    std::function<void()> onPlanningChanged;
    // Which tab asked. The editor owns the pool and the prompt fields, so it
    // decides what a roll means; this only reports that one happened.
    std::function<void(SubTab)> onDice;
    std::function<void()> onCreate;
    std::function<void()> onRemix;
    std::function<void()> onContinue;

    // The dice, drawn here so the score window's matches the one beside the prompt.
    static void drawDiceIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                             bool isHovered, bool isPressed);

private:
    enum class PromptTarget { Create = 0, Remix, Continue };

    void addToContent(juce::Component& component);
    int layoutContent(int width);
    int chromeHeight() const { return currentSubTab == SubTab::Create ? 120 : 104; }
    int preferredHeight = 0;
    void updateSubTabState();
    void updateLengthState();
    void updateInstrumentalState();
    void updateSourceState();
    // The midi slots show on remix and continue while "midi" is the source; create is unchanged.
    bool midiActive() const { return midiSourceSelected && currentSubTab != SubTab::Create; }
    YueyMidiSlot& slotFor(MidiLane lane) { return lane == MidiLane::Melody ? melodySlot : chordsSlot; }
    void selectMidiSource(bool notify);
    void openPromptPopout(PromptTarget target);
    void openLyricsPopout();
    void updateLyricsButton();
    void closeAuxiliaryWindows();
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
    // Their transcription-mode pick, kept while the instrumental adapter
    // overrides the box so toggling instrumental does not lose it.
    int chosenTranscriptionModeId = 1;

    juce::Label planningLabel;
    CustomComboBox keyRootComboBox;
    CustomComboBox keyModeComboBox;
    CustomComboBox meterComboBox;
    BpmControl bpmControl;
    juce::Label hostBpmLabel;
    juce::Label tempoLabel;
    bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();
    double hostBpm = 120.0;

    juce::Label lengthLabel;
    CustomButton naturalLengthButton;
    CustomButton fixedLengthButton;
    CustomComboBox createBarsComboBox;
    bool createFixedBars = false;

    juce::Label sourceLabel;
    juce::ToggleButton recordingSourceButton;
    juce::ToggleButton outputSourceButton;
    juce::ToggleButton midiSourceButton;
    bool midiSourceSelected = false;
    YueyMidiSlot melodySlot { "melody" };
    YueyMidiSlot chordsSlot { "chords" };
    juce::Label midiNoteLabel;
    juce::String midiNote;
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

    // Shared by all three sub-tabs: a seed means the same thing to each.
    juce::ToggleButton useSeedToggle;
    CustomTextEditor seedEditor;
    juce::Label lastSeedLabel;
    juce::String lastSeed;

    CustomButton actionButton;
    juce::Label infoLabel;
    bool canCreate = false;
    bool canRemix = false;
    bool canContinue = false;
    bool generating = false;

    std::vector<juce::Component::SafePointer<juce::DialogWindow>> auxiliaryWindows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YueyUI)
};
