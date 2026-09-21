// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "YueyUI.h"

namespace
{
void styleLabel(juce::Label& label, const juce::String& text, float size = 11.0f)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::FontOptions(size));
    label.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    label.setJustificationType(juce::Justification::centredLeft);
}

class TextPopoutContent final : public juce::Component
{
public:
    TextPopoutContent(const juce::String& heading,
                      const juce::String& initialText,
                      const juce::String& placeholder,
                      std::function<void(const juce::String&)> changed)
        : onChanged(std::move(changed))
    {
        styleLabel(title, heading, 13.0f);
        title.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        title.setColour(juce::Label::textColourId, Theme::Colors::TextPrimary);
        addAndMakeVisible(title);

        editor.setMultiLine(true);
        editor.setReturnKeyStartsNewLine(true);
        editor.setScrollbarsShown(true);
        editor.setPlaceholderText(placeholder);
        editor.setText(initialText, juce::dontSendNotification);
        editor.onTextChange = [this]()
        {
            if (onChanged)
                onChanged(editor.getText());
        };
        addAndMakeVisible(editor);

        closeButton.setButtonText("done");
        closeButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
        closeButton.onClick = [this]()
        {
            if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
                dialog->exitModalState(0);
        };
        addAndMakeVisible(closeButton);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        title.setBounds(area.removeFromTop(24));
        area.removeFromTop(6);
        auto buttons = area.removeFromBottom(34);
        closeButton.setBounds(buttons.removeFromRight(100));
        area.removeFromBottom(8);
        editor.setBounds(area);
    }

private:
    std::function<void(const juce::String&)> onChanged;
    juce::Label title;
    CustomTextEditor editor;
    CustomButton closeButton;
};

class LyricsPopoutContent final : public juce::Component
{
public:
    LyricsPopoutContent(const juce::String& initialText,
                        std::function<void(const juce::String&)> saved)
        : onSaved(std::move(saved))
    {
        styleLabel(title, "shared lyrics", 13.0f);
        title.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        title.setColour(juce::Label::textColourId, Theme::Colors::TextPrimary);
        addAndMakeVisible(title);

        styleLabel(hint, "shared with carey; SheetSage2 hears melody, not words");
        addAndMakeVisible(hint);

        editor.setMultiLine(true);
        editor.setReturnKeyStartsNewLine(true);
        editor.setScrollbarsShown(true);
        editor.setPlaceholderText("write the lyrics to retain or generate");
        editor.setText(initialText, juce::dontSendNotification);
        addAndMakeVisible(editor);

        cancelButton.setButtonText("cancel");
        cancelButton.onClick = [this]() { close(); };
        addAndMakeVisible(cancelButton);

        saveButton.setButtonText("save");
        saveButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
        saveButton.onClick = [this]()
        {
            if (onSaved)
                onSaved(editor.getText());
            close();
        };
        addAndMakeVisible(saveButton);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        title.setBounds(area.removeFromTop(22));
        hint.setBounds(area.removeFromTop(20));
        area.removeFromTop(6);
        auto buttons = area.removeFromBottom(34);
        saveButton.setBounds(buttons.removeFromRight(96));
        buttons.removeFromRight(8);
        cancelButton.setBounds(buttons.removeFromRight(96));
        area.removeFromBottom(8);
        editor.setBounds(area);
    }

private:
    void close()
    {
        if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
            dialog->exitModalState(0);
    }

    std::function<void(const juce::String&)> onSaved;
    juce::Label title;
    juce::Label hint;
    CustomTextEditor editor;
    CustomButton cancelButton;
    CustomButton saveButton;
};
}

YueyUI::YueyUI()
{
    setLookAndFeel(&customLookAndFeel);

    contentComponent = std::make_unique<juce::Component>();
    contentViewport = std::make_unique<juce::Viewport>();
    contentViewport->setViewedComponent(contentComponent.get(), false);
    contentViewport->setScrollBarsShown(true, false);
    customLookAndFeel.setScrollbarAccentColour(Theme::Colors::Terry);
    contentViewport->getVerticalScrollBar().setLookAndFeel(&customLookAndFeel);
    addAndMakeVisible(*contentViewport);

    titleLabel.setText("yuey (YuE2)", juce::dontSendNotification);
    titleLabel.setFont(Theme::Fonts::HeaderLarge);
    titleLabel.setColour(juce::Label::textColourId, Theme::Colors::TextPrimary);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    createSubTabButton.setButtonText("create");
    createSubTabButton.onClick = [this]() { setCurrentSubTab(SubTab::Create); };
    addAndMakeVisible(createSubTabButton);
    remixSubTabButton.setButtonText("remix");
    remixSubTabButton.onClick = [this]() { setCurrentSubTab(SubTab::Remix); };
    addAndMakeVisible(remixSubTabButton);
    continueSubTabButton.setButtonText("continue");
    continueSubTabButton.onClick = [this]() { setCurrentSubTab(SubTab::Continue); };
    addAndMakeVisible(continueSubTabButton);

    styleLabel(promptLabel, "prompt");
    addToContent(promptLabel);
    for (auto* editor : { &createPromptEditor, &remixPromptEditor, &continuePromptEditor })
    {
        editor->setMultiLine(false);
        editor->setReturnKeyStartsNewLine(false);
        editor->setScrollbarsShown(false);
        editor->setPlaceholderText("describe the song or reinterpretation");
        addToContent(*editor);
    }
    createPromptEditor.onTextChange = [this]()
    {
        if (onPromptChanged) onPromptChanged(SubTab::Create, createPromptEditor.getText());
    };
    remixPromptEditor.onTextChange = [this]()
    {
        if (onPromptChanged) onPromptChanged(SubTab::Remix, remixPromptEditor.getText());
    };
    continuePromptEditor.onTextChange = [this]()
    {
        if (onPromptChanged) onPromptChanged(SubTab::Continue, continuePromptEditor.getText());
    };

    for (auto* button : { &createPromptPopoutButton, &remixPromptPopoutButton,
                          &continuePromptPopoutButton })
    {
        button->setButtonText("");
        button->setButtonStyle(CustomButton::ButtonStyle::Terry);
        button->setTooltip("open a larger prompt editor");
        addToContent(*button);
    }
    createPromptPopoutButton.onClick = [this]() { openPromptPopout(PromptTarget::Create); };
    remixPromptPopoutButton.onClick = [this]() { openPromptPopout(PromptTarget::Remix); };
    continuePromptPopoutButton.onClick = [this]() { openPromptPopout(PromptTarget::Continue); };
    createPromptPopoutButton.onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        drawPopoutIcon(g, bounds.toFloat().reduced(2.0f),
                       createPromptPopoutButton.isMouseOver(), createPromptPopoutButton.isDown());
    };
    remixPromptPopoutButton.onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        drawPopoutIcon(g, bounds.toFloat().reduced(2.0f),
                       remixPromptPopoutButton.isMouseOver(), remixPromptPopoutButton.isDown());
    };
    continuePromptPopoutButton.onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        drawPopoutIcon(g, bounds.toFloat().reduced(2.0f),
                       continuePromptPopoutButton.isMouseOver(), continuePromptPopoutButton.isDown());
    };

    for (auto* button : { &createDiceButton, &remixDiceButton, &continueDiceButton })
    {
        button->setButtonText("");
        button->setButtonStyle(CustomButton::ButtonStyle::Terry);
        button->setTooltip("prompt dice is visible for the v1 layout; a curated Yuey prompt pool is still to come");
        button->onClick = []() {};
        addToContent(*button);
    }
    createDiceButton.onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        drawDiceIcon(g, bounds.toFloat().reduced(2.0f),
                     createDiceButton.isMouseOver(), createDiceButton.isDown());
    };
    remixDiceButton.onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        drawDiceIcon(g, bounds.toFloat().reduced(2.0f),
                     remixDiceButton.isMouseOver(), remixDiceButton.isDown());
    };
    continueDiceButton.onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        drawDiceIcon(g, bounds.toFloat().reduced(2.0f),
                     continueDiceButton.isMouseOver(), continueDiceButton.isDown());
    };

    lyricsButton.setButtonText("lyrics");
    lyricsButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
    lyricsButton.setTooltip("edit lyrics shared with carey");
    lyricsButton.onClick = [this]() { openLyricsPopout(); };
    addToContent(lyricsButton);

    instrumentalToggle.setButtonText("instrumental");
    instrumentalToggle.setTooltip("best-effort no-vocal conditioning");
    instrumentalToggle.onClick = [this]()
    {
        if (currentSubTab == SubTab::Create)
            createInstrumental = instrumentalToggle.getToggleState();
        else
            remixInstrumental = instrumentalToggle.getToggleState();
        updateInstrumentalState();
        if (onPlanningChanged) onPlanningChanged();
    };
    addToContent(instrumentalToggle);

    styleLabel(planningLabel, "plan");
    addAndMakeVisible(planningLabel);
    const juce::StringArray roots { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    for (int i = 0; i < roots.size(); ++i) keyRootComboBox.addItem(roots[i], i + 1);
    keyRootComboBox.setSelectedId(1, juce::dontSendNotification);
    keyModeComboBox.addItem("major", 1);
    keyModeComboBox.addItem("minor", 2);
    keyModeComboBox.setSelectedId(1, juce::dontSendNotification);
    const juce::StringArray meters { "4/4", "3/4", "6/8", "12/8", "5/4", "7/8" };
    for (int i = 0; i < meters.size(); ++i) meterComboBox.addItem(meters[i], i + 1);
    meterComboBox.setSelectedId(1, juce::dontSendNotification);
    for (auto* combo : { &keyRootComboBox, &keyModeComboBox, &meterComboBox })
    {
        combo->onChange = [this]() { if (onPlanningChanged) onPlanningChanged(); };
        addAndMakeVisible(*combo);
    }
    bpmControl.setRange(40.0, 300.0, 1.0);
    bpmControl.setValue(120.0, false);
    bpmControl.onValueChange = [this](double) { if (onPlanningChanged) onPlanningChanged(); };
    addAndMakeVisible(bpmControl);

    styleLabel(lengthLabel, "song length");
    addToContent(lengthLabel);
    naturalLengthButton.setButtonText("let yuey choose");
    naturalLengthButton.onClick = [this]() { selectCreateLength(false, true); };
    addToContent(naturalLengthButton);
    fixedLengthButton.setButtonText("choose bars");
    fixedLengthButton.onClick = [this]() { selectCreateLength(true, true); };
    addToContent(fixedLengthButton);

    const int barValues[] = { 4, 8, 16, 24, 32, 64 };
    for (int i = 0; i < 6; ++i)
    {
        createBarsComboBox.addItem(juce::String(barValues[i]) + " bars", barValues[i]);
        continueBarsComboBox.addItem("add " + juce::String(barValues[i]) + " bars", barValues[i]);
    }
    createBarsComboBox.setSelectedId(16, juce::dontSendNotification);
    continueBarsComboBox.setSelectedId(8, juce::dontSendNotification);
    createBarsComboBox.onChange = [this]() { if (onPlanningChanged) onPlanningChanged(); };
    continueBarsComboBox.onChange = [this]() { if (onPlanningChanged) onPlanningChanged(); };
    addToContent(createBarsComboBox);
    addToContent(continueBarsComboBox);

    styleLabel(sourceLabel, "source");
    addToContent(sourceLabel);
    recordingSourceButton.setButtonText("recording");
    recordingSourceButton.setRadioGroupId(4101);
    recordingSourceButton.setColour(juce::ToggleButton::tickColourId, Theme::Colors::Terry);
    recordingSourceButton.onClick = [this]()
    {
        if (!recordingSourceButton.getToggleState())
        {
            recordingSourceButton.setToggleState(true, juce::dontSendNotification);
            outputSourceButton.setToggleState(false, juce::dontSendNotification);
        }
        audioSourceRecording = true;
        if (onAudioSourceChanged)
            onAudioSourceChanged(true);
    };
    addToContent(recordingSourceButton);
    outputSourceButton.setButtonText("output");
    outputSourceButton.setRadioGroupId(4101);
    outputSourceButton.setColour(juce::ToggleButton::tickColourId, Theme::Colors::Terry);
    outputSourceButton.setToggleState(true, juce::dontSendNotification);
    outputSourceButton.onClick = [this]()
    {
        if (!outputSourceButton.getToggleState())
        {
            outputSourceButton.setToggleState(true, juce::dontSendNotification);
            recordingSourceButton.setToggleState(false, juce::dontSendNotification);
        }
        audioSourceRecording = false;
        if (onAudioSourceChanged)
            onAudioSourceChanged(false);
    };
    addToContent(outputSourceButton);

    styleLabel(continuationMethodLabel, "continuation method");
    addToContent(continuationMethodLabel);
    scoreContinuationButton.setButtonText("score continuation");
    scoreContinuationButton.setTooltip("transcribe the score, then render a fresh continuation without a real-audio codec round trip");
    scoreContinuationButton.onClick = [this]() { setContinuationMethod(ContinuationMethod::Score); };
    addToContent(scoreContinuationButton);
    audioContinuationButton.setButtonText("audio continuation");
    audioContinuationButton.setTooltip("carry semantic audio forward for stronger continuity; the complete result is reconstructed and may lose fidelity");
    audioContinuationButton.onClick = [this]() { setContinuationMethod(ContinuationMethod::Audio); };
    addToContent(audioContinuationButton);

    styleLabel(transcriptionLabel, "transcription");
    addToContent(transcriptionLabel);
    transcriptionModeComboBox.addItem("melody + chords", 1);
    transcriptionModeComboBox.addItem("full score + chords", 2);
    transcriptionModeComboBox.setSelectedId(1, juce::dontSendNotification);
    transcriptionModeComboBox.onChange = [this]() { if (onPlanningChanged) onPlanningChanged(); };
    addToContent(transcriptionModeComboBox);

    styleLabel(continueLengthLabel, "continuation length");
    addToContent(continueLengthLabel);
    continueNaturalButton.setButtonText("let yuey choose");
    continueNaturalButton.onClick = [this]() { selectContinueLength(false, true); };
    addToContent(continueNaturalButton);
    continueFixedButton.setButtonText("add bars");
    continueFixedButton.onClick = [this]() { selectContinueLength(true, true); };
    addToContent(continueFixedButton);

    actionButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
    actionButton.onClick = [this]()
    {
        if (currentSubTab == SubTab::Create)
        {
            if (onCreate) onCreate();
        }
        else if (currentSubTab == SubTab::Remix)
        {
            if (onRemix) onRemix();
        }
        else
        {
            if (onContinue) onContinue();
        }
    };
    addToContent(actionButton);

    styleLabel(infoLabel, "create plans a score; remix and continue begin with SheetSage2 transcription", 10.0f);
    infoLabel.setJustificationType(juce::Justification::centred);
    addToContent(infoLabel);

    updateSubTabState();
    updateSourceState();
    setGenerateButtonEnabled(false, false, false, false);
    // Generic wording until a backend tells us its ceiling.
    setNaturalLengthCeiling(-1.0);
}

YueyUI::~YueyUI()
{
    closeAuxiliaryWindows();
    if (contentViewport)
        contentViewport->getVerticalScrollBar().setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void YueyUI::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colors::Background);
}

void YueyUI::resized()
{
    auto chrome = getLocalBounds().reduced(8, 4);
    titleBounds = chrome.removeFromTop(24);
    titleLabel.setBounds(titleBounds);
    chrome.removeFromTop(4);

    auto tabs = chrome.removeFromTop(28);
    const int tabWidth = tabs.getWidth() / 3;
    createSubTabButton.setBounds(tabs.removeFromLeft(tabWidth).reduced(2));
    remixSubTabButton.setBounds(tabs.removeFromLeft(tabWidth).reduced(2));
    continueSubTabButton.setBounds(tabs.reduced(2));
    chrome.removeFromTop(5);

    const bool showPlanning = currentSubTab == SubTab::Create;
    planningLabel.setVisible(showPlanning);
    keyRootComboBox.setVisible(showPlanning);
    keyModeComboBox.setVisible(showPlanning);
    meterComboBox.setVisible(showPlanning);
    bpmControl.setVisible(showPlanning);
    if (showPlanning)
    {
        planningLabel.setBounds(chrome.removeFromTop(16));
        auto planRow = chrome.removeFromTop(30);
        const int quarter = planRow.getWidth() / 4;
        keyRootComboBox.setBounds(planRow.removeFromLeft(quarter).reduced(2));
        keyModeComboBox.setBounds(planRow.removeFromLeft(quarter).reduced(2));
        meterComboBox.setBounds(planRow.removeFromLeft(quarter).reduced(2));
        bpmControl.setBounds(planRow.reduced(2));
        chrome.removeFromTop(5);
    }

    contentViewport->setBounds(chrome);
    const int width = juce::jmax(280, contentViewport->getWidth() - contentViewport->getScrollBarThickness());
    const bool continueTab = currentSubTab == SubTab::Continue;
    const bool scoreContinuation = continueTab && continuationMethod == ContinuationMethod::Score;
    const int height = currentSubTab == SubTab::Create ? 240
        : currentSubTab == SubTab::Remix ? 265
        : scoreContinuation ? 360 : 315;
    contentComponent->setSize(width, height);

    auto area = contentComponent->getLocalBounds().reduced(8, 4);
    promptLabel.setBounds(area.removeFromTop(16));
    auto promptRow = area.removeFromTop(32);
    auto dice = promptRow.removeFromRight(22).withHeight(22).withY(promptRow.getY() + 2);
    promptRow.removeFromRight(3);
    auto pop = promptRow.removeFromLeft(22).withHeight(22).withY(promptRow.getY() + 2);
    promptRow.removeFromLeft(3);
    createPromptEditor.setBounds(promptRow.reduced(2));
    remixPromptEditor.setBounds(promptRow.reduced(2));
    continuePromptEditor.setBounds(promptRow.reduced(2));
    createPromptPopoutButton.setBounds(pop);
    remixPromptPopoutButton.setBounds(pop);
    continuePromptPopoutButton.setBounds(pop);
    createDiceButton.setBounds(dice);
    remixDiceButton.setBounds(dice);
    continueDiceButton.setBounds(dice);

    area.removeFromTop(4);
    auto vocalRow = area.removeFromTop(28);
    lyricsButton.setBounds(vocalRow.removeFromLeft(64).withHeight(24).withY(vocalRow.getY() + 2));
    vocalRow.removeFromLeft(4);
    instrumentalToggle.setBounds(vocalRow.removeFromLeft(135).reduced(4, 0));
    area.removeFromTop(4);

    if (currentSubTab == SubTab::Create)
    {
        lengthLabel.setBounds(area.removeFromTop(16));
        auto lengthRow = area.removeFromTop(30);
        naturalLengthButton.setBounds(lengthRow.removeFromLeft(128).reduced(2));
        fixedLengthButton.setBounds(lengthRow.removeFromLeft(100).reduced(2));
        createBarsComboBox.setBounds(lengthRow.reduced(2));
    }
    else
    {
        sourceLabel.setBounds(area.removeFromTop(16));
        auto sourceRow = area.removeFromTop(30);
        recordingSourceButton.setBounds(sourceRow.removeFromLeft(104));
        outputSourceButton.setBounds(sourceRow.removeFromLeft(88));
        if (continueTab)
        {
            area.removeFromTop(3);
            continuationMethodLabel.setBounds(area.removeFromTop(16));
            auto methodRow = area.removeFromTop(30);
            scoreContinuationButton.setBounds(methodRow.removeFromLeft(methodRow.getWidth() / 2).reduced(2));
            audioContinuationButton.setBounds(methodRow.reduced(2));
        }
        if (currentSubTab == SubTab::Remix || scoreContinuation)
        {
            area.removeFromTop(3);
            transcriptionLabel.setBounds(area.removeFromTop(16));
            transcriptionModeComboBox.setBounds(area.removeFromTop(30).reduced(2));
        }
        if (continueTab)
        {
            area.removeFromTop(3);
            continueLengthLabel.setBounds(area.removeFromTop(16));
            auto lengthRow = area.removeFromTop(30);
            continueNaturalButton.setBounds(lengthRow.removeFromLeft(128).reduced(2));
            continueFixedButton.setBounds(lengthRow.removeFromLeft(88).reduced(2));
            continueBarsComboBox.setBounds(lengthRow.reduced(2));
        }
    }

    area.removeFromTop(8);
    actionButton.setBounds(area.removeFromTop(36).reduced(2));
    infoLabel.setBounds(area.removeFromTop(28));
}

void YueyUI::addToContent(juce::Component& component)
{
    contentComponent->addAndMakeVisible(component);
}

void YueyUI::setCurrentSubTab(SubTab tab)
{
    if (currentSubTab == tab)
        return;
    currentSubTab = tab;
    updateSubTabState();
    if (onSubTabChanged) onSubTabChanged(tab);
}

void YueyUI::setContinuationMethod(ContinuationMethod method)
{
    if (continuationMethod == method)
        return;
    continuationMethod = method;
    updateSubTabState();
    if (onContinuationMethodChanged) onContinuationMethodChanged(method);
}

void YueyUI::updateSubTabState()
{
    const bool create = currentSubTab == SubTab::Create;
    const bool remix = currentSubTab == SubTab::Remix;
    const bool continuing = currentSubTab == SubTab::Continue;
    const bool showTranscription = remix
        || (continuing && continuationMethod == ContinuationMethod::Score);
    createSubTabButton.setButtonStyle(create ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
    remixSubTabButton.setButtonStyle(remix ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
    continueSubTabButton.setButtonStyle(continuing ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
    createPromptEditor.setVisible(create);
    createPromptPopoutButton.setVisible(create);
    createDiceButton.setVisible(create);
    remixPromptEditor.setVisible(remix);
    remixPromptPopoutButton.setVisible(remix);
    remixDiceButton.setVisible(remix);
    continuePromptEditor.setVisible(continuing);
    continuePromptPopoutButton.setVisible(continuing);
    continueDiceButton.setVisible(continuing);

    juce::Component* createComponents[] = {
        &planningLabel, &keyRootComboBox, &keyModeComboBox, &meterComboBox,
        &bpmControl, &lengthLabel, &naturalLengthButton, &fixedLengthButton,
        &createBarsComboBox
    };
    for (auto* component : createComponents)
        component->setVisible(create);
    juce::Component* remixComponents[] = {
        &sourceLabel, &recordingSourceButton, &outputSourceButton,
        &transcriptionLabel, &transcriptionModeComboBox, &continuationMethodLabel,
        &scoreContinuationButton, &audioContinuationButton, &continueLengthLabel,
        &continueNaturalButton, &continueFixedButton, &continueBarsComboBox
    };
    for (auto* component : remixComponents)
        component->setVisible(!create);

    transcriptionLabel.setVisible(showTranscription);
    transcriptionModeComboBox.setVisible(showTranscription);
    scoreContinuationButton.setButtonStyle(continuationMethod == ContinuationMethod::Score
        ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
    audioContinuationButton.setButtonStyle(continuationMethod == ContinuationMethod::Audio
        ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
    instrumentalToggle.setToggleState(create ? createInstrumental : remixInstrumental,
                                       juce::dontSendNotification);
    actionButton.setButtonText(generating ? "working..."
        : create ? "create with yuey"
        : continuing
            ? (continuationMethod == ContinuationMethod::Audio
                ? "continue from audio" : "continue from score")
            : "transcribe + remix");
    infoLabel.setText(create
        ? "create plans a score before rendering audio"
        : remix
            ? "remix transcribes the source score, then renders a new performance"
            : continuationMethod == ContinuationMethod::Audio
                ? "stronger continuity; reconstructs the source and may reduce fidelity"
                : "default: transcribes and extends the score without codec loss",
        juce::dontSendNotification);
    actionButton.setEnabled(!generating
        && (create ? canCreate : remix ? canRemix : canContinue));
    updateLengthState();
    updateInstrumentalState();
    updateSourceState();
    resized();
    repaint();
}

void YueyUI::updateLengthState()
{
    naturalLengthButton.setButtonStyle(!createFixedBars ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
    fixedLengthButton.setButtonStyle(createFixedBars ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
    createBarsComboBox.setVisible(currentSubTab == SubTab::Create && createFixedBars);

    continueNaturalButton.setButtonStyle(!continueFixedBars ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
    continueFixedButton.setButtonStyle(continueFixedBars ? CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
    const bool continuing = currentSubTab == SubTab::Continue;
    continuationMethodLabel.setVisible(continuing);
    scoreContinuationButton.setVisible(continuing);
    audioContinuationButton.setVisible(continuing);
    continueLengthLabel.setVisible(continuing);
    continueNaturalButton.setVisible(continuing);
    continueFixedButton.setVisible(continuing);
    continueBarsComboBox.setVisible(continuing && continueFixedBars);
}

void YueyUI::updateInstrumentalState()
{
    const bool instrumental = currentSubTab == SubTab::Create ? createInstrumental : remixInstrumental;
    lyricsButton.setVisible(!instrumental);
    instrumentalToggle.setToggleState(instrumental, juce::dontSendNotification);
}

void YueyUI::updateSourceState()
{
    recordingSourceButton.setEnabled(recordingSourceAvailable);
    outputSourceButton.setEnabled(outputSourceAvailable);
    recordingSourceButton.setToggleState(audioSourceRecording, juce::dontSendNotification);
    outputSourceButton.setToggleState(!audioSourceRecording, juce::dontSendNotification);
}

void YueyUI::drawDiceIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                          bool isHovered, bool isPressed)
{
    juce::Colour background = Theme::Colors::Terry.withAlpha(0.9f);
    if (isPressed)
        background = Theme::Colors::Terry.brighter(0.2f);
    else if (isHovered)
        background = Theme::Colors::Terry.brighter(0.3f);

    g.setColour(background);
    g.fillRoundedRectangle(bounds, 2.0f);

    const float pipRadius = bounds.getWidth() * 0.1f;
    const float offset = bounds.getWidth() * 0.25f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    g.setColour(juce::Colours::white);
    const auto drawPip = [&g, pipRadius](float x, float y)
    {
        g.fillEllipse(x - pipRadius, y - pipRadius, pipRadius * 2.0f, pipRadius * 2.0f);
    };
    drawPip(cx, cy);
    drawPip(cx - offset, cy - offset);
    drawPip(cx + offset, cy - offset);
    drawPip(cx - offset, cy + offset);
    drawPip(cx + offset, cy + offset);
}

void YueyUI::drawPopoutIcon(juce::Graphics& g, juce::Rectangle<float> bounds,
                            bool isHovered, bool isPressed)
{
    juce::Colour background = Theme::Colors::Terry.withAlpha(0.9f);
    if (isPressed)
        background = Theme::Colors::Terry.brighter(0.2f);
    else if (isHovered)
        background = Theme::Colors::Terry.brighter(0.3f);

    g.setColour(background);
    g.fillRoundedRectangle(bounds, 2.0f);
    g.setColour(juce::Colours::white);

    const auto inner = bounds.reduced(5.0f);
    const auto box = inner.withTrimmedTop(inner.getHeight() * 0.28f)
                          .withTrimmedRight(inner.getWidth() * 0.28f);
    g.drawRect(box, 1.4f);

    juce::Path arrow;
    const auto start = inner.getBottomLeft()
        + juce::Point<float>(inner.getWidth() * 0.38f, -inner.getHeight() * 0.38f);
    const auto end = inner.getTopRight();
    arrow.startNewSubPath(start);
    arrow.lineTo(end);
    arrow.lineTo(end.translated(-inner.getWidth() * 0.34f, 0.0f));
    arrow.startNewSubPath(end);
    arrow.lineTo(end.translated(0.0f, inner.getHeight() * 0.34f));
    g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
}

void YueyUI::setCreatePrompt(const juce::String& text)
{
    createPromptEditor.setText(text, juce::dontSendNotification);
}

void YueyUI::setRemixPrompt(const juce::String& text)
{
    remixPromptEditor.setText(text, juce::dontSendNotification);
}

void YueyUI::setContinuePrompt(const juce::String& text)
{
    continuePromptEditor.setText(text, juce::dontSendNotification);
}

void YueyUI::setLyricsText(const juce::String& text)
{
    lyricsText = text;
    updateLyricsButton();
}

void YueyUI::setCreateInstrumental(bool enabled)
{
    createInstrumental = enabled;
    updateInstrumentalState();
}

void YueyUI::setRemixInstrumental(bool enabled)
{
    remixInstrumental = enabled;
    updateInstrumentalState();
}

void YueyUI::setBpm(double bpm)
{
    bpmControl.setValue(juce::jlimit(40.0, 300.0, bpm), false);
}

juce::String YueyUI::getKey() const
{
    return keyRootComboBox.getText().trim() + " " + keyModeComboBox.getText().trim().toLowerCase();
}

void YueyUI::setKey(const juce::String& key)
{
    const auto clean = key.trim();
    const bool minor = clean.endsWithIgnoreCase("minor") || clean.endsWith("m");
    auto root = clean.upToFirstOccurrenceOf(" ", false, false).trim();
    if (root.endsWith("m")) root = root.dropLastCharacters(1);
    for (int i = 0; i < keyRootComboBox.getNumItems(); ++i)
        if (keyRootComboBox.getItemText(i).equalsIgnoreCase(root))
            keyRootComboBox.setSelectedItemIndex(i, juce::dontSendNotification);
    keyModeComboBox.setSelectedId(minor ? 2 : 1, juce::dontSendNotification);
}

juce::String YueyUI::getMeter() const
{
    return meterComboBox.getText().trim();
}

void YueyUI::setMeter(const juce::String& meter)
{
    for (int i = 0; i < meterComboBox.getNumItems(); ++i)
        if (meterComboBox.getItemText(i) == meter.trim())
            meterComboBox.setSelectedItemIndex(i, juce::dontSendNotification);
}

int YueyUI::selectedNumber(const CustomComboBox& combo, int fallback)
{
    const int value = combo.getSelectedId();
    return value > 0 ? value : fallback;
}

int YueyUI::getCreateBars() const { return selectedNumber(createBarsComboBox, 16); }
int YueyUI::getContinueBars() const { return selectedNumber(continueBarsComboBox, 8); }

void YueyUI::setCreateLength(bool fixedBars, int bars)
{
    createFixedBars = fixedBars;
    createBarsComboBox.setSelectedId(bars, juce::dontSendNotification);
    updateLengthState();
}

void YueyUI::setContinueLength(bool fixedBars, int bars)
{
    continueFixedBars = fixedBars;
    continueBarsComboBox.setSelectedId(bars, juce::dontSendNotification);
    updateLengthState();
}

juce::String YueyUI::getTranscriptionMode() const
{
    return transcriptionModeComboBox.getSelectedId() == 2 ? "full" : "melody";
}

void YueyUI::setTranscriptionMode(const juce::String& mode)
{
    transcriptionModeComboBox.setSelectedId(mode.equalsIgnoreCase("full") ? 2 : 1,
                                             juce::dontSendNotification);
}

void YueyUI::setAudioSourceRecording(bool recording)
{
    // Programmatic synchronization is deliberately notification-free. The
    // editor fans the shared source state out to Terry, SA3, Darius, and Yuey;
    // calling back from here would recursively re-enter that fan-out.
    audioSourceRecording = recording;
    updateSourceState();
}

void YueyUI::setAudioSourceAvailability(bool recordingAvailable, bool outputAvailable)
{
    recordingSourceAvailable = recordingAvailable;
    outputSourceAvailable = outputAvailable;
    updateSourceState();
}

void YueyUI::setGenerateButtonEnabled(bool createEnabled, bool remixEnabled,
                                      bool continueEnabled, bool isGenerating)
{
    canCreate = createEnabled;
    canRemix = remixEnabled;
    canContinue = continueEnabled;
    generating = isGenerating;
    updateSubTabState();
}

void YueyUI::selectCreateLength(bool fixedBars, bool notify)
{
    createFixedBars = fixedBars;
    updateLengthState();
    resized();
    if (notify && onPlanningChanged) onPlanningChanged();
}

void YueyUI::selectContinueLength(bool fixedBars, bool notify)
{
    continueFixedBars = fixedBars;
    updateLengthState();
    resized();
    if (notify && onPlanningChanged) onPlanningChanged();
}

void YueyUI::openPromptPopout(PromptTarget target)
{
    juce::Component::SafePointer<YueyUI> safeThis(this);
    auto* source = target == PromptTarget::Create ? &createPromptEditor
        : target == PromptTarget::Remix ? &remixPromptEditor : &continuePromptEditor;
    const auto heading = target == PromptTarget::Create ? juce::String("create prompt")
        : target == PromptTarget::Remix ? juce::String("remix prompt")
        : juce::String("continuation prompt");
    auto* content = new TextPopoutContent(
        heading,
        source->getText(), "describe the music in detail",
        [safeThis, target](const juce::String& text)
        {
            if (safeThis == nullptr) return;
            auto& editor = target == PromptTarget::Create ? safeThis->createPromptEditor
                : target == PromptTarget::Remix ? safeThis->remixPromptEditor
                : safeThis->continuePromptEditor;
            if (editor.getText() != text)
                editor.setText(text, juce::sendNotification);
        });

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(content);
    options.content->setSize(560, 300);
    options.dialogTitle = "yuey prompt";
    options.dialogBackgroundColour = juce::Colour(0xff1e1e1e);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.useBottomRightCornerResizer = true;
    options.componentToCentreAround = this;
    if (auto* window = options.launchAsync())
    {
        auxiliaryWindows.emplace_back(window);
        window->setResizeLimits(380, 220, 1000, 800);
    }
}

void YueyUI::openLyricsPopout()
{
    juce::Component::SafePointer<YueyUI> safeThis(this);
    auto* content = new LyricsPopoutContent(lyricsText, [safeThis](const juce::String& text)
    {
        if (safeThis == nullptr) return;
        safeThis->lyricsText = text;
        safeThis->updateLyricsButton();
        if (safeThis->onLyricsChanged) safeThis->onLyricsChanged(text);
    });

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(content);
    options.content->setSize(560, 360);
    options.dialogTitle = "shared lyrics";
    options.dialogBackgroundColour = juce::Colour(0xff1e1e1e);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.useBottomRightCornerResizer = true;
    options.componentToCentreAround = this;
    if (auto* window = options.launchAsync())
    {
        auxiliaryWindows.emplace_back(window);
        window->setResizeLimits(420, 260, 1000, 900);
    }
}

void YueyUI::updateLyricsButton()
{
    lyricsButton.setButtonText(lyricsText.trim().isEmpty() ? "lyrics" : "lyrics*");
}

void YueyUI::closeAuxiliaryWindows()
{
    for (auto& window : auxiliaryWindows)
        if (window != nullptr) window->exitModalState(0);
    auxiliaryWindows.clear();
}

void YueyUI::setNaturalLengthCeiling(double seconds)
{
    naturalLengthCeiling = seconds;

    const auto clock = [](double value)
    {
        const int whole = juce::roundToInt(value);
        return juce::String(whole / 60) + ":" + juce::String(whole % 60).paddedLeft('0', 2);
    };

    juce::String createTip = "yuey picks the form and the length";
    juce::String continueTip = "yuey picks how far to carry the source";

    if (seconds > 0.0)
    {
        // The ceiling holds the score, not the audio, so a capped song still
        // ends on its own terms rather than stopping partway.
        createTip += ", up to " + clock(seconds) + " on this backend";
        // A continuation's length comes from the audio it extends, so the
        // create ceiling deliberately does not apply to it.
        continueTip += "; a continuation isn't held to the " + clock(seconds) + " limit";
    }
    else if (seconds == 0.0)
    {
        createTip += ", with no limit on this backend";
    }

    naturalLengthButton.setTooltip(createTip);
    continueNaturalButton.setTooltip(continueTip);
}

void YueyUI::applyPlanMetadata(const juce::String& abc, bool adoptTempo)
{
    juce::StringArray lines;
    lines.addLines(abc);
    for (const auto& raw : lines)
    {
        const auto line = raw.trim();
        if (line.startsWith("Q:"))
        {
            const auto rhs = line.fromFirstOccurrenceOf("=", false, false).trim();
            if (adoptTempo && rhs.containsOnly("0123456789.")) setBpm(rhs.getDoubleValue());
        }
        else if (line.startsWith("M:"))
        {
            setMeter(line.substring(2).trim());
        }
        else if (line.startsWith("K:"))
        {
            auto key = line.substring(2).trim();
            const bool minor = key.endsWith("m");
            if (minor) key = key.dropLastCharacters(1) + " minor";
            else key += " major";
            setKey(key);
        }
    }
}
