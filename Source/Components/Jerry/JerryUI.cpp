#include "JerryUI.h"
#include "../../Utils/Theme.h"

namespace
{
    constexpr int kOuterMargin = 12;
    constexpr int kTitleHeight = 28;
    constexpr int kPromptLabelHeight = 12;      // Reduced from 18
    constexpr int kPromptEditorHeight = 24;     // Reduced from 26
    constexpr int kRowHeight = 20;              // Reduced from 26
    constexpr int kSmartLoopHeight = 22;        // Reduced from 28
    constexpr int kBpmHeight = 14;              // Reduced from 16
    constexpr int kButtonHeight = 32;           // Keep same
    constexpr int kLabelWidth = 70;             // Reduced from 92 (shorter labels)
    constexpr int kInterRowGap = 2;             // Reduced from 3
    constexpr int kLoopButtonGap = 4;
}

JerryUI::JerryUI()
{
    jerryLabel.setText("jerry (stable audio open small)", juce::dontSendNotification);
    jerryLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    jerryLabel.setColour(juce::Label::textColourId, Theme::Colors::TextPrimary);
    jerryLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(jerryLabel);

    // Model selector
    jerryModelLabel.setText("model", juce::dontSendNotification);
    jerryModelLabel.setFont(juce::FontOptions(12.0f));
    jerryModelLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    jerryModelLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(jerryModelLabel);

    jerryModelComboBox.setTextWhenNothingSelected("loading models...");
    jerryModelComboBox.onChange = [this]()
        {
            const int selectedId = jerryModelComboBox.getSelectedId();
            const int newIndex = selectedId > 0 ? selectedId - 1 : 0;

            if (selectedModelIndex != newIndex)
            {
                selectedModelIndex = newIndex;

                bool isFinetune = false;
                if (newIndex >= 0 && newIndex < modelIsFinetune.size())
                    isFinetune = modelIsFinetune[newIndex];

                // Update UI for model type
                updateSliderRangesForModel(isFinetune);
                updateSamplerVisibility();

                if (onModelChanged)
                    onModelChanged(newIndex, isFinetune);
            }
        };
    addAndMakeVisible(jerryModelComboBox);

    // Sampler type selector (hidden by default, shown for finetunes)
    jerrySamplerLabel.setText("sampler", juce::dontSendNotification);
    jerrySamplerLabel.setFont(juce::FontOptions(12.0f));
    jerrySamplerLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    jerrySamplerLabel.setJustificationType(juce::Justification::centredLeft);
    jerrySamplerLabel.setVisible(false);
    addAndMakeVisible(jerrySamplerLabel);

    samplerEulerButton.setButtonText("euler");
    samplerEulerButton.setRadioGroupId(2001);  // Different from Terry's 1001
    samplerEulerButton.setToggleState(false, juce::dontSendNotification);
    samplerEulerButton.onClick = [this]()
        {
            // Radio group handles mutual exclusivity automatically
            // Just update our internal state
            currentSamplerType = "euler";
            if (onSamplerTypeChanged)
                onSamplerTypeChanged(currentSamplerType);
        };
    samplerEulerButton.setVisible(false);
    addAndMakeVisible(samplerEulerButton);

    samplerDpmppButton.setButtonText("dpmpp");
    samplerDpmppButton.setRadioGroupId(2001);
    samplerDpmppButton.setToggleState(true, juce::dontSendNotification);  // Default for finetunes
    samplerDpmppButton.onClick = [this]()
        {
            // Radio group handles mutual exclusivity automatically
            // Just update our internal state
            currentSamplerType = "dpmpp";
            if (onSamplerTypeChanged)
                onSamplerTypeChanged(currentSamplerType);
        };
    samplerDpmppButton.setVisible(false);
    addAndMakeVisible(samplerDpmppButton);

    // Custom finetune section (localhost only)
    toggleCustomSectionButton.setButtonText("+");
    toggleCustomSectionButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    toggleCustomSectionButton.setTooltip("add custom finetune (localhost only)");
    toggleCustomSectionButton.onClick = [this]() { toggleCustomFinetuneSection(); };
    toggleCustomSectionButton.setVisible(false);  // Hidden by default
    addAndMakeVisible(toggleCustomSectionButton);

    customFinetuneLabel.setText("add custom finetune", juce::dontSendNotification);
    customFinetuneLabel.setFont(juce::FontOptions(11.0f));
    customFinetuneLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    customFinetuneLabel.setJustificationType(juce::Justification::centredLeft);
    customFinetuneLabel.setVisible(false);
    addAndMakeVisible(customFinetuneLabel);

    repoTextEditor.setTextToShowWhenEmpty("thepatch/jerry_grunge", juce::Colours::darkgrey);
    repoTextEditor.setMultiLine(false);
    repoTextEditor.setReturnKeyStartsNewLine(false);
    repoTextEditor.setScrollbarsShown(false);
    repoTextEditor.setBorder(juce::BorderSize<int>(2));
    repoTextEditor.setVisible(false);
    addAndMakeVisible(repoTextEditor);

    fetchCheckpointsButton.setButtonText("fetch");
    fetchCheckpointsButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    fetchCheckpointsButton.onClick = [this]() {
        auto repo = repoTextEditor.getText().trim();
        if (repo.isEmpty()) {
            repo = "thepatch/jerry_grunge";  // Default
        }
        if (onFetchCheckpoints)
            onFetchCheckpoints(repo);
    };
    fetchCheckpointsButton.setVisible(false);
    addAndMakeVisible(fetchCheckpointsButton);

    checkpointComboBox.setTextWhenNothingSelected("fetch checkpoints first...");
    checkpointComboBox.setVisible(false);
    addAndMakeVisible(checkpointComboBox);

    addModelButton.setButtonText("add to models");
    addModelButton.setButtonStyle(CustomButton::ButtonStyle::Jerry);
    addModelButton.onClick = [this]() {
        auto repo = repoTextEditor.getText().trim();
        if (repo.isEmpty()) repo = "thepatch/jerry_grunge";

        auto selectedId = checkpointComboBox.getSelectedId();
        if (selectedId <= 0) return;

        auto checkpoint = checkpointComboBox.getItemText(selectedId - 1);
        if (onAddCustomModel)
            onAddCustomModel(repo, checkpoint);

        // Collapse section after adding
        toggleCustomFinetuneSection();
    };
    addModelButton.setEnabled(false);
    addModelButton.setVisible(false);
    addAndMakeVisible(addModelButton);

    jerryPromptLabel.setText("text prompt", juce::dontSendNotification);
    jerryPromptLabel.setFont(juce::FontOptions(12.0f));
    jerryPromptLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    jerryPromptLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(jerryPromptLabel);

    jerryPromptEditor.setTextToShowWhenEmpty("enter your audio generation prompt here...", juce::Colours::darkgrey);
    jerryPromptEditor.setMultiLine(false);
    jerryPromptEditor.setReturnKeyStartsNewLine(false);
    jerryPromptEditor.setScrollbarsShown(false);
    jerryPromptEditor.setBorder(juce::BorderSize<int>(2));
    jerryPromptEditor.onTextChange = [this]()
        {
            promptText = jerryPromptEditor.getText();
            if (onPromptChanged)
                onPromptChanged(promptText);
        };
    addAndMakeVisible(jerryPromptEditor);

    jerryCfgLabel.setText("cfg scale", juce::dontSendNotification);
    jerryCfgLabel.setFont(juce::FontOptions(12.0f));
    jerryCfgLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    jerryCfgLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(jerryCfgLabel);

    jerryCfgSlider.setRange(0.5, 2.0, 0.1);
    jerryCfgSlider.setValue(cfg);
    jerryCfgSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    jerryCfgSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    jerryCfgSlider.onValueChange = [this]()
        {
            cfg = (float)jerryCfgSlider.getValue();
            if (onCfgChanged)
                onCfgChanged(cfg);
        };
    addAndMakeVisible(jerryCfgSlider);

    jerryStepsLabel.setText("steps", juce::dontSendNotification);
    jerryStepsLabel.setFont(juce::FontOptions(12.0f));
    jerryStepsLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    jerryStepsLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(jerryStepsLabel);

    jerryStepsSlider.setRange(4.0, 16.0, 1.0);
    jerryStepsSlider.setValue(steps);
    jerryStepsSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    jerryStepsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    jerryStepsSlider.onValueChange = [this]()
        {
            steps = (int)jerryStepsSlider.getValue();
            if (onStepsChanged)
                onStepsChanged(steps);
        };
    addAndMakeVisible(jerryStepsSlider);

    jerryBpmLabel.setFont(juce::FontOptions(11.0f));
    jerryBpmLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    jerryBpmLabel.setJustificationType(juce::Justification::centred);
    jerryBpmLabel.setText("bpm: " + juce::String(bpmValue) + " (from daw)", juce::dontSendNotification);
    addAndMakeVisible(jerryBpmLabel);

    generateWithJerryButton.setButtonText("generate with jerry");
    generateWithJerryButton.setButtonStyle(CustomButton::ButtonStyle::Jerry);
    generateWithJerryButton.setTooltip("generate audio from text prompt with current daw bpm");
    generateWithJerryButton.onClick = [this]()
        {
            if (onGenerate)
                onGenerate();
        };
    addAndMakeVisible(generateWithJerryButton);

    generateAsLoopButton.setButtonText("smart loop");
    generateAsLoopButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    generateAsLoopButton.setClickingTogglesState(true);
    generateAsLoopButton.onClick = [this]()
        {
            smartLoop = generateAsLoopButton.getToggleState();
            updateSmartLoopStyle();
            refreshLoopTypeVisibility();
            applyEnablement(lastCanGenerate, lastCanSmartLoop, lastIsGenerating);
            if (onSmartLoopToggled)
                onSmartLoopToggled(smartLoop);
        };
    addAndMakeVisible(generateAsLoopButton);

    auto handleLoopTypeClick = [this](int index)
        {
            loopTypeIndex = index;
            updateLoopTypeStyles();
            if (onLoopTypeChanged)
                onLoopTypeChanged(loopTypeIndex);
        };

    loopTypeAutoButton.setButtonText("auto");
    loopTypeAutoButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    loopTypeAutoButton.onClick = [this, handleLoopTypeClick]() { handleLoopTypeClick(0); };
    addAndMakeVisible(loopTypeAutoButton);

    loopTypeDrumsButton.setButtonText("drums");
    loopTypeDrumsButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    loopTypeDrumsButton.onClick = [this, handleLoopTypeClick]() { handleLoopTypeClick(1); };
    addAndMakeVisible(loopTypeDrumsButton);

    loopTypeInstrumentsButton.setButtonText("instr");
    loopTypeInstrumentsButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    loopTypeInstrumentsButton.onClick = [this, handleLoopTypeClick]() { handleLoopTypeClick(2); };
    addAndMakeVisible(loopTypeInstrumentsButton);

    updateLoopTypeStyles();
    updateSmartLoopStyle();
    refreshLoopTypeVisibility();
}

void JerryUI::paint(juce::Graphics&)
{
}

void JerryUI::resized()
{
    auto area = getLocalBounds().reduced(kOuterMargin);

    // Title at the very top
    titleBounds = area.removeFromTop(kTitleHeight);
    jerryLabel.setBounds(titleBounds);
    area.removeFromTop(kInterRowGap);

    // Model selector row with "+" button
    auto modelRow = area.removeFromTop(kRowHeight);
    auto modelLabelBounds = modelRow.removeFromLeft(kLabelWidth);
    jerryModelLabel.setBounds(modelLabelBounds);

    // Reserve space for "+" button if on localhost
    if (isUsingLocalhost) {
        auto plusButtonBounds = modelRow.removeFromRight(25);
        toggleCustomSectionButton.setBounds(plusButtonBounds);
        modelRow.removeFromRight(3);  // Small gap
    }

    jerryModelComboBox.setBounds(modelRow);
    area.removeFromTop(kInterRowGap);

    // Custom finetune section (collapsible)
    if (showingCustomFinetuneSection && isUsingLocalhost) {
        // Label
        auto customLabelBounds = area.removeFromTop(kPromptLabelHeight);
        customFinetuneLabel.setBounds(customLabelBounds);
        area.removeFromTop(kInterRowGap);

        // Repo text + fetch button
        auto repoRow = area.removeFromTop(kPromptEditorHeight);
        auto fetchButtonBounds = repoRow.removeFromRight(60);
        repoRow.removeFromRight(3);  // Gap
        repoTextEditor.setBounds(repoRow);
        fetchCheckpointsButton.setBounds(fetchButtonBounds);
        area.removeFromTop(kInterRowGap);

        // Checkpoint dropdown
        auto checkpointRow = area.removeFromTop(kRowHeight);
        checkpointComboBox.setBounds(checkpointRow);
        area.removeFromTop(kInterRowGap);

        // Add button
        auto addButtonRow = area.removeFromTop(kButtonHeight);
        auto addButtonBounds = addButtonRow.withWidth(150).withCentre(addButtonRow.getCentre());
        addModelButton.setBounds(addButtonBounds);
        area.removeFromTop(kInterRowGap);
    }

    // Sampler selector row (only visible for finetunes)
    if (showingSamplerSelector)
    {
        auto samplerRow = area.removeFromTop(kRowHeight);
        auto samplerLabelBounds = samplerRow.removeFromLeft(kLabelWidth);
        jerrySamplerLabel.setBounds(samplerLabelBounds);

        // Layout the two radio buttons side by side - MADE MORE COMPACT
        const int buttonWidth = 60;     // Reduced from 70
        const int buttonGap = 4;        // Reduced from 5

        auto eulerBounds = samplerRow.removeFromLeft(buttonWidth);
        samplerEulerButton.setBounds(eulerBounds);

        samplerRow.removeFromLeft(buttonGap);

        auto dpmppBounds = samplerRow.removeFromLeft(buttonWidth);
        samplerDpmppButton.setBounds(dpmppBounds);

        area.removeFromTop(kInterRowGap);
    }

    // Prompt label
    auto promptLabelBounds = area.removeFromTop(kPromptLabelHeight);
    jerryPromptLabel.setBounds(promptLabelBounds);
    area.removeFromTop(kInterRowGap);

    // Prompt editor
    auto promptBounds = area.removeFromTop(kPromptEditorHeight);
    jerryPromptEditor.setBounds(promptBounds);
    area.removeFromTop(kInterRowGap);

    // CFG row
    auto cfgRow = area.removeFromTop(kRowHeight);
    auto cfgLabelBounds = cfgRow.removeFromLeft(kLabelWidth);
    jerryCfgLabel.setBounds(cfgLabelBounds);
    jerryCfgSlider.setBounds(cfgRow);
    area.removeFromTop(kInterRowGap);

    // Steps row
    auto stepsRow = area.removeFromTop(kRowHeight);
    auto stepsLabelBounds = stepsRow.removeFromLeft(kLabelWidth);
    jerryStepsLabel.setBounds(stepsLabelBounds);
    jerryStepsSlider.setBounds(stepsRow);
    area.removeFromTop(kInterRowGap);

    // Smart loop row (existing logic)
    auto smartLoopRow = area.removeFromTop(kSmartLoopHeight);
    const int smartLoopWidth = juce::jmin(110, smartLoopRow.getWidth());
    auto smartLoopButtonBounds = smartLoopRow.removeFromLeft(smartLoopWidth);
    generateAsLoopButton.setBounds(smartLoopButtonBounds);

    if (smartLoopRow.getWidth() > 0)
    {
        auto autoBounds = smartLoopRow.removeFromLeft(48);
        loopTypeAutoButton.setBounds(autoBounds);

        smartLoopRow.removeFromLeft(kLoopButtonGap);
        auto drumsBounds = smartLoopRow.removeFromLeft(58);
        loopTypeDrumsButton.setBounds(drumsBounds);

        smartLoopRow.removeFromLeft(kLoopButtonGap);
        loopTypeInstrumentsButton.setBounds(smartLoopRow);
    }
    area.removeFromTop(kInterRowGap);

    // BPM label
    auto bpmBounds = area.removeFromTop(kBpmHeight);
    jerryBpmLabel.setBounds(bpmBounds);
    area.removeFromTop(kInterRowGap);

    // Generate button
    auto generateRow = area.removeFromTop(kButtonHeight);
    auto buttonWidth = juce::jmin(240, generateRow.getWidth());
    auto buttonBounds = generateRow.withWidth(buttonWidth).withCentre(generateRow.getCentre());
    generateWithJerryButton.setBounds(buttonBounds);
}

void JerryUI::setVisibleForTab(bool visible)
{
    setVisible(visible);
    setInterceptsMouseClicks(visible, visible);
}

void JerryUI::setAvailableModels(const juce::StringArray& models,
    const juce::Array<bool>& isFinetune,
    const juce::StringArray& keys,
    const juce::StringArray& types,
    const juce::StringArray& repos,
    const juce::StringArray& checkpoints)
{
    modelNames = models;
    modelIsFinetune = isFinetune;
    modelKeys = keys;
    modelTypes = types;           // NEW
    modelRepos = repos;           // NEW
    modelCheckpoints = checkpoints; // NEW

    jerryModelComboBox.clear(juce::dontSendNotification);
    for (int i = 0; i < models.size(); ++i)
        jerryModelComboBox.addItem(models[i], i + 1);

    if (models.size() > 0)
    {
        selectedModelIndex = 0;
        jerryModelComboBox.setSelectedId(1, juce::dontSendNotification);

        // Set initial state based on first model
        bool firstIsFinetune = isFinetune.size() > 0 ? isFinetune[0] : false;
        updateSliderRangesForModel(firstIsFinetune);
        updateSamplerVisibility();

        // IMPORTANT: Trigger the callback so PluginEditor knows about the initial selection
        if (onModelChanged)
            onModelChanged(0, firstIsFinetune);
    }
}

juce::String JerryUI::getSelectedModelType() const
{
    if (selectedModelIndex >= 0 && selectedModelIndex < modelTypes.size())
        return modelTypes[selectedModelIndex];
    return "standard";
}

juce::String JerryUI::getSelectedFinetuneRepo() const
{
    if (selectedModelIndex >= 0 && selectedModelIndex < modelRepos.size())
        return modelRepos[selectedModelIndex];
    return "";
}

juce::String JerryUI::getSelectedFinetuneCheckpoint() const
{
    if (selectedModelIndex >= 0 && selectedModelIndex < modelCheckpoints.size())
        return modelCheckpoints[selectedModelIndex];
    return "";
}

void JerryUI::setSelectedModel(int index)
{
    if (index >= 0 && index < modelNames.size())
    {
        selectedModelIndex = index;
        jerryModelComboBox.setSelectedId(index + 1, juce::dontSendNotification);

        bool isFinetune = modelIsFinetune[index];
        updateSliderRangesForModel(isFinetune);
        updateSamplerVisibility();
    }
}

int JerryUI::getSelectedModelIndex() const
{
    return selectedModelIndex;
}

juce::String JerryUI::getSelectedModelKey() const
{
    if (selectedModelIndex >= 0 && selectedModelIndex < modelKeys.size())
        return modelKeys[selectedModelIndex];
    return "standard_saos";
}

bool JerryUI::getSelectedModelIsFinetune() const
{
    if (selectedModelIndex >= 0 && selectedModelIndex < modelIsFinetune.size())
        return modelIsFinetune[selectedModelIndex];
    return false;
}

juce::String JerryUI::getSelectedSamplerType() const
{
    return currentSamplerType;
}

void JerryUI::updateSamplerVisibility()
{
    bool isFinetune = getSelectedModelIsFinetune();
    showingSamplerSelector = isFinetune;

    jerrySamplerLabel.setVisible(isFinetune);
    samplerEulerButton.setVisible(isFinetune);
    samplerDpmppButton.setVisible(isFinetune);

    // Reset sampler to default when switching
    if (isFinetune)
    {
        // Default to dpmpp for finetunes
        currentSamplerType = "dpmpp";
        samplerDpmppButton.setToggleState(true, juce::dontSendNotification);
        samplerEulerButton.setToggleState(false, juce::dontSendNotification);
    }
    else
    {
        // Standard model always uses pingpong
        currentSamplerType = "pingpong";
    }

    resized();  // Re-layout to show/hide sampler row
}

void JerryUI::updateSliderRangesForModel(bool isFinetune)
{
    if (isFinetune)
    {
        // Finetune ranges: steps 4-50 (default 30), cfg 1.0-7.0 (default 4.0)
        jerryStepsSlider.setRange(4.0, 50.0, 1.0);
        jerryStepsSlider.setValue(30, juce::sendNotification);

        jerryCfgSlider.setRange(1.0, 7.0, 0.1);
        jerryCfgSlider.setValue(4.0, juce::sendNotification);
    }
    else
    {
        // Standard SAOS ranges: steps 4-16 (default 8), cfg 0.5-2.0 (default 1.0)
        jerryStepsSlider.setRange(4.0, 16.0, 1.0);
        jerryStepsSlider.setValue(8, juce::sendNotification);

        jerryCfgSlider.setRange(0.5, 2.0, 0.1);
        jerryCfgSlider.setValue(1.0, juce::sendNotification);
    }

    DBG("Updated slider ranges for " + juce::String(isFinetune ? "finetune" : "standard") + " model");
}

void JerryUI::setBpm(int bpm)
{
    bpmValue = bpm;
    jerryBpmLabel.setText("bpm: " + juce::String(bpmValue) + " (from daw)", juce::dontSendNotification);
}

void JerryUI::setPromptText(const juce::String& text)
{
    promptText = text;
    jerryPromptEditor.setText(text, false);
}

void JerryUI::setCfg(float value)
{
    cfg = value;
    jerryCfgSlider.setValue(value, juce::dontSendNotification);
}

void JerryUI::setSteps(int value)
{
    steps = value;
    jerryStepsSlider.setValue(value, juce::dontSendNotification);
}

void JerryUI::setSmartLoop(bool enabled)
{
    smartLoop = enabled;
    generateAsLoopButton.setToggleState(enabled, juce::dontSendNotification);
    updateSmartLoopStyle();
    refreshLoopTypeVisibility();
    applyEnablement(lastCanGenerate, lastCanSmartLoop, lastIsGenerating);
}

void JerryUI::setLoopType(int index)
{
    loopTypeIndex = juce::jlimit(0, 2, index);
    updateLoopTypeStyles();
}

void JerryUI::setButtonsEnabled(bool canGenerate, bool canSmartLoop, bool isGenerating)
{
    applyEnablement(canGenerate, canSmartLoop, isGenerating);
}

void JerryUI::setGenerateButtonText(const juce::String& text)
{
    generateWithJerryButton.setButtonText(text);
}

juce::String JerryUI::getPromptText() const
{
    return promptText;
}

float JerryUI::getCfg() const
{
    return cfg;
}

int JerryUI::getSteps() const
{
    return steps;
}

bool JerryUI::getSmartLoop() const
{
    return smartLoop;
}

int JerryUI::getLoopType() const
{
    return loopTypeIndex;
}

juce::Rectangle<int> JerryUI::getTitleBounds() const
{
    return titleBounds;
}

void JerryUI::refreshLoopTypeVisibility()
{
    const bool showLoopButtons = smartLoop;

    loopTypeAutoButton.setVisible(showLoopButtons);
    loopTypeDrumsButton.setVisible(showLoopButtons);
    loopTypeInstrumentsButton.setVisible(showLoopButtons);
}

void JerryUI::updateLoopTypeStyles()
{
    loopTypeAutoButton.setButtonStyle(loopTypeIndex == 0 ? CustomButton::ButtonStyle::Gary
        : CustomButton::ButtonStyle::Standard);
    loopTypeDrumsButton.setButtonStyle(loopTypeIndex == 1 ? CustomButton::ButtonStyle::Gary
        : CustomButton::ButtonStyle::Standard);
    loopTypeInstrumentsButton.setButtonStyle(loopTypeIndex == 2 ? CustomButton::ButtonStyle::Gary
        : CustomButton::ButtonStyle::Standard);
}

void JerryUI::updateSmartLoopStyle()
{
    generateAsLoopButton.setRadioGroupId(0);

    if (smartLoop)
    {
        generateAsLoopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
        generateAsLoopButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        generateAsLoopButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }
    else
    {
        generateAsLoopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        generateAsLoopButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        generateAsLoopButton.setColour(juce::TextButton::textColourOnId, juce::Colours::lightgrey);
    }
}

void JerryUI::applySamplerSelection(const juce::String& samplerType)
{
    currentSamplerType = samplerType;

    // For programmatic changes, we need to explicitly set both button states
    // Use dontSendNotification to avoid triggering callbacks
    if (samplerType == "euler")
    {
        samplerEulerButton.setToggleState(true, juce::dontSendNotification);
        samplerDpmppButton.setToggleState(false, juce::dontSendNotification);
    }
    else if (samplerType == "dpmpp")
    {
        samplerDpmppButton.setToggleState(true, juce::dontSendNotification);
        samplerEulerButton.setToggleState(false, juce::dontSendNotification);
    }

    // Don't trigger callback here - this is for programmatic restoration
    // If you want to trigger callback, call it manually after this function
}

void JerryUI::applyEnablement(bool canGenerate, bool canSmartLoop, bool isGenerating)
{
    lastCanGenerate = canGenerate;
    lastCanSmartLoop = canSmartLoop;
    lastIsGenerating = isGenerating;

    // IMPORTANT: Disable if model is loading
    const bool allowGenerate = canGenerate && !isGenerating && !isLoadingModel;
    const bool allowSmartLoop = canSmartLoop && !isGenerating && !isLoadingModel;
    const bool allowLoopTypes = allowSmartLoop && smartLoop;

    generateWithJerryButton.setEnabled(allowGenerate);
    generateAsLoopButton.setEnabled(allowSmartLoop);
    loopTypeAutoButton.setEnabled(allowLoopTypes);
    loopTypeDrumsButton.setEnabled(allowLoopTypes);
    loopTypeInstrumentsButton.setEnabled(allowLoopTypes);
}

void JerryUI::setUsingLocalhost(bool localhost)
{
    isUsingLocalhost = localhost;
    toggleCustomSectionButton.setVisible(localhost);

    // If switching away from localhost, hide custom section
    if (!localhost && showingCustomFinetuneSection) {
        toggleCustomFinetuneSection();
    }

    resized();
}

void JerryUI::toggleCustomFinetuneSection()
{
    showingCustomFinetuneSection = !showingCustomFinetuneSection;

    customFinetuneLabel.setVisible(showingCustomFinetuneSection);
    repoTextEditor.setVisible(showingCustomFinetuneSection);
    fetchCheckpointsButton.setVisible(showingCustomFinetuneSection);
    checkpointComboBox.setVisible(showingCustomFinetuneSection);
    addModelButton.setVisible(showingCustomFinetuneSection);

    // Update button text
    toggleCustomSectionButton.setButtonText(showingCustomFinetuneSection ? "-" : "+");

    resized();
}

void JerryUI::setFetchingCheckpoints(bool fetching)
{
    isFetchingCheckpoints = fetching;
    fetchCheckpointsButton.setEnabled(!fetching);
    fetchCheckpointsButton.setButtonText(fetching ? "fetching..." : "fetch");
}

void JerryUI::setAvailableCheckpoints(const juce::StringArray& checkpoints)
{
    checkpointComboBox.clear();

    for (int i = 0; i < checkpoints.size(); ++i) {
        checkpointComboBox.addItem(checkpoints[i], i + 1);
    }

    addModelButton.setEnabled(checkpoints.size() > 0);

    if (checkpoints.size() > 0) {
        checkpointComboBox.setSelectedId(1);  // Select first by default
    }
}

void JerryUI::setLoadingModel(bool loading, const juce::String& modelInfo)
{
    isLoadingModel = loading;

    if (loading)
    {
        // CRITICAL: Clear the current selection to avoid showing stale model name
        jerryModelComboBox.setSelectedId(0, juce::dontSendNotification);

        // Add a temporary "Loading..." item to the dropdown and select it
        // This ensures the dropdown visibly shows the loading state
        jerryModelComboBox.clear(juce::dontSendNotification);

        if (!modelInfo.isEmpty())
        {
            jerryModelComboBox.addItem("Loading " + modelInfo + "...", 999);
        }
        else
        {
            jerryModelComboBox.addItem("Loading model...", 999);
        }

        jerryModelComboBox.setSelectedId(999, juce::dontSendNotification);

        // Disable generation during load
        generateWithJerryButton.setEnabled(false);
        generateAsLoopButton.setEnabled(false);
    }
    else
    {
        // Loading complete - the model list will be refreshed by fetchJerryAvailableModels()
        // No need to do anything here, just re-enable buttons
        applyEnablement(lastCanGenerate, lastCanSmartLoop, lastIsGenerating);
    }
}

void JerryUI::selectModelByRepo(const juce::String& repo)
{
    // Look through the current model list to find a finetune model from the specified repo
    // and select it as the active model
    for (int i = 0; i < modelRepos.size(); ++i)
    {
        if (modelRepos[i] == repo && modelIsFinetune[i])
        {
            DBG("Found and selecting model from repo: " + repo + " at index " + juce::String(i));
            setSelectedModel(i);
            break;
        }
    }
}
