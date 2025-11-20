#pragma once

#include <JuceHeader.h>

#include "../Base/CustomButton.h"
#include "../Base/CustomSlider.h"
#include "../Base/CustomTextEditor.h"

#include "BeatPrompts.h"
#include "InstrumentPrompts.h"

#include <functional>

class JerryUI : public juce::Component
{
public:
    JerryUI();

    void paint(juce::Graphics&) override;
    void resized() override;

    void setVisibleForTab(bool visible);

    void setBpm(int bpm);
    void setPromptText(const juce::String& text);
    void setCfg(float value);
    void setSteps(int value);
    void setSmartLoop(bool enabled);
    void setLoopType(int index);
    void setButtonsEnabled(bool canGenerate, bool canSmartLoop, bool isGenerating);
    void setGenerateButtonText(const juce::String& text);

    juce::String getPromptText() const;
    float getCfg() const;
    int getSteps() const;
    bool getSmartLoop() const;
    int getLoopType() const;

    // Standalone BPM control
    void setIsStandalone(bool standalone);
    void setManualBpm(int bpm);
    int getManualBpm() const;

    juce::Rectangle<int> getTitleBounds() const;

    std::function<void(const juce::String&)> onPromptChanged;
    std::function<void(float)> onCfgChanged;
    std::function<void(int)> onStepsChanged;
    std::function<void(bool)> onSmartLoopToggled;
    std::function<void(int)> onLoopTypeChanged;
    std::function<void()> onGenerate;
    std::function<void(int)> onManualBpmChanged;

    // Model selection
    void setAvailableModels(const juce::StringArray& models,
        const juce::Array<bool>& isFinetune,
        const juce::StringArray& keys,
        const juce::StringArray& types,
        const juce::StringArray& repos,
        const juce::StringArray& checkpoints);
    void setSelectedModel(int index);
    int getSelectedModelIndex() const;
    juce::String getSelectedModelKey() const;
    bool getSelectedModelIsFinetune() const;
    juce::String getSelectedSamplerType() const;

    // Callbacks
    std::function<void(int, bool)> onModelChanged;  // index, isFinetune
    std::function<void(const juce::String&)> onSamplerTypeChanged;

    // Custom finetune callbacks
    std::function<void(const juce::String&)> onFetchCheckpoints;
    std::function<void(const juce::String&, const juce::String&)> onAddCustomModel;

    juce::String getSelectedModelType() const;
    juce::String getSelectedFinetuneRepo() const;
    juce::String getSelectedFinetuneCheckpoint() const;

    // Custom finetune methods
    void setUsingLocalhost(bool localhost);
    void setFetchingCheckpoints(bool fetching);
    void setAvailableCheckpoints(const juce::StringArray& checkpoints);
    void toggleCustomFinetuneSection();
    void setLoadingModel(bool loading, const juce::String& modelInfo = "");
    void selectModelByRepo(const juce::String& repo);

    void setFinetunePromptBank(const juce::String& repo,
        const juce::String& checkpoint,
        const juce::var& promptsJson);

    //juce::String generateConditionalPrompt(); // your dice handler calls this

private:
    // Prompt generators
    BeatPrompts beatPrompts;
    InstrumentPrompts instrumentPrompts;

    // Dice button
    std::unique_ptr<CustomButton> promptDiceButton;

    // Helper methods
    void drawDiceIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isHovered, bool isPressed);
    juce::String generateConditionalPrompt();

    juce::HashMap<juce::String, juce::var> finetunePromptBanks; // key = repo + "|" + checkpoint

    void refreshLoopTypeVisibility();
    void updateLoopTypeStyles();
    void updateSmartLoopStyle();
    void applyEnablement(bool canGenerate, bool canSmartLoop, bool isGenerating);

    juce::Label jerryLabel;
    juce::Label jerryPromptLabel;
    CustomTextEditor jerryPromptEditor;
    CustomSlider jerryCfgSlider;
    juce::Label jerryCfgLabel;
    CustomSlider jerryStepsSlider;
    juce::Label jerryStepsLabel;
    juce::Label jerryBpmLabel;
    CustomSlider jerryBpmSlider;        // Manual BPM control (standalone only)
    CustomButton generateWithJerryButton;
    CustomButton generateAsLoopButton;
    CustomButton loopTypeAutoButton;
    CustomButton loopTypeDrumsButton;
    CustomButton loopTypeInstrumentsButton;

    juce::String promptText;
    float cfg { 1.0f };
    int steps { 8 };
    bool smartLoop { false };
    int loopTypeIndex { 0 };
    int bpmValue { 120 };
    bool isStandaloneMode { false };      // Track plugin vs standalone

    bool lastCanGenerate { false };
    bool lastCanSmartLoop { false };
    bool lastIsGenerating { false };

    // Model selection components
    juce::Label jerryModelLabel;
    juce::ComboBox jerryModelComboBox;

    // Sampler type components (only for finetunes)
    juce::Label jerrySamplerLabel;
    juce::ToggleButton samplerEulerButton;
    juce::ToggleButton samplerDpmppButton;

    // Model state
    juce::StringArray modelNames;
    juce::StringArray modelKeys;
    juce::StringArray modelTypes;           // NEW: 'standard' or 'finetune'
    juce::StringArray modelRepos;           // NEW: repo names
    juce::StringArray modelCheckpoints;     // NEW: checkpoint filenames
    juce::Array<bool> modelIsFinetune;
    int selectedModelIndex = 0;
    bool showingSamplerSelector = false;
    juce::String currentSamplerType = "pingpong";  // Default for standard

    // Helper methods
    void updateSamplerVisibility();
    void updateSliderRangesForModel(bool isFinetune);
    void applySamplerSelection(const juce::String& samplerType);

    // Custom finetune UI (only shown on localhost)
    juce::Label customFinetuneLabel;
    juce::TextEditor repoTextEditor;
    CustomButton fetchCheckpointsButton;
    juce::ComboBox checkpointComboBox;
    CustomButton addModelButton;
    CustomButton toggleCustomSectionButton;  // "+" button

    bool showingCustomFinetuneSection = false;
    bool isUsingLocalhost = false;
    bool isFetchingCheckpoints = false;
    bool isLoadingModel = false;

    juce::Rectangle<int> titleBounds;
};
