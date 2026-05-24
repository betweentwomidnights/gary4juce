#include "SA3UI.h"

namespace
{
    constexpr int kOuterMargin = 10;
    constexpr int kTitleHeight = 26;
    constexpr int kRowHeight = 24;
    constexpr int kLabelHeight = 16;
    constexpr int kGap = 4;
    constexpr int kLabelWidth = 66;

    const juce::StringArray& keyNames()
    {
        static const juce::StringArray keys = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return keys;
    }
}

SA3UI::SA3UI()
{
    titleLabel.setText("sa3 (stable audio 3)", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, Theme::Colors::TextPrimary);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    auto prepareSubTab = [this](CustomButton& button, const juce::String& text, SubTab tab)
    {
        button.setButtonText(text);
        button.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        button.onClick = [this, tab]() { setCurrentSubTab(tab); };
        addAndMakeVisible(button);
    };

    prepareSubTab(generateSubTabButton, "generate", SubTab::Generate);
    prepareSubTab(transformSubTabButton, "transform", SubTab::Transform);
    prepareSubTab(continueSubTabButton, "continue", SubTab::Continue);

    keyScaleLabel.setText("key", juce::dontSendNotification);
    keyScaleLabel.setFont(juce::FontOptions(11.0f));
    keyScaleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    keyScaleLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(keyScaleLabel);

    keyRootComboBox.addItem("none", 1);
    for (int i = 0; i < keyNames().size(); ++i)
        keyRootComboBox.addItem(keyNames()[i], i + 2);
    keyRootComboBox.setSelectedId(1, juce::dontSendNotification);
    keyRootComboBox.setTooltip("key root");
    keyRootComboBox.onChange = [this]() { updateKeyScaleVisibility(true); };
    addAndMakeVisible(keyRootComboBox);

    keyModeComboBox.addItem("major", 1);
    keyModeComboBox.addItem("minor", 2);
    keyModeComboBox.setSelectedId(1, juce::dontSendNotification);
    keyModeComboBox.setTooltip("major or minor");
    keyModeComboBox.onChange = [this]()
    {
        if (onKeyScaleChanged)
            onKeyScaleChanged(getKeyScale());
    };
    keyModeComboBox.setVisible(false);
    addAndMakeVisible(keyModeComboBox);

    bpmLabel.setFont(juce::FontOptions(11.0f));
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    bpmLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(bpmLabel);

    contentComponent = std::make_unique<juce::Component>();
    contentViewport = std::make_unique<juce::Viewport>();
    contentViewport->setViewedComponent(contentComponent.get(), false);
    contentViewport->setScrollBarsShown(true, false);
    customLookAndFeel.setScrollbarAccentColour(Theme::Colors::Jerry);
    contentViewport->getVerticalScrollBar().setLookAndFeel(&customLookAndFeel);
    addAndMakeVisible(contentViewport.get());

    promptLabel.setText("prompt", juce::dontSendNotification);
    promptLabel.setFont(juce::FontOptions(12.0f));
    promptLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    promptLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(promptLabel);

    promptEditor.setTextToShowWhenEmpty("describe the audio to generate...", juce::Colours::darkgrey);
    promptEditor.setMultiLine(false);
    promptEditor.setReturnKeyStartsNewLine(false);
    promptEditor.setScrollbarsShown(false);
    promptEditor.setBorder(juce::BorderSize<int>(2));
    promptEditor.onTextChange = [this]()
    {
        if (onPromptChanged)
            onPromptChanged(promptEditor.getText());
    };
    addToContent(promptEditor);

    diceButton.setButtonText("");
    diceButton.setButtonStyle(CustomButton::ButtonStyle::Jerry);
    diceButton.setTooltip("roll an SA3 prompt");
    diceButton.setEnabled(false);
    diceButton.onClick = [this]()
    {
        if (onGenerateDiceRequested)
            onGenerateDiceRequested();
    };
    diceButton.onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        drawDiceIcon(g, bounds.toFloat().reduced(2.0f), diceButton.isMouseOver(), diceButton.isDown());
    };
    addToContent(diceButton);

    transformPromptLabel.setText("prompt", juce::dontSendNotification);
    transformPromptLabel.setFont(juce::FontOptions(12.0f));
    transformPromptLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    transformPromptLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(transformPromptLabel);

    transformPromptEditor.setTextToShowWhenEmpty("describe the transformed audio...", juce::Colours::darkgrey);
    transformPromptEditor.setMultiLine(false);
    transformPromptEditor.setReturnKeyStartsNewLine(false);
    transformPromptEditor.setScrollbarsShown(false);
    transformPromptEditor.setBorder(juce::BorderSize<int>(2));
    transformPromptEditor.onTextChange = [this]()
    {
        if (onTransformPromptChanged)
            onTransformPromptChanged(transformPromptEditor.getText());
    };
    addToContent(transformPromptEditor);

    transformDiceButton.setButtonText("");
    transformDiceButton.setButtonStyle(CustomButton::ButtonStyle::Jerry);
    transformDiceButton.setTooltip("roll an SA3 transform prompt");
    transformDiceButton.setEnabled(false);
    transformDiceButton.onClick = [this]()
    {
        if (onTransformDiceRequested)
            onTransformDiceRequested();
    };
    transformDiceButton.onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        drawDiceIcon(g, bounds.toFloat().reduced(2.0f), transformDiceButton.isMouseOver(), transformDiceButton.isDown());
    };
    addToContent(transformDiceButton);

    transformSourceLabel.setText("transform", juce::dontSendNotification);
    transformSourceLabel.setFont(juce::FontOptions(12.0f));
    transformSourceLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    transformSourceLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(transformSourceLabel);

    transformRecordingButton.setButtonText("recording");
    transformRecordingButton.setRadioGroupId(3003);
    transformRecordingButton.setTooltip("transform the saved recording");
    transformRecordingButton.onClick = [this]()
    {
        if (!transformRecordingButton.getToggleState())
        {
            transformRecordingButton.setToggleState(true, juce::dontSendNotification);
            transformOutputButton.setToggleState(false, juce::dontSendNotification);
        }
        transformAudioSourceRecording = true;
        if (onTransformAudioSourceChanged)
            onTransformAudioSourceChanged(true);
    };
    addToContent(transformRecordingButton);

    transformOutputButton.setButtonText("output");
    transformOutputButton.setRadioGroupId(3003);
    transformOutputButton.setToggleState(true, juce::dontSendNotification);
    transformOutputButton.setTooltip("transform the current output");
    transformOutputButton.onClick = [this]()
    {
        if (!transformOutputButton.getToggleState())
        {
            transformOutputButton.setToggleState(true, juce::dontSendNotification);
            transformRecordingButton.setToggleState(false, juce::dontSendNotification);
        }
        transformAudioSourceRecording = false;
        if (onTransformAudioSourceChanged)
            onTransformAudioSourceChanged(false);
    };
    addToContent(transformOutputButton);

    transformStrengthLabel.setText("init noise", juce::dontSendNotification);
    transformStrengthLabel.setFont(juce::FontOptions(12.0f));
    transformStrengthLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    transformStrengthLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(transformStrengthLabel);

    transformStrengthSlider.setRange(0.01, 1.0, 0.01);
    transformStrengthSlider.setValue(0.9, juce::dontSendNotification);
    transformStrengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    transformStrengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    transformStrengthSlider.setNumDecimalPlacesToDisplay(2);
    transformStrengthSlider.setTooltip("init noise strength; lower preserves source, higher transforms more");
    transformStrengthSlider.onValueChange = [this]()
    {
        if (onTransformStrengthChanged)
            onTransformStrengthChanged(getTransformStrength());
    };
    addToContent(transformStrengthSlider);

    continuePromptLabel.setText("prompt", juce::dontSendNotification);
    continuePromptLabel.setFont(juce::FontOptions(12.0f));
    continuePromptLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    continuePromptLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(continuePromptLabel);

    continuePromptEditor.setTextToShowWhenEmpty("describe where the audio should go next...", juce::Colours::darkgrey);
    continuePromptEditor.setMultiLine(false);
    continuePromptEditor.setReturnKeyStartsNewLine(false);
    continuePromptEditor.setScrollbarsShown(false);
    continuePromptEditor.setBorder(juce::BorderSize<int>(2));
    continuePromptEditor.onTextChange = [this]()
    {
        if (onContinuePromptChanged)
            onContinuePromptChanged(continuePromptEditor.getText());
    };
    addToContent(continuePromptEditor);

    continueDiceButton.setButtonText("");
    continueDiceButton.setButtonStyle(CustomButton::ButtonStyle::Jerry);
    continueDiceButton.setTooltip("roll an SA3 continuation prompt");
    continueDiceButton.setEnabled(false);
    continueDiceButton.onClick = [this]()
    {
        if (onContinueDiceRequested)
            onContinueDiceRequested();
    };
    continueDiceButton.onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        drawDiceIcon(g, bounds.toFloat().reduced(2.0f), continueDiceButton.isMouseOver(), continueDiceButton.isDown());
    };
    addToContent(continueDiceButton);

    continueSourceLabel.setText("continue", juce::dontSendNotification);
    continueSourceLabel.setFont(juce::FontOptions(12.0f));
    continueSourceLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    continueSourceLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(continueSourceLabel);

    continueRecordingButton.setButtonText("recording");
    continueRecordingButton.setRadioGroupId(3004);
    continueRecordingButton.setTooltip("continue the saved recording");
    continueRecordingButton.onClick = [this]()
    {
        if (!continueRecordingButton.getToggleState())
        {
            continueRecordingButton.setToggleState(true, juce::dontSendNotification);
            continueOutputButton.setToggleState(false, juce::dontSendNotification);
        }
        transformAudioSourceRecording = true;
        setTransformAudioSourceRecording(true);
        if (onContinueAudioSourceChanged)
            onContinueAudioSourceChanged(true);
    };
    addToContent(continueRecordingButton);

    continueOutputButton.setButtonText("output");
    continueOutputButton.setRadioGroupId(3004);
    continueOutputButton.setToggleState(true, juce::dontSendNotification);
    continueOutputButton.setTooltip("continue the current output");
    continueOutputButton.onClick = [this]()
    {
        if (!continueOutputButton.getToggleState())
        {
            continueOutputButton.setToggleState(true, juce::dontSendNotification);
            continueRecordingButton.setToggleState(false, juce::dontSendNotification);
        }
        transformAudioSourceRecording = false;
        setTransformAudioSourceRecording(false);
        if (onContinueAudioSourceChanged)
            onContinueAudioSourceChanged(false);
    };
    addToContent(continueOutputButton);

    continuationLabel.setText("duration", juce::dontSendNotification);
    continuationLabel.setFont(juce::FontOptions(12.0f));
    continuationLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    continuationLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(continuationLabel);

    continuationSlider.setRange(1.0, 300.0, 1.0);
    continuationSlider.setValue(30.0, juce::dontSendNotification);
    continuationSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    continuationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    continuationSlider.setTooltip("total output length including the source audio");
    continuationSlider.onValueChange = [this]()
    {
        if (onContinueTotalSecondsChanged)
            onContinueTotalSecondsChanged(getContinueTotalSeconds());
    };
    addToContent(continuationSlider);

    durationLabel.setText("duration", juce::dontSendNotification);
    durationLabel.setFont(juce::FontOptions(12.0f));
    durationLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    durationLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(durationLabel);

    durationSlider.setRange(1.0, 300.0, 1.0);
    durationSlider.setValue(30.0, juce::dontSendNotification);
    durationSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    durationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    durationSlider.setTooltip("duration in seconds");
    durationSlider.onValueChange = [this]()
    {
        if (onDurationChanged)
            onDurationChanged(getDurationSeconds());
    };
    addToContent(durationSlider);

    loopToggle.setButtonText("loop");
    loopToggle.setButtonStyle(CustomButton::ButtonStyle::Standard);
    loopToggle.setClickingTogglesState(true);
    loopToggle.setTooltip("bar-aligned loop mode");
    loopToggle.onClick = [this]()
    {
        loopEnabled = loopToggle.getToggleState();
        updateLoopControls();
        if (onLoopChanged)
            onLoopChanged(loopEnabled);
    };
    addToContent(loopToggle);

    auto setupBarsButton = [this](CustomButton& button, int bars)
    {
        button.setButtonText(juce::String(bars));
        button.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        button.onClick = [this, bars]()
        {
            setBars(bars);
            if (onBarsChanged)
                onBarsChanged(selectedBars);
        };
        addToContent(button);
    };
    setupBarsButton(bars4Button, 4);
    setupBarsButton(bars8Button, 8);
    setupBarsButton(bars16Button, 16);

    advancedToggle.setButtonText(juce::String::fromUTF8("advanced \xe2\x96\xb6"));
    advancedToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
    advancedToggle.onClick = [this]()
    {
        advancedOpen = !advancedOpen;
        updateAdvancedToggleText();
        updateContentLayout();
    };
    addToContent(advancedToggle);

    stepsLabel.setText("steps", juce::dontSendNotification);
    stepsLabel.setFont(juce::FontOptions(12.0f));
    stepsLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    stepsLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(stepsLabel);

    stepsSlider.setRange(1.0, 16.0, 1.0);
    stepsSlider.setValue(8.0, juce::dontSendNotification);
    stepsSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    stepsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    stepsSlider.setTooltip("diffusion steps");
    stepsSlider.onValueChange = [this]()
    {
        if (onStepsChanged)
            onStepsChanged(getSteps());
    };
    addToContent(stepsSlider);

    cfgLabel.setText("cfg", juce::dontSendNotification);
    cfgLabel.setFont(juce::FontOptions(12.0f));
    cfgLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    cfgLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(cfgLabel);

    cfgSlider.setRange(0.5, 2.0, 0.1);
    cfgSlider.setValue(1.0, juce::dontSendNotification);
    cfgSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    cfgSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    cfgSlider.setTooltip("classifier-free guidance");
    cfgSlider.onValueChange = [this]()
    {
        if (onCfgChanged)
            onCfgChanged(getCfgScale());
    };
    addToContent(cfgSlider);

    negativePromptLabel.setText("negative", juce::dontSendNotification);
    negativePromptLabel.setFont(juce::FontOptions(12.0f));
    negativePromptLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    negativePromptLabel.setJustificationType(juce::Justification::centredLeft);
    negativePromptLabel.setTooltip("blank uses backend default 'low quality'; active LoRAs may weaken or ignore negative prompting");
    addToContent(negativePromptLabel);

    negativePromptEditor.setTextToShowWhenEmpty("low quality", juce::Colour(0xff666666));
    negativePromptEditor.setMultiLine(false);
    negativePromptEditor.setReturnKeyStartsNewLine(false);
    negativePromptEditor.setScrollbarsShown(false);
    negativePromptEditor.setBorder(juce::BorderSize<int>(2));
    negativePromptEditor.onTextChange = [this]()
    {
        if (onNegativePromptChanged)
            onNegativePromptChanged(getNegativePromptText());
    };
    addToContent(negativePromptEditor);

    useSeedToggle.setButtonText("use seed");
    useSeedToggle.setToggleState(false, juce::dontSendNotification);
    useSeedToggle.setTooltip("when enabled, submit the seed value below instead of asking the backend for a random seed");
    useSeedToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcccccc));
    useSeedToggle.setColour(juce::ToggleButton::tickColourId, Theme::Colors::Jerry);
    useSeedToggle.onClick = [this]()
    {
        const bool on = useSeedToggle.getToggleState();
        seedEditor.setEnabled(on);
        seedEditor.setAlpha(on ? 1.0f : 0.45f);
    };
    addToContent(useSeedToggle);

    seedEditor.setMultiLine(false);
    seedEditor.setTextToShowWhenEmpty("random", juce::Colour(0xff666666));
    seedEditor.setTooltip("seed for reproducible SA3 generations");
    seedEditor.setInputRestrictions(20, "0123456789");
    seedEditor.setEnabled(false);
    seedEditor.setAlpha(0.45f);
    addToContent(seedEditor);

    lastSeedLabel.setText("last seed: -", juce::dontSendNotification);
    lastSeedLabel.setFont(juce::FontOptions(11.0f));
    lastSeedLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    lastSeedLabel.setJustificationType(juce::Justification::centred);
    addToContent(lastSeedLabel);

    continueModeLabel.setText("continuation mode", juce::dontSendNotification);
    continueModeLabel.setFont(juce::FontOptions(12.0f));
    continueModeLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    continueModeLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(continueModeLabel);

    continueModeStandardButton.setButtonText("standard");
    continueModeStandardButton.setRadioGroupId(3005);
    continueModeStandardButton.setToggleState(true, juce::dontSendNotification);
    continueModeStandardButton.setTooltip("standard inpaint continuation mode");
    continueModeStandardButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcccccc));
    continueModeStandardButton.setColour(juce::ToggleButton::tickColourId, Theme::Colors::Jerry);
    continueModeStandardButton.onClick = [this]()
    {
        setContinueLatentPrefixEnabled(false);
        if (onContinueLatentPrefixChanged)
            onContinueLatentPrefixChanged(false);
    };
    addToContent(continueModeStandardButton);

    continueModeLatentPrefixButton.setButtonText("latent_prefix");
    continueModeLatentPrefixButton.setRadioGroupId(3005);
    continueModeLatentPrefixButton.setToggleState(false, juce::dontSendNotification);
    continueModeLatentPrefixButton.setTooltip("we will have to find out in realtime whether it's useful, stupid or doesn't change much");
    continueModeLatentPrefixButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcccccc));
    continueModeLatentPrefixButton.setColour(juce::ToggleButton::tickColourId, Theme::Colors::Jerry);
    continueModeLatentPrefixButton.onClick = [this]()
    {
        setContinueLatentPrefixEnabled(true);
        if (onContinueLatentPrefixChanged)
            onContinueLatentPrefixChanged(true);
    };
    addToContent(continueModeLatentPrefixButton);

    useLoraToggle.setButtonText("use lora");
    useLoraToggle.setToggleState(false, juce::dontSendNotification);
    useLoraToggle.setTooltip("reveal SA3 LoRA strength sliders");
    useLoraToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcccccc));
    useLoraToggle.setColour(juce::ToggleButton::tickColourId, Theme::Colors::Jerry);
    useLoraToggle.onClick = [this]()
    {
        updateLoraControls();
        updateContentLayout();
        if (onLoraSelectionChanged)
            onLoraSelectionChanged();
    };
    addToContent(useLoraToggle);

    loraStatusLabel.setText("loras loading...", juce::dontSendNotification);
    loraStatusLabel.setFont(juce::FontOptions(11.0f));
    loraStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    loraStatusLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(loraStatusLabel);

    shiftLabel.setText("shift", juce::dontSendNotification);
    shiftLabel.setFont(juce::FontOptions(12.0f));
    shiftLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    shiftLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(shiftLabel);

    shiftComboBox.addItem("full", 1);
    shiftComboBox.addItem("none", 2);
    shiftComboBox.addItem("logsnr", 3);
    shiftComboBox.addItem("flux", 4);
    shiftComboBox.setSelectedId(1, juce::dontSendNotification);
    shiftComboBox.setTooltip("distribution shift");
    shiftComboBox.onChange = [this]()
    {
        if (onShiftChanged)
            onShiftChanged(getShift());
    };
    addToContent(shiftComboBox);

    generateButton.setButtonText("generate with sa3");
    generateButton.setButtonStyle(CustomButton::ButtonStyle::Jerry);
    generateButton.setTooltip("generate with Stable Audio 3");
    generateButton.onClick = [this]()
    {
        if (currentSubTab == SubTab::Continue)
        {
            if (onContinue)
                onContinue();
        }
        else if (currentSubTab == SubTab::Transform)
        {
            if (onTransform)
                onTransform();
        }
        else if (onGenerate)
            onGenerate();
    };
    addToContent(generateButton);

    infoLabel.setText("", juce::dontSendNotification);
    infoLabel.setFont(juce::FontOptions(11.0f));
    infoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    infoLabel.setJustificationType(juce::Justification::centred);
    addToContent(infoLabel);

    updateSubTabButtonStyles();
    updateBarsButtonStyles();
    updateLoopControls();
    updateAdvancedToggleText();
    updateKeyScaleVisibility(false);
    setBpm(bpmValue);
}

SA3UI::~SA3UI()
{
    if (contentViewport)
        contentViewport->getVerticalScrollBar().setLookAndFeel(nullptr);
}

void SA3UI::paint(juce::Graphics&)
{
}

void SA3UI::resized()
{
    auto area = getLocalBounds().reduced(kOuterMargin);

    titleBounds = area.removeFromTop(titleHidden ? 0 : kTitleHeight);
    titleLabel.setBounds(titleBounds);
    if (!titleHidden)
        area.removeFromTop(kGap);

    auto tabRow = area.removeFromTop(24);
    const int tabW = (tabRow.getWidth() - (kGap * 2)) / 3;
    generateSubTabButton.setBounds(tabRow.removeFromLeft(tabW));
    tabRow.removeFromLeft(kGap);
    transformSubTabButton.setBounds(tabRow.removeFromLeft(tabW));
    tabRow.removeFromLeft(kGap);
    continueSubTabButton.setBounds(tabRow);
    area.removeFromTop(kGap);

    auto keyRow = area.removeFromTop(24);
    keyScaleLabel.setBounds(keyRow.removeFromLeft(28));
    keyRow.removeFromLeft(kGap);
    keyRootComboBox.setBounds(keyRow.removeFromLeft(72));
    if (keyModeComboBox.isVisible())
    {
        keyRow.removeFromLeft(kGap);
        keyModeComboBox.setBounds(keyRow.removeFromLeft(76));
    }
    bpmLabel.setBounds(keyRow);
    area.removeFromTop(kGap);

    if (contentViewport)
        contentViewport->setBounds(area);

    updateContentLayout();
}

void SA3UI::setVisibleForTab(bool visible)
{
    setVisible(visible);
    setInterceptsMouseClicks(visible, visible);
}

void SA3UI::setTitleVisible(bool visible)
{
    titleLabel.setVisible(visible);
    titleHidden = !visible;
    resized();
}

void SA3UI::setBpm(double bpm)
{
    bpmValue = bpm > 0.0 ? bpm : 120.0;
    bpmLabel.setText(juce::String(juce::roundToInt(bpmValue)) + " bpm", juce::dontSendNotification);
}

void SA3UI::setRemoteAvailable(bool available)
{
    remoteAvailable = available;
    infoLabel.setText(remoteAvailable ? "" : "sa3 offline", juce::dontSendNotification);
    setGenerateButtonEnabled(lastCanGenerate, lastIsGenerating);
    setDiceButtonsEnabled(lastDiceButtonsEnabled);
}

void SA3UI::setGenerateButtonEnabled(bool enabled, bool isGenerating)
{
    lastCanGenerate = enabled;
    lastIsGenerating = isGenerating;
    const bool isActionableSubTab = currentSubTab == SubTab::Generate
        || currentSubTab == SubTab::Transform
        || currentSubTab == SubTab::Continue;
    generateButton.setEnabled(remoteAvailable && enabled && !isGenerating && isActionableSubTab);
}

void SA3UI::setDiceButtonsEnabled(bool enabled)
{
    lastDiceButtonsEnabled = enabled;
    const bool canRoll = remoteAvailable && enabled;
    diceButton.setEnabled(canRoll);
    transformDiceButton.setEnabled(canRoll);
    continueDiceButton.setEnabled(canRoll);
}

void SA3UI::setGenerateButtonText(const juce::String& text)
{
    generateButton.setButtonText(text);
}

void SA3UI::setPromptText(const juce::String& text)
{
    promptEditor.setText(text, false);
}

void SA3UI::setNegativePromptText(const juce::String& text)
{
    negativePromptEditor.setText(text, false);
}

void SA3UI::setTransformPromptText(const juce::String& text)
{
    transformPromptEditor.setText(text, false);
}

void SA3UI::setTransformStrength(double value)
{
    transformStrengthSlider.setValue(juce::jlimit(0.01, 1.0, value), juce::dontSendNotification);
}

void SA3UI::setTransformAudioSourceRecording(bool useRecording)
{
    transformAudioSourceRecording = useRecording;
    transformRecordingButton.setToggleState(useRecording, juce::dontSendNotification);
    transformOutputButton.setToggleState(!useRecording, juce::dontSendNotification);
    continueRecordingButton.setToggleState(useRecording, juce::dontSendNotification);
    continueOutputButton.setToggleState(!useRecording, juce::dontSendNotification);
}

void SA3UI::setTransformAudioSourceAvailability(bool recordingAvailable, bool outputAvailable)
{
    transformRecordingSourceAvailable = recordingAvailable;
    transformOutputSourceAvailable = outputAvailable;
    transformRecordingButton.setEnabled(recordingAvailable);
    transformOutputButton.setEnabled(outputAvailable);
    continueRecordingButton.setEnabled(recordingAvailable);
    continueOutputButton.setEnabled(outputAvailable);
}

void SA3UI::setContinuePromptText(const juce::String& text)
{
    continuePromptEditor.setText(text, false);
}

void SA3UI::setContinueTotalSeconds(int seconds)
{
    continuationSlider.setValue(juce::jlimit(1, 300, seconds), juce::dontSendNotification);
}

void SA3UI::setContinueLatentPrefixEnabled(bool enabled)
{
    continueModeStandardButton.setToggleState(!enabled, juce::dontSendNotification);
    continueModeLatentPrefixButton.setToggleState(enabled, juce::dontSendNotification);
}

void SA3UI::setContinueAudioSourceRecording(bool useRecording)
{
    setTransformAudioSourceRecording(useRecording);
}

void SA3UI::setContinueAudioSourceAvailability(bool recordingAvailable, bool outputAvailable)
{
    setTransformAudioSourceAvailability(recordingAvailable, outputAvailable);
}

void SA3UI::setDurationSeconds(int seconds)
{
    durationSlider.setValue(juce::jlimit(1, 300, seconds), juce::dontSendNotification);
}

void SA3UI::setLoopEnabled(bool enabled)
{
    loopEnabled = enabled;
    loopToggle.setToggleState(enabled, juce::dontSendNotification);
    updateLoopControls();
}

void SA3UI::setBars(int bars)
{
    if (bars != 4 && bars != 8 && bars != 16)
        bars = 8;

    selectedBars = bars;
    updateBarsButtonStyles();
}

void SA3UI::setSteps(int steps)
{
    stepsSlider.setValue(juce::jlimit(1, 16, steps), juce::dontSendNotification);
}

void SA3UI::setCfgScale(double value)
{
    cfgSlider.setValue(juce::jlimit(0.5, 2.0, value), juce::dontSendNotification);
}

juce::int64 SA3UI::getSeed() const
{
    if (!useSeedToggle.getToggleState())
        return -1;

    const auto text = seedEditor.getText().trim();
    if (text.isEmpty())
        return -1;

    return text.getLargeIntValue();
}

void SA3UI::setLastSeed(const juce::String& seed)
{
    const auto trimmed = seed.trim();
    if (trimmed.isEmpty())
        return;

    lastSeedLabel.setText("last seed: " + trimmed, juce::dontSendNotification);
    seedEditor.setText(trimmed, false);
}

std::vector<SA3UI::LoraSelection> SA3UI::getActiveLoras() const
{
    std::vector<LoraSelection> selections;
    if (!useLoraToggle.getToggleState())
        return selections;

    for (const auto& row : loraRows)
    {
        if (!row.slider)
            continue;

        const double strength = row.slider->getValue();
        if (strength > 0.0)
            selections.push_back({ row.name, strength });
    }

    return selections;
}

void SA3UI::setAvailableLoras(const juce::StringArray& loraNames)
{
    juce::HashMap<juce::String, double> preservedStrengths;
    for (const auto& row : loraRows)
        if (row.slider)
            preservedStrengths.set(row.name, row.slider->getValue());

    clearLoraRows();

    for (const auto& name : loraNames)
    {
        LoraRow row;
        row.name = name;

        row.label = std::make_unique<juce::Label>();
        row.label->setText(name, juce::dontSendNotification);
        row.label->setFont(juce::FontOptions(11.0f));
        row.label->setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
        row.label->setJustificationType(juce::Justification::centredLeft);
        row.label->setTooltip(name);
        addToContent(*row.label);

        row.slider = std::make_unique<CustomSlider>();
        row.slider->setRange(0.0, 2.0, 0.01);
        row.slider->setValue(preservedStrengths.contains(name) ? preservedStrengths[name] : 0.0,
                             juce::dontSendNotification);
        row.slider->setSliderStyle(juce::Slider::LinearHorizontal);
        row.slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
        row.slider->setTooltip("LoRA strength");
        row.slider->onValueChange = [this]()
        {
            if (onLoraSelectionChanged)
                onLoraSelectionChanged();
        };
        addToContent(*row.slider);

        loraRows.push_back(std::move(row));
    }

    if (loraRows.empty())
    {
        useLoraToggle.setToggleState(false, juce::dontSendNotification);
        useLoraToggle.setEnabled(false);
        loraStatusLabel.setText("no loras available", juce::dontSendNotification);
    }
    else
    {
        useLoraToggle.setEnabled(true);
        loraStatusLabel.setText(juce::String(loraRows.size()) + " loras available", juce::dontSendNotification);
    }

    updateLoraControls();
    updateContentLayout();
}

juce::String SA3UI::getShift() const
{
    auto value = shiftComboBox.getText().trim().toLowerCase();
    return value.isNotEmpty() ? value : "full";
}

void SA3UI::setShift(const juce::String& shift)
{
    const auto lower = shift.trim().toLowerCase();
    if (lower == "none") shiftComboBox.setSelectedId(2, juce::dontSendNotification);
    else if (lower == "logsnr") shiftComboBox.setSelectedId(3, juce::dontSendNotification);
    else if (lower == "flux") shiftComboBox.setSelectedId(4, juce::dontSendNotification);
    else shiftComboBox.setSelectedId(1, juce::dontSendNotification);
}

juce::String SA3UI::getKeyScale() const
{
    if (keyRootComboBox.getSelectedId() <= 1)
        return {};
    return keyRootComboBox.getText() + " " + keyModeComboBox.getText();
}

void SA3UI::setKeyScale(const juce::String& keyScale)
{
    if (keyScale.trim().isEmpty())
    {
        keyRootComboBox.setSelectedId(1, juce::dontSendNotification);
        updateKeyScaleVisibility(false);
        return;
    }

    const juce::String lower = keyScale.trim().toLowerCase();
    const bool isMinor = lower.contains("minor");
    const juce::String root = keyScale.trim().upToFirstOccurrenceOf(" ", false, true).trim();
    for (int i = 0; i < keyRootComboBox.getNumItems(); ++i)
    {
        if (keyRootComboBox.getItemText(i).equalsIgnoreCase(root))
        {
            keyRootComboBox.setSelectedItemIndex(i, juce::dontSendNotification);
            break;
        }
    }

    keyModeComboBox.setSelectedId(isMinor ? 2 : 1, juce::dontSendNotification);
    updateKeyScaleVisibility(false);
}

void SA3UI::setCurrentSubTab(SubTab tab)
{
    setCurrentSubTabInternal(tab, true);
}

void SA3UI::addToContent(juce::Component& component)
{
    if (contentComponent)
        contentComponent->addAndMakeVisible(component);
}

void SA3UI::setCurrentSubTabInternal(SubTab tab, bool notify)
{
    currentSubTab = tab;
    updateSubTabButtonStyles();
    updateContentLayout();
    setGenerateButtonEnabled(lastCanGenerate, lastIsGenerating);
    if (notify && onSubTabChanged)
        onSubTabChanged(currentSubTab);
}

void SA3UI::updateSubTabButtonStyles()
{
    generateSubTabButton.setButtonStyle(currentSubTab == SubTab::Generate
        ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
    transformSubTabButton.setButtonStyle(currentSubTab == SubTab::Transform
        ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
    continueSubTabButton.setButtonStyle(currentSubTab == SubTab::Continue
        ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
}

void SA3UI::updateLoopControls()
{
    loopToggle.setButtonStyle(loopEnabled ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Standard);
    durationSlider.setEnabled(!loopEnabled);
    durationSlider.setAlpha(loopEnabled ? 0.45f : 1.0f);
    durationLabel.setAlpha(loopEnabled ? 0.45f : 1.0f);
    bars4Button.setVisible(loopEnabled);
    bars8Button.setVisible(loopEnabled);
    bars16Button.setVisible(loopEnabled);
    updateBarsButtonStyles();
    updateContentLayout();
}

void SA3UI::updateBarsButtonStyles()
{
    bars4Button.setButtonStyle(selectedBars == 4 ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
    bars8Button.setButtonStyle(selectedBars == 8 ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
    bars16Button.setButtonStyle(selectedBars == 16 ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
}

void SA3UI::updateAdvancedToggleText()
{
    advancedToggle.setButtonText(advancedOpen
        ? juce::String::fromUTF8("advanced \xe2\x96\xbc")
        : juce::String::fromUTF8("advanced \xe2\x96\xb6"));
}

void SA3UI::updateKeyScaleVisibility(bool notify)
{
    keyModeComboBox.setVisible(keyRootComboBox.getSelectedId() > 1);
    if (notify && onKeyScaleChanged)
        onKeyScaleChanged(getKeyScale());
    resized();
}

void SA3UI::updateLoraControls()
{
    const bool showRows = advancedOpen && useLoraToggle.getToggleState() && !loraRows.empty();
    for (auto& row : loraRows)
    {
        if (row.label)
            row.label->setVisible(showRows);
        if (row.slider)
            row.slider->setVisible(showRows);
    }
}

void SA3UI::clearLoraRows()
{
    if (contentComponent)
    {
        for (auto& row : loraRows)
        {
            if (row.label)
                contentComponent->removeChildComponent(row.label.get());
            if (row.slider)
                contentComponent->removeChildComponent(row.slider.get());
        }
    }

    loraRows.clear();
}

void SA3UI::updateContentLayout()
{
    if (contentComponent == nullptr || contentViewport == nullptr)
        return;

    const int viewportWidth = juce::jmax(220, contentViewport->getWidth());
    const int scrollbarWidth = contentViewport->getVerticalScrollBar().isVisible()
        ? contentViewport->getVerticalScrollBar().getWidth() : 0;
    const int contentWidth = juce::jmax(220, viewportWidth - scrollbarWidth - 4);

    constexpr int sidePadding = 8;
    int y = 8;
    const auto fullRow = [&](int height)
    {
        return juce::Rectangle<int>(sidePadding, y, contentWidth - (sidePadding * 2), height);
    };

    const bool showGenerate = currentSubTab == SubTab::Generate;
    const bool showTransform = currentSubTab == SubTab::Transform;
    const bool showContinue = currentSubTab == SubTab::Continue;
    const bool showActionTab = showGenerate || showTransform || showContinue;
    generateButton.setTooltip(showContinue
        ? "continue the selected audio source with Stable Audio 3"
        : showTransform
            ? "transform the selected audio source with Stable Audio 3"
            : "generate with Stable Audio 3");

    promptLabel.setVisible(showGenerate);
    promptEditor.setVisible(showGenerate);
    diceButton.setVisible(showGenerate);
    durationLabel.setVisible(showGenerate);
    durationSlider.setVisible(showGenerate);
    loopToggle.setVisible(showGenerate);

    transformPromptLabel.setVisible(showTransform);
    transformPromptEditor.setVisible(showTransform);
    transformDiceButton.setVisible(showTransform);
    transformSourceLabel.setVisible(showTransform);
    transformRecordingButton.setVisible(showTransform);
    transformOutputButton.setVisible(showTransform);
    transformStrengthLabel.setVisible(showTransform);
    transformStrengthSlider.setVisible(showTransform);

    continuePromptLabel.setVisible(showContinue);
    continuePromptEditor.setVisible(showContinue);
    continueDiceButton.setVisible(showContinue);
    continueSourceLabel.setVisible(showContinue);
    continueRecordingButton.setVisible(showContinue);
    continueOutputButton.setVisible(showContinue);
    continuationLabel.setVisible(showContinue);
    continuationSlider.setVisible(showContinue);

    advancedToggle.setVisible(showActionTab);
    generateButton.setVisible(showActionTab);
    lastSeedLabel.setVisible(showActionTab);
    infoLabel.setVisible(true);

    const bool showAdvancedControls = showActionTab && advancedOpen;
    stepsLabel.setVisible(showAdvancedControls);
    stepsSlider.setVisible(showAdvancedControls);
    cfgLabel.setVisible(showAdvancedControls);
    cfgSlider.setVisible(showAdvancedControls);
    negativePromptLabel.setVisible(showAdvancedControls);
    negativePromptEditor.setVisible(showAdvancedControls);
    useSeedToggle.setVisible(showAdvancedControls);
    seedEditor.setVisible(showAdvancedControls);
    continueModeLabel.setVisible(showAdvancedControls && showContinue);
    continueModeStandardButton.setVisible(showAdvancedControls && showContinue);
    continueModeLatentPrefixButton.setVisible(showAdvancedControls && showContinue);
    useLoraToggle.setVisible(showAdvancedControls);
    loraStatusLabel.setVisible(showAdvancedControls);
    shiftLabel.setVisible(showAdvancedControls);
    shiftComboBox.setVisible(showAdvancedControls);
    bars4Button.setVisible(showGenerate && loopEnabled);
    bars8Button.setVisible(showGenerate && loopEnabled);
    bars16Button.setVisible(showGenerate && loopEnabled);

    for (auto& row : loraRows)
    {
        if (row.label)
            row.label->setVisible(false);
        if (row.slider)
            row.slider->setVisible(false);
    }

    auto layoutAdvanced = [&]()
    {
        advancedToggle.setBounds(fullRow(24));
        y += 30;

        if (!advancedOpen)
            return;

        auto stepsRow = fullRow(kRowHeight);
        stepsLabel.setBounds(stepsRow.removeFromLeft(kLabelWidth));
        stepsSlider.setBounds(stepsRow);
        y += kRowHeight + kGap;

        auto cfgRow = fullRow(kRowHeight);
        cfgLabel.setBounds(cfgRow.removeFromLeft(kLabelWidth));
        cfgSlider.setBounds(cfgRow);
        y += kRowHeight + kGap;

        auto negativeRow = fullRow(26);
        negativePromptLabel.setBounds(negativeRow.removeFromLeft(kLabelWidth));
        negativePromptEditor.setBounds(negativeRow);
        y += 30;

        auto shiftRow = fullRow(kRowHeight);
        shiftLabel.setBounds(shiftRow.removeFromLeft(kLabelWidth));
        shiftComboBox.setBounds(shiftRow.removeFromLeft(120));
        y += kRowHeight + kGap;

        auto seedRow = fullRow(22);
        useSeedToggle.setBounds(seedRow.removeFromLeft(92));
        seedRow.removeFromLeft(kGap);
        seedEditor.setBounds(seedRow.removeFromLeft(120));
        y += 26;

        if (showContinue)
        {
            continueModeLabel.setBounds(fullRow(kLabelHeight));
            y += kLabelHeight + 2;

            auto continueModeRow = fullRow(24);
            continueModeStandardButton.setBounds(continueModeRow.removeFromLeft(juce::jmin(86, continueModeRow.getWidth() / 2)));
            continueModeRow.removeFromLeft(kGap);
            continueModeLatentPrefixButton.setBounds(continueModeRow);
            y += 28;
        }

        useLoraToggle.setBounds(fullRow(22));
        y += 24;

        loraStatusLabel.setBounds(fullRow(16));
        y += 18;

        const bool showLoraRows = useLoraToggle.getToggleState() && !loraRows.empty();
        for (auto& row : loraRows)
        {
            if (row.label)
            {
                row.label->setVisible(showLoraRows);
                if (showLoraRows)
                {
                    auto loraRow = fullRow(24);
                    row.label->setBounds(loraRow.removeFromLeft(118));
                    if (row.slider)
                        row.slider->setBounds(loraRow);
                }
            }
            if (row.slider)
                row.slider->setVisible(showLoraRows);

            if (showLoraRows)
                y += 28;
        }
    };

    auto layoutAction = [&]()
    {
        lastSeedLabel.setBounds(fullRow(16));
        y += 22;

        auto generateRow = fullRow(32);
        generateButton.setBounds(generateRow.withWidth(220).withCentre(generateRow.getCentre()));
        y += 38;

        infoLabel.setText(remoteAvailable ? "" : "sa3 offline", juce::dontSendNotification);
        infoLabel.setBounds(fullRow(16));
        y += 22;
    };

    if (showGenerate)
    {
        promptLabel.setBounds(fullRow(kLabelHeight)); y += kLabelHeight + 2;
        auto promptRow = fullRow(26);
        auto diceBounds = promptRow.removeFromRight(22);
        promptRow.removeFromRight(3);
        promptEditor.setBounds(promptRow);
        diceButton.setBounds(diceBounds.withHeight(22).withY(diceBounds.getY() + 2));
        y += 30;

        auto durationRow = fullRow(kRowHeight);
        durationLabel.setBounds(durationRow.removeFromLeft(kLabelWidth));
        durationSlider.setBounds(durationRow);
        y += kRowHeight + kGap;

        auto loopRow = fullRow(24);
        loopToggle.setBounds(loopRow.removeFromLeft(74));
        if (loopEnabled)
        {
            loopRow.removeFromLeft(kGap);
            const int bw = juce::jmin(46, loopRow.getWidth() / 3);
            bars4Button.setBounds(loopRow.removeFromLeft(bw));
            loopRow.removeFromLeft(kGap);
            bars8Button.setBounds(loopRow.removeFromLeft(bw));
            loopRow.removeFromLeft(kGap);
            bars16Button.setBounds(loopRow.removeFromLeft(bw));
        }
        y += 28;

        layoutAdvanced();
        layoutAction();
    }
    else if (showTransform)
    {
        transformPromptLabel.setBounds(fullRow(kLabelHeight)); y += kLabelHeight + 2;
        auto promptRow = fullRow(26);
        auto diceBounds = promptRow.removeFromRight(22);
        promptRow.removeFromRight(3);
        transformPromptEditor.setBounds(promptRow);
        transformDiceButton.setBounds(diceBounds.withHeight(22).withY(diceBounds.getY() + 2));
        y += 30;

        auto sourceRow = fullRow(24);
        transformSourceLabel.setBounds(sourceRow.removeFromLeft(kLabelWidth));
        sourceRow.removeFromLeft(kGap);
        transformRecordingButton.setBounds(sourceRow.removeFromLeft(88));
        sourceRow.removeFromLeft(kGap);
        transformOutputButton.setBounds(sourceRow.removeFromLeft(74));
        y += 28;

        auto strengthRow = fullRow(kRowHeight);
        transformStrengthLabel.setBounds(strengthRow.removeFromLeft(kLabelWidth));
        transformStrengthSlider.setBounds(strengthRow);
        y += kRowHeight + kGap;

        layoutAdvanced();
        layoutAction();
    }
    else if (showContinue)
    {
        continuePromptLabel.setBounds(fullRow(kLabelHeight)); y += kLabelHeight + 2;
        auto promptRow = fullRow(26);
        auto diceBounds = promptRow.removeFromRight(22);
        promptRow.removeFromRight(3);
        continuePromptEditor.setBounds(promptRow);
        continueDiceButton.setBounds(diceBounds.withHeight(22).withY(diceBounds.getY() + 2));
        y += 30;

        auto sourceRow = fullRow(24);
        continueSourceLabel.setBounds(sourceRow.removeFromLeft(kLabelWidth));
        sourceRow.removeFromLeft(kGap);
        continueRecordingButton.setBounds(sourceRow.removeFromLeft(88));
        sourceRow.removeFromLeft(kGap);
        continueOutputButton.setBounds(sourceRow.removeFromLeft(74));
        y += 28;

        auto continuationRow = fullRow(kRowHeight);
        continuationLabel.setBounds(continuationRow.removeFromLeft(kLabelWidth));
        continuationSlider.setBounds(continuationRow);
        y += kRowHeight + kGap;

        layoutAdvanced();
        layoutAction();
    }

    contentComponent->setSize(contentWidth, y + 4);
}

void SA3UI::drawDiceIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isHovered, bool isPressed)
{
    juce::Colour bgColour = Theme::Colors::Jerry.withAlpha(0.45f);
    if (isPressed)
        bgColour = Theme::Colors::Jerry.withAlpha(0.65f);
    else if (isHovered)
        bgColour = Theme::Colors::Jerry.withAlpha(0.55f);

    juce::Path dicePath;
    dicePath.addRoundedRectangle(bounds, 2.0f);
    g.setColour(bgColour);
    g.fillPath(dicePath);

    const float pipRadius = bounds.getWidth() * 0.1f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float offset = bounds.getWidth() * 0.25f;

    g.setColour(juce::Colours::white.withAlpha(0.75f));
    auto drawPip = [&](float x, float y)
    {
        g.fillEllipse(x - pipRadius, y - pipRadius, pipRadius * 2.0f, pipRadius * 2.0f);
    };

    drawPip(cx, cy);
    drawPip(cx - offset, cy - offset);
    drawPip(cx + offset, cy - offset);
    drawPip(cx - offset, cy + offset);
    drawPip(cx + offset, cy + offset);
}
