// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "GaryUI.h"
#include "../../Utils/Theme.h"

namespace
{
    constexpr int kOuterMargin = 12;
    constexpr int kTitleHeight = 32;
    constexpr int kRowHeight = 34;
    constexpr int kButtonHeight = 38;
    constexpr int kLabelWidth = 140;
    constexpr int kInterRowGap = 6;
    constexpr int kButtonGap = 10;
}

GaryUI::GaryUI()
{
    garyLabel.setText("gary (musicgen)", juce::dontSendNotification);
    garyLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    garyLabel.setColour(juce::Label::textColourId, Theme::Colors::TextPrimary);
    garyLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(garyLabel);

    contentComponent = std::make_unique<juce::Component>();
    contentViewport = std::make_unique<juce::Viewport>();
    contentViewport->setViewedComponent(contentComponent.get(), false);
    contentViewport->setScrollBarsShown(true, false);
    customLookAndFeel.setScrollbarAccentColour(Theme::Colors::Gary);
    contentViewport->getVerticalScrollBar().setLookAndFeel(&customLookAndFeel);
    addAndMakeVisible(contentViewport.get());

    promptDurationLabel.setText("prompt duration", juce::dontSendNotification);
    promptDurationLabel.setFont(juce::FontOptions(12.0f));
    promptDurationLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    promptDurationLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(promptDurationLabel);

    promptDurationSlider.setRange(1.0, 15.0, 1.0);
    promptDurationSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    promptDurationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    promptDurationSlider.setTextValueSuffix("s");
    promptDurationSlider.onValueChange = [this]()
    {
        promptDuration = (float)promptDurationSlider.getValue();
        refreshTooltips();
        if (onPromptDurationChanged)
            onPromptDurationChanged(promptDuration);
    };
    addToContent(promptDurationSlider);

    modelLabel.setText("model", juce::dontSendNotification);
    modelLabel.setFont(juce::FontOptions(12.0f));
    modelLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    modelLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(modelLabel);

    modelComboBox.onChange = [this]()
    {
        const int selectedId = modelComboBox.getSelectedId();
        if (selectedId <= 0)
            return;

        modelIndex = selectedId - 1;
        if (onModelChanged)
            onModelChanged(modelIndex);
    };
    addToContent(modelComboBox);

    sendToGaryButton.setButtonText("send to gary");
    sendToGaryButton.setButtonStyle(CustomButton::ButtonStyle::Gary);
    sendToGaryButton.onClick = [this]()
    {
        if (onSendToGary)
            onSendToGary();
    };
    addToContent(sendToGaryButton);

    continueButton.setButtonText("continue");
    continueButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    continueButton.onClick = [this]()
    {
        if (onContinue)
            onContinue();
    };
    addToContent(continueButton);

    retryButton.setButtonText("retry");
    retryButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    retryButton.onClick = [this]()
    {
        if (onRetry)
            onRetry();
    };
    addToContent(retryButton);

    advancedToggle.setButtonText(juce::String::fromUTF8("advanced \xe2\x96\xb6"));
    advancedToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
    advancedToggle.onClick = [this]()
    {
        advancedOpen = !advancedOpen;
        updateAdvancedToggleText();
        updateContentLayout();
        if (onLayoutHeightChanged)
            onLayoutHeightChanged();
    };
    addToContent(advancedToggle);

    cfgLabel.setText("cfg", juce::dontSendNotification);
    cfgLabel.setFont(juce::FontOptions(12.0f));
    cfgLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    cfgLabel.setJustificationType(juce::Justification::centredLeft);
    cfgLabel.setTooltip("higher = it listens to your description more");
    addToContent(cfgLabel);

    cfgSlider.setRange(1.0, 5.0, 0.1);
    cfgSlider.setValue(3.0, juce::dontSendNotification);
    cfgSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    cfgSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    cfgSlider.setTooltip("higher = it listens to your description more");
    cfgSlider.onValueChange = [this]()
    {
        if (onCfgChanged)
            onCfgChanged(getCfgCoef());
    };
    addToContent(cfgSlider);

    topKLabel.setText("top k", juce::dontSendNotification);
    topKLabel.setFont(juce::FontOptions(12.0f));
    topKLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    topKLabel.setJustificationType(juce::Justification::centredLeft);
    topKLabel.setTooltip("lower = more repetitive but sticks to your input audio more, "
                         "too high and it will get absurd and lose the bpm");
    addToContent(topKLabel);

    topKSlider.setRange(50.0, 300.0, 1.0);
    topKSlider.setValue(250.0, juce::dontSendNotification);
    topKSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    topKSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    topKSlider.setTooltip("lower = more repetitive but sticks to your input audio more, "
                          "too high and it will get absurd and lose the bpm");
    topKSlider.onValueChange = [this]()
    {
        if (onTopKChanged)
            onTopKChanged(getTopK());
    };
    addToContent(topKSlider);

    descriptionLabel.setText("description", juce::dontSendNotification);
    descriptionLabel.setFont(juce::FontOptions(12.0f));
    descriptionLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    descriptionLabel.setJustificationType(juce::Justification::centredLeft);
    descriptionLabel.setTooltip("optional text conditioning - leave blank to let the audio prompt speak for itself");
    addToContent(descriptionLabel);

    descriptionEditor.setTextToShowWhenEmpty("e.g. drums, percussion", juce::Colour(0xff666666));
    descriptionEditor.setMultiLine(false);
    descriptionEditor.setReturnKeyStartsNewLine(false);
    descriptionEditor.setScrollbarsShown(false);
    descriptionEditor.setBorder(juce::BorderSize<int>(2));
    descriptionEditor.onTextChange = [this]()
    {
        if (onDescriptionChanged)
            onDescriptionChanged(getDescription());
    };
    addToContent(descriptionEditor);

    promptDurationSlider.setValue(promptDuration, juce::dontSendNotification);
    refreshTooltips();
}

GaryUI::~GaryUI()
{
    if (contentViewport)
        contentViewport->getVerticalScrollBar().setLookAndFeel(nullptr);
}

void GaryUI::paint(juce::Graphics&)
{
}

void GaryUI::resized()
{
    auto area = getLocalBounds().reduced(kOuterMargin);

    titleBounds = area.removeFromTop(kTitleHeight);
    garyLabel.setBounds(titleBounds);
    area.removeFromTop(kInterRowGap);

    if (contentViewport)
        contentViewport->setBounds(area);

    updateContentLayout();
}

void GaryUI::addToContent(juce::Component& component)
{
    if (contentComponent)
        contentComponent->addAndMakeVisible(component);
}

void GaryUI::updateAdvancedToggleText()
{
    advancedToggle.setButtonText(advancedOpen
        ? juce::String::fromUTF8("advanced \xe2\x96\xbc")
        : juce::String::fromUTF8("advanced \xe2\x96\xb6"));
}

void GaryUI::setAdvancedOpen(bool open)
{
    if (advancedOpen == open)
        return;

    advancedOpen = open;
    updateAdvancedToggleText();
    updateContentLayout();
}

void GaryUI::updateContentLayout()
{
    if (contentComponent == nullptr || contentViewport == nullptr)
        return;

    const int viewportWidth = juce::jmax(220, contentViewport->getWidth());
    const int scrollbarWidth = contentViewport->getVerticalScrollBar().isVisible()
        ? contentViewport->getVerticalScrollBar().getWidth() : 0;
    const int contentWidth = juce::jmax(220, viewportWidth - scrollbarWidth - 4);

    // The label column has to give way on narrow layouts or the sliders vanish.
    const int labelWidth = juce::jmin(kLabelWidth, contentWidth / 3);

    int y = 0;
    const auto fullRow = [&](int height)
    {
        return juce::Rectangle<int>(0, y, contentWidth, height);
    };

    auto promptRow = fullRow(kRowHeight);
    promptDurationLabel.setBounds(promptRow.removeFromLeft(labelWidth));
    promptDurationSlider.setBounds(promptRow);
    y += kRowHeight + kInterRowGap;

    auto modelRow = fullRow(kRowHeight);
    modelLabel.setBounds(modelRow.removeFromLeft(labelWidth));
    modelComboBox.setBounds(modelRow);
    y += kRowHeight + kInterRowGap;

    advancedToggle.setBounds(fullRow(24));
    y += 24 + kInterRowGap;

    cfgLabel.setVisible(advancedOpen);
    cfgSlider.setVisible(advancedOpen);
    topKLabel.setVisible(advancedOpen);
    topKSlider.setVisible(advancedOpen);
    descriptionLabel.setVisible(advancedOpen);
    descriptionEditor.setVisible(advancedOpen);

    if (advancedOpen)
    {
        auto cfgRow = fullRow(kRowHeight);
        cfgLabel.setBounds(cfgRow.removeFromLeft(labelWidth));
        cfgSlider.setBounds(cfgRow);
        y += kRowHeight + kInterRowGap;

        auto topKRow = fullRow(kRowHeight);
        topKLabel.setBounds(topKRow.removeFromLeft(labelWidth));
        topKSlider.setBounds(topKRow);
        y += kRowHeight + kInterRowGap;

        descriptionLabel.setBounds(fullRow(16));
        y += 16 + 2;

        descriptionEditor.setBounds(fullRow(26));
        y += 26 + kInterRowGap;
    }

    auto sendRow = fullRow(kButtonHeight);
    const int sendWidth = juce::jmin(sendRow.getWidth(), 240);
    sendToGaryButton.setBounds(sendRow.withWidth(sendWidth).withCentre(sendRow.getCentre()));
    y += kButtonHeight + kInterRowGap;

    auto buttonRow = fullRow(kButtonHeight);
    continueButton.setBounds(buttonRow.removeFromLeft((buttonRow.getWidth() - kButtonGap) / 2));
    buttonRow.removeFromLeft(kButtonGap);
    retryButton.setBounds(buttonRow);
    y += kButtonHeight;

    contentHeight = y + 4;
    contentComponent->setSize(contentWidth, contentHeight);
}

int GaryUI::getPreferredHeight() const
{
    // Everything resized() takes off the top before the viewport gets what's left.
    return (kOuterMargin * 2) + kTitleHeight + kInterRowGap + contentHeight;
}

void GaryUI::setVisibleForTab(bool visible)
{
    setVisible(visible);
    setInterceptsMouseClicks(visible, visible);
}

void GaryUI::setPromptDuration(float seconds)
{
    promptDuration = seconds;
    promptDurationSlider.setValue(seconds, juce::dontSendNotification);
    refreshTooltips();
}

void GaryUI::setModelItems(const juce::StringArray& items, int selectedIndex)
{
    modelComboBox.clear(juce::dontSendNotification);
    for (int i = 0; i < items.size(); ++i)
        modelComboBox.addItem(items[i], i + 1);

    if (items.isEmpty())
    {
        modelIndex = juce::jlimit(0, 0, selectedIndex);
        modelComboBox.setText({}, juce::dontSendNotification);
        return;
    }

    setSelectedModelIndex(selectedIndex, juce::dontSendNotification);
}

void GaryUI::setModelItemEnabled(int index, bool enabled)
{
    modelComboBox.setItemEnabled(index + 1, enabled);
}

void GaryUI::setSelectedModelIndex(int index, juce::NotificationType notification)
{
    if (modelComboBox.getNumItems() == 0)
    {
        modelIndex = juce::jmax(0, index);
        return;
    }

    modelIndex = juce::jlimit(0, modelComboBox.getNumItems() - 1, index);
    modelComboBox.setSelectedId(modelIndex + 1, notification);
}

void GaryUI::setButtonsEnabled(bool hasAudio,
                               bool isConnected,
                               bool isGenerating,
                               bool retryAvailable,
                               bool continueAvailable)
{
    applyEnablement(hasAudio, isConnected, isGenerating, retryAvailable, continueAvailable);
}

void GaryUI::setSendButtonText(const juce::String& text)
{
    sendToGaryButton.setButtonText(text);
}

void GaryUI::setContinueButtonText(const juce::String& text)
{
    continueButton.setButtonText(text);
}

void GaryUI::setRetryButtonText(const juce::String& text)
{
    retryButton.setButtonText(text);
}

float GaryUI::getPromptDuration() const
{
    return promptDuration;
}

int GaryUI::getSelectedModelIndex() const
{
    return modelIndex;
}

int GaryUI::getTopK() const
{
    return juce::roundToInt(topKSlider.getValue());
}

void GaryUI::setTopK(int value)
{
    topKSlider.setValue((double)value, juce::dontSendNotification);
}

double GaryUI::getCfgCoef() const
{
    return cfgSlider.getValue();
}

void GaryUI::setCfgCoef(double value)
{
    cfgSlider.setValue(value, juce::dontSendNotification);
}

void GaryUI::setDescription(const juce::String& text)
{
    descriptionEditor.setText(text, juce::dontSendNotification);
}

juce::Rectangle<int> GaryUI::getTitleBounds() const
{
    return titleBounds;
}

void GaryUI::refreshTooltips()
{
    const int secs = juce::roundToInt(promptDuration);
    const juce::String secondsText = juce::String(secs) + " seconds";

    sendToGaryButton.setTooltip("have gary extend the recorded audio using the first " + secondsText + " as audio prompt");
    continueButton.setTooltip("have gary extend the output audio using the last " + secondsText + " as audio prompt");
    retryButton.setTooltip("have gary retry that last continuation using different prompt duration or model if you want, or just have him do it over");
}

void GaryUI::applyEnablement(bool hasAudio,
                             bool isConnected,
                             bool isGenerating,
                             bool retryAvailable,
                             bool continueAvailable)
{
    const bool canSend = hasAudio && isConnected && !isGenerating;
    const bool canContinue = continueAvailable && isConnected && !isGenerating;
    const bool canRetry = retryAvailable && isConnected && !isGenerating;

    sendToGaryButton.setEnabled(canSend);
    continueButton.setEnabled(canContinue);
    retryButton.setEnabled(canRetry);
}
