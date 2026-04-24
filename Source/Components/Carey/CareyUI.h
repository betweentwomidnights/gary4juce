#pragma once

#include <JuceHeader.h>

#include "../Base/CustomButton.h"
#include "../Base/CustomComboBox.h"
#include "../Base/CustomSlider.h"
#include "../Base/CustomTextEditor.h"
#include "../../Utils/CustomLookAndFeel.h"
#include "../../Utils/Theme.h"

#include <functional>

class CareyUI : public juce::Component
{
public:
    static constexpr int kFixedCoverSteps = 8;
    static constexpr double kFixedCoverCfg = 1.0;
    static constexpr int kDefaultCoverBaseSteps = 50;
    static constexpr double kDefaultCoverBaseCfg = 7.0;
    static constexpr int kFixedCompleteTurboSteps = 8;
    static constexpr double kFixedCompleteTurboCfg = 1.0;
    static constexpr int kDefaultCompleteBaseSteps = 50;
    static constexpr double kDefaultCompleteBaseCfg = 7.0;
    static constexpr int kDefaultExtractSteps = 80;
    static constexpr double kDefaultExtractCfg = 10.0;

    enum class SubTab
    {
        Lego = 0,
        Complete,
        Cover,
        Extract
    };

    CareyUI();
    ~CareyUI() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void setVisibleForTab(bool visible) { setVisible(visible); }
    juce::Rectangle<int> getTitleBounds() const { return titleBounds; }

    void setCurrentSubTab(SubTab tab) { setCurrentSubTabInternal(tab, false); }
    SubTab getCurrentSubTab() const { return currentSubTab; }
    void applyRandomCompleteCaption();
    void applyRandomCoverCaption();

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

    juce::String getExtractTrackName() const
    {
        const int selectedId = extractTrackComboBox.getSelectedId();
        const int index = selectedId - 1;
        if (index >= 0 && index < extractTrackOptionValues.size())
            return extractTrackOptionValues[index];
        return "drums";
    }

    int getExtractBpm() const
    {
        return juce::roundToInt(extractBpmSlider.getValue());
    }

    int getExtractSteps() const
    {
        return juce::roundToInt(extractStepsSlider.getValue());
    }

    double getExtractCfg() const
    {
        return extractCfgSlider.getValue();
    }

    juce::String getCompleteCaptionText() const
    {
        return completeCaptionEditor.getText().trim();
    }

    bool getCompleteUseLora() const
    {
        return completeUseLoraToggle.getToggleState();
    }

    juce::String getCompleteSelectedLora() const
    {
        const int selectedId = completeLoraComboBox.getSelectedId();
        const int index = selectedId - 1;
        if (index >= 0 && index < completeLoraOptionValues.size())
            return completeLoraOptionValues[index];
        return {};
    }

    juce::String getLyricsText() const
    {
        return lyricsText;
    }

    juce::String getCompleteLyricsText() const
    {
        return lyricsText;
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

    juce::String getCompleteModel() const;

    void setCaptionText(const juce::String& text)
    {
        captionEditor.setText(text, juce::dontSendNotification);
    }

    void setTrackName(const juce::String& trackName)
    {
        const int trackIndex = findTrackIndex(trackOptionValues, trackName.trim().toLowerCase());
        trackComboBox.setSelectedId(trackIndex >= 0 ? trackIndex + 1 : 1, juce::dontSendNotification);
    }

    void setSteps(int value)
    {
        stepsSlider.setValue(juce::jlimit(32, 100, value), juce::dontSendNotification);
    }

    double getLegoCfg() const { return legoCfgSlider.getValue(); }
    void setLegoCfg(double val) { legoCfgSlider.setValue(juce::jlimit(3.0, 10.0, val), juce::dontSendNotification); }

    void setExtractTrackName(const juce::String& trackName)
    {
        const int trackIndex = findTrackIndex(extractTrackOptionValues, trackName.trim().toLowerCase());
        extractTrackComboBox.setSelectedId(trackIndex >= 0 ? trackIndex + 1 : 1, juce::dontSendNotification);
    }

    void setExtractBpm(int bpm)
    {
        extractBpmSlider.setValue(juce::jlimit(20, 300, bpm), juce::dontSendNotification);
    }

    void setExtractSteps(int value)
    {
        extractStepsSlider.setValue(juce::jlimit(32, 100, value), juce::dontSendNotification);
    }

    void setExtractCfg(double value)
    {
        extractCfgSlider.setValue(juce::jlimit(3.0, 10.0, value), juce::dontSendNotification);
    }

    void setCompleteCaptionText(const juce::String& text)
    {
        completeCaptionEditor.setText(text, juce::dontSendNotification);
    }

    void setCompleteUseLora(bool enabled)
    {
        const bool shouldEnable = enabled && !completeLoraOptionValues.isEmpty();
        if (completeUseLoraToggle.getToggleState() != shouldEnable)
            completeUseLoraToggle.setToggleState(shouldEnable, juce::dontSendNotification);

        updateContentLayout();
    }

    void setAvailableCompleteLoras(const juce::StringArray& loraNames);

    void setCompleteSelectedLora(const juce::String& loraName)
    {
        const int loraIndex = findTrackIndex(completeLoraOptionValues, loraName.trim());
        completeLoraComboBox.setSelectedId(loraIndex >= 0 ? loraIndex + 1 : 0, juce::dontSendNotification);
        updateContentLayout();
    }

    void setLyricsText(const juce::String& text)
    {
        setLyricsTextInternal(text, false);
    }

    juce::String getLyricsLanguage() const { return lyricsLanguage; }
    void setLyricsLanguage(const juce::String& lang) { lyricsLanguage = lang.isNotEmpty() ? lang : "en"; }

    void setCompleteLyricsText(const juce::String& text)
    {
        setLyricsTextInternal(text, false);
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
        completeStepsSlider.setValue(juce::jlimit(8, 100, steps), juce::dontSendNotification);
    }

    double getCompleteCfg() const { return completeCfgSlider.getValue(); }
    void setCompleteCfg(double val) { completeCfgSlider.setValue(juce::jlimit(1.0, 10.0, val), juce::dontSendNotification); }

    void setCompleteModel(const juce::String& model);

    void setCompleteRemoteModelSelectionEnabled(bool enabled)
    {
        const juce::String currentModel = getCompleteModel();
        if (completeRemoteModelSelectionEnabled == enabled)
            return;

        completeRemoteModelSelectionEnabled = enabled;
        setCompleteModel(currentModel);
        updateContentLayout();
    }

    void setExtractRemoteGenerationEnabled(bool enabled)
    {
        extractRemoteGenerationEnabled = enabled;

        if (extractRemoteGenerationEnabled)
        {
            extractGenerateButton.setTooltip("we're still figuring out how to use this properly. if you have a stem separator it may work better");
            extractInfoLabel.setText("drums and vocals work best. other stem types are still experimental.", juce::dontSendNotification);
        }
        else
        {
            extractGenerateButton.setTooltip("extract isn't available yet in the gary4local api");
            extractInfoLabel.setText("extract isn't available yet in the gary4local api. switch off local backend to use it.", juce::dontSendNotification);
        }

        if (currentSubTab == SubTab::Extract)
            updateContentLayout();
    }

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

    juce::String getCoverCaptionText() const { return coverCaptionEditor.getText().trim(); }
    bool getCoverUseLora() const { return coverUseLoraToggle.getToggleState(); }
    juce::String getCoverSelectedLora() const
    {
        const int selectedId = coverLoraComboBox.getSelectedId();
        const int index = selectedId - 1;
        if (index >= 0 && index < coverLoraOptionValues.size())
            return coverLoraOptionValues[index];
        return {};
    }
    juce::String getCoverLyricsText() const { return lyricsText; }
    double getCoverNoiseStrength() const { return coverNoiseStrengthSlider.getValue(); }
    double getCoverAudioStrength() const { return coverAudioStrengthSlider.getValue(); }
    int getCoverSteps() const { return juce::roundToInt(coverStepsSlider.getValue()); }
    double getCoverCfg() const { return coverCfgSlider.getValue(); }
    juce::String getCoverModel() const;
    bool getCoverUseSrcAsRef() const { return coverUseSrcAsRefToggle.getToggleState(); }
    bool getCoverLoopAssistEnabled() const { return coverLoopAssistToggle.getToggleState(); }
    bool getCoverTrimToInputEnabled() const { return coverTrimToInputToggle.getToggleState(); }

    void setCoverCaptionText(const juce::String& text) { coverCaptionEditor.setText(text, juce::dontSendNotification); }
    void setCoverUseLora(bool enabled)
    {
        const bool shouldEnable = enabled && !coverLoraOptionValues.isEmpty();
        if (coverUseLoraToggle.getToggleState() != shouldEnable)
            coverUseLoraToggle.setToggleState(shouldEnable, juce::dontSendNotification);

        updateContentLayout();
    }
    void setAvailableCoverLoras(const juce::StringArray& loraNames);
    void setCoverModel(const juce::String& model);
    void setCoverModelSelectionEnabled(bool enabled);
    void setCoverRemoteModelSelectionEnabled(bool enabled);
    void setCoverSelectedLora(const juce::String& loraName)
    {
        const int loraIndex = findTrackIndex(coverLoraOptionValues, loraName.trim());
        coverLoraComboBox.setSelectedId(loraIndex >= 0 ? loraIndex + 1 : 0, juce::dontSendNotification);
        updateContentLayout();
    }
    void setCoverLyricsText(const juce::String& text) { setLyricsTextInternal(text, false); }
    void setCoverNoiseStrength(double val) { coverNoiseStrengthSlider.setValue(juce::jlimit(0.0, 1.0, val), juce::dontSendNotification); }
    void setCoverAudioStrength(double val) { coverAudioStrengthSlider.setValue(juce::jlimit(0.0, 1.0, val), juce::dontSendNotification); }
    void setCoverSteps(int steps);
    void setCoverCfg(double value);
    void setCoverUseSrcAsRef(bool enabled) { coverUseSrcAsRefToggle.setToggleState(enabled, juce::dontSendNotification); }
    void setCoverLoopAssistEnabled(bool enabled) { coverLoopAssistToggle.setToggleState(enabled, juce::dontSendNotification); }
    void setCoverTrimToInputEnabled(bool enabled) { coverTrimToInputToggle.setToggleState(enabled, juce::dontSendNotification); }

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

    void setExtractAdvancedOpen(bool open)
    {
        if (extractAdvancedOpen != open)
        {
            extractAdvancedOpen = open;
            extractAdvancedToggle.setButtonText(open
                ? juce::String::fromUTF8("advanced \xe2\x96\xbc")
                : juce::String::fromUTF8("advanced \xe2\x96\xb6"));
            updateContentLayout();
        }
    }

    juce::String getKeyScale() const
    {
        if (keyRootComboBox.getSelectedId() <= 1)
            return {};
        return keyRootComboBox.getText() + " " + keyModeComboBox.getText();
    }

    void setKeyScale(const juce::String& keyScale)
    {
        if (keyScale.isEmpty())
        {
            keyRootComboBox.setSelectedId(1, juce::dontSendNotification);
            keyModeComboBox.setVisible(false);
            resized();
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
        keyModeComboBox.setVisible(true);
        resized();
    }

    juce::String getTimeSig() const
    {
        const int id = timeSigComboBox.getSelectedId();
        if (id == 2) return "2";
        if (id == 3) return "3";
        if (id == 4) return "4";
        if (id == 5) return "6";
        return {};
    }

    void setTimeSig(const juce::String& timeSig)
    {
        if (timeSig == "2") timeSigComboBox.setSelectedId(2, juce::dontSendNotification);
        else if (timeSig == "3") timeSigComboBox.setSelectedId(3, juce::dontSendNotification);
        else if (timeSig == "4") timeSigComboBox.setSelectedId(4, juce::dontSendNotification);
        else if (timeSig == "6") timeSigComboBox.setSelectedId(5, juce::dontSendNotification);
        else timeSigComboBox.setSelectedId(1, juce::dontSendNotification);
    }

    void setGenerateButtonEnabled(bool enabled, bool isGenerating)
    {
        legoGenerateButton.setEnabled(enabled && !isGenerating);
        legoGenerateButton.setButtonText(isGenerating ? "generating..." : "send to carey");

        const bool completeAvailable = (bool) onCompleteGenerate;
        completeGenerateButton.setEnabled(completeAvailable && enabled && !isGenerating);
        completeGenerateButton.setButtonText(isGenerating ? "generating..." : "continue with carey");

        const bool coverAvailable = (bool) onCoverGenerate;
        coverGenerateButton.setEnabled(coverAvailable && enabled && !isGenerating);
        coverGenerateButton.setButtonText(isGenerating ? "generating..." : "remix with carey");

        const bool extractAvailable = (bool) onExtractGenerate && extractRemoteGenerationEnabled;
        extractGenerateButton.setEnabled(extractAvailable && enabled && !isGenerating);
        extractGenerateButton.setButtonText(isGenerating
            ? "generating..."
            : juce::String::fromUTF8("extract with carey \xe2\x9a\xa0"));
    }

    std::function<void(SubTab)> onSubTabChanged;
    std::function<void(const juce::String&)> onCaptionChanged;
    std::function<void(const juce::String&)> onTrackChanged;
    std::function<void(int)> onStepsChanged;
    std::function<void(double)> onLegoCfgChanged;
    std::function<void(bool)> onLoopAssistChanged;
    std::function<void(bool)> onTrimToInputChanged;
    std::function<void()> onGenerate;
    std::function<void(const juce::String&)> onLyricsChanged;
    std::function<void(const juce::String&)> onLyricsLanguageChanged;

    std::function<void(const juce::String&)> onExtractTrackChanged;
    std::function<void(int)> onExtractBpmChanged;
    std::function<void(int)> onExtractStepsChanged;
    std::function<void(double)> onExtractCfgChanged;
    std::function<void()> onExtractGenerate;

    std::function<void(const juce::String&)> onCompleteCaptionChanged;
    std::function<void()> onCompleteCaptionDiceRequested;
    std::function<void(bool)> onCompleteUseLoraChanged;
    std::function<void(const juce::String&)> onCompleteLoraChanged;
    std::function<void(const juce::String&)> onCompleteModelChanged;
    std::function<void(int)> onCompleteBpmChanged;
    std::function<void(int)> onCompleteStepsChanged;
    std::function<void(double)> onCompleteCfgChanged;
    std::function<void(int)> onCompleteDurationChanged;
    std::function<void()> onCompleteGenerate;
    std::function<void(bool)> onCompleteUseSrcAsRefChanged;

    std::function<void(const juce::String&)> onCoverCaptionChanged;
    std::function<void()> onCoverCaptionDiceRequested;
    std::function<void(bool)> onCoverUseLoraChanged;
    std::function<void(const juce::String&)> onCoverLoraChanged;
    std::function<void(const juce::String&)> onCoverModelChanged;
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
    void addToContent(juce::Component& component);
    void setCurrentSubTabInternal(SubTab tab, bool notify);
    void updateSubTabButtonStyles();
    void updateContentLayout();
    void setLegoControlsVisible(bool shouldBeVisible);
    void setCompleteControlsVisible(bool shouldBeVisible);
    void updateCompleteModelControls(bool notify);
    void updateCompleteModelSelectorCopy();
    void setCoverControlsVisible(bool shouldBeVisible);
    void updateCoverModelControls(bool notify);
    void updateCoverModelSelectorCopy();
    void setExtractControlsVisible(bool shouldBeVisible);
    void onKeyScaleSelectionChanged();
    void initializeTrackOptions();
    void initializeExtractTrackOptions();
    void addTrackOption(juce::StringArray& optionValues,
                        CustomComboBox& comboBox,
                        const juce::String& label,
                        const juce::String& value);
    int findTrackIndex(const juce::StringArray& optionValues, const juce::String& normalizedTrackName) const;
    void drawDiceIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isHovered, bool isPressed);
    juce::String pickRandomCaption(const juce::StringArray& prompts, const juce::String& fallback);
    juce::StringArray getLegoPromptBankForTrack(const juce::String& trackName) const;
    juce::StringArray getCompletePromptBank() const;
    void applyRandomLegoCaption();
    juce::StringArray getCoverPromptBank() const;
    void setLyricsTextInternal(const juce::String& text, bool notify);
    void updateLyricsButtonLabels();
    void openLyricsEditor();

    SubTab currentSubTab = SubTab::Lego;
    juce::Random random;

    juce::Label careyLabel;
    CustomButton legoSubTabButton;
    CustomButton completeSubTabButton;
    CustomButton coverSubTabButton;
    CustomButton extractSubTabButton;
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
    juce::String lyricsText;
    juce::String lyricsLanguage = "en";

    juce::Label completeCaptionLabel;
    CustomTextEditor completeCaptionEditor;
    CustomButton completeCaptionDiceButton;
    CustomButton completeLyricsButton;
    juce::ToggleButton completeUseLoraToggle;
    juce::Label completeLoraLabel;
    CustomComboBox completeLoraComboBox;
    juce::StringArray completeLoraOptionValues;
    juce::Label completeModelLabel;
    juce::ToggleButton completeTurboModelToggle;
    juce::ToggleButton completeBaseModelToggle;
    juce::ToggleButton completeSftModelToggle;
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

    juce::Label keyScaleLabel;
    juce::Label timeSigLabel;
    CustomComboBox timeSigComboBox;
    CustomComboBox keyRootComboBox;
    CustomComboBox keyModeComboBox;

    juce::Label coverCaptionLabel;
    CustomTextEditor coverCaptionEditor;
    CustomButton coverCaptionDiceButton;
    CustomButton coverLyricsButton;
    juce::ToggleButton coverUseLoraToggle;
    juce::Label coverLoraLabel;
    CustomComboBox coverLoraComboBox;
    juce::StringArray coverLoraOptionValues;
    juce::Label coverModelLabel;
    juce::ToggleButton coverTurboModelToggle;
    juce::ToggleButton coverBaseModelToggle;
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

    juce::Label extractTrackLabel;
    CustomComboBox extractTrackComboBox;
    juce::StringArray extractTrackOptionValues;
    juce::Label extractBpmLabel;
    CustomSlider extractBpmSlider;
    juce::Label extractStepsLabel;
    CustomSlider extractStepsSlider;
    juce::Label extractCfgLabel;
    CustomSlider extractCfgSlider;
    CustomButton extractGenerateButton;
    juce::Label extractInfoLabel;

    CustomButton legoAdvancedToggle;
    CustomButton completeAdvancedToggle;
    CustomButton coverAdvancedToggle;
    CustomButton extractAdvancedToggle;
    bool legoAdvancedOpen = false;
    bool completeAdvancedOpen = false;
    bool coverAdvancedOpen = false;
    bool extractAdvancedOpen = false;
    bool completeRemoteModelSelectionEnabled = false;
    bool coverModelSelectionEnabled = true;
    bool coverRemoteModelSelectionEnabled = true;
    bool extractRemoteGenerationEnabled = true;

    juce::Rectangle<int> titleBounds;
};
