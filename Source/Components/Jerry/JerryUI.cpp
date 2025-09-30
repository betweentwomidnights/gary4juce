#include "JerryUI.h"
#include "../../Utils/Theme.h"

namespace
{
    constexpr int kOuterMargin = 12;
    constexpr int kTitleHeight = 28;
    constexpr int kPromptLabelHeight = 18;
    constexpr int kPromptEditorHeight = 26;
    constexpr int kRowHeight = 26;
    constexpr int kSmartLoopHeight = 28;
    constexpr int kBpmHeight = 16;
    constexpr int kButtonHeight = 32;
    constexpr int kLabelWidth = 92;
    constexpr int kInterRowGap = 3;
    constexpr int kLoopButtonGap = 4;
}

JerryUI::JerryUI()
{
    jerryLabel.setText("jerry (stable audio open small)", juce::dontSendNotification);
    jerryLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    jerryLabel.setColour(juce::Label::textColourId, Theme::Colors::TextPrimary);
    jerryLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(jerryLabel);

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

    titleBounds = area.removeFromTop(kTitleHeight);
    jerryLabel.setBounds(titleBounds);
    area.removeFromTop(kInterRowGap);

    auto promptLabelBounds = area.removeFromTop(kPromptLabelHeight);
    jerryPromptLabel.setBounds(promptLabelBounds);
    area.removeFromTop(kInterRowGap);

    auto promptBounds = area.removeFromTop(kPromptEditorHeight);
    jerryPromptEditor.setBounds(promptBounds);
    area.removeFromTop(kInterRowGap);

    auto cfgRow = area.removeFromTop(kRowHeight);
    auto cfgLabelBounds = cfgRow.removeFromLeft(kLabelWidth);
    jerryCfgLabel.setBounds(cfgLabelBounds);
    jerryCfgSlider.setBounds(cfgRow);
    area.removeFromTop(kInterRowGap);

    auto stepsRow = area.removeFromTop(kRowHeight);
    auto stepsLabelBounds = stepsRow.removeFromLeft(kLabelWidth);
    jerryStepsLabel.setBounds(stepsLabelBounds);
    jerryStepsSlider.setBounds(stepsRow);
    area.removeFromTop(kInterRowGap);

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

    auto bpmBounds = area.removeFromTop(kBpmHeight);
    jerryBpmLabel.setBounds(bpmBounds);
    area.removeFromTop(kInterRowGap);

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

void JerryUI::applyEnablement(bool canGenerate, bool canSmartLoop, bool isGenerating)
{
    lastCanGenerate = canGenerate;
    lastCanSmartLoop = canSmartLoop;
    lastIsGenerating = isGenerating;

    const bool allowGenerate = canGenerate && !isGenerating;
    const bool allowSmartLoop = canSmartLoop && !isGenerating;
    const bool allowLoopTypes = allowSmartLoop && smartLoop;

    generateWithJerryButton.setEnabled(allowGenerate);
    generateAsLoopButton.setEnabled(allowSmartLoop);
    loopTypeAutoButton.setEnabled(allowLoopTypes);
    loopTypeDrumsButton.setEnabled(allowLoopTypes);
    loopTypeInstrumentsButton.setEnabled(allowLoopTypes);
}
