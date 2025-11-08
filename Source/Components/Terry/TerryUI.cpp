#include "TerryUI.h"
#include "../../Utils/Theme.h"

namespace
{
    constexpr int kOuterMargin = 12;
}

TerryUI::TerryUI()
{
    terryLabel.setText("terry (melodyflow)", juce::dontSendNotification);
    terryLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    terryLabel.setColour(juce::Label::textColourId, Theme::Colors::TextPrimary);
    terryLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(terryLabel);

    terryVariationLabel.setText("variation", juce::dontSendNotification);
    terryVariationLabel.setFont(juce::FontOptions(12.0f));
    terryVariationLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    terryVariationLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terryVariationLabel);

    terryVariationComboBox.setTextWhenNothingSelected("choose a preset variation...");
    terryVariationComboBox.onChange = [this]()
    {
        const int selectedId = terryVariationComboBox.getSelectedId();
        const int newIndex = selectedId > 0 ? selectedId - 1 : -1;
        if (variationIndex != newIndex)
            variationIndex = newIndex;

        if (variationIndex >= 0)
        {
            const bool hadPrompt = customPrompt.trim().isNotEmpty();
            customPrompt.clear();
            terryCustomPromptEditor.setText("", false);
            if (hadPrompt && onCustomPromptChanged)
                onCustomPromptChanged(customPrompt);
        }

        if (onVariationChanged)
            onVariationChanged(variationIndex);
    };
    addAndMakeVisible(terryVariationComboBox);

    terryCustomPromptLabel.setText("custom prompt", juce::dontSendNotification);
    terryCustomPromptLabel.setFont(juce::FontOptions(12.0f));
    terryCustomPromptLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    terryCustomPromptLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terryCustomPromptLabel);

    terryCustomPromptEditor.setTextToShowWhenEmpty("or enter custom prompt...", juce::Colours::darkgrey);
    terryCustomPromptEditor.setMultiLine(false);
    terryCustomPromptEditor.setReturnKeyStartsNewLine(false);
    terryCustomPromptEditor.setScrollbarsShown(false);
    terryCustomPromptEditor.setBorder(juce::BorderSize<int>(2));
    terryCustomPromptEditor.onTextChange = [this]()
    {
        customPrompt = terryCustomPromptEditor.getText();
        const bool hasPrompt = customPrompt.trim().isNotEmpty();
        if (hasPrompt)
        {
            variationIndex = -1;
            terryVariationComboBox.setSelectedId(0, juce::dontSendNotification);
        }
        if (onCustomPromptChanged)
            onCustomPromptChanged(customPrompt);
    };
    addAndMakeVisible(terryCustomPromptEditor);

    terryFlowstepLabel.setText("flowstep", juce::dontSendNotification);
    terryFlowstepLabel.setFont(juce::FontOptions(12.0f));
    terryFlowstepLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    terryFlowstepLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terryFlowstepLabel);

    terryFlowstepSlider.setRange(0.050, 0.150, 0.001);
    terryFlowstepSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    terryFlowstepSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    terryFlowstepSlider.setNumDecimalPlacesToDisplay(3);
    terryFlowstepSlider.setValue(flowstep);
    terryFlowstepSlider.onValueChange = [this]()
    {
        flowstep = (float)terryFlowstepSlider.getValue();
        if (onFlowstepChanged)
            onFlowstepChanged(flowstep);
    };
    addAndMakeVisible(terryFlowstepSlider);

    terrySolverLabel.setText("solver", juce::dontSendNotification);
    terrySolverLabel.setFont(juce::FontOptions(12.0f));
    terrySolverLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    terrySolverLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terrySolverLabel);

    terrySolverToggle.setButtonText("use midpoint solver");
    terrySolverToggle.setToggleState(useMidpoint, juce::dontSendNotification);
    terrySolverToggle.onClick = [this]()
    {
        useMidpoint = terrySolverToggle.getToggleState();
        if (onSolverChanged)
            onSolverChanged(useMidpoint);
    };
    addAndMakeVisible(terrySolverToggle);

    terrySourceLabel.setText("transform", juce::dontSendNotification);
    terrySourceLabel.setFont(juce::FontOptions(12.0f));
    terrySourceLabel.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    terrySourceLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terrySourceLabel);

    transformRecordingButton.setButtonText("recording");
    transformRecordingButton.setRadioGroupId(1001);
    transformRecordingButton.onClick = [this]()
    {
        if (!transformRecordingButton.getToggleState())
        {
            transformRecordingButton.setToggleState(true, juce::dontSendNotification);
            transformOutputButton.setToggleState(false, juce::dontSendNotification);
        }
        audioSourceRecording = true;
        if (onAudioSourceChanged)
            onAudioSourceChanged(true);
    };
    addAndMakeVisible(transformRecordingButton);

    transformOutputButton.setButtonText("output");
    transformOutputButton.setRadioGroupId(1001);
    transformOutputButton.setToggleState(true, juce::dontSendNotification);
    transformOutputButton.onClick = [this]()
    {
        if (!transformOutputButton.getToggleState())
        {
            transformOutputButton.setToggleState(true, juce::dontSendNotification);
            transformRecordingButton.setToggleState(false, juce::dontSendNotification);
        }
        audioSourceRecording = false;
        if (onAudioSourceChanged)
            onAudioSourceChanged(false);
    };
    addAndMakeVisible(transformOutputButton);

    transformWithTerryButton.setButtonText("transform with terry");
    transformWithTerryButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
    transformWithTerryButton.setTooltip("transform selected audio source according to variation or custom prompt. max: ~40 seconds");
    transformWithTerryButton.onClick = [this]()
    {
        if (onTransform)
            onTransform();
    };
    addAndMakeVisible(transformWithTerryButton);

    undoTransformButton.setButtonText("undo transform");
    undoTransformButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    undoTransformButton.setTooltip("restore audio to state before last transformation");
    undoTransformButton.onClick = [this]()
    {
        if (onUndo)
            onUndo();
    };
    addAndMakeVisible(undoTransformButton);

    bpmLabel.setFont(juce::FontOptions(11.0f));
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
    bpmLabel.setJustificationType(juce::Justification::centredLeft);
    bpmLabel.setVisible(false);
    addAndMakeVisible(bpmLabel);

    applyEnablement(false, false, false);
    setAudioSourceAvailability(false, false);
}

void TerryUI::paint(juce::Graphics&)
{
}

void TerryUI::resized()
{
    auto terryBounds = getLocalBounds().reduced(kOuterMargin);

    juce::FlexBox column;
    column.flexDirection = juce::FlexBox::Direction::column;
    column.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    juce::FlexItem titleItem(terryLabel);
    titleItem.height = 30;
    titleItem.margin = juce::FlexItem::Margin(5, 0, 5, 0);

    juce::Component variationRowComponent;
    juce::FlexItem variationRowItem(variationRowComponent);
    variationRowItem.height = 30;
    variationRowItem.margin = juce::FlexItem::Margin(3, 0, 3, 0);

    juce::FlexItem promptLabelItem(terryCustomPromptLabel);
    promptLabelItem.height = 18;  // Changed from 15
    promptLabelItem.margin = juce::FlexItem::Margin(2, 0, 4, 0);

    juce::FlexItem promptEditorItem(terryCustomPromptEditor);
    promptEditorItem.height = 28;
    promptEditorItem.margin = juce::FlexItem::Margin(0, 5, 5, 5);

    juce::Component flowRowComponent;
    juce::FlexItem flowRowItem(flowRowComponent);
    flowRowItem.height = 30;
    flowRowItem.margin = juce::FlexItem::Margin(3, 0, 3, 0);

    juce::Component solverRowComponent;
    juce::FlexItem solverRowItem(solverRowComponent);
    solverRowItem.height = 25;
    solverRowItem.margin = juce::FlexItem::Margin(3, 0, 3, 0);

    juce::Component sourceRowComponent;
    juce::FlexItem sourceRowItem(sourceRowComponent);
    sourceRowItem.height = 25;
    sourceRowItem.margin = juce::FlexItem::Margin(3, 0, 6, 0);

    juce::FlexItem transformItem(transformWithTerryButton);
    transformItem.height = 35;
    transformItem.margin = juce::FlexItem::Margin(5, 50, 5, 50);

    juce::FlexItem undoItem(undoTransformButton);
    undoItem.height = 35;
    undoItem.margin = juce::FlexItem::Margin(5, 50, 5, 50);

    column.items.add(titleItem);
    column.items.add(variationRowItem);
    column.items.add(promptLabelItem);
    column.items.add(promptEditorItem);
    column.items.add(flowRowItem);
    column.items.add(solverRowItem);
    column.items.add(sourceRowItem);
    column.items.add(transformItem);
    column.items.add(undoItem);

    column.performLayout(terryBounds);

    titleBounds = column.items[0].currentBounds.toNearestInt();
    terryLabel.setBounds(titleBounds);

    auto variationRowBounds = column.items[1].currentBounds.toNearestInt();
    juce::FlexBox variationRow;
    variationRow.flexDirection = juce::FlexBox::Direction::row;
    variationRow.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    juce::FlexItem variationLabelItem(terryVariationLabel);
    variationLabelItem.width = 80;
    variationLabelItem.margin = juce::FlexItem::Margin(0, 5, 0, 0);
    juce::FlexItem variationComboItem(terryVariationComboBox);
    variationComboItem.flexGrow = 1;
    variationComboItem.margin = juce::FlexItem::Margin(0, 0, 0, 5);
    variationRow.items.add(variationLabelItem);
    variationRow.items.add(variationComboItem);
    variationRow.performLayout(variationRowBounds);

    auto promptLabelBounds = column.items[2].currentBounds.toNearestInt();
    terryCustomPromptLabel.setBounds(promptLabelBounds);

    auto promptEditorBounds = column.items[3].currentBounds.toNearestInt();
    terryCustomPromptEditor.setBounds(promptEditorBounds);

    auto flowRowBounds = column.items[4].currentBounds.toNearestInt();
    juce::FlexBox flowRow;
    flowRow.flexDirection = juce::FlexBox::Direction::row;
    flowRow.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    juce::FlexItem flowLabelItem(terryFlowstepLabel);
    flowLabelItem.width = 80;
    flowLabelItem.margin = juce::FlexItem::Margin(0, 5, 0, 0);
    juce::FlexItem flowSliderItem(terryFlowstepSlider);
    flowSliderItem.flexGrow = 1;
    flowSliderItem.margin = juce::FlexItem::Margin(0, 0, 0, 5);
    flowRow.items.add(flowLabelItem);
    flowRow.items.add(flowSliderItem);
    flowRow.performLayout(flowRowBounds);

    auto solverRowBounds = column.items[5].currentBounds.toNearestInt();
    juce::FlexBox solverRow;
    solverRow.flexDirection = juce::FlexBox::Direction::row;
    solverRow.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    juce::FlexItem solverLabelItem(terrySolverLabel);
    solverLabelItem.width = 80;
    solverLabelItem.margin = juce::FlexItem::Margin(0, 5, 0, 0);
    juce::FlexItem solverToggleItem(terrySolverToggle);
    solverToggleItem.flexGrow = 1;
    solverToggleItem.margin = juce::FlexItem::Margin(0, 0, 0, 5);
    solverRow.items.add(solverLabelItem);
    solverRow.items.add(solverToggleItem);
    if (bpmLabel.isVisible())
    {
        juce::FlexItem bpmFlex(bpmLabel);
        bpmFlex.width = 120;
        bpmFlex.margin = juce::FlexItem::Margin(0, 0, 0, 0);
        bpmFlex.alignSelf = juce::FlexItem::AlignSelf::center;
        solverRow.items.add(bpmFlex);
    }
    solverRow.performLayout(solverRowBounds);

    if (!bpmLabel.isVisible())
        bpmLabel.setBounds({});

    auto sourceRowBounds = column.items[6].currentBounds.toNearestInt();
    juce::FlexBox sourceRow;
    sourceRow.flexDirection = juce::FlexBox::Direction::row;
    sourceRow.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    juce::FlexItem sourceLabelItem(terrySourceLabel);
    sourceLabelItem.width = 80;
    sourceLabelItem.margin = juce::FlexItem::Margin(0, 5, 0, 0);
    juce::FlexItem recordingItem(transformRecordingButton);
    recordingItem.width = 80;
    recordingItem.margin = juce::FlexItem::Margin(0, 5, 0, 5);
    juce::FlexItem outputItem(transformOutputButton);
    outputItem.width = 80;
    outputItem.margin = juce::FlexItem::Margin(0, 0, 0, 0);
    sourceRow.items.add(sourceLabelItem);
    sourceRow.items.add(recordingItem);
    sourceRow.items.add(outputItem);
    sourceRow.performLayout(sourceRowBounds);

    auto transformBounds = column.items[7].currentBounds.toNearestInt();
    auto transformButtonArea = transformBounds.withWidth(juce::jmin(220, transformBounds.getWidth()))
        .withCentre(transformBounds.getCentre());
    transformWithTerryButton.setBounds(transformButtonArea);

    auto undoBounds = column.items[8].currentBounds.toNearestInt();
    auto undoButtonArea = undoBounds.withWidth(juce::jmin(170, undoBounds.getWidth()))
        .withCentre(undoBounds.getCentre());
    undoTransformButton.setBounds(undoButtonArea);
}

void TerryUI::setVariations(const juce::StringArray& items, int selectedIndex)
{
    terryVariationComboBox.clear(juce::dontSendNotification);
    for (int i = 0; i < items.size(); ++i)
        terryVariationComboBox.addItem(items[i], i + 1);

    variationIndex = selectedIndex;
    const int selectedId = selectedIndex >= 0 ? selectedIndex + 1 : 0;
    terryVariationComboBox.setSelectedId(selectedId, juce::dontSendNotification);
}

void TerryUI::setCustomPrompt(const juce::String& text)
{
    customPrompt = text;
    terryCustomPromptEditor.setText(text, false);
    if (text.trim().isNotEmpty())
    {
        variationIndex = -1;
        terryVariationComboBox.setSelectedId(0, juce::dontSendNotification);
    }
}

void TerryUI::setFlowstep(float v)
{
    flowstep = v;
    terryFlowstepSlider.setValue(v, juce::dontSendNotification);
}

void TerryUI::setUseMidpointSolver(bool useMidpointSolver)
{
    useMidpoint = useMidpointSolver;
    terrySolverToggle.setToggleState(useMidpoint, juce::dontSendNotification);
}

void TerryUI::setAudioSourceRecording(bool useRecording)
{
    audioSourceRecording = useRecording;
    transformRecordingButton.setToggleState(useRecording, juce::dontSendNotification);
    transformOutputButton.setToggleState(!useRecording, juce::dontSendNotification);
}

void TerryUI::setAudioSourceAvailability(bool recordingAvailable, bool outputAvailable)
{
    recordingSourceAvailable = recordingAvailable;
    outputSourceAvailable = outputAvailable;
    transformRecordingButton.setEnabled(recordingAvailable);
    transformOutputButton.setEnabled(outputAvailable);
}

void TerryUI::setButtonsEnabled(bool canTransform, bool isGenerating, bool undoAvailable)
{
    applyEnablement(canTransform, isGenerating, undoAvailable);
}

void TerryUI::setTransformButtonText(const juce::String& text)
{
    transformWithTerryButton.setButtonText(text);
}

void TerryUI::setUndoButtonText(const juce::String& text)
{
    undoTransformButton.setButtonText(text);
}

void TerryUI::setVisibleForTab(bool visible)
{
    setVisible(visible);
    setInterceptsMouseClicks(visible, visible);
}

void TerryUI::setBpm(double bpm)
{
    bpmValue = bpm;
    if (bpmValue > 0.0)
    {
        bpmLabel.setText("bpm: " + juce::String(juce::roundToInt(bpmValue)) + " (from daw)", juce::dontSendNotification);
        bpmLabel.setVisible(true);
    }
    else
    {
        bpmLabel.setText("", juce::dontSendNotification);
        bpmLabel.setVisible(false);
    }
    resized();
}

int TerryUI::getSelectedVariationIndex() const
{
    return variationIndex;
}

juce::String TerryUI::getCustomPrompt() const
{
    return customPrompt;
}

float TerryUI::getFlowstep() const
{
    return flowstep;
}

bool TerryUI::getUseMidpointSolver() const
{
    return useMidpoint;
}

bool TerryUI::getAudioSourceRecording() const
{
    return audioSourceRecording;
}

juce::Rectangle<int> TerryUI::getTitleBounds() const
{
    return titleBounds;
}

void TerryUI::applyEnablement(bool canTransform, bool isGenerating, bool undoAvailable)
{
    if (lastCanTransform == canTransform &&
        lastIsGenerating == isGenerating &&
        lastUndoAvailable == undoAvailable)
        return;

    lastCanTransform = canTransform;
    lastIsGenerating = isGenerating;
    lastUndoAvailable = undoAvailable;

    transformWithTerryButton.setEnabled(canTransform && !isGenerating);
    undoTransformButton.setEnabled(undoAvailable && !isGenerating);

    // Keep audio source buttons responsive when available
    transformRecordingButton.setEnabled(recordingSourceAvailable);
    transformOutputButton.setEnabled(outputSourceAvailable);
}
