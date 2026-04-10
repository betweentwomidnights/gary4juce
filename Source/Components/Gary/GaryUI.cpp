#include "GaryUI.h"
#include "../../Utils/Theme.h"

namespace
{
    constexpr int kOuterMargin = 12;
    constexpr int kTitleHeight = 32;
    constexpr int kRowHeight = 34;
    constexpr int kButtonHeight = 38;
    constexpr int kLabelWidth = 140;
    constexpr int kQuantizationLabelWidth = 96;
    constexpr int kInterRowGap = 6;
    constexpr int kButtonGap = 10;
    constexpr int kQuantizationButtonGap = 6;
    constexpr int kQuantizationRadioGroup = 4204;
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

    quantizationLabel.setText("quantization", juce::dontSendNotification);
    quantizationLabel.setFont(juce::FontOptions(12.0f));
    quantizationLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    quantizationLabel.setJustificationType(juce::Justification::centredLeft);
    quantizationLabel.setVisible(false);
    addAndMakeVisible(quantizationLabel);

    auto setupQuantizationButton = [this](CustomButton& button,
                                          const juce::String& text,
                                          const juce::String& mode,
                                          const juce::String& tooltip)
    {
        button.setButtonText(text);
        button.setButtonStyle(CustomButton::ButtonStyle::Standard);
        button.setClickingTogglesState(true);
        button.setRadioGroupId(kQuantizationRadioGroup);
        button.setTooltip(tooltip);
        button.setVisible(false);
        button.onClick = [this, &button, mode]()
        {
            if (!button.getToggleState())
                button.setToggleState(true, juce::dontSendNotification);

            setQuantizationMode(mode, juce::sendNotification);
        };
        addAndMakeVisible(button);
    };

    setupQuantizationButton(quantizationNoneButton,
                            "none",
                            "none",
                            "full precision: best audio adherence, slowest generation");
    setupQuantizationButton(quantizationQ8Button,
                            "8-bit",
                            "q8_decoder_linears",
                            "8-bit decoder linears: faster generation with moderate quality tradeoff");
    setupQuantizationButton(quantizationQ4Button,
                            "4-bit",
                            "q4_decoder_linears",
                            "4-bit decoder linears: major speedup with reduced prompt adherence");
    setupQuantizationButton(quantizationQ4EmbButton,
                            "4bit+emb",
                            "q4_decoder_linears_emb",
                            "4-bit decoder + embeddings: highest speed, strongest quality tradeoff");

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
    setQuantizationMode(quantizationMode, juce::dontSendNotification);
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

    if (isUsingLocalhostMode)
    {
        auto quantizationRow = area.removeFromTop(kRowHeight);
        auto quantizationLabelArea = quantizationRow.removeFromLeft(kQuantizationLabelWidth);
        quantizationLabel.setBounds(quantizationLabelArea);

        const int totalGap = kQuantizationButtonGap * 3;
        const int totalButtonSpace = juce::jmax(0, quantizationRow.getWidth() - totalGap);
        int longButtonWidth = juce::jlimit(80, 104, totalButtonSpace / 3);
        int shortButtonWidth = juce::jmax(36, (totalButtonSpace - longButtonWidth) / 3);

        // Keep exact fit after integer rounding by assigning any remainder to the long button.
        int usedButtonSpace = shortButtonWidth * 3 + longButtonWidth;
        if (usedButtonSpace < totalButtonSpace)
            longButtonWidth += totalButtonSpace - usedButtonSpace;
        else if (usedButtonSpace > totalButtonSpace)
            longButtonWidth = juce::jmax(68, totalButtonSpace - (shortButtonWidth * 3));

        auto buttonRow = quantizationRow.withWidth(shortButtonWidth * 3 + longButtonWidth + totalGap)
                                        .withCentre(quantizationRow.getCentre());

        quantizationNoneButton.setBounds(buttonRow.removeFromLeft(shortButtonWidth));
        buttonRow.removeFromLeft(kQuantizationButtonGap);
        quantizationQ8Button.setBounds(buttonRow.removeFromLeft(shortButtonWidth));
        buttonRow.removeFromLeft(kQuantizationButtonGap);
        quantizationQ4Button.setBounds(buttonRow.removeFromLeft(shortButtonWidth));
        buttonRow.removeFromLeft(kQuantizationButtonGap);
        quantizationQ4EmbButton.setBounds(buttonRow);

        area.removeFromTop(kInterRowGap);
    }
    else
    {
        quantizationLabel.setBounds({});
        quantizationNoneButton.setBounds({});
        quantizationQ8Button.setBounds({});
        quantizationQ4Button.setBounds({});
        quantizationQ4EmbButton.setBounds({});
    }

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

void GaryUI::setUsingLocalhost(bool useLocalhost)
{
    if (isUsingLocalhostMode == useLocalhost)
        return;

    isUsingLocalhostMode = useLocalhost;

    quantizationLabel.setVisible(useLocalhost);
    quantizationNoneButton.setVisible(useLocalhost);
    quantizationQ8Button.setVisible(useLocalhost);
    quantizationQ4Button.setVisible(useLocalhost);
    quantizationQ4EmbButton.setVisible(useLocalhost);

    resized();
}

void GaryUI::setQuantizationMode(const juce::String& mode, juce::NotificationType notification)
{
    juce::String normalized = mode.trim().toLowerCase();

    if (normalized == "q8")
        normalized = "q8_decoder_linears";
    else if (normalized == "q4")
        normalized = "q4_decoder_linears";
    else if (normalized == "q4_emb" || normalized == "q4_decoder_linears_embedding")
        normalized = "q4_decoder_linears_emb";

    if (normalized != "none"
        && normalized != "q8_decoder_linears"
        && normalized != "q4_decoder_linears"
        && normalized != "q4_decoder_linears_emb")
    {
        normalized = "q4_decoder_linears";
    }

    quantizationMode = normalized;

    quantizationNoneButton.setToggleState(quantizationMode == "none", juce::dontSendNotification);
    quantizationQ8Button.setToggleState(quantizationMode == "q8_decoder_linears", juce::dontSendNotification);
    quantizationQ4Button.setToggleState(quantizationMode == "q4_decoder_linears", juce::dontSendNotification);
    quantizationQ4EmbButton.setToggleState(quantizationMode == "q4_decoder_linears_emb", juce::dontSendNotification);

    if (notification != juce::dontSendNotification && onQuantizationModeChanged)
        onQuantizationModeChanged(quantizationMode);
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

juce::String GaryUI::getQuantizationMode() const
{
    return quantizationMode;
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

    const bool canChangeQuantization = isUsingLocalhostMode && !isGenerating;
    quantizationNoneButton.setEnabled(canChangeQuantization);
    quantizationQ8Button.setEnabled(canChangeQuantization);
    quantizationQ4Button.setEnabled(canChangeQuantization);
    quantizationQ4EmbButton.setEnabled(canChangeQuantization);
}
