#include "JerryUI.h"
#include "../../Utils/Theme.h"
#include "PromptHelpers.h"

namespace
{
    constexpr int kOuterMargin = 12;
    constexpr int kTitleHeight = 28;
    constexpr int kPromptLabelHeight = 12;      // Reduced from 18
    constexpr int kPromptEditorHeight = 24;     // Reduced from 26
    constexpr int kRowHeight = 20;              // Reduced from 26
    constexpr int kSmartLoopHeight = 22;        // Reduced from 28
    constexpr int kBpmHeight = 14;              // Reduced from 16
    constexpr int kButtonHeight = 32;           // Keep same
    constexpr int kLabelWidth = 70;             // Reduced from 92 (shorter labels)
    constexpr int kInterRowGap = 2;             // Reduced from 3
    constexpr int kLoopButtonGap = 4;
}

JerryUI::JerryUI()
{
    jerryLabel.setText("jerry (stable audio open small)", juce::dontSendNotification);
    jerryLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    jerryLabel.setColour(juce::Label::textColourId, Theme::Colors::TextPrimary);
    jerryLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(jerryLabel);

    // Model selector
    jerryModelLabel.setText("model", juce::dontSendNotification);
    jerryModelLabel.setFont(juce::FontOptions(12.0f));
    jerryModelLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    jerryModelLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(jerryModelLabel);

    jerryModelComboBox.setTextWhenNothingSelected("loading models...");
    jerryModelComboBox.onChange = [this]()
        {
            const int selectedId = jerryModelComboBox.getSelectedId();
            const int newIndex = selectedId > 0 ? selectedId - 1 : 0;

            if (selectedModelIndex != newIndex)
            {
                selectedModelIndex = newIndex;

                bool isFinetune = false;
                if (newIndex >= 0 && newIndex < modelIsFinetune.size())
                    isFinetune = modelIsFinetune[newIndex];

                // Update UI for model type
                updateSliderRangesForModel(isFinetune);
                updateSamplerVisibility();

                if (onModelChanged)
                    onModelChanged(newIndex, isFinetune);
            }
        };
    addAndMakeVisible(jerryModelComboBox);

    // Sampler type selector (hidden by default, shown for finetunes)
    jerrySamplerLabel.setText("sampler", juce::dontSendNotification);
    jerrySamplerLabel.setFont(juce::FontOptions(12.0f));
    jerrySamplerLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    jerrySamplerLabel.setJustificationType(juce::Justification::centredLeft);
    jerrySamplerLabel.setVisible(false);
    addAndMakeVisible(jerrySamplerLabel);

    samplerEulerButton.setButtonText("euler");
    samplerEulerButton.getProperties().set("samplerType", "euler");
    samplerEulerButton.setRadioGroupId(2001);  // Different from Terry's 1001
    samplerEulerButton.setToggleState(false, juce::dontSendNotification);
    samplerEulerButton.onClick = [this]()
        {
            currentSamplerType = getSamplerTypeForButton(samplerEulerButton);
            if (onSamplerTypeChanged)
                onSamplerTypeChanged(currentSamplerType);
        };
    samplerEulerButton.setVisible(false);
    addAndMakeVisible(samplerEulerButton);

    samplerDpmppButton.setButtonText("dpmpp");
    samplerDpmppButton.getProperties().set("samplerType", "dpmpp");
    samplerDpmppButton.setRadioGroupId(2001);
    samplerDpmppButton.setToggleState(true, juce::dontSendNotification);  // Default for finetunes
    samplerDpmppButton.onClick = [this]()
        {
            currentSamplerType = getSamplerTypeForButton(samplerDpmppButton);
            if (onSamplerTypeChanged)
                onSamplerTypeChanged(currentSamplerType);
        };
    samplerDpmppButton.setVisible(false);
    addAndMakeVisible(samplerDpmppButton);

    samplerThirdButton.setButtonText("k-heun");
    samplerThirdButton.getProperties().set("samplerType", "k-heun");
    samplerThirdButton.setRadioGroupId(2001);
    samplerThirdButton.setToggleState(false, juce::dontSendNotification);
    samplerThirdButton.onClick = [this]()
        {
            currentSamplerType = getSamplerTypeForButton(samplerThirdButton);
            if (onSamplerTypeChanged)
                onSamplerTypeChanged(currentSamplerType);
        };
    samplerThirdButton.setVisible(false);
    addAndMakeVisible(samplerThirdButton);

    // Custom finetune section (localhost only)
    toggleCustomSectionButton.setButtonText("+");
    toggleCustomSectionButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    toggleCustomSectionButton.setTooltip("add custom finetune (localhost only)");
    toggleCustomSectionButton.onClick = [this]() { toggleCustomFinetuneSection(); };
    toggleCustomSectionButton.setVisible(false);  // Hidden by default
    addAndMakeVisible(toggleCustomSectionButton);

    customFinetuneLabel.setText("add custom finetune", juce::dontSendNotification);
    customFinetuneLabel.setFont(juce::FontOptions(11.0f));
    customFinetuneLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    customFinetuneLabel.setJustificationType(juce::Justification::centredLeft);
    customFinetuneLabel.setVisible(false);
    addAndMakeVisible(customFinetuneLabel);

    repoTextEditor.setTextToShowWhenEmpty("thepatch/jerry_grunge", juce::Colours::darkgrey);
    repoTextEditor.setMultiLine(false);
    repoTextEditor.setReturnKeyStartsNewLine(false);
    repoTextEditor.setScrollbarsShown(false);
    repoTextEditor.setBorder(juce::BorderSize<int>(2));
    repoTextEditor.setVisible(false);
    addAndMakeVisible(repoTextEditor);

    fetchCheckpointsButton.setButtonText("fetch");
    fetchCheckpointsButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    fetchCheckpointsButton.onClick = [this]() {
        auto repo = repoTextEditor.getText().trim();
        if (repo.isEmpty()) {
            repo = "thepatch/jerry_grunge";  // Default
        }
        if (onFetchCheckpoints)
            onFetchCheckpoints(repo);
    };
    fetchCheckpointsButton.setVisible(false);
    addAndMakeVisible(fetchCheckpointsButton);

    checkpointComboBox.setTextWhenNothingSelected("fetch checkpoints first...");
    checkpointComboBox.setVisible(false);
    addAndMakeVisible(checkpointComboBox);

    addModelButton.setButtonText("add to models");
    addModelButton.setButtonStyle(CustomButton::ButtonStyle::Jerry);
    addModelButton.onClick = [this]() {
        auto repo = repoTextEditor.getText().trim();
        if (repo.isEmpty()) repo = "thepatch/jerry_grunge";

        auto selectedId = checkpointComboBox.getSelectedId();
        if (selectedId <= 0) return;

        auto checkpoint = checkpointComboBox.getItemText(selectedId - 1);
        if (onAddCustomModel)
            onAddCustomModel(repo, checkpoint);

        // Collapse section after adding
        toggleCustomFinetuneSection();
    };
    addModelButton.setEnabled(false);
    addModelButton.setVisible(false);
    addAndMakeVisible(addModelButton);

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

    // Dice button for prompt generation
    promptDiceButton = std::make_unique<CustomButton>();
    promptDiceButton->setButtonText("");
    promptDiceButton->setButtonStyle(CustomButton::ButtonStyle::Jerry);
    promptDiceButton->setTooltip("Generate random prompt");

    promptDiceButton->onClick = [this]()
        {
            juce::String prompt = generateConditionalPrompt();
            jerryPromptEditor.setText(prompt, juce::sendNotification);
            setPromptText(prompt);
        };

    promptDiceButton->onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
        {
            bool isHovered = promptDiceButton->isMouseOver();
            bool isPressed = promptDiceButton->isDown();
            drawDiceIcon(g, bounds.toFloat().reduced(2), isHovered, isPressed);
        };

    addAndMakeVisible(*promptDiceButton);

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

    // Manual BPM slider (hidden by default, shown in standalone)
    jerryBpmSlider.setRange(40.0, 200.0, 1.0);  // 40-200 BPM, 1 BPM increments
    jerryBpmSlider.setValue(120.0);              // Default 120 BPM
    jerryBpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    jerryBpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    jerryBpmSlider.setVisible(false);  // Hidden by default (plugin mode)
    jerryBpmSlider.onValueChange = [this]()
        {
            int newBpm = (int)jerryBpmSlider.getValue();
            if (onManualBpmChanged)
                onManualBpmChanged(newBpm);
        };
    addAndMakeVisible(jerryBpmSlider);

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

    // Title at the very top
    titleBounds = area.removeFromTop(kTitleHeight);
    jerryLabel.setBounds(titleBounds);
    area.removeFromTop(kInterRowGap);

    // Model selector row with "+" button
    auto modelRow = area.removeFromTop(kRowHeight);
    auto modelLabelBounds = modelRow.removeFromLeft(kLabelWidth);
    jerryModelLabel.setBounds(modelLabelBounds);

    // Reserve space for "+" button if on localhost
    if (isUsingLocalhost) {
        auto plusButtonBounds = modelRow.removeFromRight(25);
        toggleCustomSectionButton.setBounds(plusButtonBounds);
        modelRow.removeFromRight(3);  // Small gap
    }

    jerryModelComboBox.setBounds(modelRow);
    area.removeFromTop(kInterRowGap);

    // Custom finetune section (collapsible)
    if (showingCustomFinetuneSection && isUsingLocalhost) {
        // Label
        auto customLabelBounds = area.removeFromTop(kPromptLabelHeight);
        customFinetuneLabel.setBounds(customLabelBounds);
        area.removeFromTop(kInterRowGap);

        // Repo text + fetch button
        auto repoRow = area.removeFromTop(kPromptEditorHeight);
        auto fetchButtonBounds = repoRow.removeFromRight(60);
        repoRow.removeFromRight(3);  // Gap
        repoTextEditor.setBounds(repoRow);
        fetchCheckpointsButton.setBounds(fetchButtonBounds);
        area.removeFromTop(kInterRowGap);

        // Checkpoint dropdown
        auto checkpointRow = area.removeFromTop(kRowHeight);
        checkpointComboBox.setBounds(checkpointRow);
        area.removeFromTop(kInterRowGap);

        // Add button
        auto addButtonRow = area.removeFromTop(kButtonHeight);
        auto addButtonBounds = addButtonRow.withWidth(150).withCentre(addButtonRow.getCentre());
        addModelButton.setBounds(addButtonBounds);
        area.removeFromTop(kInterRowGap);
    }

    // Sampler selector row (only visible for finetunes)
    if (showingSamplerSelector)
    {
        auto samplerRow = area.removeFromTop(kRowHeight);
        auto samplerLabelBounds = samplerRow.removeFromLeft(kLabelWidth);
        jerrySamplerLabel.setBounds(samplerLabelBounds);

        juce::Array<juce::ToggleButton*> visibleSamplerButtons;
        if (samplerEulerButton.isVisible())
            visibleSamplerButtons.add(&samplerEulerButton);
        if (samplerDpmppButton.isVisible())
            visibleSamplerButtons.add(&samplerDpmppButton);
        if (samplerThirdButton.isVisible())
            visibleSamplerButtons.add(&samplerThirdButton);

        const int buttonGap = 4;
        const int visibleCount = visibleSamplerButtons.size();
        const int totalGap = juce::jmax(0, visibleCount - 1) * buttonGap;
        const int availableWidth = juce::jmax(0, samplerRow.getWidth() - totalGap);
        const int buttonWidth = visibleCount > 0
            ? juce::jmax(1, availableWidth / visibleCount)
            : 0;

        for (int i = 0; i < visibleCount; ++i)
        {
            auto* button = visibleSamplerButtons.getUnchecked(i);
            button->setBounds(samplerRow.removeFromLeft(buttonWidth));
            if (i < visibleCount - 1)
                samplerRow.removeFromLeft(buttonGap);
        }

        area.removeFromTop(kInterRowGap);
    }

    // Prompt label
    auto promptLabelBounds = area.removeFromTop(kPromptLabelHeight);
    jerryPromptLabel.setBounds(promptLabelBounds);
    area.removeFromTop(kInterRowGap);

    // Prompt editor with dice button
    auto promptRow = area.removeFromTop(kPromptEditorHeight);
    const int diceW = 22;
    auto diceBounds = promptRow.removeFromRight(diceW);
    promptRow.removeFromRight(2);
    jerryPromptEditor.setBounds(promptRow);

    if (promptDiceButton)
    {
        auto diceSquare = diceBounds.withHeight(diceW).withY(diceBounds.getY() + (kPromptEditorHeight - diceW) / 2);
        promptDiceButton->setBounds(diceSquare);
    }

    area.removeFromTop(kInterRowGap);

    // CFG row
    auto cfgRow = area.removeFromTop(kRowHeight);
    auto cfgLabelBounds = cfgRow.removeFromLeft(kLabelWidth);
    jerryCfgLabel.setBounds(cfgLabelBounds);
    jerryCfgSlider.setBounds(cfgRow);
    area.removeFromTop(kInterRowGap);

    // Steps row
    auto stepsRow = area.removeFromTop(kRowHeight);
    auto stepsLabelBounds = stepsRow.removeFromLeft(kLabelWidth);
    jerryStepsLabel.setBounds(stepsLabelBounds);
    jerryStepsSlider.setBounds(stepsRow);
    area.removeFromTop(kInterRowGap);

    // Smart loop row (existing logic)
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

    // BPM display (conditional: label in plugin mode, label+slider in standalone mode)
    auto bpmBounds = area.removeFromTop(isStandaloneMode ? kRowHeight : kBpmHeight);
    if (isStandaloneMode)
    {
        // Standalone mode: "bpm:" label + slider
        const int bpmLabelWidth = 35;  // Just enough for "bpm:"
        auto labelPart = bpmBounds.removeFromLeft(bpmLabelWidth);

        // Label shows just "bpm:" aligned right
        jerryBpmLabel.setText("bpm:", juce::dontSendNotification);
        jerryBpmLabel.setJustificationType(juce::Justification::centredRight);
        jerryBpmLabel.setBounds(labelPart);

        // Slider takes the remaining space
        jerryBpmSlider.setBounds(bpmBounds);
    }
    else
    {
        // Plugin mode: full-width label showing DAW BPM
        jerryBpmLabel.setText("bpm: " + juce::String(bpmValue) + " (from daw)", juce::dontSendNotification);
        jerryBpmLabel.setJustificationType(juce::Justification::centred);
        jerryBpmLabel.setBounds(bpmBounds);
    }
    area.removeFromTop(kInterRowGap);

    // Generate button
    auto generateRow = area.removeFromTop(kButtonHeight);
    auto buttonWidth = juce::jmin(240, generateRow.getWidth());
    auto buttonBounds = generateRow.withWidth(buttonWidth).withCentre(generateRow.getCentre());
    generateWithJerryButton.setBounds(buttonBounds);
}

void JerryUI::setVisibleForTab(bool visible)
{
    setVisible(visible);
    setInterceptsMouseClicks(visible, visible);

    if (promptDiceButton)
        promptDiceButton->setVisible(visible);
}

// --- Helpers ---

static juce::String stripBpm(juce::String s)
{
    // Remove the substring "bpm" regardless of case
    s = s.replace("bpm", "", true /* ignoreCase */);

    // Strip a bit of punctuation; keep hyphens like "reverb-heavy" if you like them
    s = s.removeCharacters(".,;:");

    // Tokenize on space/tab
    juce::StringArray toks;
    toks.addTokens(s, " \t", "");
    toks.removeEmptyStrings(true);

    // Keep only tokens with no digits
    juce::StringArray out;
    for (auto t : toks)
    {
        bool hasDigit = false;
        for (auto ch : t)
        {
            if (juce::CharacterFunctions::isDigit(ch)) { hasDigit = true; break; }
        }
        if (!hasDigit) out.add(t);
    }

    return out.joinIntoString(" ");
}

static juce::String shrinkTokensRandom(juce::String in, int minKeep = 3, int maxKeep = 5)
{
    juce::StringArray toks;
    toks.addTokens(in, " \t", "");
    toks.removeEmptyStrings(true);
    if (toks.isEmpty()) return {};

    auto& rnd = juce::Random::getSystemRandom();

    // Choose K in [minKeep, maxKeep]
    const int K = juce::jlimit(minKeep, maxKeep, rnd.nextInt(juce::Range<int>(minKeep, maxKeep + 1)));

    // Sample K indices without replacement
    juce::Array<int> idx;
    idx.ensureStorageAllocated(toks.size());
    for (int i = 0; i < toks.size(); ++i) idx.add(i);
    for (int i = idx.size() - 1; i > 0; --i) idx.swap(i, rnd.nextInt(i + 1));
    idx.removeRange(K, idx.size() - K);   // keep first K after shuffle
    idx.sort();                           // restore original order

    juce::StringArray kept;
    kept.ensureStorageAllocated(K);
    for (int i = 0; i < idx.size(); ++i) kept.add(toks[idx[i]]);

    return kept.joinIntoString(" ");
}


// Pull top_unigrams (supports both [["term",count], ...] and [{"term":..,"count":..}, ...])
static juce::StringArray getTopUnigrams(const juce::var& root, int limit = 32)
{
    juce::StringArray out;
    if (!root.isObject()) return out;
    if (auto* obj = root.getDynamicObject())
    {
        auto terms = obj->getProperty("terms");
        if (terms.isObject())
        {
            if (auto* t = terms.getDynamicObject())
            {
                auto uni = t->getProperty("top_unigrams");
                if (auto* arr = uni.getArray())
                {
                    for (int i = 0; i < arr->size() && out.size() < limit; ++i)
                    {
                        const auto& row = (*arr)[i];
                        if (row.isArray())
                        {
                            auto* r = row.getArray();
                            if (r->size() > 0) out.add(r->getReference(0).toString());
                        }
                        else if (row.isObject())
                        {
                            if (auto* o = row.getDynamicObject())
                                out.add(o->getProperty("term").toString());
                        }
                    }
                }
            }
        }
    }
    return out;
}

// Pull top_bigrams (same dual format support)
static juce::StringArray getTopBigrams(const juce::var& root, int limit = 16)
{
    juce::StringArray out;
    if (!root.isObject()) return out;
    if (auto* obj = root.getDynamicObject())
    {
        auto terms = obj->getProperty("terms");
        if (terms.isObject())
        {
            if (auto* t = terms.getDynamicObject())
            {
                auto bi = t->getProperty("top_bigrams");
                if (auto* arr = bi.getArray())
                {
                    for (int i = 0; i < arr->size() && out.size() < limit; ++i)
                    {
                        const auto& row = (*arr)[i];
                        if (row.isArray())
                        {
                            auto* r = row.getArray();
                            if (r->size() > 0) out.add(r->getReference(0).toString());
                        }
                        else if (row.isObject())
                        {
                            if (auto* o = row.getDynamicObject())
                                out.add(o->getProperty("bigram").toString());
                        }
                    }
                }
            }
        }
    }
    return out;
}


static const juce::Array<juce::var>* getDiceArray(const juce::var& root, const char* key)
{
    if (!root.isObject()) return nullptr;
    if (auto* obj = root.getDynamicObject())
    {
        auto diceVar = obj->getProperty("dice");
        if (diceVar.isObject())
        {
            if (auto* dice = diceVar.getDynamicObject())
            {
                auto v = dice->getProperty(juce::Identifier(key));
                if (auto* arr = v.getArray(); arr && !arr->isEmpty())
                    return arr;
            }
        }
    }
    return nullptr;
}

// Return an Array* for prompts.prompt_bank.generic if present and non-empty
static const juce::Array<juce::var>* getPromptBankGeneric(const juce::var& root)
{
    if (!root.isObject()) return nullptr;
    if (auto* obj = root.getDynamicObject())
    {
        auto bank = obj->getProperty("prompt_bank");
        if (bank.isObject())
        {
            if (auto* b = bank.getDynamicObject())
            {
                auto v = b->getProperty("generic");
                if (auto* arr = v.getArray(); arr && !arr->isEmpty())
                    return arr;
            }
        }
    }
    return nullptr;
}

void JerryUI::setFinetunePromptBank(const juce::String& repo,
    const juce::String& checkpoint,
    const juce::var& promptsJson)
{
    const auto key = repo + "|" + checkpoint;
    finetunePromptBanks.set(key, promptsJson);

    // Debug sizes
    if (auto* g = getPromptBankGeneric(promptsJson))
        DBG("[dice] bank set for " + key + " generic size=" + juce::String(g->size()));
    else
        DBG("[dice] bank set for " + key + " generic size=0");
}



juce::String JerryUI::generateConditionalPrompt()
{
    auto& rnd = juce::Random::getSystemRandom();

    // Try to use finetune bank when the currently selected finetune matches a loaded bank
    const auto repo = getSelectedFinetuneRepo();
    const auto ckpt = getSelectedFinetuneCheckpoint();
    const auto key = repo + "|" + ckpt;

    auto pickFromArray = [&](const juce::Array<juce::var>* arr) -> juce::String
        {
            if (!arr || arr->isEmpty()) return {};
            return (*arr)[rnd.nextInt(arr->size())].toString();
        };

    auto getBucket = [&](const juce::var& prompts, const juce::Identifier& which) -> const juce::Array<juce::var>*
        {
            if (!prompts.isObject()) return nullptr;
            if (auto* obj = prompts.getDynamicObject())
            {
                auto diceVar = obj->getProperty("dice");
                if (diceVar.isObject())
                {
                    if (auto* dice = diceVar.getDynamicObject())
                    {
                        auto arrVar = dice->getProperty(which);
                        return arrVar.getArray();
                    }
                }
            }
            return nullptr;
        };

    auto fallbackBeatOrInstr = [&]() -> juce::String
        {
            const bool useBeat = rnd.nextBool();
            return useBeat ? beatPrompts.getRandomPrompt()
                : instrumentPrompts.getRandomGenrePrompt();
        };

    // If we have a bank for the current finetune, try to use it
    if (finetunePromptBanks.contains(key))
    {
        const juce::var& prompts = finetunePromptBanks[key];

        // NEW: prefer prompt_bank.generic if present (your current backend schema)
        if (auto* genericPB = getPromptBankGeneric(prompts))
        {
            auto& rnd = juce::Random::getSystemRandom();

            // 30% of the time: compose from stats (short + varied)
            if (rnd.nextDouble() < 0.30)
            {
                auto unis = getTopUnigrams(prompts, 24);
                auto bigs = getTopBigrams(prompts, 12);

                juce::StringArray pieces;

                // pick 0-1 bigram
                if (bigs.size() > 0 && rnd.nextBool())
                {
                    auto bi = bigs[rnd.nextInt(bigs.size())];
                    bi = stripBpm(bi);
                    bi = bi.replaceCharacter('_', ' '); // just in case
                    pieces.add(bi);
                }

                // pick 1-3 unigrams
                const int uCount = 1 + rnd.nextInt(3); // 1..3
                for (int i = 0; i < uCount && unis.size() > 0; ++i)
                {
                    auto u = unis[rnd.nextInt(unis.size())];
                    pieces.add(u);
                }

                // Dedup + trim
                juce::String joined = pieces.joinIntoString(" ");
                joined = stripBpm(joined);
                joined = shrinkTokensRandom(joined, 2, 5); // keep it short

                if (joined.isNotEmpty())
                    return PromptHelpers::cleanupPrompt(joined, loopTypeIndex, smartLoop);
                // else fall back to bank prompt path
            }

            // 70% (or fallback) use a bank prompt, but shorten it
            auto s = (*genericPB)[rnd.nextInt(genericPB->size())].toString();
            s = stripBpm(s);
            s = shrinkTokensRandom(s, 3, 6);
            if (!s.isEmpty())
                return PromptHelpers::cleanupPrompt(s, loopTypeIndex, smartLoop);
        }

        // Back-compat: older schema with dice.*
        if (!smartLoop || loopTypeIndex == 0)
        {
            if (auto* generic = getBucket(prompts, juce::Identifier("generic")))
                if (!generic->isEmpty())
                    return PromptHelpers::cleanupPrompt(pickFromArray(generic), loopTypeIndex, smartLoop);

            const bool chooseDrums = rnd.nextBool();
            if (chooseDrums)
            {
                if (auto* drums = getBucket(prompts, juce::Identifier("drums")))
                    if (!drums->isEmpty())
                        return PromptHelpers::cleanupPrompt(pickFromArray(drums), loopTypeIndex, smartLoop);
            }
            else
            {
                if (auto* instr = getBucket(prompts, juce::Identifier("instrumental")))
                    if (!instr->isEmpty())
                        return PromptHelpers::cleanupPrompt(pickFromArray(instr), loopTypeIndex, smartLoop);
            }
            return PromptHelpers::cleanupPrompt(fallbackBeatOrInstr(), loopTypeIndex, smartLoop);
        }

        if (loopTypeIndex == 1)  // drums
        {
            if (auto* drums = getBucket(prompts, juce::Identifier("drums")))
                if (!drums->isEmpty())
                    return PromptHelpers::cleanupPrompt(pickFromArray(drums), loopTypeIndex, smartLoop);
            return PromptHelpers::cleanupPrompt(beatPrompts.getRandomPrompt(), loopTypeIndex, smartLoop);
        }

        if (loopTypeIndex == 2)  // instrumental
        {
            if (auto* instr = getBucket(prompts, juce::Identifier("instrumental")))
                if (!instr->isEmpty())
                    return PromptHelpers::cleanupPrompt(pickFromArray(instr), loopTypeIndex, smartLoop);
            return PromptHelpers::cleanupPrompt(instrumentPrompts.getRandomGenrePrompt(), loopTypeIndex, smartLoop);
        }

        return PromptHelpers::cleanupPrompt(beatPrompts.getRandomPrompt(), loopTypeIndex, smartLoop);
    }

    // No finetune bank ? original behavior
    if (!smartLoop || loopTypeIndex == 0)
    {
        const bool useBeat = rnd.nextBool();
        return PromptHelpers::cleanupPrompt(useBeat ? beatPrompts.getRandomPrompt()
            : instrumentPrompts.getRandomGenrePrompt(), loopTypeIndex, smartLoop);
    }
    if (loopTypeIndex == 1) return PromptHelpers::cleanupPrompt(beatPrompts.getRandomPrompt(), loopTypeIndex, smartLoop);
    if (loopTypeIndex == 2) return PromptHelpers::cleanupPrompt(instrumentPrompts.getRandomGenrePrompt(), loopTypeIndex, smartLoop);
    return PromptHelpers::cleanupPrompt(beatPrompts.getRandomPrompt(), loopTypeIndex, smartLoop);
}



void JerryUI::drawDiceIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool isHovered, bool isPressed)
{
    juce::Colour bgColour;
    juce::Colour pipColour;

    if (isPressed)
    {
        bgColour = Theme::Colors::Jerry.brighter(0.2f);
        pipColour = juce::Colours::white;
    }
    else if (isHovered)
    {
        bgColour = Theme::Colors::Jerry.brighter(0.3f);
        pipColour = juce::Colours::white;
    }
    else
    {
        bgColour = Theme::Colors::Jerry.withAlpha(0.9f);
        pipColour = juce::Colours::white;
    }

    juce::Path dicePath;
    dicePath.addRoundedRectangle(bounds, 2.0f);
    g.setColour(bgColour);
    g.fillPath(dicePath);

    const float pipRadius = bounds.getWidth() * 0.12f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float offset = bounds.getWidth() * 0.25f;

    g.setColour(pipColour);
    g.fillEllipse(cx - pipRadius, cy - pipRadius, pipRadius * 2.0f, pipRadius * 2.0f);

    const auto drawPip = [&](float x, float y)
    {
        g.fillEllipse(x - pipRadius, y - pipRadius, pipRadius * 2.0f, pipRadius * 2.0f);
    };

    drawPip(cx - offset, cy - offset);
    drawPip(cx + offset, cy - offset);
    drawPip(cx - offset, cy + offset);
    drawPip(cx + offset, cy + offset);
}

void JerryUI::setAvailableModels(const juce::StringArray& models,
    const juce::Array<bool>& isFinetune,
    const juce::StringArray& keys,
    const juce::StringArray& types,
    const juce::StringArray& repos,
    const juce::StringArray& checkpoints,
    const juce::StringArray& samplerProfiles)
{
    modelNames = models;
    modelIsFinetune = isFinetune;
    modelKeys = keys;
    modelTypes = types;           // NEW
    modelRepos = repos;           // NEW
    modelCheckpoints = checkpoints; // NEW
    modelSamplerProfiles = samplerProfiles;

    if (modelSamplerProfiles.size() != models.size())
    {
        modelSamplerProfiles.clear();
        for (int i = 0; i < models.size(); ++i)
            modelSamplerProfiles.add(i < isFinetune.size() && isFinetune[i] ? "saos_finetune" : "standard");
    }

    jerryModelComboBox.clear(juce::dontSendNotification);
    for (int i = 0; i < models.size(); ++i)
        jerryModelComboBox.addItem(models[i], i + 1);

    if (models.size() > 0)
    {
        selectedModelIndex = 0;
        jerryModelComboBox.setSelectedId(1, juce::dontSendNotification);

        // Set initial state based on first model
        bool firstIsFinetune = isFinetune.size() > 0 ? isFinetune[0] : false;
        updateSliderRangesForModel(firstIsFinetune);
        updateSamplerVisibility();

        // IMPORTANT: Trigger the callback so PluginEditor knows about the initial selection
        if (onModelChanged)
            onModelChanged(0, firstIsFinetune);
    }
}

juce::String JerryUI::getSelectedModelType() const
{
    if (selectedModelIndex >= 0 && selectedModelIndex < modelTypes.size())
        return modelTypes[selectedModelIndex];
    return "standard";
}

juce::String JerryUI::getSelectedFinetuneRepo() const
{
    if (selectedModelIndex >= 0 && selectedModelIndex < modelRepos.size())
        return modelRepos[selectedModelIndex];
    return "";
}

juce::String JerryUI::getSelectedFinetuneCheckpoint() const
{
    if (selectedModelIndex >= 0 && selectedModelIndex < modelCheckpoints.size())
        return modelCheckpoints[selectedModelIndex];
    return "";
}

void JerryUI::setSelectedModel(int index)
{
    if (index >= 0 && index < modelNames.size())
    {
        selectedModelIndex = index;
        jerryModelComboBox.setSelectedId(index + 1, juce::dontSendNotification);

        bool isFinetune = modelIsFinetune[index];
        updateSliderRangesForModel(isFinetune);
        updateSamplerVisibility();
    }
}

int JerryUI::getSelectedModelIndex() const
{
    return selectedModelIndex;
}

juce::String JerryUI::getSelectedModelKey() const
{
    if (selectedModelIndex >= 0 && selectedModelIndex < modelKeys.size())
        return modelKeys[selectedModelIndex];
    return "standard_saos";
}

bool JerryUI::getSelectedModelIsFinetune() const
{
    if (selectedModelIndex >= 0 && selectedModelIndex < modelIsFinetune.size())
        return modelIsFinetune[selectedModelIndex];
    return false;
}

juce::String JerryUI::getSelectedSamplerType() const
{
    return currentSamplerType;
}

void JerryUI::updateSamplerVisibility()
{
    const auto profile = getSelectedSamplerProfile();
    const auto previousSamplerType = currentSamplerType;

    if (profile == "sao10")
    {
        showingSamplerSelector = true;
        jerrySamplerLabel.setVisible(true);

        samplerEulerButton.setVisible(true);
        samplerDpmppButton.setVisible(true);
        samplerThirdButton.setVisible(true);

        configureSamplerButton(samplerEulerButton, "dpmpp-3m-sde", "dpmpp-3m-sde", false);
        configureSamplerButton(samplerDpmppButton, "dpmpp-2m-sde", "dpmpp-2m-sde", false);
        configureSamplerButton(samplerThirdButton, "k-heun", "k-heun", false);

        const bool keepPrevious =
            previousSamplerType == "dpmpp-3m-sde" ||
            previousSamplerType == "dpmpp-2m-sde" ||
            previousSamplerType == "k-heun";
        applySamplerSelection(keepPrevious ? previousSamplerType : "dpmpp-3m-sde");
    }
    else if (profile == "saos_finetune")
    {
        showingSamplerSelector = true;
        jerrySamplerLabel.setVisible(true);

        samplerEulerButton.setVisible(true);
        samplerDpmppButton.setVisible(true);
        samplerThirdButton.setVisible(false);

        configureSamplerButton(samplerEulerButton, "euler", "euler", false);
        configureSamplerButton(samplerDpmppButton, "dpmpp", "dpmpp", false);
        configureSamplerButton(samplerThirdButton, "k-heun", "k-heun", false);

        const bool keepPrevious =
            previousSamplerType == "euler" ||
            previousSamplerType == "dpmpp";
        applySamplerSelection(keepPrevious ? previousSamplerType : "dpmpp");
    }
    else
    {
        showingSamplerSelector = false;
        jerrySamplerLabel.setVisible(false);
        samplerEulerButton.setVisible(false);
        samplerDpmppButton.setVisible(false);
        samplerThirdButton.setVisible(false);
        currentSamplerType = "pingpong";
        samplerEulerButton.setToggleState(false, juce::dontSendNotification);
        samplerDpmppButton.setToggleState(false, juce::dontSendNotification);
        samplerThirdButton.setToggleState(false, juce::dontSendNotification);
    }

    resized();  // Re-layout to show/hide sampler row
}

void JerryUI::updateSliderRangesForModel(bool isFinetune)
{
    if (isFinetune)
    {
        // Finetune ranges: steps 4-50 (default 30), cfg 1.0-7.0 (default 4.0)
        jerryStepsSlider.setRange(4.0, 50.0, 1.0);
        jerryStepsSlider.setValue(30, juce::sendNotification);

        jerryCfgSlider.setRange(1.0, 7.0, 0.1);
        jerryCfgSlider.setValue(4.0, juce::sendNotification);
    }
    else
    {
        // Standard SAOS ranges: steps 4-16 (default 8), cfg 0.5-2.0 (default 1.0)
        jerryStepsSlider.setRange(4.0, 16.0, 1.0);
        jerryStepsSlider.setValue(8, juce::sendNotification);

        jerryCfgSlider.setRange(0.5, 2.0, 0.1);
        jerryCfgSlider.setValue(1.0, juce::sendNotification);
    }

    DBG("Updated slider ranges for " + juce::String(isFinetune ? "finetune" : "standard") + " model");
}

void JerryUI::setBpm(int bpm)
{
    bpmValue = bpm;

    if (isStandaloneMode)
    {
        // In standalone, this updates the manual slider
        // (though typically the slider drives the value, not vice versa)
        jerryBpmSlider.setValue(bpm, juce::dontSendNotification);
    }
    else
    {
        // In plugin mode, this updates the label with DAW BPM
        jerryBpmLabel.setText("bpm: " + juce::String(bpmValue) + " (from daw)", juce::dontSendNotification);
    }
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

void JerryUI::applySamplerSelection(const juce::String& samplerType)
{
    currentSamplerType = samplerType;
    const bool eulerSelected =
        samplerEulerButton.isVisible() && getSamplerTypeForButton(samplerEulerButton) == samplerType;
    const bool dpmppSelected =
        samplerDpmppButton.isVisible() && getSamplerTypeForButton(samplerDpmppButton) == samplerType;
    const bool thirdSelected =
        samplerThirdButton.isVisible() && getSamplerTypeForButton(samplerThirdButton) == samplerType;

    if (!eulerSelected && !dpmppSelected && !thirdSelected)
    {
        if (samplerEulerButton.isVisible())
        {
            currentSamplerType = getSamplerTypeForButton(samplerEulerButton);
            samplerEulerButton.setToggleState(true, juce::dontSendNotification);
            samplerDpmppButton.setToggleState(false, juce::dontSendNotification);
            samplerThirdButton.setToggleState(false, juce::dontSendNotification);
        }
        else if (samplerDpmppButton.isVisible())
        {
            currentSamplerType = getSamplerTypeForButton(samplerDpmppButton);
            samplerEulerButton.setToggleState(false, juce::dontSendNotification);
            samplerDpmppButton.setToggleState(true, juce::dontSendNotification);
            samplerThirdButton.setToggleState(false, juce::dontSendNotification);
        }
        else if (samplerThirdButton.isVisible())
        {
            currentSamplerType = getSamplerTypeForButton(samplerThirdButton);
            samplerEulerButton.setToggleState(false, juce::dontSendNotification);
            samplerDpmppButton.setToggleState(false, juce::dontSendNotification);
            samplerThirdButton.setToggleState(true, juce::dontSendNotification);
        }
        return;
    }

    samplerEulerButton.setToggleState(eulerSelected, juce::dontSendNotification);
    samplerDpmppButton.setToggleState(dpmppSelected, juce::dontSendNotification);
    samplerThirdButton.setToggleState(thirdSelected, juce::dontSendNotification);
}

juce::String JerryUI::getSelectedSamplerProfile() const
{
    if (selectedModelIndex >= 0
        && selectedModelIndex < modelSamplerProfiles.size()
        && modelSamplerProfiles[selectedModelIndex].isNotEmpty())
    {
        return modelSamplerProfiles[selectedModelIndex];
    }

    return getSelectedModelIsFinetune() ? "saos_finetune" : "standard";
}

juce::String JerryUI::getSamplerTypeForButton(const juce::ToggleButton& button) const
{
    const auto samplerType = button.getProperties().getWithDefault("samplerType", {}).toString();
    return samplerType.isNotEmpty() ? samplerType : button.getButtonText();
}

void JerryUI::configureSamplerButton(juce::ToggleButton& button,
                                     const juce::String& buttonText,
                                     const juce::String& samplerType,
                                     bool isSelected)
{
    button.setButtonText(buttonText);
    button.getProperties().set("samplerType", samplerType);
    button.setToggleState(isSelected, juce::dontSendNotification);
}

void JerryUI::applyEnablement(bool canGenerate, bool canSmartLoop, bool isGenerating)
{
    lastCanGenerate = canGenerate;
    lastCanSmartLoop = canSmartLoop;
    lastIsGenerating = isGenerating;

    // IMPORTANT: Disable if model is loading
    const bool allowGenerate = canGenerate && !isGenerating && !isLoadingModel;
    const bool allowSmartLoop = canSmartLoop && !isGenerating && !isLoadingModel;
    const bool allowLoopTypes = allowSmartLoop && smartLoop;

    generateWithJerryButton.setEnabled(allowGenerate);
    generateAsLoopButton.setEnabled(allowSmartLoop);
    loopTypeAutoButton.setEnabled(allowLoopTypes);
    loopTypeDrumsButton.setEnabled(allowLoopTypes);
    loopTypeInstrumentsButton.setEnabled(allowLoopTypes);
}

void JerryUI::setUsingLocalhost(bool localhost)
{
    isUsingLocalhost = localhost;
    toggleCustomSectionButton.setVisible(localhost);

    // If switching away from localhost, hide custom section
    if (!localhost && showingCustomFinetuneSection) {
        toggleCustomFinetuneSection();
    }

    resized();
}

void JerryUI::toggleCustomFinetuneSection()
{
    showingCustomFinetuneSection = !showingCustomFinetuneSection;

    customFinetuneLabel.setVisible(showingCustomFinetuneSection);
    repoTextEditor.setVisible(showingCustomFinetuneSection);
    fetchCheckpointsButton.setVisible(showingCustomFinetuneSection);
    checkpointComboBox.setVisible(showingCustomFinetuneSection);
    addModelButton.setVisible(showingCustomFinetuneSection);

    // Update button text
    toggleCustomSectionButton.setButtonText(showingCustomFinetuneSection ? "-" : "+");

    resized();
}

void JerryUI::setFetchingCheckpoints(bool fetching)
{
    isFetchingCheckpoints = fetching;
    fetchCheckpointsButton.setEnabled(!fetching);
    fetchCheckpointsButton.setButtonText(fetching ? "fetching..." : "fetch");
}

void JerryUI::setAvailableCheckpoints(const juce::StringArray& checkpoints)
{
    checkpointComboBox.clear();

    for (int i = 0; i < checkpoints.size(); ++i) {
        checkpointComboBox.addItem(checkpoints[i], i + 1);
    }

    addModelButton.setEnabled(checkpoints.size() > 0);

    if (checkpoints.size() > 0) {
        checkpointComboBox.setSelectedId(1);  // Select first by default
    }
}

void JerryUI::setLoadingModel(bool loading, const juce::String& modelInfo)
{
    isLoadingModel = loading;

    if (loading)
    {
        // CRITICAL: Clear the current selection to avoid showing stale model name
        jerryModelComboBox.setSelectedId(0, juce::dontSendNotification);

        // Add a temporary "Loading..." item to the dropdown and select it
        // This ensures the dropdown visibly shows the loading state
        jerryModelComboBox.clear(juce::dontSendNotification);

        if (!modelInfo.isEmpty())
        {
            jerryModelComboBox.addItem("Loading " + modelInfo + "...", 999);
        }
        else
        {
            jerryModelComboBox.addItem("Loading model...", 999);
        }

        jerryModelComboBox.setSelectedId(999, juce::dontSendNotification);

        // Disable generation during load
        generateWithJerryButton.setEnabled(false);
        generateAsLoopButton.setEnabled(false);
    }
    else
    {
        // Loading complete - the model list will be refreshed by fetchJerryAvailableModels()
        // No need to do anything here, just re-enable buttons
        applyEnablement(lastCanGenerate, lastCanSmartLoop, lastIsGenerating);
    }
}

void JerryUI::selectModelByRepo(const juce::String& repo)
{
    // Look through the current model list to find a finetune model from the specified repo
    // and select it as the active model
    for (int i = 0; i < modelRepos.size(); ++i)
    {
        if (modelRepos[i] == repo && modelIsFinetune[i])
        {
            DBG("Found and selecting model from repo: " + repo + " at index " + juce::String(i));
            setSelectedModel(i);
            break;
        }
    }
}

void JerryUI::setIsStandalone(bool standalone)
{
    isStandaloneMode = standalone;

    // Update component visibility based on mode
    if (isStandaloneMode)
    {
        // Standalone: show slider, update label
        jerryBpmLabel.setText("bpm:", juce::dontSendNotification);
        jerryBpmLabel.setJustificationType(juce::Justification::centredRight);
        jerryBpmSlider.setVisible(true);

        DBG("JerryUI: Switched to standalone mode (manual BPM control)");
    }
    else
    {
        // Plugin: hide slider, show full label
        jerryBpmLabel.setText("bpm: " + juce::String(bpmValue) + " (from daw)", juce::dontSendNotification);
        jerryBpmLabel.setJustificationType(juce::Justification::centred);
        jerryBpmSlider.setVisible(false);

        DBG("JerryUI: Switched to plugin mode (DAW BPM)");
    }

    resized();  // Re-layout to show/hide components
}

void JerryUI::setManualBpm(int bpm)
{
    bpmValue = bpm;
    jerryBpmSlider.setValue(bpm, juce::dontSendNotification);

    DBG("JerryUI: Manual BPM set to " + juce::String(bpm));
}

int JerryUI::getManualBpm() const
{
    // In standalone, return slider value; in plugin, return DAW BPM
    int result = isStandaloneMode ? (int)jerryBpmSlider.getValue() : bpmValue;

    DBG("JerryUI: getManualBpm() returning " + juce::String(result) +
        " (mode: " + juce::String(isStandaloneMode ? "standalone" : "plugin") + ")");

    return result;
}
