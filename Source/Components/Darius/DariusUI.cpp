#include "DariusUI.h"

#include "../../Utils/Theme.h"

DariusUI::DariusUI()
{
    setInterceptsMouseClicks(true, true);

    // === Header ===
    dariusLabel.setText("darius (magentaRT)", juce::dontSendNotification);
    dariusLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    dariusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    dariusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(dariusLabel);

    // Backend URL editor
    dariusUrlEditor.setMultiLine(false);
    dariusUrlEditor.setReturnKeyStartsNewLine(false);
    dariusUrlEditor.onTextChange = [this]() {
        backendUrl = dariusUrlEditor.getText();
        if (onUrlChanged)
            onUrlChanged(backendUrl);
    };
    addAndMakeVisible(dariusUrlEditor);

    dariusUrlLabel.setText("backend url", juce::dontSendNotification);
    dariusUrlLabel.setFont(juce::FontOptions(12.0f));
    dariusUrlLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    dariusUrlLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(dariusUrlLabel);

    dariusHealthCheckButton.setButtonText("check connection");
    dariusHealthCheckButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    dariusHealthCheckButton.setTooltip("check magentaRT backend connection");
    dariusHealthCheckButton.onClick = [this]() {
        if (onHealthCheckRequested)
            onHealthCheckRequested();
    };
    addAndMakeVisible(dariusHealthCheckButton);

    dariusStatusLabel.setText(connectionStatusText, juce::dontSendNotification);
    dariusStatusLabel.setFont(juce::FontOptions(11.0f));
    dariusStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    dariusStatusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(dariusStatusLabel);

    // Subtab buttons
    auto prepSubTabButton = [this](CustomButton& button, const juce::String& text, SubTab tab) {
        button.setButtonText(text);
        button.setButtonStyle(CustomButton::ButtonStyle::Standard);
        button.onClick = [this, tab]() { setCurrentSubTab(tab); };
        addAndMakeVisible(button);
    };
    prepSubTabButton(dariusBackendTabButton, "backend", SubTab::Backend);
    prepSubTabButton(dariusModelTabButton, "model", SubTab::Model);
    prepSubTabButton(dariusGenerationTabButton, "generation", SubTab::Generation);

    // Model viewport/content
    dariusModelContent = std::make_unique<juce::Component>();
    dariusModelViewport = std::make_unique<juce::Viewport>();
    dariusModelViewport->setViewedComponent(dariusModelContent.get(), false);
    addAndMakeVisible(dariusModelViewport.get());

    // Model UI elements
    dariusModelHeaderLabel.setText("current model", juce::dontSendNotification);
    dariusModelHeaderLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    dariusModelHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    dariusModelHeaderLabel.setJustificationType(juce::Justification::centredLeft);
    dariusModelContent->addAndMakeVisible(dariusModelHeaderLabel);

    dariusModelGuardLabel.setText("backend offline or in template mode. run health check on the backend tab",
                                  juce::dontSendNotification);
    dariusModelGuardLabel.setFont(juce::FontOptions(12.0f));
    dariusModelGuardLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    dariusModelGuardLabel.setJustificationType(juce::Justification::centredLeft);
    dariusModelContent->addAndMakeVisible(dariusModelGuardLabel);

    dariusRefreshConfigButton.setButtonText("refresh config");
    dariusRefreshConfigButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    dariusRefreshConfigButton.onClick = [this]() {
        if (onRefreshConfigRequested)
            onRefreshConfigRequested();
    };
    dariusModelContent->addAndMakeVisible(dariusRefreshConfigButton);

    dariusUseBaseModelToggle.setButtonText("use base model");
    dariusUseBaseModelToggle.setToggleState(useBaseModel, juce::dontSendNotification);
    dariusUseBaseModelToggle.onClick = [this]() {
        const bool newState = dariusUseBaseModelToggle.getToggleState();
        if (useBaseModel != newState)
        {
            useBaseModel = newState;
            if (onUseBaseModelToggled)
                onUseBaseModelToggled(useBaseModel);
        }
        refreshModelControls();
    };
    dariusModelContent->addAndMakeVisible(dariusUseBaseModelToggle);

    auto prepStatusRow = [this](juce::Label& label, const juce::String& name) {
        label.setText(name + ": -", juce::dontSendNotification);
        label.setFont(juce::FontOptions(12.0f));
        label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        label.setJustificationType(juce::Justification::centredLeft);
        dariusModelContent->addAndMakeVisible(label);
    };
    prepStatusRow(dariusActiveSizeLabel, "Active size");
    prepStatusRow(dariusRepoLabel, "Repo");
    prepStatusRow(dariusStepLabel, "Step");
    prepStatusRow(dariusLoadedLabel, "Loaded");
    prepStatusRow(dariusWarmupLabel, "Warmup");

    dariusRepoFieldLabel.setText("finetune repo", juce::dontSendNotification);
    dariusRepoFieldLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    dariusRepoFieldLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    dariusRepoFieldLabel.setJustificationType(juce::Justification::centredLeft);
    dariusModelContent->addAndMakeVisible(dariusRepoFieldLabel);

    dariusRepoField.setMultiLine(false);
    dariusRepoField.setReturnKeyStartsNewLine(false);
    dariusRepoField.setInputRestrictions(256);
    dariusRepoField.setScrollbarsShown(false);
    dariusRepoField.setJustification(juce::Justification::centredLeft);
    dariusRepoField.setTooltip("e.g. thepatch/magenta-ft");
    dariusRepoField.onReturnKey = [this]() {
        finetuneRepoText = dariusRepoField.getText().trim();
        if (onFinetuneRepoChanged)
            onFinetuneRepoChanged(finetuneRepoText);
    };
    dariusRepoField.onFocusLost = [this]() {
        finetuneRepoText = dariusRepoField.getText().trim();
        if (onFinetuneRepoChanged)
            onFinetuneRepoChanged(finetuneRepoText);
    };
    dariusRepoField.onTextChange = [this]() {
        finetuneRepoText = dariusRepoField.getText().trim();
        if (onFinetuneRepoChanged)
            onFinetuneRepoChanged(finetuneRepoText);
    };
    dariusModelContent->addAndMakeVisible(dariusRepoField);

    dariusCheckpointButton.setButtonText("checkpoint: latest");
    dariusCheckpointButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    dariusCheckpointButton.onClick = [this]() { handleCheckpointButtonClicked(); };
    dariusModelContent->addAndMakeVisible(dariusCheckpointButton);

    dariusApplyWarmButton.setButtonText("apply & warm");
    dariusApplyWarmButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    dariusApplyWarmButton.onClick = [this]() {
        if (onApplyWarmRequested)
            onApplyWarmRequested();
    };
    dariusModelContent->addAndMakeVisible(dariusApplyWarmButton);

    dariusWarmStatusLabel.setText("", juce::dontSendNotification);
    dariusWarmStatusLabel.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    dariusWarmStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    dariusWarmStatusLabel.setJustificationType(juce::Justification::centredLeft);
    dariusWarmStatusLabel.setVisible(false);
    dariusModelContent->addAndMakeVisible(dariusWarmStatusLabel);

    // Generation viewport/content
    dariusGenerationContent = std::make_unique<juce::Component>();
    dariusGenerationViewport = std::make_unique<juce::Viewport>();
    dariusGenerationViewport->setViewedComponent(dariusGenerationContent.get(), false);
    addAndMakeVisible(dariusGenerationViewport.get());

    genStylesHeaderLabel.setText("styles & weights", juce::dontSendNotification);
    genStylesHeaderLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    genStylesHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    genStylesHeaderLabel.setJustificationType(juce::Justification::centredLeft);
    dariusGenerationContent->addAndMakeVisible(genStylesHeaderLabel);

    genAddStyleButton.setButtonText("+");
    genAddStyleButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    genAddStyleButton.onClick = [this]() { handleAddStyleRow(); };
    dariusGenerationContent->addAndMakeVisible(genAddStyleButton);

    addGenStyleRowInternal("", 1.0);
    rebuildGenStylesUI();

    genLoopLabel.setText("loop influence: 0.50", juce::dontSendNotification);
    genLoopLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    genLoopLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    genLoopLabel.setJustificationType(juce::Justification::centredLeft);
    dariusGenerationContent->addAndMakeVisible(genLoopLabel);

    genLoopSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    genLoopSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    genLoopSlider.setRange(0.0, 1.0, 0.01);
    genLoopSlider.setValue(genLoopInfluence, juce::dontSendNotification);
    genLoopSlider.setTooltip("How strongly the loop steers generation (0.00–1.00)");
    genLoopSlider.onValueChange = [this]() {
        genLoopInfluence = genLoopSlider.getValue();
        updateGenLoopLabel();
    };
    dariusGenerationContent->addAndMakeVisible(genLoopSlider);

    genAdvancedToggle.setButtonText("advanced settings");
    genAdvancedToggle.setButtonStyle(CustomButton::ButtonStyle::Standard);
    genAdvancedToggle.onClick = [this]() {
        genAdvancedOpen = !genAdvancedOpen;
        updateGenAdvancedToggleText();
        resized();
    };
    dariusGenerationContent->addAndMakeVisible(genAdvancedToggle);

    genTempLabel.setText("temperature: 1.20", juce::dontSendNotification);
    genTempLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    genTempLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    dariusGenerationContent->addAndMakeVisible(genTempLabel);

    genTempSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    genTempSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    genTempSlider.setRange(0.0, 10.0, 0.01);
    genTempSlider.setValue(genTemperature, juce::dontSendNotification);
    genTempSlider.onValueChange = [this]() {
        genTemperature = genTempSlider.getValue();
        genTempLabel.setText("temperature: " + juce::String(genTemperature, 2), juce::dontSendNotification);
    };
    dariusGenerationContent->addAndMakeVisible(genTempSlider);

    genTopKLabel.setText("top-k: 40", juce::dontSendNotification);
    genTopKLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    genTopKLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    dariusGenerationContent->addAndMakeVisible(genTopKLabel);

    genTopKSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    genTopKSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    genTopKSlider.setRange(1.0, 300.0, 1.0);
    genTopKSlider.setValue(genTopK, juce::dontSendNotification);
    genTopKSlider.onValueChange = [this]() {
        genTopK = (int)genTopKSlider.getValue();
        genTopKLabel.setText("top-k: " + juce::String(genTopK), juce::dontSendNotification);
    };
    dariusGenerationContent->addAndMakeVisible(genTopKSlider);

    genGuidanceLabel.setText("guidance: 5.00", juce::dontSendNotification);
    genGuidanceLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    genGuidanceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    dariusGenerationContent->addAndMakeVisible(genGuidanceLabel);

    genGuidanceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    genGuidanceSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    genGuidanceSlider.setRange(0.0, 10.0, 0.01);
    genGuidanceSlider.setValue(genGuidance, juce::dontSendNotification);
    genGuidanceSlider.onValueChange = [this]() {
        genGuidance = genGuidanceSlider.getValue();
        genGuidanceLabel.setText("guidance: " + juce::String(genGuidance, 2), juce::dontSendNotification);
    };
    dariusGenerationContent->addAndMakeVisible(genGuidanceSlider);

    genBarsLabel.setText("bars", juce::dontSendNotification);
    genBarsLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    genBarsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    dariusGenerationContent->addAndMakeVisible(genBarsLabel);

    genBars4Button.setButtonText("4");
    genBars4Button.setButtonStyle(CustomButton::ButtonStyle::Standard);
    genBars4Button.onClick = [this]() {
        genBars = 4;
        updateGenBarsButtons();
    };
    dariusGenerationContent->addAndMakeVisible(genBars4Button);

    genBars8Button.setButtonText("8");
    genBars8Button.setButtonStyle(CustomButton::ButtonStyle::Standard);
    genBars8Button.onClick = [this]() {
        genBars = 8;
        updateGenBarsButtons();
    };
    dariusGenerationContent->addAndMakeVisible(genBars8Button);

    genBars16Button.setButtonText("16");
    genBars16Button.setButtonStyle(CustomButton::ButtonStyle::Standard);
    genBars16Button.onClick = [this]() {
        genBars = 16;
        updateGenBarsButtons();
    };
    dariusGenerationContent->addAndMakeVisible(genBars16Button);

    genBpmLabel.setText("bpm", juce::dontSendNotification);
    genBpmLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    genBpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    dariusGenerationContent->addAndMakeVisible(genBpmLabel);

    genBpmValueLabel.setText(juce::String(genBPM, 1), juce::dontSendNotification);
    genBpmValueLabel.setFont(juce::FontOptions(12.0f));
    genBpmValueLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    dariusGenerationContent->addAndMakeVisible(genBpmValueLabel);

    genSourceLabel.setText("source", juce::dontSendNotification);
    genSourceLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    genSourceLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    dariusGenerationContent->addAndMakeVisible(genSourceLabel);

    genRecordingButton.setButtonText("recording");
    genRecordingButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    genRecordingButton.onClick = [this]() {
        if (!savedRecordingAvailable)
            return;
        if (genAudioSource != GenAudioSource::Recording)
        {
            genAudioSource = GenAudioSource::Recording;
            updateGenSourceButtons();
            if (onAudioSourceChanged)
                onAudioSourceChanged(true);
        }
    };
    dariusGenerationContent->addAndMakeVisible(genRecordingButton);

    genOutputButton.setButtonText("output");
    genOutputButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    genOutputButton.onClick = [this]() {
        if (genAudioSource != GenAudioSource::Output)
        {
            genAudioSource = GenAudioSource::Output;
            updateGenSourceButtons();
            if (onAudioSourceChanged)
                onAudioSourceChanged(false);
        }
    };
    dariusGenerationContent->addAndMakeVisible(genOutputButton);

    genSourceGuardLabel.setText("no saved recording found", juce::dontSendNotification);
    genSourceGuardLabel.setFont(juce::FontOptions(11.0f));
    genSourceGuardLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    genSourceGuardLabel.setJustificationType(juce::Justification::centredLeft);
    genSourceGuardLabel.setVisible(false);
    dariusGenerationContent->addAndMakeVisible(genSourceGuardLabel);

    genGenerateButton.setButtonText("generate");
    genGenerateButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    genGenerateButton.onClick = [this]() {
        if (onGenerateRequested)
            onGenerateRequested();
    };
    dariusGenerationContent->addAndMakeVisible(genGenerateButton);

    genSteeringToggle.setButtonText("steering");
    genSteeringToggle.setButtonStyle(CustomButton::ButtonStyle::Standard);
    genSteeringToggle.onClick = [this]() {
        genSteeringOpen = !genSteeringOpen;
        updateGenSteeringToggleText();
        resized();
    };
    dariusGenerationContent->addAndMakeVisible(genSteeringToggle);

    genMeanLabel.setText("mean: 1.00", juce::dontSendNotification);
    genMeanLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    genMeanLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    dariusGenerationContent->addAndMakeVisible(genMeanLabel);

    genMeanSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    genMeanSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    genMeanSlider.setRange(0.0, 2.0, 0.01);
    genMeanSlider.setValue(genMean, juce::dontSendNotification);
    genMeanSlider.onValueChange = [this]() {
        genMean = genMeanSlider.getValue();
        genMeanLabel.setText("mean: " + juce::String(genMean, 2), juce::dontSendNotification);
    };
    dariusGenerationContent->addAndMakeVisible(genMeanSlider);

    genCentroidsHeaderLabel.setText("centroids", juce::dontSendNotification);
    genCentroidsHeaderLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    genCentroidsHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    dariusGenerationContent->addAndMakeVisible(genCentroidsHeaderLabel);

    updateGenAdvancedToggleText();
    updateGenBarsButtons();
    updateGenSourceButtons();
    updateGenSourceEnabled();
    updateGenSteeringToggleText();
    refreshModelControls();
    setCurrentSubTab(SubTab::Backend);
}

void DariusUI::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void DariusUI::resized()
{
    auto bounds = getLocalBounds();

    juce::FlexBox dariusMainFlexBox;
    dariusMainFlexBox.flexDirection = juce::FlexBox::Direction::column;
    dariusMainFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    juce::FlexItem dariusTitleItem(dariusLabel);
    dariusTitleItem.height = 30;
    dariusTitleItem.margin = juce::FlexItem::Margin(5, 0, 5, 0);

    juce::Component subTabDummy;
    juce::FlexItem subTabItem(subTabDummy);
    subTabItem.height = 35;
    subTabItem.margin = juce::FlexItem::Margin(5, 0, 10, 0);

    juce::Component contentDummy;
    juce::FlexItem contentItem(contentDummy);
    contentItem.flexGrow = 1;
    contentItem.margin = juce::FlexItem::Margin(0, 5, 5, 5);

    dariusMainFlexBox.items.add(dariusTitleItem);
    dariusMainFlexBox.items.add(subTabItem);
    dariusMainFlexBox.items.add(contentItem);

    dariusMainFlexBox.performLayout(bounds);

    auto subtabRowBounds = dariusMainFlexBox.items[1].currentBounds.toNearestInt();
    auto subtabButtonWidth = subtabRowBounds.getWidth() / 3;
    auto backendBounds = subtabRowBounds.removeFromLeft(subtabButtonWidth);
    auto modelBounds = subtabRowBounds.removeFromLeft(subtabButtonWidth);
    auto generationBounds = subtabRowBounds;

    dariusBackendTabButton.setBounds(backendBounds.reduced(2, 2));
    dariusModelTabButton.setBounds(modelBounds.reduced(2, 2));
    dariusGenerationTabButton.setBounds(generationBounds.reduced(2, 2));

    auto contentBounds = dariusMainFlexBox.items[2].currentBounds.toNearestInt();

    if (currentSubTab == SubTab::Backend)
    {
        juce::FlexBox backendFlexBox;
        backendFlexBox.flexDirection = juce::FlexBox::Direction::column;
        backendFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

        juce::FlexItem urlLabelItem(dariusUrlLabel);
        urlLabelItem.height = 15;
        urlLabelItem.margin = juce::FlexItem::Margin(2, 0, 2, 0);

        juce::FlexItem urlEditorItem(dariusUrlEditor);
        urlEditorItem.height = 25;
        urlEditorItem.margin = juce::FlexItem::Margin(0, 5, 5, 5);

        juce::FlexItem healthItem(dariusHealthCheckButton);
        healthItem.height = 35;
        healthItem.margin = juce::FlexItem::Margin(5, 50, 5, 50);

        juce::FlexItem statusItem(dariusStatusLabel);
        statusItem.height = 20;
        statusItem.margin = juce::FlexItem::Margin(5, 5, 5, 5);

        backendFlexBox.items.add(urlLabelItem);
        backendFlexBox.items.add(urlEditorItem);
        backendFlexBox.items.add(healthItem);
        backendFlexBox.items.add(statusItem);

        backendFlexBox.performLayout(contentBounds);
    }
    else if (currentSubTab == SubTab::Model)
    {
        dariusModelViewport->setBounds(contentBounds);
        const auto contentW = contentBounds.getWidth() - 20;
        dariusModelContent->setSize(contentW, 300);

        auto area = juce::Rectangle<int>(10, 10, contentW - 20, 300);

        auto headerRow = area.removeFromTop(22);
        dariusModelHeaderLabel.setBounds(headerRow.removeFromLeft(headerRow.getWidth() - 140));
        dariusRefreshConfigButton.setBounds(headerRow.removeFromRight(130).reduced(4, 0));

        auto toggleRow = area.removeFromTop(22);
        dariusUseBaseModelToggle.setBounds(toggleRow.removeFromLeft(220));
        area.removeFromTop(6);

        auto repoLabelRow = area.removeFromTop(16);
        dariusRepoFieldLabel.setBounds(repoLabelRow.removeFromLeft(220));
        auto repoFieldRow = area.removeFromTop(22);
        dariusRepoField.setBounds(repoFieldRow.removeFromLeft(220));
        area.removeFromTop(6);

        auto ckptRow = area.removeFromTop(22);
        dariusCheckpointButton.setBounds(ckptRow.removeFromLeft(220));
        area.removeFromTop(6);

        auto applyRow = area.removeFromTop(26);
        dariusApplyWarmButton.setBounds(applyRow.removeFromLeft(220));
        dariusWarmStatusLabel.setBounds(applyRow.removeFromLeft(120).reduced(2));
        area.removeFromTop(6);

        auto guardRow = area.removeFromTop(18);
        dariusModelGuardLabel.setBounds(guardRow);

        const int rowH = 18;
        dariusActiveSizeLabel.setBounds(area.removeFromTop(rowH));
        area.removeFromTop(4);
        dariusRepoLabel.setBounds(area.removeFromTop(rowH));
        area.removeFromTop(4);
        dariusStepLabel.setBounds(area.removeFromTop(rowH));
        area.removeFromTop(4);
        dariusLoadedLabel.setBounds(area.removeFromTop(rowH));
        area.removeFromTop(4);
        dariusWarmupLabel.setBounds(area.removeFromTop(rowH));
    }
    else if (currentSubTab == SubTab::Generation)
    {
        dariusGenerationViewport->setBounds(contentBounds);
        const int contentW = contentBounds.getWidth() - 20;
        auto area = juce::Rectangle<int>(10, 10, contentW - 20, 600);

        auto row = area.removeFromTop(24);
        genBarsLabel.setBounds(row.removeFromLeft(40));
        auto bars4 = row.removeFromLeft(28);  genBars4Button.setBounds(bars4);
        row.removeFromLeft(6);
        auto bars8 = row.removeFromLeft(28);  genBars8Button.setBounds(bars8);
        row.removeFromLeft(6);
        auto bars16 = row.removeFromLeft(28); genBars16Button.setBounds(bars16);

        auto bpmValueW = 60;
        auto bpmVal = row.removeFromRight(bpmValueW);
        genBpmValueLabel.setBounds(bpmVal);
        genBpmLabel.setBounds(row.removeFromRight(36));

        area.removeFromTop(8);

        auto srcRow = area.removeFromTop(24);
        genSourceLabel.setBounds(srcRow.removeFromLeft(50));
        auto recBounds = srcRow.removeFromLeft(92);
        genRecordingButton.setBounds(recBounds);
        srcRow.removeFromLeft(6);
        auto outBounds = srcRow.removeFromLeft(78);
        genOutputButton.setBounds(outBounds);

        auto guardRow = area.removeFromTop(16);
        genSourceGuardLabel.setBounds(guardRow);
        area.removeFromTop(6);

        {
            auto headerRow = area.removeFromTop(20);
            auto headerLeft = headerRow.removeFromLeft(juce::jmax(0, headerRow.getWidth() - 28));
            genStylesHeaderLabel.setBounds(headerLeft);
            genAddStyleButton.setBounds(headerRow.removeFromRight(24));
            area.removeFromTop(6);
        }

        layoutGenStylesUI(area);
        area.removeFromTop(4);

        auto loopLabelRow = area.removeFromTop(18);
        genLoopLabel.setBounds(loopLabelRow.removeFromLeft(220));
        auto loopSliderRow = area.removeFromTop(22);
        genLoopSlider.setBounds(loopSliderRow.removeFromLeft(220));
        area.removeFromTop(8);

        auto advRow = area.removeFromTop(22);
        genAdvancedToggle.setBounds(advRow.removeFromLeft(220));
        area.removeFromTop(6);
        if (genAdvancedOpen)
            layoutGenAdvancedUI(area);
        else
        {
            genTempLabel.setVisible(false); genTempSlider.setVisible(false);
            genTopKLabel.setVisible(false); genTopKSlider.setVisible(false);
            genGuidanceLabel.setVisible(false); genGuidanceSlider.setVisible(false);
        }

        if (steeringMeanAvailable || steeringCentroidCount > 0)
        {
            auto steerRow = area.removeFromTop(22);
            genSteeringToggle.setBounds(steerRow.removeFromLeft(220));
            area.removeFromTop(6);

            if (genSteeringOpen)
                layoutGenSteeringUI(area);
            else
            {
                genMeanLabel.setVisible(false);
                genMeanSlider.setVisible(false);
                genCentroidsHeaderLabel.setVisible(false);
                for (auto* l : genCentroidLabels)  if (l) l->setVisible(false);
                for (auto* s : genCentroidSliders) if (s) s->setVisible(false);
            }
        }
        else
        {
            genSteeringToggle.setBounds(0, 0, 0, 0);
            genMeanLabel.setVisible(false);
            genMeanSlider.setVisible(false);
            genCentroidsHeaderLabel.setVisible(false);
            for (auto* l : genCentroidLabels)  if (l) l->setVisible(false);
            for (auto* s : genCentroidSliders) if (s) s->setVisible(false);
        }

        auto genRow = area.removeFromTop(28);
        genGenerateButton.setBounds(genRow.removeFromLeft(220));
        area.removeFromTop(10);

        const int contentH = area.getY() + 16;
        dariusGenerationContent->setSize(contentW, juce::jmax(contentH, 620));
    }
}

void DariusUI::setBackendUrl(const juce::String& url)
{
    backendUrl = url;
    if (dariusUrlEditor.getText() != url)
        dariusUrlEditor.setText(url, juce::dontSendNotification);
}

void DariusUI::setConnectionStatusText(const juce::String& text)
{
    setConnectionStatus(text, juce::Colours::lightgrey);
}

void DariusUI::setConnectionStatus(const juce::String& text, juce::Colour colour)
{
    connectionStatusText = text;
    dariusStatusLabel.setText(connectionStatusText, juce::dontSendNotification);
    dariusStatusLabel.setColour(juce::Label::textColourId, colour);
}

void DariusUI::setConnected(bool shouldBeConnected)
{
    if (connected == shouldBeConnected)
        return;

    connected = shouldBeConnected;
    refreshModelControls();
}

void DariusUI::setUsingBaseModel(bool flag)
{
    useBaseModel = flag;
    dariusUseBaseModelToggle.setToggleState(flag, juce::dontSendNotification);
    refreshModelControls();
}

void DariusUI::setFinetuneRepo(const juce::String& repoText)
{
    finetuneRepoText = repoText;
    if (dariusRepoField.getText() != repoText)
        dariusRepoField.setText(repoText, juce::dontSendNotification);
}

void DariusUI::setCheckpointSteps(const juce::Array<int>& steps)
{
    checkpointSteps = steps;
    refreshCheckpointButton();
    if (openMenuAfterFetch && !checkpointSteps.isEmpty())
    {
        openMenuAfterFetch = false;
        openCheckpointMenu();
    }
}

void DariusUI::setSelectedCheckpointStep(const juce::String& stepText)
{
    selectedCheckpointStep = stepText.trim().isEmpty() ? juce::String("latest") : stepText;
    refreshCheckpointButton();
}

void DariusUI::setIsFetchingCheckpoints(bool fetching)
{
    if (isFetchingCheckpoints == fetching)
        return;

    isFetchingCheckpoints = fetching;
    refreshCheckpointButton();
}

void DariusUI::requestOpenCheckpointMenuAfterFetch()
{
    openMenuAfterFetch = true;
}

void DariusUI::setApplyInProgress(bool applying)
{
    isApplying = applying;
    refreshModelControls();
}

void DariusUI::setWarmInProgress(bool warming)
{
    isWarming = warming;
    refreshModelControls();
}
void DariusUI::setHealthCheckInProgress(bool inProgress)
{
    healthCheckInProgress = inProgress;
    dariusHealthCheckButton.setEnabled(!inProgress);
    dariusHealthCheckButton.setButtonText(inProgress ? "checking..." : "check connection");
}


void DariusUI::startWarmDots()
{
    if (warmDotsTicking)
        return;

    warmDotsTicking = true;
    warmDots = 0;
    dariusWarmStatusLabel.setVisible(true);

    auto tick = [this]() {
        if (!warmDotsTicking)
            return;

        const juce::String dots = juce::String::repeatedString(".", (size_t)((warmDots % 3) + 1));
        dariusWarmStatusLabel.setText("warming" + dots, juce::dontSendNotification);
        ++warmDots;

        juce::Timer::callAfterDelay(300, [this]() { startWarmDots(); });
    };

    tick();
}

void DariusUI::stopWarmDots()
{
    warmDotsTicking = false;
    dariusWarmStatusLabel.setVisible(false);
    dariusWarmStatusLabel.setText("", juce::dontSendNotification);
}

void DariusUI::setModelStatus(const juce::String& size,
                              const juce::String& repo,
                              const juce::String& step,
                              bool loaded,
                              bool warm)
{
    dariusActiveSizeLabel.setText("Active size: " + (size.isEmpty() ? " " : size), juce::dontSendNotification);
    dariusRepoLabel.setText("Repo: " + (repo.isEmpty() ? " " : repo), juce::dontSendNotification);
    dariusStepLabel.setText("Step: " + (step.isEmpty() ? " " : step), juce::dontSendNotification);
    dariusLoadedLabel.setText("Loaded: " + juce::String(loaded ? "yes" : "no"), juce::dontSendNotification);
    dariusWarmupLabel.setText("Warmup: " + juce::String(warm ? "ready" : " "), juce::dontSendNotification);

    if (warm)
        stopWarmDots();
}

void DariusUI::setBpm(double bpm)
{
    genBPM = bpm;
    genBpmValueLabel.setText(juce::String(genBPM, 1), juce::dontSendNotification);
}

void DariusUI::setSavedRecordingAvailable(bool available)
{
    savedRecordingAvailable = available;
    updateGenSourceEnabled();
}

void DariusUI::setOutputAudioAvailable(bool available)
{
    outputAudioAvailable = available;
    updateGenSourceEnabled();
}

void DariusUI::setAudioSourceRecording(bool useRecording)
{
    genAudioSource = useRecording ? GenAudioSource::Recording : GenAudioSource::Output;
    updateGenSourceButtons();
    updateGenSourceEnabled();
}

void DariusUI::setGenerating(bool generating)
{
    if (genIsGenerating == generating)
        return;

    genIsGenerating = generating;
    genGenerateButton.setButtonText(generating ? "generating" : "generate");
    genGenerateButton.setEnabled(!generating);
}

void DariusUI::setCurrentSubTab(SubTab tab)
{
    currentSubTab = tab;
    updateSubTabStates();

    const bool showBackend = (currentSubTab == SubTab::Backend);
    const bool showModel = (currentSubTab == SubTab::Model);
    const bool showGeneration = (currentSubTab == SubTab::Generation);

    dariusUrlEditor.setVisible(showBackend);
    dariusUrlLabel.setVisible(showBackend);
    dariusHealthCheckButton.setVisible(showBackend);
    dariusStatusLabel.setVisible(showBackend);

    if (dariusModelViewport)
        dariusModelViewport->setVisible(showModel);
    if (dariusGenerationViewport)
        dariusGenerationViewport->setVisible(showGeneration);

    resized();
}

void DariusUI::setSteeringAssets(bool meanAvailable,
                                 int centroidCount,
                                 const std::vector<double>& centroidWeights)
{
    steeringMeanAvailable = meanAvailable;
    steeringCentroidCount = juce::jmax(0, centroidCount);
    genCentroidWeights = centroidWeights;
    if ((int)genCentroidWeights.size() < steeringCentroidCount)
        genCentroidWeights.resize((size_t)steeringCentroidCount, 0.0);
    rebuildGenCentroidRows();
    updateGenSteeringToggleText();
    resized();
}

void DariusUI::setSteeringWeights(const std::vector<double>& centroidWeights)
{
    genCentroidWeights = centroidWeights;
    const int limit = juce::jmin((int)genCentroidSliders.size(), (int)genCentroidWeights.size());
    for (int i = 0; i < limit; ++i)
        if (auto* slider = genCentroidSliders[i])
            slider->setValue(genCentroidWeights[(size_t)i], juce::dontSendNotification);
}

void DariusUI::setMeanValue(double mean)
{
    genMean = mean;
    genMeanSlider.setValue(genMean, juce::dontSendNotification);
    genMeanLabel.setText("mean: " + juce::String(genMean, 2), juce::dontSendNotification);
}

void DariusUI::setSteeringOpen(bool open)
{
    if (genSteeringOpen == open)
        return;

    genSteeringOpen = open;
    updateGenSteeringToggleText();
    resized();
}

void DariusUI::setAdvancedOpen(bool open)
{
    if (genAdvancedOpen == open)
        return;

    genAdvancedOpen = open;
    updateGenAdvancedToggleText();
    resized();
}

juce::String DariusUI::getBackendUrl() const
{
    return backendUrl;
}

bool DariusUI::getUsingBaseModel() const
{
    return useBaseModel;
}

juce::String DariusUI::getFinetuneRepo() const
{
    return finetuneRepoText;
}

juce::String DariusUI::getSelectedCheckpointStep() const
{
    return selectedCheckpointStep;
}

juce::String DariusUI::getStylesCSV() const
{
    juce::StringArray arr;
    for (auto const& row : genStyleRows)
        if (row.text)
            arr.add(row.text->getText().trim());
    return arr.joinIntoString(",");
}

juce::String DariusUI::getStyleWeightsCSV() const
{
    juce::StringArray arr;
    for (auto const& row : genStyleRows)
        if (row.weight)
            arr.add(juce::String(row.weight->getValue(), 2));
    return arr.joinIntoString(",");
}

double DariusUI::getLoopInfluence() const
{
    return genLoopInfluence;
}

double DariusUI::getTemperature() const
{
    return genTemperature;
}

int DariusUI::getTopK() const
{
    return genTopK;
}

double DariusUI::getGuidance() const
{
    return genGuidance;
}

int DariusUI::getBars() const
{
    return genBars;
}

double DariusUI::getBpm() const
{
    return genBPM;
}

bool DariusUI::getAudioSourceUsesRecording() const
{
    return genAudioSource == GenAudioSource::Recording;
}

double DariusUI::getMean() const
{
    return genMean;
}

const std::vector<double>& DariusUI::getCentroidWeights() const
{
    return genCentroidWeights;
}

void DariusUI::refreshModelControls()
{
    dariusUseBaseModelToggle.setEnabled(connected);
    dariusRefreshConfigButton.setEnabled(connected);

    const bool finetuneEnabled = connected && !useBaseModel;
    dariusRepoFieldLabel.setEnabled(finetuneEnabled);
    dariusRepoField.setEnabled(finetuneEnabled);
    dariusCheckpointButton.setEnabled(finetuneEnabled && !isFetchingCheckpoints);

    const bool canApply = connected && !isApplying && !isWarming;
    dariusApplyWarmButton.setEnabled(canApply);

    dariusModelGuardLabel.setVisible(!connected);

    refreshCheckpointButton();
}

void DariusUI::refreshCheckpointButton()
{
    juce::String label = "checkpoint: ";
    if (isFetchingCheckpoints)
        label << "loading…";
    else
        label << selectedCheckpointStep;
    label << " ▾";
    dariusCheckpointButton.setButtonText(label);
}

void DariusUI::ensureStylesRowCount(int rows)
{
    rows = juce::jlimit(1, genStylesMax, rows);
    while ((int)genStyleRows.size() < rows)
        addGenStyleRowInternal("", 1.0);
    while ((int)genStyleRows.size() > rows)
        handleRemoveStyleRow((int)genStyleRows.size() - 1);
    rebuildGenStylesUI();
    resized();
}

void DariusUI::openCheckpointMenu()
{
    if (checkpointSteps.isEmpty())
        return;

    if (onOpenCheckpointMenuRequested)
        onOpenCheckpointMenuRequested();

    juce::PopupMenu menu;
    constexpr int idLatest = 1;
    constexpr int idNone = 2;
    int nextId = 100;

    const auto isSelected = [this](const juce::String& value) {
        if (selectedCheckpointStep.isEmpty())
            return value == "latest";
        return selectedCheckpointStep == value;
    };

    menu.addItem(idLatest, "latest", true, isSelected("latest"));
    menu.addItem(idNone, "none", true, isSelected("none"));

    if (!checkpointSteps.isEmpty())
    {
        menu.addSeparator();
        for (auto step : checkpointSteps)
        {
            const juce::String stepText(step);
            menu.addItem(nextId, stepText, true, isSelected(stepText));
            ++nextId;
        }
    }

    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this, nextId](int result) {
            if (result == 0)
                return;

            juce::String chosen;
            if (result == 1)      chosen = "latest";
            else if (result == 2) chosen = "none";
            else if (result >= 100)
            {
                const int idx = result - 100;
                if (juce::isPositiveAndBelow(idx, checkpointSteps.size()))
                    chosen = juce::String(checkpointSteps[(int)idx]);
            }

            if (chosen.isEmpty())
                return;

            selectedCheckpointStep = chosen;
            refreshCheckpointButton();
            if (onCheckpointSelected)
                onCheckpointSelected(selectedCheckpointStep);
        });
}

void DariusUI::addGenStyleRowInternal(const juce::String& text, double weight)
{
    if ((int)genStyleRows.size() >= genStylesMax)
        return;

    GenStyleRow row;
    row.text = std::make_unique<juce::TextEditor>();
    row.text->setMultiLine(false);
    row.text->setReturnKeyStartsNewLine(false);
    row.text->setText(text, juce::dontSendNotification);

    row.weight = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    row.weight->setRange(0.0, 1.0, 0.01);
    row.weight->setValue(juce::jlimit(0.0, 1.0, weight), juce::dontSendNotification);

    row.remove = std::make_unique<CustomButton>();
    row.remove->setButtonText("-");
    row.remove->setButtonStyle(CustomButton::ButtonStyle::Standard);
    auto* removeBtnPtr = row.remove.get();
    row.remove->onClick = [this, removeBtnPtr]()
    {
        juce::MessageManager::callAsync([this, removeBtnPtr]()
        {
            int idx = -1;
            for (size_t i = 0; i < genStyleRows.size(); ++i)
            {
                if (genStyleRows[i].remove.get() == removeBtnPtr)
                {
                    idx = (int)i;
                    break;
                }
            }

            if (idx <= 0)
                return;

            handleRemoveStyleRow(idx);
        });
    };

    dariusGenerationContent->addAndMakeVisible(*row.text);
    dariusGenerationContent->addAndMakeVisible(*row.weight);
    dariusGenerationContent->addAndMakeVisible(*row.remove);

    genStyleRows.push_back(std::move(row));
}

void DariusUI::rebuildGenStylesUI()
{
    const bool canAdd = ((int)genStyleRows.size() < genStylesMax);
    genAddStyleButton.setEnabled(canAdd);

    for (size_t i = 0; i < genStyleRows.size(); ++i)
    {
        auto& row = genStyleRows[i];
        const bool showRemove = (genStyleRows.size() > 1) && (i > 0);

        row.remove->setVisible(showRemove);
        row.remove->setEnabled(showRemove);

        row.text->setVisible(true);
        row.weight->setVisible(true);
    }
}

void DariusUI::layoutGenStylesUI(juce::Rectangle<int>& area)
{
    const int rowH = 24;
    const int gapY = 6;
    const int textW = 180;
    const int removeW = 22;

    for (size_t i = 0; i < genStyleRows.size(); ++i)
    {
        auto slice = area.removeFromTop(rowH);
        auto& row = genStyleRows[i];
        if (!row.text || !row.weight || !row.remove)
            continue;

        auto textBounds = slice.removeFromLeft(textW);
        auto removeBounds = slice.removeFromRight(removeW);
        auto weightBounds = slice;

        row.text->setBounds(textBounds);
        row.weight->setBounds(weightBounds.reduced(4, 6));
        row.remove->setBounds(removeBounds);

        area.removeFromTop(gapY);
    }
}

void DariusUI::layoutGenAdvancedUI(juce::Rectangle<int>& area)
{
    const int labelH = 18;
    const int sliderH = 22;
    const int colW = 220;

    genTempLabel.setVisible(true);
    genTempSlider.setVisible(true);
    genTopKLabel.setVisible(true);
    genTopKSlider.setVisible(true);
    genGuidanceLabel.setVisible(true);
    genGuidanceSlider.setVisible(true);

    auto tLabel = area.removeFromTop(labelH);
    genTempLabel.setBounds(tLabel.removeFromLeft(colW));
    auto tSlide = area.removeFromTop(sliderH);
    genTempSlider.setBounds(tSlide.removeFromLeft(colW));
    area.removeFromTop(6);

    auto kLabel = area.removeFromTop(labelH);
    genTopKLabel.setBounds(kLabel.removeFromLeft(colW));
    auto kSlide = area.removeFromTop(sliderH);
    genTopKSlider.setBounds(kSlide.removeFromLeft(colW));
    area.removeFromTop(6);

    auto gLabel = area.removeFromTop(labelH);
    genGuidanceLabel.setBounds(gLabel.removeFromLeft(colW));
    auto gSlide = area.removeFromTop(sliderH);
    genGuidanceSlider.setBounds(gSlide.removeFromLeft(colW));
    area.removeFromTop(8);
}

void DariusUI::layoutGenSteeringUI(juce::Rectangle<int>& area)
{
    const int labelH = 18;
    const int sliderH = 22;
    const int colW = 220;

    if (steeringMeanAvailable)
    {
        genMeanLabel.setVisible(true);
        genMeanSlider.setVisible(true);

        auto meanLabelRow = area.removeFromTop(labelH);
        genMeanLabel.setBounds(meanLabelRow.removeFromLeft(colW));
        auto meanSliderRow = area.removeFromTop(sliderH);
        genMeanSlider.setBounds(meanSliderRow.removeFromLeft(colW));
        area.removeFromTop(6);
    }
    else
    {
        genMeanLabel.setVisible(false);
        genMeanSlider.setVisible(false);
    }

    if (steeringCentroidCount > 0 && genCentroidSliders.size() > 0)
    {
        genCentroidsHeaderLabel.setVisible(true);
        auto headerRow = area.removeFromTop(labelH);
        genCentroidsHeaderLabel.setBounds(headerRow.removeFromLeft(colW));
        area.removeFromTop(2);

        const int showCount = genCentroidSliders.size();
        for (int i = 0; i < showCount; ++i)
        {
            auto row = area.removeFromTop(sliderH);
            if (auto* label = genCentroidLabels[i]) label->setBounds(row.removeFromLeft(30));
            if (auto* slider = genCentroidSliders[i]) slider->setBounds(row.removeFromLeft(colW - 36));
            area.removeFromTop(4);
        }
        area.removeFromTop(6);
    }
    else
    {
        genCentroidsHeaderLabel.setVisible(false);
        for (auto* l : genCentroidLabels)  if (l) l->setVisible(false);
        for (auto* s : genCentroidSliders) if (s) s->setVisible(false);
    }
}

void DariusUI::updateGenLoopLabel()
{
    genLoopLabel.setText("loop influence: " + juce::String(genLoopInfluence, 2), juce::dontSendNotification);
}

void DariusUI::updateGenAdvancedToggleText()
{
    genAdvancedToggle.setButtonText("advanced");
}

void DariusUI::updateGenBarsButtons()
{
    genBars4Button.setButtonStyle(genBars == 4 ? CustomButton::ButtonStyle::Darius
                                               : CustomButton::ButtonStyle::Inactive);
    genBars8Button.setButtonStyle(genBars == 8 ? CustomButton::ButtonStyle::Darius
                                               : CustomButton::ButtonStyle::Inactive);
    genBars16Button.setButtonStyle(genBars == 16 ? CustomButton::ButtonStyle::Darius
                                                 : CustomButton::ButtonStyle::Inactive);
}

void DariusUI::updateGenSourceButtons()
{
    genRecordingButton.setButtonStyle(genAudioSource == GenAudioSource::Recording
                                          ? CustomButton::ButtonStyle::Darius
                                          : CustomButton::ButtonStyle::Inactive);
    genOutputButton.setButtonStyle(genAudioSource == GenAudioSource::Output
                                        ? CustomButton::ButtonStyle::Darius
                                        : CustomButton::ButtonStyle::Inactive);
}

void DariusUI::updateGenSourceEnabled()
{
    if (!savedRecordingAvailable && genAudioSource == GenAudioSource::Recording)
    {
        genAudioSource = GenAudioSource::Output;
        updateGenSourceButtons();
        if (onAudioSourceChanged)
            onAudioSourceChanged(false);
    }

    if (!outputAudioAvailable && genAudioSource == GenAudioSource::Output && savedRecordingAvailable)
    {
        genAudioSource = GenAudioSource::Recording;
        updateGenSourceButtons();
        if (onAudioSourceChanged)
            onAudioSourceChanged(true);
    }

    genRecordingButton.setEnabled(savedRecordingAvailable);
    genOutputButton.setEnabled(outputAudioAvailable);
    genSourceGuardLabel.setVisible(!savedRecordingAvailable);
}

void DariusUI::updateGenSteeringToggleText()
{
    if (steeringMeanAvailable || steeringCentroidCount > 0)
        genSteeringToggle.setButtonText(genSteeringOpen ? "steering" : "steering");
    else
        genSteeringToggle.setButtonText("steering");
}

void DariusUI::rebuildGenCentroidRows()
{
    for (auto* label : genCentroidLabels)
        if (label != nullptr)
            dariusGenerationContent->removeChildComponent(label);
    for (auto* slider : genCentroidSliders)
        if (slider != nullptr)
            dariusGenerationContent->removeChildComponent(slider);
    genCentroidLabels.clear();
    genCentroidSliders.clear();

    const int showCount = juce::jmin(kMaxCentroidsUI, steeringCentroidCount);
    genCentroidWeights.resize((size_t)juce::jmax(showCount, 0), 0.0);

    for (int i = 0; i < showCount; ++i)
    {
        auto* label = new juce::Label();
        label->setText("C" + juce::String(i + 1) + ":", juce::dontSendNotification);
        label->setFont(juce::FontOptions(12.0f));
        label->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        genCentroidLabels.add(label);
        dariusGenerationContent->addAndMakeVisible(label);

        auto* slider = new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        slider->setRange(0.0, 2.0, 0.01);
        if ((size_t)i < genCentroidWeights.size())
            slider->setValue(genCentroidWeights[(size_t)i], juce::dontSendNotification);
        const int idx = i;
        slider->onValueChange = [this, idx, slider]() {
            if ((size_t)idx < genCentroidWeights.size())
                genCentroidWeights[(size_t)idx] = slider->getValue();
        };
        genCentroidSliders.add(slider);
        dariusGenerationContent->addAndMakeVisible(slider);
    }
}

void DariusUI::handleCheckpointButtonClicked()
{
    if (!connected || useBaseModel)
        return;

    if (isFetchingCheckpoints)
        return;

    if (checkpointSteps.isEmpty())
    {
        if (onFetchCheckpointsRequested)
            onFetchCheckpointsRequested();
        openMenuAfterFetch = true;
        return;
    }

    openCheckpointMenu();
}

void DariusUI::handleAddStyleRow()
{
    addGenStyleRowInternal("", 1.0);
    rebuildGenStylesUI();
    resized();
}

void DariusUI::handleRemoveStyleRow(int index)
{
    if (!juce::isPositiveAndBelow(index, (int)genStyleRows.size()))
        return;

    // never remove first row
    if (index == 0)
        return;

    auto& row = genStyleRows[(size_t)index];
    juce::Component* comps[] = { row.text.get(), row.weight.get(), row.remove.get() };
    for (auto* comp : comps)
        if (comp != nullptr)
        {
            comp->setVisible(false);
            dariusGenerationContent->removeChildComponent(comp);
        }

    genStyleRows.erase(genStyleRows.begin() + index);
    rebuildGenStylesUI();
    resized();
}

void DariusUI::updateSubTabStates()
{
    dariusBackendTabButton.setButtonStyle(currentSubTab == SubTab::Backend
                                              ? CustomButton::ButtonStyle::Darius
                                              : CustomButton::ButtonStyle::Inactive);
    dariusModelTabButton.setButtonStyle(currentSubTab == SubTab::Model
                                            ? CustomButton::ButtonStyle::Darius
                                            : CustomButton::ButtonStyle::Inactive);
    dariusGenerationTabButton.setButtonStyle(currentSubTab == SubTab::Generation
                                                 ? CustomButton::ButtonStyle::Darius
                                                 : CustomButton::ButtonStyle::Inactive);
}
