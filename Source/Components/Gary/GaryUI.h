#pragma once

#include <JuceHeader.h>

#include "../Base/CustomButton.h"
#include "../Base/CustomSlider.h"
#include "../Base/CustomComboBox.h"

#include <functional>

class GaryUI : public juce::Component
{
public:
    GaryUI();

    void paint(juce::Graphics&) override;
    void resized() override;

    void setVisibleForTab(bool visible);

    void setPromptDuration(float seconds);
    void setModelItems(const juce::StringArray& items, int selectedIndex);
    void setModelItemEnabled(int index, bool enabled);
    void setSelectedModelIndex(int index, juce::NotificationType notification = juce::dontSendNotification);
    void setButtonsEnabled(bool hasAudio,
                           bool isConnected,
                           bool isGenerating,
                           bool retryAvailable,
                           bool continueAvailable);
    void setSendButtonText(const juce::String& text);
    void setContinueButtonText(const juce::String& text);
    void setRetryButtonText(const juce::String& text);

    float getPromptDuration() const;
    int getSelectedModelIndex() const;

    juce::Rectangle<int> getTitleBounds() const;

    std::function<void(float)> onPromptDurationChanged;
    std::function<void(int)> onModelChanged;
    std::function<void()> onSendToGary;
    std::function<void()> onContinue;
    std::function<void()> onRetry;

private:
    void refreshTooltips();
    void applyEnablement(bool hasAudio,
                         bool isConnected,
                         bool isGenerating,
                         bool retryAvailable,
                         bool continueAvailable);

    juce::Label garyLabel;
    CustomSlider promptDurationSlider;
    juce::Label promptDurationLabel;
    CustomComboBox modelComboBox;
    juce::Label modelLabel;
    CustomButton sendToGaryButton;
    CustomButton continueButton;
    CustomButton retryButton;

    float promptDuration { 6.0f };
    int modelIndex { 0 };

    juce::Rectangle<int> titleBounds;
};
