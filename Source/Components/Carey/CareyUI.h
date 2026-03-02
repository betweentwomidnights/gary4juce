#pragma once

#include <JuceHeader.h>

#include "../Base/CustomButton.h"
#include "../Base/CustomComboBox.h"
#include "../Base/CustomSlider.h"
#include "../Base/CustomTextEditor.h"
#include "../../Utils/CustomLookAndFeel.h"
#include "../../Utils/Theme.h"

#include <functional>

class CareyLyricsDialogContent : public juce::Component
{
public:
    explicit CareyLyricsDialogContent(const juce::String& initialText,
                                      std::function<void(const juce::String&)> onSaveCallback)
        : onSave(std::move(onSaveCallback))
    {
        titleLabel.setText("lyrics (optional)", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        hintLabel.setText("use one line per phrase", juce::dontSendNotification);
        hintLabel.setFont(juce::FontOptions(11.0f));
        hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        hintLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(hintLabel);

        lyricsEditor.setMultiLine(true);
        lyricsEditor.setReturnKeyStartsNewLine(true);
        lyricsEditor.setScrollbarsShown(true);
        lyricsEditor.setText(initialText, juce::dontSendNotification);
        lyricsEditor.setTextToShowWhenEmpty("leave blank for wordless generation", juce::Colour(0xff666666));
        addAndMakeVisible(lyricsEditor);

        cancelButton.setButtonText("cancel");
        cancelButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
        cancelButton.onClick = [this]() { closeDialog(); };
        addAndMakeVisible(cancelButton);

        saveButton.setButtonText("save");
        saveButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
        saveButton.onClick = [this]()
        {
            if (onSave)
                onSave(lyricsEditor.getText());
            closeDialog();
        };
        addAndMakeVisible(saveButton);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        titleLabel.setBounds(area.removeFromTop(20));
        hintLabel.setBounds(area.removeFromTop(16));
        area.removeFromTop(8);

        auto buttonRow = area.removeFromBottom(34);
        auto saveBounds = buttonRow.removeFromRight(96);
        buttonRow.removeFromRight(8);
        auto cancelBounds = buttonRow.removeFromRight(96);
        saveButton.setBounds(saveBounds);
        cancelButton.setBounds(cancelBounds);

        area.removeFromBottom(8);
        lyricsEditor.setBounds(area);
    }

private:
    void closeDialog()
    {
        if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
            dialog->exitModalState(0);
    }

    std::function<void(const juce::String&)> onSave;
    juce::Label titleLabel;
    juce::Label hintLabel;
    CustomTextEditor lyricsEditor;
    CustomButton cancelButton;
    CustomButton saveButton;
};

class CareyUI : public juce::Component
{
public:
    enum class SubTab
    {
        Lego = 0,
        Complete
    };

    CareyUI()
    {
        careyLabel.setText("carey (ace-step lego)", juce::dontSendNotification);
        careyLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        careyLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        careyLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(careyLabel);

        auto prepareSubTabButton = [this](CustomButton& button, const juce::String& text, SubTab tab)
        {
            button.setButtonText(text);
            button.setButtonStyle(CustomButton::ButtonStyle::Inactive);
            button.onClick = [this, tab]() { setCurrentSubTabInternal(tab, true); };
            addAndMakeVisible(button);
        };

        prepareSubTabButton(legoSubTabButton, "lego", SubTab::Lego);
        prepareSubTabButton(completeSubTabButton, "complete", SubTab::Complete);

        contentComponent = std::make_unique<juce::Component>();
        contentViewport = std::make_unique<juce::Viewport>();
        contentViewport->setViewedComponent(contentComponent.get(), false);
        contentViewport->setScrollBarsShown(true, false);
        customLookAndFeel.setScrollbarAccentColour(Theme::Colors::Terry);
        contentViewport->getVerticalScrollBar().setLookAndFeel(&customLookAndFeel);
        addAndMakeVisible(contentViewport.get());

        captionLabel.setText("caption", juce::dontSendNotification);
        captionLabel.setFont(juce::FontOptions(12.0f));
        captionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        captionLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(captionLabel);

        captionEditor.setTextToShowWhenEmpty("describe the vocal or drum stem...", juce::Colour(0xff666666));
        captionEditor.setMultiLine(false);
        captionEditor.setReturnKeyStartsNewLine(false);
        captionEditor.setScrollbarsShown(false);
        captionEditor.onTextChange = [this]()
        {
            if (onCaptionChanged)
                onCaptionChanged(captionEditor.getText());
        };
        addToContent(captionEditor);

        captionDiceButton.setButtonText("");
        captionDiceButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
        captionDiceButton.setTooltip("generate a random caption for selected track");
        captionDiceButton.onClick = [this]() { applyRandomLegoCaption(); };
        captionDiceButton.onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
        {
            drawDiceIcon(g, bounds.toFloat().reduced(2.0f), captionDiceButton.isMouseOver(), captionDiceButton.isDown());
        };
        addToContent(captionDiceButton);

        legoLyricsButton.setButtonText("lyrics");
        legoLyricsButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
        legoLyricsButton.setTooltip("open multiline lyrics editor (optional)");
        legoLyricsButton.onClick = [this]() { openLyricsEditor(false); };
        addToContent(legoLyricsButton);

        trackLabel.setText("track", juce::dontSendNotification);
        trackLabel.setFont(juce::FontOptions(12.0f));
        trackLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        trackLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(trackLabel);

        initializeTrackOptions();
        trackComboBox.setTextWhenNothingSelected("select stem track...");
        trackComboBox.onChange = [this]()
        {
            if (onTrackChanged)
                onTrackChanged(getTrackName());
        };
        trackComboBox.setSelectedId(1, juce::dontSendNotification); // vocals
        addToContent(trackComboBox);

        stepsLabel.setText("steps", juce::dontSendNotification);
        stepsLabel.setFont(juce::FontOptions(12.0f));
        stepsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        stepsLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(stepsLabel);

        stepsSlider.setRange(32, 100, 1);
        stepsSlider.setValue(50);
        stepsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
        stepsSlider.onValueChange = [this]()
        {
            if (onStepsChanged)
                onStepsChanged(getSteps());
        };
        addToContent(stepsSlider);

        loopAssistLabel.setText("loop assist", juce::dontSendNotification);
        loopAssistLabel.setFont(juce::FontOptions(12.0f));
        loopAssistLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        loopAssistLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(loopAssistLabel);

        loopAssistToggle.setButtonText("");
        loopAssistToggle.setToggleState(true, juce::dontSendNotification);
        loopAssistToggle.setTooltip("normalize conditioning audio to a fixed 32-bar context");
        loopAssistToggle.onClick = [this]()
        {
            if (onLoopAssistChanged)
                onLoopAssistChanged(loopAssistToggle.getToggleState());
        };
        addToContent(loopAssistToggle);

        trimToInputLabel.setText("trim to input", juce::dontSendNotification);
        trimToInputLabel.setFont(juce::FontOptions(12.0f));
        trimToInputLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        trimToInputLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(trimToInputLabel);

        trimToInputToggle.setButtonText("");
        trimToInputToggle.setToggleState(false, juce::dontSendNotification);
        trimToInputToggle.setTooltip("trim generated output back to your recorded buffer length");
        trimToInputToggle.onClick = [this]()
        {
            if (onTrimToInputChanged)
                onTrimToInputChanged(trimToInputToggle.getToggleState());
        };
        addToContent(trimToInputToggle);

        legoGenerateButton.setButtonText("send to carey");
        legoGenerateButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
        legoGenerateButton.setTooltip("generate a carey stem from your saved buffer");
        legoGenerateButton.setEnabled(false);
        legoGenerateButton.onClick = [this]()
        {
            if (onGenerate)
                onGenerate();
        };
        addToContent(legoGenerateButton);

        legoInfoLabel.setText("carey local backend remains experimental. remote is recommended.", juce::dontSendNotification);
        legoInfoLabel.setFont(juce::FontOptions(11.0f));
        legoInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        legoInfoLabel.setJustificationType(juce::Justification::centred);
        addToContent(legoInfoLabel);

        completeCaptionLabel.setText("caption", juce::dontSendNotification);
        completeCaptionLabel.setFont(juce::FontOptions(12.0f));
        completeCaptionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        completeCaptionLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(completeCaptionLabel);

        completeCaptionEditor.setTextToShowWhenEmpty("describe the continuation style...", juce::Colour(0xff666666));
        completeCaptionEditor.setMultiLine(false);
        completeCaptionEditor.setReturnKeyStartsNewLine(false);
        completeCaptionEditor.setScrollbarsShown(false);
        completeCaptionEditor.onTextChange = [this]()
        {
            if (onCompleteCaptionChanged)
                onCompleteCaptionChanged(completeCaptionEditor.getText());
        };
        addToContent(completeCaptionEditor);

        completeCaptionDiceButton.setButtonText("");
        completeCaptionDiceButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
        completeCaptionDiceButton.setTooltip("generate a random continuation caption");
        completeCaptionDiceButton.onClick = [this]() { applyRandomCompleteCaption(); };
        completeCaptionDiceButton.onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
        {
            drawDiceIcon(g, bounds.toFloat().reduced(2.0f), completeCaptionDiceButton.isMouseOver(), completeCaptionDiceButton.isDown());
        };
        addToContent(completeCaptionDiceButton);

        completeLyricsButton.setButtonText("lyrics");
        completeLyricsButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
        completeLyricsButton.setTooltip("open multiline lyrics editor (optional)");
        completeLyricsButton.onClick = [this]() { openLyricsEditor(true); };
        addToContent(completeLyricsButton);

        completeBpmLabel.setText("bpm", juce::dontSendNotification);
        completeBpmLabel.setFont(juce::FontOptions(12.0f));
        completeBpmLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        completeBpmLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(completeBpmLabel);

        completeBpmSlider.setRange(40, 240, 1);
        completeBpmSlider.setValue(120);
        completeBpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        completeBpmSlider.onValueChange = [this]()
        {
            if (onCompleteBpmChanged)
                onCompleteBpmChanged(getCompleteBpm());
        };
        addToContent(completeBpmSlider);

        completeStepsLabel.setText("steps", juce::dontSendNotification);
        completeStepsLabel.setFont(juce::FontOptions(12.0f));
        completeStepsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        completeStepsLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(completeStepsLabel);

        completeStepsSlider.setRange(32, 100, 1);
        completeStepsSlider.setValue(50);
        completeStepsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        completeStepsSlider.onValueChange = [this]()
        {
            if (onCompleteStepsChanged)
                onCompleteStepsChanged(getCompleteSteps());
        };
        addToContent(completeStepsSlider);

        completeDurationLabel.setText("duration (sec)", juce::dontSendNotification);
        completeDurationLabel.setFont(juce::FontOptions(12.0f));
        completeDurationLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        completeDurationLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(completeDurationLabel);

        completeDurationSlider.setRange(30, 180, 1);
        completeDurationSlider.setValue(120);
        completeDurationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        completeDurationSlider.onValueChange = [this]()
        {
            if (onCompleteDurationChanged)
                onCompleteDurationChanged(getCompleteDurationSeconds());
        };
        addToContent(completeDurationSlider);

        completeGenerateButton.setButtonText("continue with carey");
        completeGenerateButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
        completeGenerateButton.setTooltip("complete mode request wiring in progress");
        completeGenerateButton.setEnabled(false);
        completeGenerateButton.onClick = [this]()
        {
            if (onCompleteGenerate)
                onCompleteGenerate();
        };
        addToContent(completeGenerateButton);

        completeInfoLabel.setText("complete mode extends your audio up to 3:00. use dice for ideas and lyrics when needed.", juce::dontSendNotification);
        completeInfoLabel.setFont(juce::FontOptions(11.0f));
        completeInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        completeInfoLabel.setJustificationType(juce::Justification::centred);
        addToContent(completeInfoLabel);

        updateLyricsButtonLabels();
        setCurrentSubTabInternal(SubTab::Lego, false);
    }

    ~CareyUI() override
    {
        if (contentViewport != nullptr)
            contentViewport->getVerticalScrollBar().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics&) override
    {
    }

    void resized() override
    {
        constexpr int margin = 12;
        auto area = getLocalBounds().reduced(margin);

        titleBounds = area.removeFromTop(30);
        careyLabel.setBounds(titleBounds);

        area.removeFromTop(6);
        auto subTabRow = area.removeFromTop(30);
        const int tabWidth = subTabRow.getWidth() / 2;
        legoSubTabButton.setBounds(subTabRow.removeFromLeft(tabWidth).reduced(2, 2));
        completeSubTabButton.setBounds(subTabRow.reduced(2, 2));

        area.removeFromTop(8);
        if (contentViewport != nullptr)
            contentViewport->setBounds(area);

        updateContentLayout();
    }

    void setVisibleForTab(bool visible)
    {
        setVisible(visible);
    }

    juce::Rectangle<int> getTitleBounds() const
    {
        return titleBounds;
    }

    void setCurrentSubTab(SubTab tab)
    {
        setCurrentSubTabInternal(tab, false);
    }

    SubTab getCurrentSubTab() const
    {
        return currentSubTab;
    }

    juce::String getCaptionText() const
    {
        return captionEditor.getText().trim();
    }

    juce::String getTrackName() const
    {
        const int selectedId = trackComboBox.getSelectedId();
        const int index = selectedId - 1;
        if (index >= 0 && index < trackOptionValues.size())
            return trackOptionValues[index];
        return "vocals";
    }

    int getSteps() const
    {
        return juce::roundToInt(stepsSlider.getValue());
    }

    juce::String getCompleteCaptionText() const
    {
        return completeCaptionEditor.getText().trim();
    }

    juce::String getLyricsText() const
    {
        return legoLyricsText;
    }

    juce::String getCompleteLyricsText() const
    {
        return completeLyricsText;
    }

    int getCompleteDurationSeconds() const
    {
        return juce::roundToInt(completeDurationSlider.getValue());
    }

    int getCompleteBpm() const
    {
        return juce::roundToInt(completeBpmSlider.getValue());
    }

    int getCompleteSteps() const
    {
        return juce::roundToInt(completeStepsSlider.getValue());
    }

    void setCaptionText(const juce::String& text)
    {
        captionEditor.setText(text, juce::dontSendNotification);
    }

    void setTrackName(const juce::String& trackName)
    {
        const int trackIndex = findTrackIndex(trackName.trim().toLowerCase());
        trackComboBox.setSelectedId(trackIndex >= 0 ? trackIndex + 1 : 1, juce::dontSendNotification);
    }

    void setSteps(int value)
    {
        stepsSlider.setValue(juce::jlimit(32, 100, value), juce::dontSendNotification);
    }

    void setCompleteCaptionText(const juce::String& text)
    {
        completeCaptionEditor.setText(text, juce::dontSendNotification);
    }

    void setLyricsText(const juce::String& text)
    {
        setLegoLyricsTextInternal(text, false);
    }

    void setCompleteLyricsText(const juce::String& text)
    {
        setCompleteLyricsTextInternal(text, false);
    }

    void setCompleteDurationSeconds(int seconds)
    {
        completeDurationSlider.setValue(juce::jlimit(30, 180, seconds), juce::dontSendNotification);
    }

    void setCompleteBpm(int bpm)
    {
        completeBpmSlider.setValue(juce::jlimit(40, 240, bpm), juce::dontSendNotification);
    }

    void setCompleteSteps(int steps)
    {
        completeStepsSlider.setValue(juce::jlimit(32, 100, steps), juce::dontSendNotification);
    }

    void setLoopAssistEnabled(bool enabled)
    {
        loopAssistToggle.setToggleState(enabled, juce::dontSendNotification);
    }

    bool getLoopAssistEnabled() const
    {
        return loopAssistToggle.getToggleState();
    }

    void setTrimToInputEnabled(bool enabled)
    {
        trimToInputToggle.setToggleState(enabled, juce::dontSendNotification);
    }

    bool getTrimToInputEnabled() const
    {
        return trimToInputToggle.getToggleState();
    }

    void setGenerateButtonEnabled(bool enabled, bool isGenerating)
    {
        legoGenerateButton.setEnabled(enabled && !isGenerating);
        legoGenerateButton.setButtonText(isGenerating ? "generating..." : "send to carey");

        const bool completeAvailable = (bool)onCompleteGenerate;
        completeGenerateButton.setEnabled(completeAvailable && enabled && !isGenerating);
        completeGenerateButton.setButtonText(isGenerating ? "generating..." : "continue with carey");
    }

    std::function<void(SubTab)> onSubTabChanged;
    std::function<void(const juce::String&)> onCaptionChanged;
    std::function<void(const juce::String&)> onTrackChanged;
    std::function<void(int)> onStepsChanged;
    std::function<void(bool)> onLoopAssistChanged;
    std::function<void(bool)> onTrimToInputChanged;
    std::function<void()> onGenerate;
    std::function<void(const juce::String&)> onLyricsChanged;

    std::function<void(const juce::String&)> onCompleteCaptionChanged;
    std::function<void(int)> onCompleteBpmChanged;
    std::function<void(int)> onCompleteStepsChanged;
    std::function<void(int)> onCompleteDurationChanged;
    std::function<void()> onCompleteGenerate;
    std::function<void(const juce::String&)> onCompleteLyricsChanged;

private:
    void addToContent(juce::Component& component)
    {
        if (contentComponent != nullptr)
            contentComponent->addAndMakeVisible(component);
    }

    void setCurrentSubTabInternal(SubTab tab, bool notify)
    {
        if (currentSubTab == tab && notify)
            return;

        currentSubTab = tab;
        updateSubTabButtonStyles();
        updateContentLayout();

        if (notify && onSubTabChanged)
            onSubTabChanged(currentSubTab);
    }

    void updateSubTabButtonStyles()
    {
        const bool legoActive = (currentSubTab == SubTab::Lego);
        legoSubTabButton.setButtonStyle(legoActive ? CustomButton::ButtonStyle::Terry
                                                   : CustomButton::ButtonStyle::Inactive);
        completeSubTabButton.setButtonStyle(!legoActive ? CustomButton::ButtonStyle::Terry
                                                        : CustomButton::ButtonStyle::Inactive);
    }

    void updateContentLayout()
    {
        if (contentComponent == nullptr || contentViewport == nullptr)
            return;

        const int viewportWidth = juce::jmax(220, contentViewport->getWidth());
        const int scrollbarWidth = contentViewport->getVerticalScrollBar().isVisible()
            ? contentViewport->getVerticalScrollBar().getWidth()
            : 0;
        const int contentWidth = juce::jmax(220, viewportWidth - scrollbarWidth - 4);

        const bool showingLego = (currentSubTab == SubTab::Lego);
        setLegoControlsVisible(showingLego);
        setCompleteControlsVisible(!showingLego);

        constexpr int sidePadding = 8;
        int y = 8;
        auto contentArea = juce::Rectangle<int>(0, 0, contentWidth, 0);
        juce::ignoreUnused(contentArea);

        const auto fullRow = [&](int height) { return juce::Rectangle<int>(sidePadding, y, contentWidth - (sidePadding * 2), height); };

        if (showingLego)
        {
            captionLabel.setBounds(fullRow(18));
            y += 20;

            auto captionRow = fullRow(28);
            auto lyricsBounds = captionRow.removeFromRight(64);
            captionRow.removeFromRight(4);
            auto diceBounds = captionRow.removeFromRight(22);
            captionRow.removeFromRight(4);
            captionEditor.setBounds(captionRow);
            const auto diceSquare = diceBounds.withHeight(22).withY(diceBounds.getY() + 3);
            captionDiceButton.setBounds(diceSquare);
            legoLyricsButton.setBounds(lyricsBounds.withHeight(24).withY(lyricsBounds.getY() + 2));
            y += 36;

            trackLabel.setBounds(fullRow(18));
            y += 20;

            trackComboBox.setBounds(fullRow(28));
            y += 36;

            auto stepsRow = fullRow(28);
            stepsLabel.setBounds(stepsRow.removeFromLeft(60));
            stepsSlider.setBounds(stepsRow);
            y += 36;

            auto assistRow = fullRow(22);
            auto leftAssist = assistRow.removeFromLeft(assistRow.getWidth() / 2);
            loopAssistLabel.setBounds(leftAssist.removeFromLeft(80));
            loopAssistToggle.setBounds(leftAssist.removeFromLeft(22));

            trimToInputLabel.setBounds(assistRow.removeFromLeft(90));
            trimToInputToggle.setBounds(assistRow.removeFromLeft(22));
            y += 30;

            auto generateRow = fullRow(34).reduced(50, 0);
            legoGenerateButton.setBounds(generateRow);
            y += 44;

            legoInfoLabel.setBounds(fullRow(24));
            y += 32;
        }
        else
        {
            const bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();

            completeCaptionLabel.setBounds(fullRow(18));
            y += 20;

            auto completeCaptionRow = fullRow(28);
            auto completeLyricsBounds = completeCaptionRow.removeFromRight(64);
            completeCaptionRow.removeFromRight(4);
            auto completeDiceBounds = completeCaptionRow.removeFromRight(22);
            completeCaptionRow.removeFromRight(4);
            completeCaptionEditor.setBounds(completeCaptionRow);
            const auto completeDiceSquare = completeDiceBounds.withHeight(22).withY(completeDiceBounds.getY() + 3);
            completeCaptionDiceButton.setBounds(completeDiceSquare);
            completeLyricsButton.setBounds(completeLyricsBounds.withHeight(24).withY(completeLyricsBounds.getY() + 2));
            y += 36;

            if (isStandalone)
            {
                auto bpmRow = fullRow(28);
                completeBpmLabel.setBounds(bpmRow.removeFromLeft(110));
                completeBpmSlider.setBounds(bpmRow);
                y += 36;
            }

            auto stepsRow = fullRow(28);
            completeStepsLabel.setBounds(stepsRow.removeFromLeft(110));
            completeStepsSlider.setBounds(stepsRow);
            y += 36;

            auto durationRow = fullRow(28);
            completeDurationLabel.setBounds(durationRow.removeFromLeft(110));
            completeDurationSlider.setBounds(durationRow);
            y += 36;

            auto completeGenerateRow = fullRow(34).reduced(50, 0);
            completeGenerateButton.setBounds(completeGenerateRow);
            y += 44;

            completeInfoLabel.setBounds(fullRow(32));
            y += 40;
        }

        const int minHeight = juce::jmax(y + 8, contentViewport->getHeight());
        contentComponent->setSize(contentWidth, minHeight);
    }

    void setLegoControlsVisible(bool shouldBeVisible)
    {
        captionLabel.setVisible(shouldBeVisible);
        captionEditor.setVisible(shouldBeVisible);
        captionDiceButton.setVisible(shouldBeVisible);
        legoLyricsButton.setVisible(shouldBeVisible);
        trackLabel.setVisible(shouldBeVisible);
        trackComboBox.setVisible(shouldBeVisible);
        stepsLabel.setVisible(shouldBeVisible);
        stepsSlider.setVisible(shouldBeVisible);
        loopAssistLabel.setVisible(shouldBeVisible);
        loopAssistToggle.setVisible(shouldBeVisible);
        trimToInputLabel.setVisible(shouldBeVisible);
        trimToInputToggle.setVisible(shouldBeVisible);
        legoGenerateButton.setVisible(shouldBeVisible);
        legoInfoLabel.setVisible(shouldBeVisible);
    }

    void setCompleteControlsVisible(bool shouldBeVisible)
    {
        const bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();
        completeCaptionLabel.setVisible(shouldBeVisible);
        completeCaptionEditor.setVisible(shouldBeVisible);
        completeCaptionDiceButton.setVisible(shouldBeVisible);
        completeLyricsButton.setVisible(shouldBeVisible);
        completeBpmLabel.setVisible(shouldBeVisible && isStandalone);
        completeBpmSlider.setVisible(shouldBeVisible && isStandalone);
        completeStepsLabel.setVisible(shouldBeVisible);
        completeStepsSlider.setVisible(shouldBeVisible);
        completeDurationLabel.setVisible(shouldBeVisible);
        completeDurationSlider.setVisible(shouldBeVisible);
        completeGenerateButton.setVisible(shouldBeVisible);
        completeInfoLabel.setVisible(shouldBeVisible);
    }

    void initializeTrackOptions()
    {
        addTrackOption("vocals", "vocals");
        addTrackOption("backing vocals", "backing_vocals");
        addTrackOption("drums", "drums");
        addTrackOption("bass", "bass");
        addTrackOption("guitar", "guitar");
        addTrackOption("piano", "piano");
        addTrackOption("strings", "strings");
        addTrackOption("synth", "synth");
        addTrackOption("keyboard", "keyboard");
        addTrackOption("percussion", "percussion");
        addTrackOption("brass", "brass");
        addTrackOption("woodwinds", "woodwinds");
    }

    void addTrackOption(const juce::String& label, const juce::String& value)
    {
        trackOptionValues.add(value);
        trackComboBox.addItem(label, trackOptionValues.size());
    }

    int findTrackIndex(const juce::String& normalizedTrackName) const
    {
        for (int i = 0; i < trackOptionValues.size(); ++i)
        {
            if (trackOptionValues[i].equalsIgnoreCase(normalizedTrackName))
                return i;
        }
        return -1;
    }

    void drawDiceIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isHovered, bool isPressed)
    {
        juce::Colour bgColour = Theme::Colors::Terry.withAlpha(0.9f);
        if (isPressed)
            bgColour = Theme::Colors::Terry.brighter(0.2f);
        else if (isHovered)
            bgColour = Theme::Colors::Terry.brighter(0.3f);

        g.setColour(bgColour);
        g.fillRoundedRectangle(bounds, 2.0f);

        const float pipRadius = bounds.getWidth() * 0.12f;
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float offset = bounds.getWidth() * 0.25f;

        g.setColour(juce::Colours::white);
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

    juce::String pickRandomCaption(const juce::StringArray& prompts, const juce::String& fallback)
    {
        if (prompts.isEmpty())
            return fallback;
        return prompts[random.nextInt(prompts.size())];
    }

    juce::StringArray getLegoPromptBankForTrack(const juce::String& trackName) const
    {
        const juce::String track = trackName.trim().toLowerCase();

        if (track == "vocals")
        {
            return {
                "soulful lead vocal, intimate phrasing, warm tone, wordless melodic hooks, centered dry mix",
                "airy indie lead vocal, expressive falsetto moments, soft breath texture, emotional ad-libs, modern pop clarity",
                "neo-soul lead vocal, smooth runs, conversational delivery, layered doubles in chorus, mellow character",
                "cinematic lead voice, haunting vowel-driven melody, evolving intensity, focused mono center image"
            };
        }

        if (track == "backing_vocals")
        {
            return {
                "lush backing vocal stack, soft ooh and ahh harmonies, stereo spread, gentle movement",
                "tight backing harmonies, supportive thirds and fifths, smooth blend, no lead phrasing",
                "ethereal backing choir texture, airy pads of voices, restrained dynamics, warm ambience",
                "rhythmic backing vocal accents, call-and-response style layers, clean articulation, subtle width"
            };
        }

        if (track == "drums")
        {
            return {
                "punchy acoustic drum kit, tight kick and snare, controlled room tone, dynamic ghost notes",
                "lo-fi drum groove, dusty hats, soft transient shaping, pocket-focused swing feel",
                "modern pop drums, clean transient snap, crisp hi-hats, balanced low-end and punch",
                "live drum performance, humanized timing, expressive cymbal work, natural kit resonance"
            };
        }

        if (track == "bass")
            return { "melodic electric bass line, warm low mids, pocket-locked groove, rounded transient attack" };
        if (track == "guitar")
            return { "supporting electric guitar layer, clean articulation, rhythmic chord stabs, tasteful ambience" };
        if (track == "piano")
            return { "supporting piano stem, clear voicings, gentle dynamics, natural room response" };
        if (track == "strings")
            return { "cinematic string arrangement, smooth legato motion, supportive harmonic bed, warm texture" };
        if (track == "synth")
            return { "analog-style synth layer, controlled harmonics, expressive filter motion, supportive energy" };
        if (track == "keyboard")
            return { "keyboard comping layer, tight rhythmic placement, warm tone, supportive harmonic movement" };
        if (track == "percussion")
            return { "percussion layer with shakers and hand drums, controlled transients, groove-focused accents" };
        if (track == "brass")
            return { "brass section stabs, tight ensemble articulation, punchy attacks, musical phrasing" };
        if (track == "woodwinds")
            return { "woodwind harmony layer, smooth breath phrasing, lyrical movement, natural ensemble blend" };

        return {
            "supporting stem with clear articulation, musical phrasing, controlled dynamics, and cohesive mix balance"
        };
    }

    juce::StringArray getCompletePromptBank() const
    {
        return {
            "A dark, aggressive phonk and trap track built on chopped soul samples and a crushing 808 bassline, with pitched-down vocal chops, menacing minor-key synth stabs, and relentless trap hi-hat patterns. The energy escalates through a heavy, cinematic drop with layered distorted synths and punishing sub bass. The arrangement is dense and relentless, driving forward with a locked groove and an atmosphere that is suffocating, menacing, and intensely physical.",
            "A warm indie folk arrangement with fingerpicked acoustic guitar, soft upright bass, brushed drums, and intimate vocal textures. Gentle strings bloom in the background while piano doubles key melodic moments. The track grows naturally from sparse verses into a wide, emotional chorus with layered harmonies, preserving an organic feel and hand-played dynamics throughout.",
            "A modern alt-pop production with tight drums, pulsing synth bass, and glossy vocal layers that alternate between breathy restraint and powerful hook delivery. The arrangement balances clean verse sections with a high-impact chorus, adding subtle ear-candy, reverse textures, and rhythmic vocal chops while maintaining a polished, radio-ready mix.",
            "A cinematic post-rock soundscape driven by evolving clean electric guitars, wide reverbs, and emotional piano motifs over a steady rhythm section. The dynamics rise in waves, moving from introspective ambient passages into broad, cathartic crescendos with sustained harmonic tension, thick low-end support, and expansive stereo depth.",
            "A soulful neo-RnB groove with warm Rhodes chords, tight pocket drums, melodic electric bass, and expressive lead vocals supported by lush harmonies. The production favors smooth transitions, tasteful guitar fills, and rich midrange detail, creating a late-night atmosphere that feels intimate, confident, and deeply musical.",
            "A high-energy electro-pop and dance track with punchy four-on-the-floor drums, sidechained synth stacks, and euphoric topline hooks. Bright arpeggios and riser effects build momentum into explosive drops, while layered vocals and rhythmic ad-libs keep the arrangement energetic, modern, and festival-ready.",
            "A moody lo-fi hip-hop and jazz fusion piece with dusty drums, mellow piano voicings, tape-style saturation, and subtle ambient textures. The groove stays relaxed but focused, with soft melodic motifs and understated harmonic movement that feel reflective, nocturnal, and emotionally grounded.",
            "A contemporary rock production with driving drums, gritty rhythm guitars, melodic lead lines, and anthemic vocal phrasing. The arrangement combines tight verse groove, soaring choruses, and a dynamic bridge with cinematic lift, delivering raw energy while keeping instrument separation and mix clarity strong."
        };
    }

    void applyRandomLegoCaption()
    {
        const juce::String caption = pickRandomCaption(getLegoPromptBankForTrack(getTrackName()),
                                                       "supporting stem with musical clarity");
        captionEditor.setText(caption, juce::sendNotification);
    }

    void applyRandomCompleteCaption()
    {
        const juce::String caption = pickRandomCaption(getCompletePromptBank(),
                                                       "A dark, aggressive phonk and trap track built on chopped soul samples and crushing low-end, with menacing synth movement and relentless rhythmic drive.");
        completeCaptionEditor.setText(caption, juce::sendNotification);
    }

    void setLegoLyricsTextInternal(const juce::String& text, bool notify)
    {
        legoLyricsText = text;
        updateLyricsButtonLabels();
        if (notify && onLyricsChanged)
            onLyricsChanged(legoLyricsText);
    }

    void setCompleteLyricsTextInternal(const juce::String& text, bool notify)
    {
        completeLyricsText = text;
        updateLyricsButtonLabels();
        if (notify && onCompleteLyricsChanged)
            onCompleteLyricsChanged(completeLyricsText);
    }

    void updateLyricsButtonLabels()
    {
        const bool legoHasLyrics = !legoLyricsText.trim().isEmpty();
        const bool completeHasLyrics = !completeLyricsText.trim().isEmpty();
        legoLyricsButton.setButtonText(legoHasLyrics ? "lyrics*" : "lyrics");
        completeLyricsButton.setButtonText(completeHasLyrics ? "lyrics*" : "lyrics");
    }

    void openLyricsEditor(bool forComplete)
    {
        const juce::String initialText = forComplete ? completeLyricsText : legoLyricsText;
        auto* dialogContent = new CareyLyricsDialogContent(
            initialText,
            [this, forComplete](const juce::String& text)
            {
                if (forComplete)
                    setCompleteLyricsTextInternal(text, true);
                else
                    setLegoLyricsTextInternal(text, true);
            });

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(dialogContent);
        options.content->setSize(560, 360);
        options.dialogTitle = forComplete ? "carey complete lyrics" : "carey lego lyrics";
        options.dialogBackgroundColour = juce::Colour(0x1e, 0x1e, 0x1e);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.useBottomRightCornerResizer = true;
        options.componentToCentreAround = this;

        if (auto* window = options.launchAsync())
            window->setResizeLimits(420, 260, 1000, 900);
    }

    SubTab currentSubTab = SubTab::Lego;
    juce::Random random;

    juce::Label careyLabel;
    CustomButton legoSubTabButton;
    CustomButton completeSubTabButton;
    CustomLookAndFeel customLookAndFeel;
    std::unique_ptr<juce::Viewport> contentViewport;
    std::unique_ptr<juce::Component> contentComponent;

    juce::Label captionLabel;
    CustomTextEditor captionEditor;
    CustomButton captionDiceButton;
    CustomButton legoLyricsButton;
    juce::Label trackLabel;
    CustomComboBox trackComboBox;
    juce::StringArray trackOptionValues;
    juce::Label stepsLabel;
    CustomSlider stepsSlider;
    juce::Label loopAssistLabel;
    juce::ToggleButton loopAssistToggle;
    juce::Label trimToInputLabel;
    juce::ToggleButton trimToInputToggle;
    CustomButton legoGenerateButton;
    juce::Label legoInfoLabel;
    juce::String legoLyricsText;

    juce::Label completeCaptionLabel;
    CustomTextEditor completeCaptionEditor;
    CustomButton completeCaptionDiceButton;
    CustomButton completeLyricsButton;
    juce::Label completeBpmLabel;
    CustomSlider completeBpmSlider;
    juce::Label completeStepsLabel;
    CustomSlider completeStepsSlider;
    juce::Label completeDurationLabel;
    CustomSlider completeDurationSlider;
    CustomButton completeGenerateButton;
    juce::Label completeInfoLabel;
    juce::String completeLyricsText;

    juce::Rectangle<int> titleBounds;
};
