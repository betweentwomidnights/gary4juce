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

    promptDurationLabel.setText("prompt duration", juce::dontSendNotification);
    promptDurationLabel.setFont(juce::FontOptions(12.0f));
    promptDurationLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    promptDurationLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(promptDurationLabel);

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
    addAndMakeVisible(promptDurationSlider);

    modelLabel.setText("model", juce::dontSendNotification);
    modelLabel.setFont(juce::FontOptions(12.0f));
    modelLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    modelLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(modelLabel);

    modelComboBox.onChange = [this]()
    {
        const int selectedId = modelComboBox.getSelectedId();
        if (selectedId <= 0)
            return;

        modelIndex = selectedId - 1;
        if (onModelChanged)
            onModelChanged(modelIndex);
    };
    addAndMakeVisible(modelComboBox);

    sendToGaryButton.setButtonText("send to gary");
    sendToGaryButton.setButtonStyle(CustomButton::ButtonStyle::Gary);
    sendToGaryButton.onClick = [this]()
    {
        if (onSendToGary)
            onSendToGary();
    };
    addAndMakeVisible(sendToGaryButton);

    continueButton.setButtonText("continue");
    continueButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    continueButton.onClick = [this]()
    {
        if (onContinue)
            onContinue();
    };
    addAndMakeVisible(continueButton);

    retryButton.setButtonText("retry");
    retryButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    retryButton.onClick = [this]()
    {
        if (onRetry)
            onRetry();
    };
    addAndMakeVisible(retryButton);

    promptDurationSlider.setValue(promptDuration, juce::dontSendNotification);
    refreshTooltips();
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

    auto promptRow = area.removeFromTop(kRowHeight);
    auto promptLabelArea = promptRow.removeFromLeft(kLabelWidth);
    promptDurationLabel.setBounds(promptLabelArea);
    promptDurationSlider.setBounds(promptRow);
    area.removeFromTop(kInterRowGap);

    auto modelRow = area.removeFromTop(kRowHeight);
    auto modelLabelArea = modelRow.removeFromLeft(kLabelWidth);
    modelLabel.setBounds(modelLabelArea);
    modelComboBox.setBounds(modelRow);
    area.removeFromTop(kInterRowGap);

    auto sendRow = area.removeFromTop(kButtonHeight);
    auto sendWidth = juce::jmin(sendRow.getWidth(), 240);
    auto sendBounds = sendRow.withWidth(sendWidth).withCentre(sendRow.getCentre());
    sendToGaryButton.setBounds(sendBounds);
    area.removeFromTop(kInterRowGap);

    auto buttonRow = area.removeFromTop(kButtonHeight);
    auto continueBounds = buttonRow.removeFromLeft((buttonRow.getWidth() - kButtonGap) / 2);
    continueButton.setBounds(continueBounds);
    buttonRow.removeFromLeft(kButtonGap);
    retryButton.setBounds(buttonRow);
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
