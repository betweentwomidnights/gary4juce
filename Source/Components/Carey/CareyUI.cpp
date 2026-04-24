#include "CareyUI.h"

namespace
{
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

        languageLabel.setText("language", juce::dontSendNotification);
        languageLabel.setFont(juce::FontOptions(11.0f));
        languageLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        languageLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(languageLabel);

        initializeLanguageOptions();
        const int langIdx = languageCodes.indexOf(initialLanguage);
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
                const int selectedIdx = languageComboBox.getSelectedId() - 1;
                const juce::String langCode = (selectedIdx >= 0 && selectedIdx < languageCodes.size())
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
        struct LangEntry { const char* code; const char* name; };
        const LangEntry languages[] = {
            { "en", "English" }, { "ar", "Arabic" }, { "az", "Azerbaijani" }, { "bn", "Bengali" },
            { "bg", "Bulgarian" }, { "yue", "Cantonese" }, { "ca", "Catalan" }, { "zh", "Chinese" },
            { "hr", "Croatian" }, { "cs", "Czech" }, { "da", "Danish" }, { "nl", "Dutch" },
            { "fi", "Finnish" }, { "fr", "French" }, { "de", "German" }, { "el", "Greek" },
            { "ht", "Haitian Creole" }, { "he", "Hebrew" }, { "hi", "Hindi" }, { "hu", "Hungarian" },
            { "is", "Icelandic" }, { "id", "Indonesian" }, { "it", "Italian" }, { "ja", "Japanese" },
            { "ko", "Korean" }, { "la", "Latin" }, { "lt", "Lithuanian" }, { "ms", "Malay" },
            { "ne", "Nepali" }, { "no", "Norwegian" }, { "fa", "Persian" }, { "pl", "Polish" },
            { "pt", "Portuguese" }, { "pa", "Punjabi" }, { "ro", "Romanian" }, { "ru", "Russian" },
            { "sa", "Sanskrit" }, { "sr", "Serbian" }, { "sk", "Slovak" }, { "es", "Spanish" },
            { "sw", "Swahili" }, { "sv", "Swedish" }, { "tl", "Tagalog" }, { "ta", "Tamil" },
            { "te", "Telugu" }, { "th", "Thai" }, { "tr", "Turkish" }, { "uk", "Ukrainian" },
            { "ur", "Urdu" }, { "vi", "Vietnamese" },
        };

        for (int i = 0; i < (int) (sizeof(languages) / sizeof(languages[0])); ++i)
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
}

CareyUI::CareyUI()
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
    prepareSubTabButton(completeSubTabButton, juce::String::fromUTF8("complete \xe2\x9a\xa0"), SubTab::Complete);
    completeSubTabButton.setTooltip("this mode is pretty unhinged...expect to chop out the samples amongst the madness");
    prepareSubTabButton(coverSubTabButton, juce::String::fromUTF8("cover \xe2\x9a\xa0"), SubTab::Cover);
    coverSubTabButton.setTooltip("experimental - results may vary. use at your own risk");
    prepareSubTabButton(extractSubTabButton, juce::String::fromUTF8("extract \xe2\x9a\xa0"), SubTab::Extract);
    extractSubTabButton.setTooltip("we're still figuring out how to use this properly. if you have a stem separator it may work better");

    keyScaleLabel.setText("key", juce::dontSendNotification);
    keyScaleLabel.setFont(juce::FontOptions(11.0f));
    keyScaleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    keyScaleLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(keyScaleLabel);

    keyRootComboBox.addItem("none", 1);
    const juce::StringArray keyNames = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < keyNames.size(); ++i)
        keyRootComboBox.addItem(keyNames[i], i + 2);
    keyRootComboBox.setSelectedId(1, juce::dontSendNotification);
    keyRootComboBox.setTooltip("key root (optional - helps guide generation)");
    keyRootComboBox.onChange = [this]() { onKeyScaleSelectionChanged(); };
    addAndMakeVisible(keyRootComboBox);

    keyModeComboBox.addItem("major", 1);
    keyModeComboBox.addItem("minor", 2);
    keyModeComboBox.setSelectedId(1, juce::dontSendNotification);
    keyModeComboBox.setTooltip("major or minor scale");
    keyModeComboBox.onChange = [this]() { onKeyScaleSelectionChanged(); };
    addAndMakeVisible(keyModeComboBox);
    keyModeComboBox.setVisible(false);

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
    timeSigComboBox.setSelectedId(1, juce::dontSendNotification);
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
    trackComboBox.setSelectedId(1, juce::dontSendNotification);
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
    completeCaptionDiceButton.setTooltip("fetch a backend caption for complete mode");
    completeCaptionDiceButton.onClick = [this]()
    {
        if (onCompleteCaptionDiceRequested)
            onCompleteCaptionDiceRequested();
    };
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

    completeUseLoraToggle.setButtonText("use lora");
    completeUseLoraToggle.setToggleState(false, juce::dontSendNotification);
    completeUseLoraToggle.setTooltip("enable a backend LoRA adapter and use its caption pool for the dice button");
    completeUseLoraToggle.setEnabled(false);
    completeUseLoraToggle.onClick = [this]()
    {
        const bool enabled = completeUseLoraToggle.getToggleState() && !completeLoraOptionValues.isEmpty();
        completeUseLoraToggle.setToggleState(enabled, juce::dontSendNotification);
        if (onCompleteUseLoraChanged)
            onCompleteUseLoraChanged(enabled);
        updateContentLayout();
    };
    addToContent(completeUseLoraToggle);

    completeLoraLabel.setText("lora", juce::dontSendNotification);
    completeLoraLabel.setFont(juce::FontOptions(12.0f));
    completeLoraLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
    completeLoraLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(completeLoraLabel);

    completeLoraComboBox.setTextWhenNothingSelected("select lora...");
    completeLoraComboBox.setTooltip("choose which backend LoRA adapter to use for complete mode");
    completeLoraComboBox.setEnabled(false);
    completeLoraComboBox.onChange = [this]()
    {
        if (onCompleteLoraChanged)
            onCompleteLoraChanged(getCompleteSelectedLora());
    };
    addToContent(completeLoraComboBox);

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

    completeModelLabel.setText("model", juce::dontSendNotification);
    completeModelLabel.setFont(juce::FontOptions(12.0f));
    completeModelLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
    completeModelLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(completeModelLabel);

    completeTurboModelToggle.setToggleState(true, juce::dontSendNotification);
    completeTurboModelToggle.onClick = [this]() { setCompleteModel(completeRemoteModelSelectionEnabled ? "xl-turbo" : "turbo"); };
    addToContent(completeTurboModelToggle);

    completeBaseModelToggle.setToggleState(false, juce::dontSendNotification);
    completeBaseModelToggle.onClick = [this]() { setCompleteModel(completeRemoteModelSelectionEnabled ? "xl-base" : "base"); };
    addToContent(completeBaseModelToggle);

    completeSftModelToggle.setToggleState(false, juce::dontSendNotification);
    completeSftModelToggle.onClick = [this]() { setCompleteModel("sft"); };
    addToContent(completeSftModelToggle);

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
    coverCaptionDiceButton.setTooltip("fetch a backend caption for cover mode");
    coverCaptionDiceButton.onClick = [this]()
    {
        if (onCoverCaptionDiceRequested)
            onCoverCaptionDiceRequested();
    };
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

    coverUseLoraToggle.setButtonText("use lora");
    coverUseLoraToggle.setToggleState(false, juce::dontSendNotification);
    coverUseLoraToggle.setTooltip("enable a backend LoRA adapter and use its caption pool for the dice button");
    coverUseLoraToggle.setEnabled(false);
    coverUseLoraToggle.onClick = [this]()
    {
        const bool enabled = coverUseLoraToggle.getToggleState() && !coverLoraOptionValues.isEmpty();
        coverUseLoraToggle.setToggleState(enabled, juce::dontSendNotification);
        if (onCoverUseLoraChanged)
            onCoverUseLoraChanged(enabled);
        updateContentLayout();
    };
    addToContent(coverUseLoraToggle);

    coverLoraLabel.setText("lora", juce::dontSendNotification);
    coverLoraLabel.setFont(juce::FontOptions(12.0f));
    coverLoraLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
    coverLoraLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(coverLoraLabel);

    coverLoraComboBox.setTextWhenNothingSelected("select lora...");
    coverLoraComboBox.setTooltip("choose which backend LoRA adapter to use for cover mode");
    coverLoraComboBox.setEnabled(false);
    coverLoraComboBox.onChange = [this]()
    {
        if (onCoverLoraChanged)
            onCoverLoraChanged(getCoverSelectedLora());
    };
    addToContent(coverLoraComboBox);

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

    coverModelLabel.setText("model", juce::dontSendNotification);
    coverModelLabel.setFont(juce::FontOptions(12.0f));
    coverModelLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
    coverModelLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(coverModelLabel);

    coverTurboModelToggle.setToggleState(true, juce::dontSendNotification);
    coverTurboModelToggle.onClick = [this]() { setCoverModel(coverRemoteModelSelectionEnabled ? "xl-turbo" : "turbo"); };
    addToContent(coverTurboModelToggle);

    coverBaseModelToggle.setToggleState(false, juce::dontSendNotification);
    coverBaseModelToggle.onClick = [this]() { setCoverModel(coverRemoteModelSelectionEnabled ? "xl-base" : "base"); };
    addToContent(coverBaseModelToggle);

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
    coverStepsSlider.setValue(kFixedCoverSteps);
    coverStepsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
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

    coverCfgSlider.setRange(1.0, 10.0, 0.1);
    coverCfgSlider.setValue(kFixedCoverCfg);
    coverCfgSlider.setNumDecimalPlacesToDisplay(1);
    coverCfgSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
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

    extractTrackLabel.setText("track", juce::dontSendNotification);
    extractTrackLabel.setFont(juce::FontOptions(12.0f));
    extractTrackLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
    extractTrackLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(extractTrackLabel);

    initializeExtractTrackOptions();
    extractTrackComboBox.setTextWhenNothingSelected("select stem to extract...");
    extractTrackComboBox.setTooltip("drums and vocals are the most reliable. other stems are still experimental.");
    extractTrackComboBox.onChange = [this]()
    {
        if (onExtractTrackChanged)
            onExtractTrackChanged(getExtractTrackName());
    };
    extractTrackComboBox.setSelectedId(1, juce::dontSendNotification);
    addToContent(extractTrackComboBox);

    extractBpmLabel.setText("bpm", juce::dontSendNotification);
    extractBpmLabel.setFont(juce::FontOptions(12.0f));
    extractBpmLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
    extractBpmLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(extractBpmLabel);

    extractBpmSlider.setRange(20, 300, 1);
    extractBpmSlider.setValue(120);
    extractBpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    extractBpmSlider.setTooltip("BPM for the source audio. extract works best when this matches the input.");
    extractBpmSlider.onValueChange = [this]()
    {
        if (onExtractBpmChanged)
            onExtractBpmChanged(getExtractBpm());
    };
    addToContent(extractBpmSlider);

    extractAdvancedToggle.setButtonText(juce::String::fromUTF8("advanced \xe2\x96\xb6"));
    extractAdvancedToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
    extractAdvancedToggle.setTooltip("show/hide advanced extraction settings");
    extractAdvancedToggle.onClick = [this]() {
        extractAdvancedOpen = !extractAdvancedOpen;
        extractAdvancedToggle.setButtonText(extractAdvancedOpen
            ? juce::String::fromUTF8("advanced \xe2\x96\xbc")
            : juce::String::fromUTF8("advanced \xe2\x96\xb6"));
        updateContentLayout();
    };
    addToContent(extractAdvancedToggle);

    extractStepsLabel.setText("steps", juce::dontSendNotification);
    extractStepsLabel.setFont(juce::FontOptions(12.0f));
    extractStepsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
    extractStepsLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(extractStepsLabel);

    extractStepsSlider.setRange(32, 100, 1);
    extractStepsSlider.setValue(kDefaultExtractSteps);
    extractStepsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    extractStepsSlider.setTooltip("diffusion steps for extract. more = slower but usually cleaner. recommended: 80");
    extractStepsSlider.onValueChange = [this]()
    {
        if (onExtractStepsChanged)
            onExtractStepsChanged(getExtractSteps());
    };
    addToContent(extractStepsSlider);

    extractCfgLabel.setText("cfg scale", juce::dontSendNotification);
    extractCfgLabel.setFont(juce::FontOptions(12.0f));
    extractCfgLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
    extractCfgLabel.setJustificationType(juce::Justification::centredLeft);
    addToContent(extractCfgLabel);

    extractCfgSlider.setRange(3.0, 10.0, 0.1);
    extractCfgSlider.setValue(kDefaultExtractCfg);
    extractCfgSlider.setNumDecimalPlacesToDisplay(1);
    extractCfgSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    extractCfgSlider.setTooltip("guidance scale for extract. recommended: 10.0");
    extractCfgSlider.onValueChange = [this]()
    {
        if (onExtractCfgChanged)
            onExtractCfgChanged(getExtractCfg());
    };
    addToContent(extractCfgSlider);

    extractGenerateButton.setButtonText(juce::String::fromUTF8("extract with carey \xe2\x9a\xa0"));
    extractGenerateButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
    extractGenerateButton.setTooltip("we're still figuring out how to use this properly. if you have a stem separator it may work better");
    extractGenerateButton.setEnabled(false);
    extractGenerateButton.onClick = [this]()
    {
        if (onExtractGenerate)
            onExtractGenerate();
    };
    addToContent(extractGenerateButton);

    extractInfoLabel.setText("drums and vocals work best. other stem types are still experimental.", juce::dontSendNotification);
    extractInfoLabel.setFont(juce::FontOptions(11.0f));
    extractInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    extractInfoLabel.setJustificationType(juce::Justification::centred);
    addToContent(extractInfoLabel);

    updateCompleteModelSelectorCopy();
    updateCoverModelSelectorCopy();
    updateCoverModelControls(false);
    updateLyricsButtonLabels();
    setCurrentSubTabInternal(SubTab::Lego, false);
}

juce::String CareyUI::getCompleteModel() const
{
    if (completeRemoteModelSelectionEnabled)
        return completeBaseModelToggle.getToggleState() ? "xl-base" : "xl-turbo";

    if (completeSftModelToggle.getToggleState())
        return "sft";
    if (completeBaseModelToggle.getToggleState())
        return "base";
    return "turbo";
}

void CareyUI::setCompleteModel(const juce::String& model)
{
    const juce::String normalized = model.trim().toLowerCase();
    const bool useTurboModel = normalized.isEmpty() || normalized.contains("turbo");
    const bool useSftModel = !useTurboModel && normalized.contains("sft");
    const bool useBaseModel = !useTurboModel && !useSftModel;

    if (completeRemoteModelSelectionEnabled)
    {
        completeTurboModelToggle.setToggleState(useTurboModel, juce::dontSendNotification);
        completeBaseModelToggle.setToggleState(!useTurboModel, juce::dontSendNotification);
        completeSftModelToggle.setToggleState(false, juce::dontSendNotification);
    }
    else
    {
        completeTurboModelToggle.setToggleState(useTurboModel, juce::dontSendNotification);
        completeBaseModelToggle.setToggleState(useBaseModel, juce::dontSendNotification);
        completeSftModelToggle.setToggleState(useSftModel, juce::dontSendNotification);
    }

    updateCompleteModelControls(true);
}

void CareyUI::updateCompleteModelSelectorCopy()
{
    if (completeRemoteModelSelectionEnabled)
    {
        completeModelLabel.setTooltip("remote complete model: xl-turbo is fixed at 8 steps / cfg 1.0; xl-base keeps editable steps and cfg.");

        completeTurboModelToggle.setButtonText("xl-turbo");
        completeTurboModelToggle.setTooltip("fast remote complete model, fixed at 8 steps and cfg 1.0.");

        completeBaseModelToggle.setButtonText("xl-base");
        completeBaseModelToggle.setTooltip("remote complete base model with editable steps and cfg.");

        completeSftModelToggle.setButtonText("sft");
        completeSftModelToggle.setTooltip({});
    }
    else
    {
        const juce::String localHint = "if you want the xl variants instead, toggle carey xl models in gary4local. recommended: 16 GB VRAM.";

        completeModelLabel.setTooltip("localhost complete model: turbo is fixed at 8 steps / cfg 1.0. base and sft keep editable steps and cfg. " + localHint);

        completeTurboModelToggle.setButtonText("turbo");
        completeTurboModelToggle.setTooltip("fast localhost complete model, fixed at 8 steps and cfg 1.0. " + localHint);

        completeBaseModelToggle.setButtonText("base");
        completeBaseModelToggle.setTooltip("localhost complete base model with editable steps and cfg. " + localHint);

        completeSftModelToggle.setButtonText("sft");
        completeSftModelToggle.setTooltip("localhost complete sft model with editable steps and cfg. " + localHint);
    }
}

juce::String CareyUI::getCoverModel() const
{
    return coverBaseModelToggle.getToggleState()
        ? (coverRemoteModelSelectionEnabled ? "xl-base" : "base")
        : (coverRemoteModelSelectionEnabled ? "xl-turbo" : "turbo");
}

void CareyUI::setCoverModel(const juce::String& model)
{
    const juce::String normalized = model.trim().toLowerCase();
    const bool useTurboModel = normalized.isEmpty() || normalized.contains("turbo");

    coverTurboModelToggle.setToggleState(useTurboModel, juce::dontSendNotification);
    coverBaseModelToggle.setToggleState(!useTurboModel, juce::dontSendNotification);

    updateCoverModelControls(true);
}

void CareyUI::setCoverModelSelectionEnabled(bool enabled)
{
    if (coverModelSelectionEnabled == enabled)
        return;

    coverModelSelectionEnabled = enabled;
    if (!coverModelSelectionEnabled)
        setCoverModel(coverRemoteModelSelectionEnabled ? "xl-turbo" : "turbo");
    else
        updateCoverModelControls(true);

    updateContentLayout();
}

void CareyUI::setCoverRemoteModelSelectionEnabled(bool enabled)
{
    const juce::String currentModel = getCoverModel();
    if (coverRemoteModelSelectionEnabled == enabled)
        return;

    coverRemoteModelSelectionEnabled = enabled;
    setCoverModel(currentModel);
    updateContentLayout();
}

void CareyUI::setCoverSteps(int steps)
{
    if (getCoverModel().containsIgnoreCase("turbo"))
        coverStepsSlider.setValue(kFixedCoverSteps, juce::dontSendNotification);
    else
        coverStepsSlider.setValue(juce::jlimit(8, 100, steps), juce::dontSendNotification);
}

void CareyUI::setCoverCfg(double value)
{
    if (getCoverModel().containsIgnoreCase("turbo"))
        coverCfgSlider.setValue(kFixedCoverCfg, juce::dontSendNotification);
    else
        coverCfgSlider.setValue(juce::jlimit(3.0, 10.0, value), juce::dontSendNotification);
}

void CareyUI::updateCoverModelSelectorCopy()
{
    if (coverRemoteModelSelectionEnabled)
    {
        coverModelLabel.setTooltip("remote cover model: xl-turbo is fixed at 8 steps / cfg 1.0; xl-base unlocks editable steps and cfg.");

        coverTurboModelToggle.setButtonText("xl-turbo");
        coverTurboModelToggle.setTooltip("fast remote cover model, fixed at 8 steps and cfg 1.0.");

        coverBaseModelToggle.setButtonText("xl-base");
        coverBaseModelToggle.setTooltip("remote cover base model with editable steps and cfg.");
    }
    else
    {
        const juce::String localHint = "if you want the xl variants instead, toggle carey xl models in gary4local. recommended: 16 GB VRAM.";

        coverModelLabel.setTooltip("localhost cover model: turbo is fixed at 8 steps / cfg 1.0. base keeps editable steps and cfg. " + localHint);

        coverTurboModelToggle.setButtonText("turbo");
        coverTurboModelToggle.setTooltip("fast localhost cover model, fixed at 8 steps and cfg 1.0. " + localHint);

        coverBaseModelToggle.setButtonText("base");
        coverBaseModelToggle.setTooltip("localhost cover base model with editable steps and cfg. " + localHint);
    }
}

CareyUI::~CareyUI()
{
    if (contentViewport != nullptr)
        contentViewport->getVerticalScrollBar().setLookAndFeel(nullptr);
}

void CareyUI::paint(juce::Graphics&)
{
}

void CareyUI::resized()
{
    constexpr int margin = 12;
    auto area = getLocalBounds().reduced(margin);

    titleBounds = area.removeFromTop(30);
    careyLabel.setBounds(titleBounds);

    area.removeFromTop(6);
    auto subTabRow = area.removeFromTop(30);
    const int tabWidth = subTabRow.getWidth() / 4;
    legoSubTabButton.setBounds(subTabRow.removeFromLeft(tabWidth).reduced(2, 2));
    completeSubTabButton.setBounds(subTabRow.removeFromLeft(tabWidth).reduced(2, 2));
    coverSubTabButton.setBounds(subTabRow.removeFromLeft(tabWidth).reduced(2, 2));
    extractSubTabButton.setBounds(subTabRow.reduced(2, 2));

    const bool showGlobalMusicFields = (currentSubTab != SubTab::Extract);
    keyScaleLabel.setVisible(showGlobalMusicFields);
    keyRootComboBox.setVisible(showGlobalMusicFields);
    keyModeComboBox.setVisible(showGlobalMusicFields && keyRootComboBox.getSelectedId() > 1);
    timeSigLabel.setVisible(showGlobalMusicFields);
    timeSigComboBox.setVisible(showGlobalMusicFields);

    area.removeFromTop(4);
    if (showGlobalMusicFields)
    {
        auto keyRow = area.removeFromTop(22);
        keyScaleLabel.setBounds(keyRow.removeFromLeft(26));
        keyRow.removeFromLeft(4);
        keyRootComboBox.setBounds(keyRow.removeFromLeft(72));
        if (keyModeComboBox.getSelectedId() > 0 && keyModeComboBox.isVisible())
        {
            keyRow.removeFromLeft(4);
            keyModeComboBox.setBounds(keyRow.removeFromLeft(68));
        }
        keyRow.removeFromLeft(4);
        timeSigLabel.setBounds(keyRow.removeFromLeft(30));
        keyRow.removeFromLeft(4);
        timeSigComboBox.setBounds(keyRow.removeFromLeft(68));

        area.removeFromTop(4);
    }
    else
    {
        area.removeFromTop(2);
    }

    if (contentViewport != nullptr)
        contentViewport->setBounds(area);

    updateContentLayout();
}

void CareyUI::addToContent(juce::Component& component)
{
    if (contentComponent != nullptr)
        contentComponent->addAndMakeVisible(component);
}

void CareyUI::setCurrentSubTabInternal(SubTab tab, bool notify)
{
    if (currentSubTab == tab && notify)
        return;

    currentSubTab = tab;
    updateSubTabButtonStyles();
    resized();

    if (notify && onSubTabChanged)
        onSubTabChanged(currentSubTab);
}

void CareyUI::updateSubTabButtonStyles()
{
    legoSubTabButton.setButtonStyle(currentSubTab == SubTab::Lego
        ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
    completeSubTabButton.setButtonStyle(currentSubTab == SubTab::Complete
        ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
    coverSubTabButton.setButtonStyle(currentSubTab == SubTab::Cover
        ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
    extractSubTabButton.setButtonStyle(currentSubTab == SubTab::Extract
        ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
}

void CareyUI::updateContentLayout()
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
    const bool showingExtract = (currentSubTab == SubTab::Extract);
    setLegoControlsVisible(showingLego);
    setCompleteControlsVisible(showingComplete);
    setCoverControlsVisible(showingCover);
    setExtractControlsVisible(showingExtract);

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
            auto modelRow = fullRow(28);
            completeModelLabel.setBounds(modelRow.removeFromLeft(110));
            const int toggleCount = completeRemoteModelSelectionEnabled ? 2 : 3;
            const int toggleGap = 8;
            const int totalGap = toggleGap * (toggleCount - 1);
            const int toggleWidth = (modelRow.getWidth() - totalGap) / toggleCount;
            completeTurboModelToggle.setBounds(modelRow.removeFromLeft(toggleWidth));
            modelRow.removeFromLeft(toggleGap);
            completeBaseModelToggle.setBounds(modelRow.removeFromLeft(toggleWidth));
            if (!completeRemoteModelSelectionEnabled)
            {
                modelRow.removeFromLeft(toggleGap);
                completeSftModelToggle.setBounds(modelRow);
            }
            y += 36;

            completeUseLoraToggle.setBounds(fullRow(22));
            y += 30;

            if (completeUseLoraToggle.getToggleState() && !completeLoraOptionValues.isEmpty())
            {
                auto completeLoraRow = fullRow(28);
                completeLoraLabel.setBounds(completeLoraRow.removeFromLeft(110));
                completeLoraComboBox.setBounds(completeLoraRow);
                y += 36;
            }

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
    else if (showingCover)
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
            if (coverModelSelectionEnabled)
            {
                auto modelRow = fullRow(28);
                coverModelLabel.setBounds(modelRow.removeFromLeft(110));
                const int toggleGap = 8;
                const int toggleWidth = (modelRow.getWidth() - toggleGap) / 2;
                coverTurboModelToggle.setBounds(modelRow.removeFromLeft(toggleWidth));
                modelRow.removeFromLeft(toggleGap);
                coverBaseModelToggle.setBounds(modelRow);
                y += 36;
            }

            coverUseLoraToggle.setBounds(fullRow(22));
            y += 30;

            if (coverUseLoraToggle.getToggleState() && !coverLoraOptionValues.isEmpty())
            {
                auto coverLoraRow = fullRow(28);
                coverLoraLabel.setBounds(coverLoraRow.removeFromLeft(110));
                coverLoraComboBox.setBounds(coverLoraRow);
                y += 36;
            }

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
    else
    {
        const bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();

        extractTrackLabel.setBounds(fullRow(18));
        y += 20;

        extractTrackComboBox.setBounds(fullRow(28));
        y += 36;

        if (isStandalone)
        {
            auto bpmRow = fullRow(28);
            extractBpmLabel.setBounds(bpmRow.removeFromLeft(60));
            extractBpmSlider.setBounds(bpmRow);
            y += 36;
        }

        extractAdvancedToggle.setBounds(fullRow(26));
        y += 32;

        if (extractAdvancedOpen)
        {
            auto stepsRow = fullRow(28);
            extractStepsLabel.setBounds(stepsRow.removeFromLeft(110));
            extractStepsSlider.setBounds(stepsRow);
            y += 36;

            auto cfgRow = fullRow(28);
            extractCfgLabel.setBounds(cfgRow.removeFromLeft(110));
            extractCfgSlider.setBounds(cfgRow);
            y += 36;
        }

        auto extractGenerateRow = fullRow(34).reduced(36, 0);
        extractGenerateButton.setBounds(extractGenerateRow);
        y += 44;

        extractInfoLabel.setBounds(fullRow(32));
        y += 40;
    }

    const int minHeight = juce::jmax(y + 8, contentViewport->getHeight());
    contentComponent->setSize(contentWidth, minHeight);
}

void CareyUI::setLegoControlsVisible(bool shouldBeVisible)
{
    captionLabel.setVisible(shouldBeVisible);
    captionEditor.setVisible(shouldBeVisible);
    captionDiceButton.setVisible(shouldBeVisible);
    legoLyricsButton.setVisible(shouldBeVisible);
    trackLabel.setVisible(shouldBeVisible);
    trackComboBox.setVisible(shouldBeVisible);
    legoAdvancedToggle.setVisible(shouldBeVisible);
    legoGenerateButton.setVisible(shouldBeVisible);
    legoInfoLabel.setVisible(shouldBeVisible);

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

void CareyUI::setCompleteControlsVisible(bool shouldBeVisible)
{
    const bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();
    const bool showAdvanced = shouldBeVisible && completeAdvancedOpen;

    completeCaptionLabel.setVisible(shouldBeVisible);
    completeCaptionEditor.setVisible(shouldBeVisible);
    completeCaptionDiceButton.setVisible(shouldBeVisible);
    completeLyricsButton.setVisible(shouldBeVisible);
    completeUseLoraToggle.setVisible(showAdvanced);
    const bool showLoraSelector = showAdvanced && completeUseLoraToggle.getToggleState() && !completeLoraOptionValues.isEmpty();
    completeLoraLabel.setVisible(showLoraSelector);
    completeLoraComboBox.setVisible(showLoraSelector);
    completeAdvancedToggle.setVisible(shouldBeVisible);
    completeGenerateButton.setVisible(shouldBeVisible);
    completeInfoLabel.setVisible(shouldBeVisible);

    completeModelLabel.setVisible(showAdvanced);
    completeTurboModelToggle.setVisible(showAdvanced);
    completeBaseModelToggle.setVisible(showAdvanced);
    completeSftModelToggle.setVisible(showAdvanced && !completeRemoteModelSelectionEnabled);
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

void CareyUI::updateCompleteModelControls(bool notify)
{
    updateCompleteModelSelectorCopy();

    const bool turboSelected = getCompleteModel().containsIgnoreCase("turbo");
    const bool heldTurboValues = getCompleteSteps() <= kFixedCompleteTurboSteps
        && getCompleteCfg() <= kFixedCompleteTurboCfg;

    if (turboSelected)
    {
        completeStepsSlider.setRange(kFixedCompleteTurboSteps, 100, 1);
        completeStepsSlider.setValue(kFixedCompleteTurboSteps, juce::dontSendNotification);
        completeStepsSlider.setEnabled(false);
        completeStepsSlider.setTooltip("turbo is fixed at 8 steps for complete generation.");
        completeStepsLabel.setTooltip("turbo is fixed at 8 steps for complete generation.");

        completeCfgSlider.setRange(kFixedCompleteTurboCfg, 10.0, 0.1);
        completeCfgSlider.setValue(kFixedCompleteTurboCfg, juce::dontSendNotification);
        completeCfgSlider.setEnabled(false);
        completeCfgSlider.setTooltip("turbo is fixed at cfg 1.0 for complete generation.");
        completeCfgLabel.setTooltip("turbo is fixed at cfg 1.0 for complete generation.");
    }
    else
    {
        completeStepsSlider.setRange(32, 100, 1);
        if (heldTurboValues)
            completeStepsSlider.setValue(kDefaultCompleteBaseSteps, juce::dontSendNotification);
        completeStepsSlider.setEnabled(true);
        completeStepsSlider.setTooltip("diffusion steps. more = refined but slower. recommended: 50");
        completeStepsLabel.setTooltip({});

        completeCfgSlider.setRange(3.0, 10.0, 0.1);
        if (heldTurboValues)
            completeCfgSlider.setValue(kDefaultCompleteBaseCfg, juce::dontSendNotification);
        completeCfgSlider.setEnabled(true);
        completeCfgSlider.setTooltip("cfg scale - how strongly the model follows the caption. higher = more literal. 3=loose, 7=balanced, 10=strict");
        completeCfgLabel.setTooltip({});
    }

    if (notify)
    {
        if (onCompleteModelChanged)
            onCompleteModelChanged(getCompleteModel());
        if (onCompleteStepsChanged)
            onCompleteStepsChanged(getCompleteSteps());
        if (onCompleteCfgChanged)
            onCompleteCfgChanged(getCompleteCfg());
    }
}

void CareyUI::setAvailableCompleteLoras(const juce::StringArray& loraNames)
{
    const juce::String previousSelection = getCompleteSelectedLora();
    const bool wasUsingLora = completeUseLoraToggle.getToggleState();

    completeLoraOptionValues.clear();
    completeLoraComboBox.clear(juce::dontSendNotification);

    juce::StringArray normalizedLoraNames;
    for (const auto& rawName : loraNames)
    {
        const juce::String name = rawName.trim();
        if (name.isNotEmpty() && !normalizedLoraNames.contains(name))
            normalizedLoraNames.add(name);
    }

    normalizedLoraNames.sortNatural();

    for (int i = 0; i < normalizedLoraNames.size(); ++i)
    {
        completeLoraOptionValues.add(normalizedLoraNames[i]);
        completeLoraComboBox.addItem(normalizedLoraNames[i], i + 1);
    }

    const bool hasLoras = !completeLoraOptionValues.isEmpty();
    completeUseLoraToggle.setEnabled(hasLoras);
    completeUseLoraToggle.setTooltip(hasLoras
        ? "enable a backend LoRA adapter and use its caption pool for the dice button"
        : "no LoRA adapters are available on the current Carey backend");
    completeLoraComboBox.setEnabled(hasLoras);
    completeLoraComboBox.setTextWhenNothingSelected(hasLoras ? "select lora..." : "no loras available");

    if (hasLoras)
    {
        const int restoredIndex = findTrackIndex(completeLoraOptionValues, previousSelection.trim());
        completeLoraComboBox.setSelectedId(restoredIndex >= 0 ? restoredIndex + 1 : 1, juce::dontSendNotification);
    }
    else
    {
        completeUseLoraToggle.setToggleState(false, juce::dontSendNotification);
        completeLoraComboBox.setSelectedId(0, juce::dontSendNotification);
    }

    completeUseLoraToggle.setToggleState(hasLoras && wasUsingLora, juce::dontSendNotification);

    updateContentLayout();
}

void CareyUI::setAvailableCoverLoras(const juce::StringArray& loraNames)
{
    const juce::String previousSelection = getCoverSelectedLora();
    const bool wasUsingLora = coverUseLoraToggle.getToggleState();

    coverLoraOptionValues.clear();
    coverLoraComboBox.clear(juce::dontSendNotification);

    juce::StringArray normalizedLoraNames;
    for (const auto& rawName : loraNames)
    {
        const juce::String name = rawName.trim();
        if (name.isNotEmpty() && !normalizedLoraNames.contains(name))
            normalizedLoraNames.add(name);
    }

    normalizedLoraNames.sortNatural();

    for (int i = 0; i < normalizedLoraNames.size(); ++i)
    {
        coverLoraOptionValues.add(normalizedLoraNames[i]);
        coverLoraComboBox.addItem(normalizedLoraNames[i], i + 1);
    }

    const bool hasLoras = !coverLoraOptionValues.isEmpty();
    coverUseLoraToggle.setEnabled(hasLoras);
    coverUseLoraToggle.setTooltip(hasLoras
        ? "enable a backend LoRA adapter and use its caption pool for the dice button"
        : "no LoRA adapters are available on the current Carey backend");
    coverLoraComboBox.setEnabled(hasLoras);
    coverLoraComboBox.setTextWhenNothingSelected(hasLoras ? "select lora..." : "no loras available");

    if (hasLoras)
    {
        const int restoredIndex = findTrackIndex(coverLoraOptionValues, previousSelection.trim());
        coverLoraComboBox.setSelectedId(restoredIndex >= 0 ? restoredIndex + 1 : 1, juce::dontSendNotification);
    }
    else
    {
        coverUseLoraToggle.setToggleState(false, juce::dontSendNotification);
        coverLoraComboBox.setSelectedId(0, juce::dontSendNotification);
    }

    coverUseLoraToggle.setToggleState(hasLoras && wasUsingLora, juce::dontSendNotification);

    updateContentLayout();
}

void CareyUI::setCoverControlsVisible(bool shouldBeVisible)
{
    coverCaptionLabel.setVisible(shouldBeVisible);
    coverCaptionEditor.setVisible(shouldBeVisible);
    coverCaptionDiceButton.setVisible(shouldBeVisible);
    coverLyricsButton.setVisible(shouldBeVisible);
    coverAdvancedToggle.setVisible(shouldBeVisible);
    coverGenerateButton.setVisible(shouldBeVisible);
    coverInfoLabel.setVisible(shouldBeVisible);

    const bool showAdvanced = shouldBeVisible && coverAdvancedOpen;
    const bool showModelSelector = showAdvanced && coverModelSelectionEnabled;
    coverModelLabel.setVisible(showModelSelector);
    coverTurboModelToggle.setVisible(showModelSelector);
    coverBaseModelToggle.setVisible(showModelSelector);
    coverUseLoraToggle.setVisible(showAdvanced);
    const bool showLoraSelector = showAdvanced && coverUseLoraToggle.getToggleState() && !coverLoraOptionValues.isEmpty();
    coverLoraLabel.setVisible(showLoraSelector);
    coverLoraComboBox.setVisible(showLoraSelector);
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

void CareyUI::updateCoverModelControls(bool notify)
{
    updateCoverModelSelectorCopy();

    const bool turboSelected = getCoverModel().containsIgnoreCase("turbo");
    const bool heldTurboValues = getCoverSteps() <= kFixedCoverSteps
        && getCoverCfg() <= kFixedCoverCfg;

    if (turboSelected)
    {
        coverStepsSlider.setRange(kFixedCoverSteps, 100, 1);
        coverStepsSlider.setValue(kFixedCoverSteps, juce::dontSendNotification);
        coverStepsSlider.setEnabled(false);
        coverStepsSlider.setTooltip("turbo is fixed at 8 steps for cover generation.");
        coverStepsLabel.setTooltip("turbo is fixed at 8 steps for cover generation.");

        coverCfgSlider.setRange(kFixedCoverCfg, 10.0, 0.1);
        coverCfgSlider.setValue(kFixedCoverCfg, juce::dontSendNotification);
        coverCfgSlider.setEnabled(false);
        coverCfgSlider.setTooltip("turbo is fixed at cfg 1.0 for cover generation.");
        coverCfgLabel.setTooltip("turbo is fixed at cfg 1.0 for cover generation.");
    }
    else
    {
        coverStepsSlider.setRange(8, 100, 1);
        if (heldTurboValues)
            coverStepsSlider.setValue(kDefaultCoverBaseSteps, juce::dontSendNotification);
        coverStepsSlider.setEnabled(true);
        coverStepsSlider.setTooltip("diffusion steps. more = refined but slower. cover mode supports quick tests down to 8.");
        coverStepsLabel.setTooltip({});

        coverCfgSlider.setRange(3.0, 10.0, 0.1);
        if (heldTurboValues)
            coverCfgSlider.setValue(kDefaultCoverBaseCfg, juce::dontSendNotification);
        coverCfgSlider.setEnabled(true);
        coverCfgSlider.setTooltip("cfg scale - how strongly the model follows the caption. higher = more literal. 3=loose, 7=balanced, 10=strict");
        coverCfgLabel.setTooltip({});
    }

    if (notify)
    {
        if (onCoverModelChanged)
            onCoverModelChanged(getCoverModel());
        if (onCoverStepsChanged)
            onCoverStepsChanged(getCoverSteps());
        if (onCoverCfgChanged)
            onCoverCfgChanged(getCoverCfg());
    }
}

void CareyUI::setExtractControlsVisible(bool shouldBeVisible)
{
    const bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();

    extractTrackLabel.setVisible(shouldBeVisible);
    extractTrackComboBox.setVisible(shouldBeVisible);
    extractBpmLabel.setVisible(shouldBeVisible && isStandalone);
    extractBpmSlider.setVisible(shouldBeVisible && isStandalone);
    extractAdvancedToggle.setVisible(shouldBeVisible);
    extractGenerateButton.setVisible(shouldBeVisible);
    extractInfoLabel.setVisible(shouldBeVisible);

    const bool showAdvanced = shouldBeVisible && extractAdvancedOpen;
    extractStepsLabel.setVisible(showAdvanced);
    extractStepsSlider.setVisible(showAdvanced);
    extractCfgLabel.setVisible(showAdvanced);
    extractCfgSlider.setVisible(showAdvanced);
}

void CareyUI::onKeyScaleSelectionChanged()
{
    const bool keySelected = (keyRootComboBox.getSelectedId() > 1);
    keyModeComboBox.setVisible(keySelected);
    resized();

    if (onKeyScaleChanged)
        onKeyScaleChanged(getKeyScale());
}

void CareyUI::initializeTrackOptions()
{
    addTrackOption(trackOptionValues, trackComboBox, "vocals", "vocals");
    addTrackOption(trackOptionValues, trackComboBox, "backing vocals", "backing_vocals");
    addTrackOption(trackOptionValues, trackComboBox, "drums", "drums");
    addTrackOption(trackOptionValues, trackComboBox, "bass", "bass");
    addTrackOption(trackOptionValues, trackComboBox, "guitar", "guitar");
    addTrackOption(trackOptionValues, trackComboBox, "piano", "piano");
    addTrackOption(trackOptionValues, trackComboBox, "strings", "strings");
    addTrackOption(trackOptionValues, trackComboBox, "synth", "synth");
    addTrackOption(trackOptionValues, trackComboBox, "keyboard", "keyboard");
    addTrackOption(trackOptionValues, trackComboBox, "percussion", "percussion");
    addTrackOption(trackOptionValues, trackComboBox, "brass", "brass");
    addTrackOption(trackOptionValues, trackComboBox, "woodwinds", "woodwinds");
}

void CareyUI::initializeExtractTrackOptions()
{
    const juce::String warningSuffix = juce::String::fromUTF8(" \xe2\x9a\xa0");

    addTrackOption(extractTrackOptionValues, extractTrackComboBox, "drums", "drums");
    addTrackOption(extractTrackOptionValues, extractTrackComboBox, "vocals", "vocals");
    addTrackOption(extractTrackOptionValues, extractTrackComboBox, "backing vocals" + warningSuffix, "backing_vocals");
    addTrackOption(extractTrackOptionValues, extractTrackComboBox, "bass" + warningSuffix, "bass");
    addTrackOption(extractTrackOptionValues, extractTrackComboBox, "guitar" + warningSuffix, "guitar");
    addTrackOption(extractTrackOptionValues, extractTrackComboBox, "piano" + warningSuffix, "piano");
    addTrackOption(extractTrackOptionValues, extractTrackComboBox, "synth" + warningSuffix, "synth");
    addTrackOption(extractTrackOptionValues, extractTrackComboBox, "keyboard" + warningSuffix, "keyboard");
    addTrackOption(extractTrackOptionValues, extractTrackComboBox, "strings" + warningSuffix, "strings");
    addTrackOption(extractTrackOptionValues, extractTrackComboBox, "percussion" + warningSuffix, "percussion");
    addTrackOption(extractTrackOptionValues, extractTrackComboBox, "brass" + warningSuffix, "brass");
    addTrackOption(extractTrackOptionValues, extractTrackComboBox, "woodwinds" + warningSuffix, "woodwinds");
}

void CareyUI::addTrackOption(juce::StringArray& optionValues,
                             CustomComboBox& comboBox,
                             const juce::String& label,
                             const juce::String& value)
{
    optionValues.add(value);
    comboBox.addItem(label, optionValues.size());
}

int CareyUI::findTrackIndex(const juce::StringArray& optionValues, const juce::String& normalizedTrackName) const
{
    for (int i = 0; i < optionValues.size(); ++i)
    {
        if (optionValues[i].equalsIgnoreCase(normalizedTrackName))
            return i;
    }
    return -1;
}

void CareyUI::drawDiceIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isHovered, bool isPressed)
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

juce::String CareyUI::pickRandomCaption(const juce::StringArray& prompts, const juce::String& fallback)
{
    if (prompts.isEmpty())
        return fallback;
    return prompts[random.nextInt(prompts.size())];
}

juce::StringArray CareyUI::getLegoPromptBankForTrack(const juce::String& trackName) const
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

juce::StringArray CareyUI::getCompletePromptBank() const
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

void CareyUI::applyRandomLegoCaption()
{
    const juce::String caption = pickRandomCaption(getLegoPromptBankForTrack(getTrackName()),
                                                   "supporting stem with musical clarity");
    captionEditor.setText(caption, juce::sendNotification);
}

void CareyUI::applyRandomCompleteCaption()
{
    const juce::String caption = pickRandomCaption(getCompletePromptBank(),
                                                   "A dark, aggressive phonk and trap track built on chopped soul samples and crushing low-end, with menacing synth movement and relentless rhythmic drive.");
    completeCaptionEditor.setText(caption, juce::sendNotification);
}

juce::StringArray CareyUI::getCoverPromptBank() const
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

void CareyUI::applyRandomCoverCaption()
{
    const juce::String caption = pickRandomCaption(getCoverPromptBank(),
                                                   "orchestral symphonic arrangement with lush strings and cinematic dynamics");
    coverCaptionEditor.setText(caption, juce::sendNotification);
}

void CareyUI::setLyricsTextInternal(const juce::String& text, bool notify)
{
    lyricsText = text;
    updateLyricsButtonLabels();
    if (notify && onLyricsChanged)
        onLyricsChanged(lyricsText);
}

void CareyUI::updateLyricsButtonLabels()
{
    const bool hasLyrics = !lyricsText.trim().isEmpty();
    legoLyricsButton.setButtonText(hasLyrics ? "lyrics*" : "lyrics");
    completeLyricsButton.setButtonText(hasLyrics ? "lyrics*" : "lyrics");
    coverLyricsButton.setButtonText(hasLyrics ? "lyrics*" : "lyrics");
}

void CareyUI::openLyricsEditor()
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
