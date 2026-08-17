// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <JuceHeader.h>

#include "../Base/CustomButton.h"
#include "../Base/CustomSlider.h"
#include "../Base/CustomComboBox.h"
#include "../Base/CustomTextEditor.h"
#include "../../Utils/CustomLookAndFeel.h"

#include <functional>

class GaryUI : public juce::Component
{
public:
    GaryUI();
    ~GaryUI() override;

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

    int getTopK() const;
    void setTopK(int value);
    double getCfgCoef() const;
    void setCfgCoef(double value);
    juce::String getDescription() const { return descriptionEditor.getText().trim(); }
    void setDescription(const juce::String& text);

    bool getAdvancedOpen() const { return advancedOpen; }
    void setAdvancedOpen(bool open);

    // Height this panel needs to show its content without scrolling.
    int getPreferredHeight() const;

    juce::Rectangle<int> getTitleBounds() const;

    // Access to model ComboBox for hierarchical menu setup
    CustomComboBox& getModelComboBox() { return modelComboBox; }
    const CustomComboBox& getModelComboBox() const { return modelComboBox; }

    std::function<void(float)> onPromptDurationChanged;
    std::function<void(int)> onModelChanged;
    std::function<void()> onSendToGary;
    std::function<void()> onContinue;
    std::function<void()> onRetry;
    std::function<void(int)> onTopKChanged;
    std::function<void(double)> onCfgChanged;
    std::function<void(const juce::String&)> onDescriptionChanged;
    std::function<void()> onLayoutHeightChanged;

private:
    void addToContent(juce::Component& component);
    void updateAdvancedToggleText();
    void updateContentLayout();
    void refreshTooltips();
    void applyEnablement(bool hasAudio,
                         bool isConnected,
                         bool isGenerating,
                         bool retryAvailable,
                         bool continueAvailable);

    juce::Label garyLabel;

    std::unique_ptr<juce::Component> contentComponent;
    std::unique_ptr<juce::Viewport> contentViewport;
    CustomLookAndFeel customLookAndFeel;

    CustomSlider promptDurationSlider;
    juce::Label promptDurationLabel;
    CustomComboBox modelComboBox;
    juce::Label modelLabel;
    CustomButton sendToGaryButton;
    CustomButton continueButton;
    CustomButton retryButton;

    CustomButton advancedToggle;
    bool advancedOpen { false };
    juce::Label cfgLabel;
    CustomSlider cfgSlider;
    juce::Label topKLabel;
    CustomSlider topKSlider;
    juce::Label descriptionLabel;
    CustomTextEditor descriptionEditor;

    float promptDuration { 6.0f };
    int modelIndex { 0 };
    int contentHeight { 0 };

    juce::Rectangle<int> titleBounds;
};
