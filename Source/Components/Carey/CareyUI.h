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
                                      const juce::String& initialLanguage,
                                      std::function<void(const juce::String&, const juce::String&)> onSaveCallback)
        : onSave(std::move(onSaveCallback))
    {
        titleLabel.setText("lyrics (optional)", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        hintLabel.setText("use one line per phrase - write lyrics in the selected language", juce::dontSendNotification);
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

        // Language dropdown
        languageLabel.setText("language", juce::dontSendNotification);
        languageLabel.setFont(juce::FontOptions(11.0f));
        languageLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        languageLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(languageLabel);

        initializeLanguageOptions();
        // Select the initial language
        int langIdx = languageCodes.indexOf(initialLanguage);
        if (langIdx >= 0)
            languageComboBox.setSelectedId(langIdx + 1, juce::dontSendNotification);
        else
            languageComboBox.setSelectedId(languageCodes.indexOf("en") + 1, juce::dontSendNotification);
        addAndMakeVisible(languageComboBox);

        cancelButton.setButtonText("cancel");
        cancelButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
        cancelButton.onClick = [this]() { closeDialog(); };
        addAndMakeVisible(cancelButton);

        saveButton.setButtonText("save");
        saveButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
        saveButton.onClick = [this]()
        {
            if (onSave)
            {
                int selectedIdx = languageComboBox.getSelectedId() - 1;
                juce::String langCode = (selectedIdx >= 0 && selectedIdx < languageCodes.size())
                    ? languageCodes[selectedIdx] : "en";
                onSave(lyricsEditor.getText(), langCode);
            }
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

        // Bottom: language row + button row
        auto buttonRow = area.removeFromBottom(34);
        auto saveBounds = buttonRow.removeFromRight(96);
        buttonRow.removeFromRight(8);
        auto cancelBounds = buttonRow.removeFromRight(96);
        saveButton.setBounds(saveBounds);
        cancelButton.setBounds(cancelBounds);

        area.removeFromBottom(8);

        auto langRow = area.removeFromBottom(26);
        languageLabel.setBounds(langRow.removeFromLeft(60));
        languageComboBox.setBounds(langRow.removeFromLeft(160));

        area.removeFromBottom(6);
        lyricsEditor.setBounds(area);
    }

private:
    void closeDialog()
    {
        if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
            dialog->exitModalState(0);
    }

    void initializeLanguageOptions()
    {
        // ACE-Step supported languages — English first, then alphabetical by display name
        struct LangEntry { const char* code; const char* name; };
        const LangEntry languages[] = {
            { "en", "English" },
            { "ar", "Arabic" },
            { "az", "Azerbaijani" },
            { "bn", "Bengali" },
            { "bg", "Bulgarian" },
            { "yue", "Cantonese" },
            { "ca", "Catalan" },
            { "zh", "Chinese" },
            { "hr", "Croatian" },
            { "cs", "Czech" },
            { "da", "Danish" },
            { "nl", "Dutch" },
            { "fi", "Finnish" },
            { "fr", "French" },
            { "de", "German" },
            { "el", "Greek" },
            { "ht", "Haitian Creole" },
            { "he", "Hebrew" },
            { "hi", "Hindi" },
            { "hu", "Hungarian" },
            { "is", "Icelandic" },
            { "id", "Indonesian" },
            { "it", "Italian" },
            { "ja", "Japanese" },
            { "ko", "Korean" },
            { "la", "Latin" },
            { "lt", "Lithuanian" },
            { "ms", "Malay" },
            { "ne", "Nepali" },
            { "no", "Norwegian" },
            { "fa", "Persian" },
            { "pl", "Polish" },
            { "pt", "Portuguese" },
            { "pa", "Punjabi" },
            { "ro", "Romanian" },
            { "ru", "Russian" },
            { "sa", "Sanskrit" },
            { "sr", "Serbian" },
            { "sk", "Slovak" },
            { "es", "Spanish" },
            { "sw", "Swahili" },
            { "sv", "Swedish" },
            { "tl", "Tagalog" },
            { "ta", "Tamil" },
            { "te", "Telugu" },
            { "th", "Thai" },
            { "tr", "Turkish" },
            { "uk", "Ukrainian" },
            { "ur", "Urdu" },
            { "vi", "Vietnamese" },
        };

        for (int i = 0; i < (int)(sizeof(languages) / sizeof(languages[0])); ++i)
        {
            languageCodes.add(languages[i].code);
            languageComboBox.addItem(languages[i].name, i + 1);
        }
    }

    std::function<void(const juce::String&, const juce::String&)> onSave;
    juce::Label titleLabel;
    juce::Label hintLabel;
    CustomTextEditor lyricsEditor;
    juce::Label languageLabel;
    CustomComboBox languageComboBox;
    juce::StringArray languageCodes;
    CustomButton cancelButton;
    CustomButton saveButton;
};

class CareyUI : public juce::Component
{
public:
    enum class SubTab
    {
        Lego = 0,
        Complete,
        Cover
    };

    // Lyrics are shared across all 3 tabs (lego/complete/cover)

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
        prepareSubTabButton(coverSubTabButton, juce::String::fromUTF8("cover \xe2\x9a\xa0"), SubTab::Cover);
        coverSubTabButton.setTooltip("experimental - results may vary. use at your own risk");

        // Key/scale dropdowns (persistent across subtabs)
        keyScaleLabel.setText("key", juce::dontSendNotification);
        keyScaleLabel.setFont(juce::FontOptions(11.0f));
        keyScaleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        keyScaleLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(keyScaleLabel);

        keyRootComboBox.addItem("none", 1);
        const juce::StringArray keyNames = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < keyNames.size(); ++i)
            keyRootComboBox.addItem(keyNames[i], i + 2); // IDs 2-13
        keyRootComboBox.setSelectedId(1, juce::dontSendNotification); // default: none
        keyRootComboBox.setTooltip("key root (optional - helps guide generation)");
        keyRootComboBox.onChange = [this]() { onKeyScaleSelectionChanged(); };
        addAndMakeVisible(keyRootComboBox);

        keyModeComboBox.addItem("major", 1);
        keyModeComboBox.addItem("minor", 2);
        keyModeComboBox.setSelectedId(1, juce::dontSendNotification); // default: major
        keyModeComboBox.setTooltip("major or minor scale");
        keyModeComboBox.onChange = [this]() { onKeyScaleSelectionChanged(); };
        addAndMakeVisible(keyModeComboBox);
        keyModeComboBox.setVisible(false); // hidden until a key is selected

        // Time signature dropdown (persistent across subtabs)
        timeSigLabel.setText("time", juce::dontSendNotification);
        timeSigLabel.setFont(juce::FontOptions(11.0f));
        timeSigLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        timeSigLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(timeSigLabel);

        timeSigComboBox.addItem("auto", 1);
        timeSigComboBox.addItem("2/4", 2);
        timeSigComboBox.addItem("3/4", 3);
        timeSigComboBox.addItem("4/4", 4);
        timeSigComboBox.addItem("6/8", 5);
        timeSigComboBox.setSelectedId(1, juce::dontSendNotification); // default: auto
        timeSigComboBox.setTooltip("time signature (auto-detected if not set)");
        timeSigComboBox.onChange = [this]()
        {
            if (onTimeSigChanged)
                onTimeSigChanged(getTimeSig());
        };
        addAndMakeVisible(timeSigComboBox);

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
        legoLyricsButton.onClick = [this]() { openLyricsEditor(); };
        addToContent(legoLyricsButton);

        trackLabel.setText("track", juce::dontSendNotification);
        trackLabel.setFont(juce::FontOptions(12.0f));
        trackLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        trackLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(trackLabel);

        initializeTrackOptions();
        trackComboBox.setTextWhenNothingSelected("select stem track...");
        trackComboBox.setTooltip("which stem type to generate (vocals, backing vocals, etc.)");
        trackComboBox.onChange = [this]()
        {
            if (onTrackChanged)
                onTrackChanged(getTrackName());
        };
        trackComboBox.setSelectedId(1, juce::dontSendNotification); // vocals
        addToContent(trackComboBox);

        legoAdvancedToggle.setButtonText(juce::String::fromUTF8("advanced \xe2\x96\xb6"));
        legoAdvancedToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        legoAdvancedToggle.setTooltip("show/hide advanced generation settings");
        legoAdvancedToggle.onClick = [this]() {
            legoAdvancedOpen = !legoAdvancedOpen;
            legoAdvancedToggle.setButtonText(legoAdvancedOpen
                ? juce::String::fromUTF8("advanced \xe2\x96\xbc")
                : juce::String::fromUTF8("advanced \xe2\x96\xb6"));
            updateContentLayout();
        };
        addToContent(legoAdvancedToggle);

        stepsLabel.setText("steps", juce::dontSendNotification);
        stepsLabel.setFont(juce::FontOptions(12.0f));
        stepsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        stepsLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(stepsLabel);

        stepsSlider.setRange(32, 100, 1);
        stepsSlider.setValue(50);
        stepsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
        stepsSlider.setTooltip("diffusion steps. more = refined but slower. recommended: 50");
        stepsSlider.onValueChange = [this]()
        {
            if (onStepsChanged)
                onStepsChanged(getSteps());
        };
        addToContent(stepsSlider);

        legoCfgLabel.setText("cfg scale", juce::dontSendNotification);
        legoCfgLabel.setFont(juce::FontOptions(12.0f));
        legoCfgLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        legoCfgLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(legoCfgLabel);

        legoCfgSlider.setRange(3.0, 10.0, 0.1);
        legoCfgSlider.setValue(7.0);
        legoCfgSlider.setNumDecimalPlacesToDisplay(1);
        legoCfgSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        legoCfgSlider.setTooltip("cfg scale - how strongly the model follows the caption. higher = more literal. 3=loose, 7=balanced, 10=strict");
        legoCfgSlider.onValueChange = [this]()
        {
            if (onLegoCfgChanged)
                onLegoCfgChanged(getLegoCfg());
        };
        addToContent(legoCfgSlider);

        loopAssistLabel.setText("loop assist", juce::dontSendNotification);
        loopAssistLabel.setFont(juce::FontOptions(12.0f));
        loopAssistLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        loopAssistLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(loopAssistLabel);

        loopAssistToggle.setButtonText("");
        loopAssistToggle.setToggleState(true, juce::dontSendNotification);
        loopAssistToggle.setTooltip("loops short input to at least 2 minutes for better output quality. leave on unless your input is already long enough");
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
        trimToInputToggle.setToggleState(true, juce::dontSendNotification);
        trimToInputToggle.setTooltip("trim output to your input length. disable for full 32-bar output");
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
        completeLyricsButton.onClick = [this]() { openLyricsEditor(); };
        addToContent(completeLyricsButton);

        completeAdvancedToggle.setButtonText(juce::String::fromUTF8("advanced \xe2\x96\xb6"));
        completeAdvancedToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        completeAdvancedToggle.setTooltip("show/hide advanced generation settings");
        completeAdvancedToggle.onClick = [this]() {
            completeAdvancedOpen = !completeAdvancedOpen;
            completeAdvancedToggle.setButtonText(completeAdvancedOpen
                ? juce::String::fromUTF8("advanced \xe2\x96\xbc")
                : juce::String::fromUTF8("advanced \xe2\x96\xb6"));
            updateContentLayout();
        };
        addToContent(completeAdvancedToggle);

        completeBpmLabel.setText("bpm", juce::dontSendNotification);
        completeBpmLabel.setFont(juce::FontOptions(12.0f));
        completeBpmLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        completeBpmLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(completeBpmLabel);

        completeBpmSlider.setRange(40, 240, 1);
        completeBpmSlider.setValue(120);
        completeBpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        completeBpmSlider.setTooltip("BPM for generation (standalone only - DAW mode syncs automatically)");
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
        completeStepsSlider.setTooltip("diffusion steps. more = refined but slower. recommended: 50");
        completeStepsSlider.onValueChange = [this]()
        {
            if (onCompleteStepsChanged)
                onCompleteStepsChanged(getCompleteSteps());
        };
        addToContent(completeStepsSlider);

        completeCfgLabel.setText("cfg scale", juce::dontSendNotification);
        completeCfgLabel.setFont(juce::FontOptions(12.0f));
        completeCfgLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        completeCfgLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(completeCfgLabel);

        completeCfgSlider.setRange(3.0, 10.0, 0.1);
        completeCfgSlider.setValue(7.0);
        completeCfgSlider.setNumDecimalPlacesToDisplay(1);
        completeCfgSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        completeCfgSlider.setTooltip("cfg scale - how strongly the model follows the caption. higher = more literal. 3=loose, 7=balanced, 10=strict");
        completeCfgSlider.onValueChange = [this]()
        {
            if (onCompleteCfgChanged)
                onCompleteCfgChanged(getCompleteCfg());
        };
        addToContent(completeCfgSlider);

        completeDurationLabel.setText("duration (sec)", juce::dontSendNotification);
        completeDurationLabel.setFont(juce::FontOptions(12.0f));
        completeDurationLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        completeDurationLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(completeDurationLabel);

        completeDurationSlider.setRange(30, 180, 1);
        completeDurationSlider.setValue(120);
        completeDurationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        completeDurationSlider.setTooltip("output duration in seconds (30-180). includes your input audio at the start");
        completeDurationSlider.onValueChange = [this]()
        {
            if (onCompleteDurationChanged)
                onCompleteDurationChanged(getCompleteDurationSeconds());
        };
        addToContent(completeDurationSlider);

        completeUseSrcAsRefToggle.setButtonText("use source as reference");
        completeUseSrcAsRefToggle.setToggleState(false, juce::dontSendNotification);
        completeUseSrcAsRefToggle.setTooltip("pass source audio as ref_audio for subtler continuation that retains more of the original character");
        completeUseSrcAsRefToggle.onClick = [this]()
        {
            if (onCompleteUseSrcAsRefChanged)
                onCompleteUseSrcAsRefChanged(completeUseSrcAsRefToggle.getToggleState());
        };
        addToContent(completeUseSrcAsRefToggle);

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

        // ===== COVER CONTROLS =====

        coverCaptionLabel.setText("caption", juce::dontSendNotification);
        coverCaptionLabel.setFont(juce::FontOptions(12.0f));
        coverCaptionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        coverCaptionLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(coverCaptionLabel);

        coverCaptionEditor.setTextToShowWhenEmpty("describe the remix style...", juce::Colour(0xff666666));
        coverCaptionEditor.setMultiLine(false);
        coverCaptionEditor.setReturnKeyStartsNewLine(false);
        coverCaptionEditor.setScrollbarsShown(false);
        coverCaptionEditor.onTextChange = [this]()
        {
            if (onCoverCaptionChanged)
                onCoverCaptionChanged(coverCaptionEditor.getText());
        };
        addToContent(coverCaptionEditor);

        coverCaptionDiceButton.setButtonText("");
        coverCaptionDiceButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
        coverCaptionDiceButton.setTooltip("generate a random cover/remix style caption");
        coverCaptionDiceButton.onClick = [this]() { applyRandomCoverCaption(); };
        coverCaptionDiceButton.onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
        {
            drawDiceIcon(g, bounds.toFloat().reduced(2.0f), coverCaptionDiceButton.isMouseOver(), coverCaptionDiceButton.isDown());
        };
        addToContent(coverCaptionDiceButton);

        coverLyricsButton.setButtonText("lyrics");
        coverLyricsButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
        coverLyricsButton.setTooltip("open multiline lyrics editor (optional)");
        coverLyricsButton.onClick = [this]() { openLyricsEditor(); };
        addToContent(coverLyricsButton);

        coverAdvancedToggle.setButtonText(juce::String::fromUTF8("advanced \xe2\x96\xb6"));
        coverAdvancedToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        coverAdvancedToggle.setTooltip("show/hide advanced generation settings");
        coverAdvancedToggle.onClick = [this]() {
            coverAdvancedOpen = !coverAdvancedOpen;
            coverAdvancedToggle.setButtonText(coverAdvancedOpen
                ? juce::String::fromUTF8("advanced \xe2\x96\xbc")
                : juce::String::fromUTF8("advanced \xe2\x96\xb6"));
            updateContentLayout();
        };
        addToContent(coverAdvancedToggle);

        coverNoiseStrengthLabel.setText("noise strength", juce::dontSendNotification);
        coverNoiseStrengthLabel.setFont(juce::FontOptions(12.0f));
        coverNoiseStrengthLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        coverNoiseStrengthLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(coverNoiseStrengthLabel);

        coverNoiseStrengthSlider.setRange(0.0, 1.0, 0.001);
        coverNoiseStrengthSlider.setValue(0.2);
        coverNoiseStrengthSlider.setNumDecimalPlacesToDisplay(3);
        coverNoiseStrengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        coverNoiseStrengthSlider.setTooltip("how close the starting point is to your source. higher = more faithful, lower = more creative. recommended: 0.2");
        coverNoiseStrengthSlider.onValueChange = [this]()
        {
            if (onCoverNoiseStrengthChanged)
                onCoverNoiseStrengthChanged(getCoverNoiseStrength());
        };
        addToContent(coverNoiseStrengthSlider);

        coverAudioStrengthLabel.setText("audio strength", juce::dontSendNotification);
        coverAudioStrengthLabel.setFont(juce::FontOptions(12.0f));
        coverAudioStrengthLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        coverAudioStrengthLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(coverAudioStrengthLabel);

        coverAudioStrengthSlider.setRange(0.0, 1.0, 0.01);
        coverAudioStrengthSlider.setValue(0.3);
        coverAudioStrengthSlider.setNumDecimalPlacesToDisplay(2);
        coverAudioStrengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        coverAudioStrengthSlider.setTooltip("fraction of steps guided by source harmonic structure. 0.3 for instrumental, 0.5-0.7 for vocal tracks");
        coverAudioStrengthSlider.onValueChange = [this]()
        {
            if (onCoverAudioStrengthChanged)
                onCoverAudioStrengthChanged(getCoverAudioStrength());
        };
        addToContent(coverAudioStrengthSlider);

        coverStepsLabel.setText("steps", juce::dontSendNotification);
        coverStepsLabel.setFont(juce::FontOptions(12.0f));
        coverStepsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        coverStepsLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(coverStepsLabel);

        coverStepsSlider.setRange(8, 100, 1);
        coverStepsSlider.setValue(50);
        coverStepsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        coverStepsSlider.setTooltip("diffusion steps. more = refined but slower. recommended: 50");
        coverStepsSlider.onValueChange = [this]()
        {
            if (onCoverStepsChanged)
                onCoverStepsChanged(getCoverSteps());
        };
        addToContent(coverStepsSlider);

        coverCfgLabel.setText("cfg scale", juce::dontSendNotification);
        coverCfgLabel.setFont(juce::FontOptions(12.0f));
        coverCfgLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        coverCfgLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(coverCfgLabel);

        coverCfgSlider.setRange(3.0, 10.0, 0.1);
        coverCfgSlider.setValue(7.0);
        coverCfgSlider.setNumDecimalPlacesToDisplay(1);
        coverCfgSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        coverCfgSlider.setTooltip("how strictly the model follows the caption. higher = stronger style push. recommended: 7-9");
        coverCfgSlider.onValueChange = [this]()
        {
            if (onCoverCfgChanged)
                onCoverCfgChanged(getCoverCfg());
        };
        addToContent(coverCfgSlider);

        coverLoopAssistLabel.setText("loop assist", juce::dontSendNotification);
        coverLoopAssistLabel.setFont(juce::FontOptions(12.0f));
        coverLoopAssistLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        coverLoopAssistLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(coverLoopAssistLabel);

        coverLoopAssistToggle.setButtonText("");
        coverLoopAssistToggle.setToggleState(true, juce::dontSendNotification);
        coverLoopAssistToggle.setTooltip("extends short input audio to 32 bars for better output quality. leave on unless your input is already long enough");
        coverLoopAssistToggle.onClick = [this]()
        {
            if (onCoverLoopAssistChanged)
                onCoverLoopAssistChanged(coverLoopAssistToggle.getToggleState());
        };
        addToContent(coverLoopAssistToggle);

        coverTrimToInputLabel.setText("trim to input", juce::dontSendNotification);
        coverTrimToInputLabel.setFont(juce::FontOptions(12.0f));
        coverTrimToInputLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        coverTrimToInputLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(coverTrimToInputLabel);

        coverTrimToInputToggle.setButtonText("");
        coverTrimToInputToggle.setToggleState(true, juce::dontSendNotification);
        coverTrimToInputToggle.setTooltip("trim output to your input length. disable for full 32-bar output");
        coverTrimToInputToggle.onClick = [this]()
        {
            if (onCoverTrimToInputChanged)
                onCoverTrimToInputChanged(coverTrimToInputToggle.getToggleState());
        };
        addToContent(coverTrimToInputToggle);

        coverUseSrcAsRefToggle.setButtonText("use source as reference");
        coverUseSrcAsRefToggle.setToggleState(false, juce::dontSendNotification);
        coverUseSrcAsRefToggle.setTooltip("pass source audio as ref_audio for subtler transformation that retains more of the original sound");
        coverUseSrcAsRefToggle.onClick = [this]()
        {
            if (onCoverUseSrcAsRefChanged)
                onCoverUseSrcAsRefChanged(coverUseSrcAsRefToggle.getToggleState());
        };
        addToContent(coverUseSrcAsRefToggle);

        coverGenerateButton.setButtonText("remix with carey");
        coverGenerateButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
        coverGenerateButton.setTooltip("send audio to carey cover mode for remix/arrangement");
        coverGenerateButton.setEnabled(false);
        coverGenerateButton.onClick = [this]()
        {
            if (onCoverGenerate)
                onCoverGenerate();
        };
        addToContent(coverGenerateButton);

        coverInfoLabel.setText("cover mode remixes your audio into a new arrangement guided by the caption. adjust noise/audio strength to control faithfulness vs. creativity.", juce::dontSendNotification);
        coverInfoLabel.setFont(juce::FontOptions(11.0f));
        coverInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        coverInfoLabel.setJustificationType(juce::Justification::centred);
        addToContent(coverInfoLabel);

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
        const int tabWidth = subTabRow.getWidth() / 3;
        legoSubTabButton.setBounds(subTabRow.removeFromLeft(tabWidth).reduced(2, 2));
        completeSubTabButton.setBounds(subTabRow.removeFromLeft(tabWidth).reduced(2, 2));
        coverSubTabButton.setBounds(subTabRow.reduced(2, 2));

        area.removeFromTop(4);
        auto keyRow = area.removeFromTop(22);
        keyScaleLabel.setBounds(keyRow.removeFromLeft(26));
        keyRow.removeFromLeft(4);
        keyRootComboBox.setBounds(keyRow.removeFromLeft(72));
        if (keyModeComboBox.isVisible())
        {
            keyRow.removeFromLeft(4);
            keyModeComboBox.setBounds(keyRow.removeFromLeft(68));
        }
        keyRow.removeFromLeft(8);
        timeSigLabel.setBounds(keyRow.removeFromLeft(30));
        keyRow.removeFromLeft(4);
        timeSigComboBox.setBounds(keyRow.removeFromLeft(68));

        area.removeFromTop(4);
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
        return lyricsText;
    }

    juce::String getCompleteLyricsText() const
    {
        return lyricsText;  // Shared across all tabs
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

    double getLegoCfg() const { return legoCfgSlider.getValue(); }
    void setLegoCfg(double val) { legoCfgSlider.setValue(juce::jlimit(3.0, 10.0, val), juce::dontSendNotification); }

    void setCompleteCaptionText(const juce::String& text)
    {
        completeCaptionEditor.setText(text, juce::dontSendNotification);
    }

    void setLyricsText(const juce::String& text)
    {
        setLyricsTextInternal(text, false);
    }

    juce::String getLyricsLanguage() const { return lyricsLanguage; }
    void setLyricsLanguage(const juce::String& lang) { lyricsLanguage = lang.isNotEmpty() ? lang : "en"; }

    void setCompleteLyricsText(const juce::String& text)
    {
        setLyricsTextInternal(text, false);  // Shared across all tabs
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

    double getCompleteCfg() const { return completeCfgSlider.getValue(); }
    void setCompleteCfg(double val) { completeCfgSlider.setValue(juce::jlimit(3.0, 10.0, val), juce::dontSendNotification); }

    bool getCompleteUseSrcAsRef() const { return completeUseSrcAsRefToggle.getToggleState(); }
    void setCompleteUseSrcAsRef(bool enabled) { completeUseSrcAsRefToggle.setToggleState(enabled, juce::dontSendNotification); }

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

    // Cover getters
    juce::String getCoverCaptionText() const { return coverCaptionEditor.getText().trim(); }
    juce::String getCoverLyricsText() const { return lyricsText; }  // Shared across all tabs
    double getCoverNoiseStrength() const { return coverNoiseStrengthSlider.getValue(); }
    double getCoverAudioStrength() const { return coverAudioStrengthSlider.getValue(); }
    int getCoverSteps() const { return juce::roundToInt(coverStepsSlider.getValue()); }
    double getCoverCfg() const { return coverCfgSlider.getValue(); }
    bool getCoverUseSrcAsRef() const { return coverUseSrcAsRefToggle.getToggleState(); }
    bool getCoverLoopAssistEnabled() const { return coverLoopAssistToggle.getToggleState(); }
    bool getCoverTrimToInputEnabled() const { return coverTrimToInputToggle.getToggleState(); }

    // Cover setters
    void setCoverCaptionText(const juce::String& text) { coverCaptionEditor.setText(text, juce::dontSendNotification); }
    void setCoverLyricsText(const juce::String& text) { setLyricsTextInternal(text, false); }  // Shared across all tabs
    void setCoverNoiseStrength(double val) { coverNoiseStrengthSlider.setValue(juce::jlimit(0.0, 1.0, val), juce::dontSendNotification); }
    void setCoverAudioStrength(double val) { coverAudioStrengthSlider.setValue(juce::jlimit(0.0, 1.0, val), juce::dontSendNotification); }
    void setCoverSteps(int val) { coverStepsSlider.setValue(juce::jlimit(8, 100, val), juce::dontSendNotification); }
    void setCoverCfg(double val) { coverCfgSlider.setValue(juce::jlimit(3.0, 10.0, val), juce::dontSendNotification); }
    void setCoverUseSrcAsRef(bool enabled) { coverUseSrcAsRefToggle.setToggleState(enabled, juce::dontSendNotification); }
    void setCoverLoopAssistEnabled(bool enabled) { coverLoopAssistToggle.setToggleState(enabled, juce::dontSendNotification); }
    void setCoverTrimToInputEnabled(bool enabled) { coverTrimToInputToggle.setToggleState(enabled, juce::dontSendNotification); }

    // Advanced section open/close
    void setLegoAdvancedOpen(bool open)
    {
        if (legoAdvancedOpen != open)
        {
            legoAdvancedOpen = open;
            legoAdvancedToggle.setButtonText(open
                ? juce::String::fromUTF8("advanced \xe2\x96\xbc")
                : juce::String::fromUTF8("advanced \xe2\x96\xb6"));
            updateContentLayout();
        }
    }

    void setCompleteAdvancedOpen(bool open)
    {
        if (completeAdvancedOpen != open)
        {
            completeAdvancedOpen = open;
            completeAdvancedToggle.setButtonText(open
                ? juce::String::fromUTF8("advanced \xe2\x96\xbc")
                : juce::String::fromUTF8("advanced \xe2\x96\xb6"));
            updateContentLayout();
        }
    }

    void setCoverAdvancedOpen(bool open)
    {
        if (coverAdvancedOpen != open)
        {
            coverAdvancedOpen = open;
            coverAdvancedToggle.setButtonText(open
                ? juce::String::fromUTF8("advanced \xe2\x96\xbc")
                : juce::String::fromUTF8("advanced \xe2\x96\xb6"));
            updateContentLayout();
        }
    }

    // Key/scale (global across subtabs)
    juce::String getKeyScale() const
    {
        if (keyRootComboBox.getSelectedId() <= 1) // "none"
            return {};
        return keyRootComboBox.getText() + " " + keyModeComboBox.getText();
    }

    void setKeyScale(const juce::String& keyScale)
    {
        if (keyScale.isEmpty())
        {
            keyRootComboBox.setSelectedId(1, juce::dontSendNotification); // "none"
            keyModeComboBox.setVisible(false);
            resized();
            return;
        }
        // Parse "C# minor" → root="C#", mode="minor"
        const juce::String lower = keyScale.trim().toLowerCase();
        const bool isMinor = lower.contains("minor");
        const juce::String root = keyScale.trim().upToFirstOccurrenceOf(" ", false, true).trim();
        // Find matching root in combo box
        for (int i = 0; i < keyRootComboBox.getNumItems(); ++i)
        {
            if (keyRootComboBox.getItemText(i).equalsIgnoreCase(root))
            {
                keyRootComboBox.setSelectedItemIndex(i, juce::dontSendNotification);
                break;
            }
        }
        keyModeComboBox.setSelectedId(isMinor ? 2 : 1, juce::dontSendNotification);
        keyModeComboBox.setVisible(true);
        resized();
    }

    // Time signature (global across subtabs)
    juce::String getTimeSig() const
    {
        const int id = timeSigComboBox.getSelectedId();
        if (id == 2) return "2";
        if (id == 3) return "3";
        if (id == 4) return "4";
        if (id == 5) return "6";
        return {};  // auto
    }

    void setTimeSig(const juce::String& timeSig)
    {
        if (timeSig == "2") timeSigComboBox.setSelectedId(2, juce::dontSendNotification);
        else if (timeSig == "3") timeSigComboBox.setSelectedId(3, juce::dontSendNotification);
        else if (timeSig == "4") timeSigComboBox.setSelectedId(4, juce::dontSendNotification);
        else if (timeSig == "6") timeSigComboBox.setSelectedId(5, juce::dontSendNotification);
        else timeSigComboBox.setSelectedId(1, juce::dontSendNotification);  // auto
    }

    void setGenerateButtonEnabled(bool enabled, bool isGenerating)
    {
        legoGenerateButton.setEnabled(enabled && !isGenerating);
        legoGenerateButton.setButtonText(isGenerating ? "generating..." : "send to carey");

        const bool completeAvailable = (bool)onCompleteGenerate;
        completeGenerateButton.setEnabled(completeAvailable && enabled && !isGenerating);
        completeGenerateButton.setButtonText(isGenerating ? "generating..." : "continue with carey");

        const bool coverAvailable = (bool)onCoverGenerate;
        coverGenerateButton.setEnabled(coverAvailable && enabled && !isGenerating);
        coverGenerateButton.setButtonText(isGenerating ? "generating..." : "remix with carey");
    }

    std::function<void(SubTab)> onSubTabChanged;
    std::function<void(const juce::String&)> onCaptionChanged;
    std::function<void(const juce::String&)> onTrackChanged;
    std::function<void(int)> onStepsChanged;
    std::function<void(double)> onLegoCfgChanged;
    std::function<void(bool)> onLoopAssistChanged;
    std::function<void(bool)> onTrimToInputChanged;
    std::function<void()> onGenerate;
    std::function<void(const juce::String&)> onLyricsChanged;          // Shared: fires from any tab's lyrics button
    std::function<void(const juce::String&)> onLyricsLanguageChanged;  // Fires when language selection changes

    std::function<void(const juce::String&)> onCompleteCaptionChanged;
    std::function<void(int)> onCompleteBpmChanged;
    std::function<void(int)> onCompleteStepsChanged;
    std::function<void(double)> onCompleteCfgChanged;
    std::function<void(int)> onCompleteDurationChanged;
    std::function<void()> onCompleteGenerate;
    // onCompleteLyricsChanged removed - lyrics are shared, use onLyricsChanged
    std::function<void(bool)> onCompleteUseSrcAsRefChanged;

    std::function<void(const juce::String&)> onCoverCaptionChanged;
    // onCoverLyricsChanged removed - lyrics are shared, use onLyricsChanged
    std::function<void(double)> onCoverNoiseStrengthChanged;
    std::function<void(double)> onCoverAudioStrengthChanged;
    std::function<void(int)> onCoverStepsChanged;
    std::function<void(double)> onCoverCfgChanged;
    std::function<void(bool)> onCoverUseSrcAsRefChanged;
    std::function<void(bool)> onCoverLoopAssistChanged;
    std::function<void(bool)> onCoverTrimToInputChanged;
    std::function<void()> onCoverGenerate;

    std::function<void(const juce::String&)> onKeyScaleChanged;
    std::function<void(const juce::String&)> onTimeSigChanged;

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
        legoSubTabButton.setButtonStyle(currentSubTab == SubTab::Lego
            ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
        completeSubTabButton.setButtonStyle(currentSubTab == SubTab::Complete
            ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
        coverSubTabButton.setButtonStyle(currentSubTab == SubTab::Cover
            ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
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
        const bool showingComplete = (currentSubTab == SubTab::Complete);
        const bool showingCover = (currentSubTab == SubTab::Cover);
        setLegoControlsVisible(showingLego);
        setCompleteControlsVisible(showingComplete);
        setCoverControlsVisible(showingCover);

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

            legoAdvancedToggle.setBounds(fullRow(26));
            y += 32;

            if (legoAdvancedOpen)
            {
                auto stepsRow = fullRow(28);
                stepsLabel.setBounds(stepsRow.removeFromLeft(60));
                stepsSlider.setBounds(stepsRow);
                y += 36;

                auto legoCfgRow = fullRow(28);
                legoCfgLabel.setBounds(legoCfgRow.removeFromLeft(60));
                legoCfgSlider.setBounds(legoCfgRow);
                y += 36;

                auto assistRow = fullRow(22);
                auto leftAssist = assistRow.removeFromLeft(assistRow.getWidth() / 2);
                loopAssistLabel.setBounds(leftAssist.removeFromLeft(80));
                loopAssistToggle.setBounds(leftAssist.removeFromLeft(22));

                trimToInputLabel.setBounds(assistRow.removeFromLeft(90));
                trimToInputToggle.setBounds(assistRow.removeFromLeft(22));
                y += 30;
            }

            auto generateRow = fullRow(34).reduced(50, 0);
            legoGenerateButton.setBounds(generateRow);
            y += 44;

            legoInfoLabel.setBounds(fullRow(24));
            y += 32;
        }
        else if (showingComplete)
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

            completeAdvancedToggle.setBounds(fullRow(26));
            y += 32;

            if (completeAdvancedOpen)
            {
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

                auto completeCfgRow = fullRow(28);
                completeCfgLabel.setBounds(completeCfgRow.removeFromLeft(110));
                completeCfgSlider.setBounds(completeCfgRow);
                y += 36;

                auto durationRow = fullRow(28);
                completeDurationLabel.setBounds(durationRow.removeFromLeft(110));
                completeDurationSlider.setBounds(durationRow);
                y += 36;

                completeUseSrcAsRefToggle.setBounds(fullRow(22));
                y += 30;
            }

            auto completeGenerateRow = fullRow(34).reduced(50, 0);
            completeGenerateButton.setBounds(completeGenerateRow);
            y += 44;

            completeInfoLabel.setBounds(fullRow(32));
            y += 40;
        }
        else // Cover
        {
            coverCaptionLabel.setBounds(fullRow(18));
            y += 20;

            auto coverCaptionRow = fullRow(28);
            auto coverLyricsBounds = coverCaptionRow.removeFromRight(64);
            coverCaptionRow.removeFromRight(4);
            auto coverDiceBounds = coverCaptionRow.removeFromRight(22);
            coverCaptionRow.removeFromRight(4);
            coverCaptionEditor.setBounds(coverCaptionRow);
            const auto coverDiceSquare = coverDiceBounds.withHeight(22).withY(coverDiceBounds.getY() + 3);
            coverCaptionDiceButton.setBounds(coverDiceSquare);
            coverLyricsButton.setBounds(coverLyricsBounds.withHeight(24).withY(coverLyricsBounds.getY() + 2));
            y += 36;

            coverAdvancedToggle.setBounds(fullRow(26));
            y += 32;

            if (coverAdvancedOpen)
            {
                auto noiseRow = fullRow(28);
                coverNoiseStrengthLabel.setBounds(noiseRow.removeFromLeft(110));
                coverNoiseStrengthSlider.setBounds(noiseRow);
                y += 36;

                auto audioRow = fullRow(28);
                coverAudioStrengthLabel.setBounds(audioRow.removeFromLeft(110));
                coverAudioStrengthSlider.setBounds(audioRow);
                y += 36;

                auto stepsRow = fullRow(28);
                coverStepsLabel.setBounds(stepsRow.removeFromLeft(110));
                coverStepsSlider.setBounds(stepsRow);
                y += 36;

                auto cfgRow = fullRow(28);
                coverCfgLabel.setBounds(cfgRow.removeFromLeft(110));
                coverCfgSlider.setBounds(cfgRow);
                y += 36;

                auto coverAssistRow = fullRow(22);
                auto leftCoverAssist = coverAssistRow.removeFromLeft(coverAssistRow.getWidth() / 2);
                coverLoopAssistLabel.setBounds(leftCoverAssist.removeFromLeft(80));
                coverLoopAssistToggle.setBounds(leftCoverAssist.removeFromLeft(22));

                coverTrimToInputLabel.setBounds(coverAssistRow.removeFromLeft(90));
                coverTrimToInputToggle.setBounds(coverAssistRow.removeFromLeft(22));
                y += 30;

                coverUseSrcAsRefToggle.setBounds(fullRow(22));
                y += 30;
            }

            auto coverGenerateRow = fullRow(34).reduced(50, 0);
            coverGenerateButton.setBounds(coverGenerateRow);
            y += 44;

            coverInfoLabel.setBounds(fullRow(40));
            y += 48;
        }

        const int minHeight = juce::jmax(y + 8, contentViewport->getHeight());
        contentComponent->setSize(contentWidth, minHeight);
    }

    void setLegoControlsVisible(bool shouldBeVisible)
    {
        // Always visible when tab is active
        captionLabel.setVisible(shouldBeVisible);
        captionEditor.setVisible(shouldBeVisible);
        captionDiceButton.setVisible(shouldBeVisible);
        legoLyricsButton.setVisible(shouldBeVisible);
        trackLabel.setVisible(shouldBeVisible);
        trackComboBox.setVisible(shouldBeVisible);
        legoAdvancedToggle.setVisible(shouldBeVisible);
        legoGenerateButton.setVisible(shouldBeVisible);
        legoInfoLabel.setVisible(shouldBeVisible);

        // Advanced: only visible if open AND tab is active
        const bool showAdvanced = shouldBeVisible && legoAdvancedOpen;
        stepsLabel.setVisible(showAdvanced);
        stepsSlider.setVisible(showAdvanced);
        legoCfgLabel.setVisible(showAdvanced);
        legoCfgSlider.setVisible(showAdvanced);
        loopAssistLabel.setVisible(showAdvanced);
        loopAssistToggle.setVisible(showAdvanced);
        trimToInputLabel.setVisible(showAdvanced);
        trimToInputToggle.setVisible(showAdvanced);
    }

    void setCompleteControlsVisible(bool shouldBeVisible)
    {
        const bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();

        // Always visible when tab is active
        completeCaptionLabel.setVisible(shouldBeVisible);
        completeCaptionEditor.setVisible(shouldBeVisible);
        completeCaptionDiceButton.setVisible(shouldBeVisible);
        completeLyricsButton.setVisible(shouldBeVisible);
        completeAdvancedToggle.setVisible(shouldBeVisible);
        completeGenerateButton.setVisible(shouldBeVisible);
        completeInfoLabel.setVisible(shouldBeVisible);

        // Advanced: only visible if open AND tab is active
        const bool showAdvanced = shouldBeVisible && completeAdvancedOpen;
        completeBpmLabel.setVisible(showAdvanced && isStandalone);
        completeBpmSlider.setVisible(showAdvanced && isStandalone);
        completeStepsLabel.setVisible(showAdvanced);
        completeStepsSlider.setVisible(showAdvanced);
        completeCfgLabel.setVisible(showAdvanced);
        completeCfgSlider.setVisible(showAdvanced);
        completeDurationLabel.setVisible(showAdvanced);
        completeDurationSlider.setVisible(showAdvanced);
        completeUseSrcAsRefToggle.setVisible(showAdvanced);
    }

    void setCoverControlsVisible(bool shouldBeVisible)
    {
        // Always visible when tab is active
        coverCaptionLabel.setVisible(shouldBeVisible);
        coverCaptionEditor.setVisible(shouldBeVisible);
        coverCaptionDiceButton.setVisible(shouldBeVisible);
        coverLyricsButton.setVisible(shouldBeVisible);
        coverAdvancedToggle.setVisible(shouldBeVisible);
        coverGenerateButton.setVisible(shouldBeVisible);
        coverInfoLabel.setVisible(shouldBeVisible);

        // Advanced: only visible if open AND tab is active
        const bool showAdvanced = shouldBeVisible && coverAdvancedOpen;
        coverNoiseStrengthLabel.setVisible(showAdvanced);
        coverNoiseStrengthSlider.setVisible(showAdvanced);
        coverAudioStrengthLabel.setVisible(showAdvanced);
        coverAudioStrengthSlider.setVisible(showAdvanced);
        coverStepsLabel.setVisible(showAdvanced);
        coverStepsSlider.setVisible(showAdvanced);
        coverCfgLabel.setVisible(showAdvanced);
        coverCfgSlider.setVisible(showAdvanced);
        coverLoopAssistLabel.setVisible(showAdvanced);
        coverLoopAssistToggle.setVisible(showAdvanced);
        coverTrimToInputLabel.setVisible(showAdvanced);
        coverTrimToInputToggle.setVisible(showAdvanced);
        coverUseSrcAsRefToggle.setVisible(showAdvanced);
    }

    void onKeyScaleSelectionChanged()
    {
        const bool keySelected = (keyRootComboBox.getSelectedId() > 1);
        keyModeComboBox.setVisible(keySelected);
        resized(); // relayout to show/hide mode combo

        if (onKeyScaleChanged)
            onKeyScaleChanged(getKeyScale());
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

    juce::StringArray getCoverPromptBank() const
    {
        return {
            "orchestral symphonic arrangement with lush strings and cinematic dynamics",
            "lo-fi bedroom pop reimagining with tape warmth and dreamy vocal processing",
            "stripped-down acoustic folk cover with fingerpicked guitar and intimate vocals",
            "heavy metal reinterpretation with distorted guitars and aggressive energy",
            "jazzy lounge arrangement with smooth piano, walking bass, and mellow horns",
            "electronic synthwave rework with retro synths, gated reverb, and pulsing arpeggios",
            "reggae dub version with offbeat guitar, deep bass, and echo-heavy mixing",
            "bossa nova arrangement with nylon guitar, soft percussion, and gentle phrasing"
        };
    }

    void applyRandomCoverCaption()
    {
        const juce::String caption = pickRandomCaption(getCoverPromptBank(),
                                                       "orchestral symphonic arrangement with lush strings and cinematic dynamics");
        coverCaptionEditor.setText(caption, juce::sendNotification);
    }

    void setLyricsTextInternal(const juce::String& text, bool notify)
    {
        lyricsText = text;
        updateLyricsButtonLabels();
        if (notify && onLyricsChanged)
            onLyricsChanged(lyricsText);
    }

    void updateLyricsButtonLabels()
    {
        const bool hasLyrics = !lyricsText.trim().isEmpty();
        legoLyricsButton.setButtonText(hasLyrics ? "lyrics*" : "lyrics");
        completeLyricsButton.setButtonText(hasLyrics ? "lyrics*" : "lyrics");
        coverLyricsButton.setButtonText(hasLyrics ? "lyrics*" : "lyrics");
    }

    void openLyricsEditor()
    {
        auto* dialogContent = new CareyLyricsDialogContent(
            lyricsText,
            lyricsLanguage,
            [this](const juce::String& text, const juce::String& language)
            {
                setLyricsTextInternal(text, true);
                lyricsLanguage = language;
                if (onLyricsLanguageChanged)
                    onLyricsLanguageChanged(lyricsLanguage);
            });

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(dialogContent);
        options.content->setSize(560, 360);
        options.dialogTitle = "carey lyrics";
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
    juce::Label legoCfgLabel;
    CustomSlider legoCfgSlider;
    juce::Label loopAssistLabel;
    juce::ToggleButton loopAssistToggle;
    juce::Label trimToInputLabel;
    juce::ToggleButton trimToInputToggle;
    CustomButton legoGenerateButton;
    juce::Label legoInfoLabel;
    juce::String lyricsText;      // Shared across all tabs (lego/complete/cover)
    juce::String lyricsLanguage = "en";  // Language code for lyrics vocalization

    juce::Label completeCaptionLabel;
    CustomTextEditor completeCaptionEditor;
    CustomButton completeCaptionDiceButton;
    CustomButton completeLyricsButton;
    juce::Label completeBpmLabel;
    CustomSlider completeBpmSlider;
    juce::Label completeStepsLabel;
    CustomSlider completeStepsSlider;
    juce::Label completeCfgLabel;
    CustomSlider completeCfgSlider;
    juce::Label completeDurationLabel;
    CustomSlider completeDurationSlider;
    juce::ToggleButton completeUseSrcAsRefToggle;
    CustomButton completeGenerateButton;
    juce::Label completeInfoLabel;
    // completeLyricsText removed - lyrics are shared via lyricsText

    CustomButton coverSubTabButton;
    juce::Label keyScaleLabel;
    juce::Label timeSigLabel;
    CustomComboBox timeSigComboBox;
    CustomComboBox keyRootComboBox;
    CustomComboBox keyModeComboBox;

    juce::Label coverCaptionLabel;
    CustomTextEditor coverCaptionEditor;
    CustomButton coverCaptionDiceButton;
    CustomButton coverLyricsButton;
    juce::Label coverNoiseStrengthLabel;
    CustomSlider coverNoiseStrengthSlider;
    juce::Label coverAudioStrengthLabel;
    CustomSlider coverAudioStrengthSlider;
    juce::Label coverStepsLabel;
    CustomSlider coverStepsSlider;
    juce::Label coverCfgLabel;
    CustomSlider coverCfgSlider;
    juce::Label coverLoopAssistLabel;
    juce::ToggleButton coverLoopAssistToggle;
    juce::Label coverTrimToInputLabel;
    juce::ToggleButton coverTrimToInputToggle;
    juce::ToggleButton coverUseSrcAsRefToggle;
    CustomButton coverGenerateButton;
    juce::Label coverInfoLabel;
    // coverLyricsText removed - lyrics are shared via lyricsText

    CustomButton legoAdvancedToggle;
    CustomButton completeAdvancedToggle;
    CustomButton coverAdvancedToggle;
    bool legoAdvancedOpen = false;
    bool completeAdvancedOpen = false;
    bool coverAdvancedOpen = false;

    juce::Rectangle<int> titleBounds;
};
