/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Gary4juceAudioProcessorEditor::Gary4juceAudioProcessorEditor(Gary4juceAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
    cropButton("Crop", juce::DrawableButton::ImageFitted)

{
    setSize(400, 850);  // Made taller to accommodate controls

    // Check initial backend connection status
    isConnected = audioProcessor.isBackendConnected();
    DBG("Editor created, backend connection status: " + juce::String(isConnected ? "Connected" : "Disconnected"));

    // ========== TAB BUTTONS SETUP ==========
    garyTabButton.setButtonText("Gary");
    garyTabButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    garyTabButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    garyTabButton.onClick = [this]() { switchToTab(ModelTab::Gary); };
    addAndMakeVisible(garyTabButton);

    jerryTabButton.setButtonText("Jerry");
    jerryTabButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    jerryTabButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    jerryTabButton.onClick = [this]() { switchToTab(ModelTab::Jerry); };
    addAndMakeVisible(jerryTabButton);

    terryTabButton.setButtonText("Terry");
    terryTabButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    terryTabButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    terryTabButton.onClick = [this]() { switchToTab(ModelTab::Terry); };
    // terryTabButton.setEnabled(false);  // Disabled until implemented
    addAndMakeVisible(terryTabButton);

    // ========== GARY CONTROLS SETUP ==========
    garyLabel.setText("Gary (MusicGen)", juce::dontSendNotification);
    garyLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    garyLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    garyLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(garyLabel);

    // Prompt duration slider (1-15 seconds)
    promptDurationSlider.setRange(1.0, 15.0, 1.0);
    promptDurationSlider.setValue(6.0);
    promptDurationSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    promptDurationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    promptDurationSlider.setTextValueSuffix("s");
    promptDurationSlider.onValueChange = [this]() {
        currentPromptDuration = (float)promptDurationSlider.getValue();
    };
    addAndMakeVisible(promptDurationSlider);

    promptDurationLabel.setText("Prompt Duration", juce::dontSendNotification);
    promptDurationLabel.setFont(juce::FontOptions(12.0f));
    promptDurationLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    promptDurationLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(promptDurationLabel);

    // Model selection dropdown
    modelComboBox.addItem("vanya_ai_dnb_0.1", 1);
    modelComboBox.addItem("bleeps-medium", 2);
    modelComboBox.addItem("gary_orchestra_2", 3);
    modelComboBox.addItem("hoenn_lofi", 4);
    modelComboBox.setSelectedId(1);
    modelComboBox.onChange = [this]() {
        currentModelIndex = modelComboBox.getSelectedId() - 1;
    };
    addAndMakeVisible(modelComboBox);

    modelLabel.setText("Model", juce::dontSendNotification);
    modelLabel.setFont(juce::FontOptions(12.0f));
    modelLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    modelLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(modelLabel);

    // Send to Gary button
    sendToGaryButton.setButtonText("Send to Gary");
    sendToGaryButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    sendToGaryButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    sendToGaryButton.onClick = [this]() { sendToGary(); };
    sendToGaryButton.setEnabled(false); // Initially disabled until we have audio
    addAndMakeVisible(sendToGaryButton);

    // Continue button for Gary
    continueButton.setButtonText("Continue");
    continueButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkblue);
    continueButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    continueButton.onClick = [this]() { continueMusic(); };
    continueButton.setEnabled(false); // Initially disabled
    addAndMakeVisible(continueButton);

    // ========== JERRY CONTROLS SETUP ==========
    jerryLabel.setText("Jerry (Stable Audio)", juce::dontSendNotification);
    jerryLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    jerryLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    jerryLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(jerryLabel);

    // Text prompt editor
    jerryPromptEditor.setMultiLine(true);
    jerryPromptEditor.setReturnKeyStartsNewLine(true);
    jerryPromptEditor.setTextToShowWhenEmpty("Enter your audio generation prompt here...", juce::Colours::darkgrey);
    jerryPromptEditor.onTextChange = [this]() {
        currentJerryPrompt = jerryPromptEditor.getText();
        // Update button state whenever prompt changes
        generateWithJerryButton.setEnabled(isConnected && !currentJerryPrompt.trim().isEmpty());
        };
    addAndMakeVisible(jerryPromptEditor);

    jerryPromptLabel.setText("Text Prompt", juce::dontSendNotification);
    jerryPromptLabel.setFont(juce::FontOptions(12.0f));
    jerryPromptLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    jerryPromptLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(jerryPromptLabel);

    // CFG slider (0.5 to 2.0 in 0.1 increments)
    jerryCfgSlider.setRange(0.5, 2.0, 0.1);
    jerryCfgSlider.setValue(1.0);
    jerryCfgSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    jerryCfgSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    jerryCfgSlider.onValueChange = [this]() {
        currentJerryCfg = (float)jerryCfgSlider.getValue();
        };
    addAndMakeVisible(jerryCfgSlider);

    jerryCfgLabel.setText("CFG Scale", juce::dontSendNotification);
    jerryCfgLabel.setFont(juce::FontOptions(12.0f));
    jerryCfgLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    jerryCfgLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(jerryCfgLabel);

    // Steps slider (4 to 16 in increments of 1)
    jerryStepsSlider.setRange(4.0, 16.0, 1.0);
    jerryStepsSlider.setValue(8.0);
    jerryStepsSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    jerryStepsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    jerryStepsSlider.onValueChange = [this]() {
        currentJerrySteps = (int)jerryStepsSlider.getValue();
        };
    addAndMakeVisible(jerryStepsSlider);

    jerryStepsLabel.setText("Steps", juce::dontSendNotification);
    jerryStepsLabel.setFont(juce::FontOptions(12.0f));
    jerryStepsLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    jerryStepsLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(jerryStepsLabel);

    // BPM info label (will show DAW BPM)
    jerryBpmLabel.setText("BPM: " + juce::String((int)audioProcessor.getCurrentBPM()),
        juce::dontSendNotification);
    jerryBpmLabel.setFont(juce::FontOptions(11.0f));
    jerryBpmLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    jerryBpmLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(jerryBpmLabel);

    // Generate with Jerry button
    generateWithJerryButton.setButtonText("Generate with Jerry");
    generateWithJerryButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
    generateWithJerryButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    generateWithJerryButton.onClick = [this]() { sendToJerry(); };
    generateWithJerryButton.setEnabled(false); // Initially disabled until connected
    addAndMakeVisible(generateWithJerryButton);

    // Smart loop button (rectangular toggle style) - HORIZONTAL LAYOUT
    generateAsLoopButton.setButtonText("smart loop");  // SHORTER text for horizontal layout
    generateAsLoopButton.setClickingTogglesState(true);
    generateAsLoopButton.onClick = [this]() {
        generateAsLoop = generateAsLoopButton.getToggleState();
        updateLoopTypeVisibility();  // Show/hide the three type buttons
        styleSmartLoopButton();      // Update visual styling
        };
    addAndMakeVisible(generateAsLoopButton);

    // Loop type buttons (compact rectangular buttons for horizontal layout)
    loopTypeAutoButton.setButtonText("Auto");
    loopTypeAutoButton.onClick = [this]() { setLoopType("auto"); };

    loopTypeDrumsButton.setButtonText("Drums");
    loopTypeDrumsButton.onClick = [this]() { setLoopType("drums"); };

    loopTypeInstrumentsButton.setButtonText("Instr");  // SHORTENED for compact layout
    loopTypeInstrumentsButton.onClick = [this]() { setLoopType("instruments"); };

    addAndMakeVisible(loopTypeAutoButton);
    addAndMakeVisible(loopTypeDrumsButton);
    addAndMakeVisible(loopTypeInstrumentsButton);

    // Initial styling and state
    styleSmartLoopButton();
    setLoopType("auto");  // This will style the segmented control
    updateLoopTypeVisibility();  // Initially hide loop type controls

    // Make buttons perfectly rectangular (remove default JUCE rounding)
    generateAsLoopButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    loopTypeAutoButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    loopTypeDrumsButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    loopTypeInstrumentsButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);

    // ========== TERRY CONTROLS SETUP ==========
    terryLabel.setText("Terry (MelodyFlow Transform)", juce::dontSendNotification);
    terryLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    terryLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    terryLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(terryLabel);

    // Variation dropdown - populate with all variations
    terryVariationComboBox.addItem("accordion_folk", 1);
    terryVariationComboBox.addItem("banjo_bluegrass", 2);
    terryVariationComboBox.addItem("piano_classical", 3);
    terryVariationComboBox.addItem("celtic", 4);
    terryVariationComboBox.addItem("strings_quartet", 5);
    terryVariationComboBox.addItem("synth_retro", 6);
    terryVariationComboBox.addItem("synth_modern", 7);
    terryVariationComboBox.addItem("synth_edm", 8);
    terryVariationComboBox.addItem("lofi_chill", 9);
    terryVariationComboBox.addItem("synth_bass", 10);
    terryVariationComboBox.addItem("rock_band", 11);
    terryVariationComboBox.addItem("cinematic_epic", 12);
    terryVariationComboBox.addItem("retro_rpg", 13);
    terryVariationComboBox.addItem("chiptune", 14);
    terryVariationComboBox.addItem("steel_drums", 15);
    terryVariationComboBox.addItem("gamelan_fusion", 16);
    terryVariationComboBox.addItem("music_box", 17);
    terryVariationComboBox.addItem("trap_808", 18);
    terryVariationComboBox.addItem("lo_fi_drums", 19);
    terryVariationComboBox.addItem("boom_bap", 20);
    terryVariationComboBox.addItem("percussion_ensemble", 21);
    terryVariationComboBox.addItem("future_bass", 22);
    terryVariationComboBox.addItem("synthwave_retro", 23);
    terryVariationComboBox.addItem("melodic_techno", 24);
    terryVariationComboBox.addItem("dubstep_wobble", 25);
    terryVariationComboBox.addItem("glitch_hop", 26);
    terryVariationComboBox.addItem("digital_disruption", 27);
    terryVariationComboBox.addItem("circuit_bent", 28);
    terryVariationComboBox.addItem("orchestral_glitch", 29);
    terryVariationComboBox.addItem("vapor_drums", 30);
    terryVariationComboBox.addItem("industrial_textures", 31);
    terryVariationComboBox.addItem("jungle_breaks", 32);
    terryVariationComboBox.setSelectedId(1); // Default to first variation
    terryVariationComboBox.onChange = [this]() {
        currentTerryVariation = terryVariationComboBox.getSelectedId() - 1;
        // Clear custom prompt when variation is selected
        if (currentTerryVariation >= 0) {
            terryCustomPromptEditor.clear();
            currentTerryCustomPrompt = "";
        }
        };
    addAndMakeVisible(terryVariationComboBox);

    terryVariationLabel.setText("Variation", juce::dontSendNotification);
    terryVariationLabel.setFont(juce::FontOptions(12.0f));
    terryVariationLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    terryVariationLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terryVariationLabel);

    // Custom prompt editor (alternative to variations)
    terryCustomPromptEditor.setMultiLine(false);
    terryCustomPromptEditor.setReturnKeyStartsNewLine(false);
    terryCustomPromptEditor.setTextToShowWhenEmpty("Or enter custom prompt...", juce::Colours::darkgrey);
    terryCustomPromptEditor.onTextChange = [this]() {
        currentTerryCustomPrompt = terryCustomPromptEditor.getText();
        // Clear variation selection when custom prompt is used
        if (!currentTerryCustomPrompt.trim().isEmpty()) {
            terryVariationComboBox.setSelectedId(0); // Deselect
            currentTerryVariation = -1;
        }
        };
    addAndMakeVisible(terryCustomPromptEditor);

    terryCustomPromptLabel.setText("Custom Prompt", juce::dontSendNotification);
    terryCustomPromptLabel.setFont(juce::FontOptions(12.0f));
    terryCustomPromptLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    terryCustomPromptLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terryCustomPromptLabel);

    // Flowstep slider (0.050 to 0.150, default 0.130)
    terryFlowstepSlider.setRange(0.050, 0.150, 0.001);
    terryFlowstepSlider.setValue(0.130);
    terryFlowstepSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    terryFlowstepSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    terryFlowstepSlider.setNumDecimalPlacesToDisplay(3);
    terryFlowstepSlider.onValueChange = [this]() {
        currentTerryFlowstep = (float)terryFlowstepSlider.getValue();
        };
    addAndMakeVisible(terryFlowstepSlider);

    terryFlowstepLabel.setText("Flowstep", juce::dontSendNotification);
    terryFlowstepLabel.setFont(juce::FontOptions(12.0f));
    terryFlowstepLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    terryFlowstepLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terryFlowstepLabel);

    // Solver toggle (euler/midpoint)
    terrySolverToggle.setButtonText("Use Midpoint Solver");
    terrySolverToggle.setToggleState(false, juce::dontSendNotification); // Default to euler
    terrySolverToggle.onClick = [this]() {
        useMidpointSolver = terrySolverToggle.getToggleState();
        DBG("Solver changed to: " + juce::String(useMidpointSolver ? "midpoint" : "euler"));
        };
    addAndMakeVisible(terrySolverToggle);

    terrySolverLabel.setText("Solver", juce::dontSendNotification);
    terrySolverLabel.setFont(juce::FontOptions(12.0f));
    terrySolverLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    terrySolverLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terrySolverLabel);

    // Audio source selection
    terrySourceLabel.setText("Transform", juce::dontSendNotification);
    terrySourceLabel.setFont(juce::FontOptions(12.0f));
    terrySourceLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    terrySourceLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terrySourceLabel);

    transformRecordingButton.setButtonText("Recording");
    transformRecordingButton.setRadioGroupId(1001); // Same group for mutual exclusion
    transformRecordingButton.onClick = [this]() { setTerryAudioSource(true); };
    addAndMakeVisible(transformRecordingButton);

    transformOutputButton.setButtonText("Output");
    transformOutputButton.setRadioGroupId(1001); // Same group for mutual exclusion
    transformOutputButton.setToggleState(true, juce::dontSendNotification); // Default selection
    transformOutputButton.onClick = [this]() { setTerryAudioSource(false); };
    addAndMakeVisible(transformOutputButton);

    // Transform button
    transformWithTerryButton.setButtonText("Transform with Terry");
    transformWithTerryButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkblue);
    transformWithTerryButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    transformWithTerryButton.onClick = [this]() { sendToTerry(); };
    transformWithTerryButton.setEnabled(false); // Initially disabled until we have audio
    addAndMakeVisible(transformWithTerryButton);

    // Undo button  
    undoTransformButton.setButtonText("Undo Transform");
    undoTransformButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkorange);
    undoTransformButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    undoTransformButton.onClick = [this]() { undoTerryTransform(); };
    undoTransformButton.setEnabled(false); // Initially disabled
    addAndMakeVisible(undoTransformButton);

    // ========== REMAINING SETUP (unchanged) ==========
    // Set up the "Check Connection" button
    checkConnectionButton.setButtonText("Check Backend Connection");
    checkConnectionButton.onClick = [this]() {
        DBG("Manual backend health check requested");
        audioProcessor.checkBackendHealth();
        checkConnectionButton.setButtonText("Checking...");
        checkConnectionButton.setEnabled(false);
        juce::Timer::callAfterDelay(3000, [this]() {
            checkConnectionButton.setButtonText("Check Backend Connection");
            checkConnectionButton.setEnabled(true);
        });
    };

    // Set up the "Save Buffer" button
    saveBufferButton.setButtonText("Save Recording Buffer");
    saveBufferButton.onClick = [this]() { saveRecordingBuffer(); };
    saveBufferButton.setEnabled(false); // Initially disabled

    // Set up the "Clear Buffer" button
    clearBufferButton.setButtonText("Clear Buffer");
    clearBufferButton.onClick = [this]() { clearRecordingBuffer(); };

    addAndMakeVisible(checkConnectionButton);
    addAndMakeVisible(saveBufferButton);
    addAndMakeVisible(clearBufferButton);

    // Start timer to update recording status (refresh every 50ms for smooth waveform)
    startTimer(50);

    // Initial status update
    updateRecordingStatus();

    // Set up output audio controls
    outputLabel.setText("Generated Output", juce::dontSendNotification);
    outputLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    outputLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    outputLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputLabel);

    // Play output button
    playOutputButton.setButtonText("Play Output");
    playOutputButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
    playOutputButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    playOutputButton.onClick = [this]() { playOutputAudio(); };
    playOutputButton.setEnabled(false);
    addAndMakeVisible(playOutputButton);

    // Clear output button
    clearOutputButton.setButtonText("Clear Output");
    clearOutputButton.onClick = [this]() { clearOutputAudio(); };
    clearOutputButton.setEnabled(false); // Initially disabled
    addAndMakeVisible(clearOutputButton);

    // Stop output button  
    stopOutputButton.setButtonText("Stop");
    stopOutputButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    stopOutputButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    stopOutputButton.onClick = [this]() { fullStopOutputPlayback(); };
    stopOutputButton.setEnabled(false); // Initially disabled
    addAndMakeVisible(stopOutputButton);

    // Crop button - simple approach with custom drawing
    createCropIcon();
    
    cropButton.setTooltip("Crop audio at current playback position");
    cropButton.onClick = [this]() { cropAudioAtCurrentPosition(); };
    cropButton.setEnabled(false);
    cropButton.setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    cropButton.setColour(juce::DrawableButton::backgroundOnColourId, juce::Colours::orange.withAlpha(0.3f));
    addAndMakeVisible(cropButton);

    // Initialize output file path
    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");
    outputAudioFile = garyDir.getChildFile("myOutput.wav");

    // Check if output file already exists and load it
    if (outputAudioFile.exists())
    {
        loadOutputAudioFile();
    }

    // Enable drag and drop for this component
    setWantsKeyboardFocus(false); // Don't steal focus during drag operations
    outputLabel.setTooltip("Drag generated audio to your DAW timeline");
    setInterceptsMouseClicks(true, true);

    // Set initial tab state
    updateTabButtonStates();
    
    // CRITICAL: Set initial visibility state for tabs
    switchToTab(ModelTab::Gary);
}

Gary4juceAudioProcessorEditor::~Gary4juceAudioProcessorEditor()
{
    // CRITICAL: Stop all timers first to prevent any callbacks during destruction
    stopTimer();

    // CRITICAL: Stop all audio playback immediately before any cleanup
    if (transportSource)
    {
        transportSource->stop();
        transportSource->setSource(nullptr);
    }

    // Reset playback state flags to prevent any ongoing callbacks from accessing destroyed objects
    isPlayingOutput = false;
    isPausedOutput = false;

    // Remove audio callback BEFORE destroying any audio objects
    if (audioDeviceManager && audioSourcePlayer)
    {
        audioDeviceManager->removeAudioCallback(audioSourcePlayer.get());
    }

    // Disconnect audio source player from transport source
    if (audioSourcePlayer)
    {
        audioSourcePlayer->setSource(nullptr);
    }

    // Clean up audio objects in proper order (reverse of creation)
    readerSource.reset();           // First, destroy the file reader
    transportSource.reset();        // Then the transport source
    audioSourcePlayer.reset();      // Then the source player
    audioDeviceManager.reset();     // Then the device manager
    audioFormatManager.reset();     // Finally the format manager

    DBG("Audio playback safely cleaned up");
}

// ========== TAB SWITCHING IMPLEMENTATION ==========

void Gary4juceAudioProcessorEditor::switchToTab(ModelTab tab)
{
    if (currentTab == tab) return; // Already on this tab

    currentTab = tab;
    updateTabButtonStates();

    // Show/hide appropriate controls
    bool showGary = (tab == ModelTab::Gary);
    bool showJerry = (tab == ModelTab::Jerry);

    // Gary controls visibility
    garyLabel.setVisible(showGary);
    promptDurationSlider.setVisible(showGary);
    promptDurationLabel.setVisible(showGary);
    modelComboBox.setVisible(showGary);
    modelLabel.setVisible(showGary);
    sendToGaryButton.setVisible(showGary);
    continueButton.setVisible(showGary);

    // Jerry controls visibility
    jerryLabel.setVisible(showJerry);
    jerryPromptEditor.setVisible(showJerry);
    jerryPromptLabel.setVisible(showJerry);
    jerryCfgSlider.setVisible(showJerry);
    jerryCfgLabel.setVisible(showJerry);
    jerryStepsSlider.setVisible(showJerry);
    jerryStepsLabel.setVisible(showJerry);
    jerryBpmLabel.setVisible(showJerry);
    generateWithJerryButton.setVisible(showJerry);

    // FIXED: Show/hide the smart loop toggle button
    generateAsLoopButton.setVisible(showJerry);

    // Loop type controls follow the smart loop toggle visibility
    if (showJerry) {
        updateLoopTypeVisibility();  // This will show/hide based on toggle state
    }
    else {
        // Hide loop type controls when Jerry tab is hidden
        loopTypeAutoButton.setVisible(false);
        loopTypeDrumsButton.setVisible(false);
        loopTypeInstrumentsButton.setVisible(false);
    }

    // Terry controls visibility
    bool showTerry = (tab == ModelTab::Terry);

    terryLabel.setVisible(showTerry);
    terryVariationComboBox.setVisible(showTerry);
    terryVariationLabel.setVisible(showTerry);
    terryCustomPromptEditor.setVisible(showTerry);
    terryCustomPromptLabel.setVisible(showTerry);
    terryFlowstepSlider.setVisible(showTerry);
    terryFlowstepLabel.setVisible(showTerry);
    terrySolverToggle.setVisible(showTerry);
    terrySolverLabel.setVisible(showTerry);
    terrySourceLabel.setVisible(showTerry);
    transformRecordingButton.setVisible(showTerry);
    transformOutputButton.setVisible(showTerry);
    transformWithTerryButton.setVisible(showTerry);
    undoTransformButton.setVisible(showTerry);

    // ========== ADD TERRY LAYOUT TO resized() METHOD ==========
    // Add this after the Jerry FlexBox section:

    // ========== TERRY CONTROLS LAYOUT (FlexBox Implementation) ==========
    auto terryBounds = modelControlsArea;

    // Create main FlexBox for Terry controls (vertical layout)
    juce::FlexBox terryFlexBox;
    terryFlexBox.flexDirection = juce::FlexBox::Direction::column;
    terryFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    // 1. Terry section title
    juce::FlexItem terryTitleItem(terryLabel);
    terryTitleItem.height = 30;
    terryTitleItem.margin = juce::FlexItem::Margin(5, 0, 5, 0);

    // 2. Variation controls row
    juce::Component terryVariationRowComponent;
    juce::FlexItem terryVariationRowItem(terryVariationRowComponent);
    terryVariationRowItem.height = 30;
    terryVariationRowItem.margin = juce::FlexItem::Margin(3, 0, 3, 0);

    // 3. Custom prompt label
    juce::FlexItem terryCustomPromptLabelItem(terryCustomPromptLabel);
    terryCustomPromptLabelItem.height = 15;
    terryCustomPromptLabelItem.margin = juce::FlexItem::Margin(2, 0, 2, 0);

    // 4. Custom prompt editor
    juce::FlexItem terryCustomPromptEditorItem(terryCustomPromptEditor);
    terryCustomPromptEditorItem.height = 25;
    terryCustomPromptEditorItem.margin = juce::FlexItem::Margin(0, 5, 5, 5);

    // 5. Flowstep controls row
    juce::Component terryFlowstepRowComponent;
    juce::FlexItem terryFlowstepRowItem(terryFlowstepRowComponent);
    terryFlowstepRowItem.height = 30;
    terryFlowstepRowItem.margin = juce::FlexItem::Margin(3, 0, 3, 0);

    // 6. Solver controls row
    juce::Component terrySolverRowComponent;
    juce::FlexItem terrySolverRowItem(terrySolverRowComponent);
    terrySolverRowItem.height = 25;
    terrySolverRowItem.margin = juce::FlexItem::Margin(3, 0, 3, 0);

    // 7. Audio source selection row
    juce::Component terrySourceRowComponent;
    juce::FlexItem terrySourceRowItem(terrySourceRowComponent);
    terrySourceRowItem.height = 25;
    terrySourceRowItem.margin = juce::FlexItem::Margin(3, 0, 8, 0);

    // 8. Transform button
    juce::FlexItem transformTerryItem(transformWithTerryButton);
    transformTerryItem.height = 35;
    transformTerryItem.margin = juce::FlexItem::Margin(5, 50, 5, 50);

    // 9. Undo button
    juce::FlexItem undoTerryItem(undoTransformButton);
    undoTerryItem.height = 35;
    undoTerryItem.margin = juce::FlexItem::Margin(5, 50, 5, 50);

    // Add all items to Terry FlexBox
    terryFlexBox.items.add(terryTitleItem);
    terryFlexBox.items.add(terryVariationRowItem);
    terryFlexBox.items.add(terryCustomPromptLabelItem);
    terryFlexBox.items.add(terryCustomPromptEditorItem);
    terryFlexBox.items.add(terryFlowstepRowItem);
    terryFlexBox.items.add(terrySolverRowItem);
    terryFlexBox.items.add(terrySourceRowItem);
    terryFlexBox.items.add(transformTerryItem);
    terryFlexBox.items.add(undoTerryItem);

    // Perform layout for Terry controls
    terryFlexBox.performLayout(terryBounds);

    // Extract calculated bounds for control rows
    auto terryVariationRowBounds = terryFlexBox.items[1].currentBounds.toNearestInt();
    auto terryFlowstepRowBounds = terryFlexBox.items[4].currentBounds.toNearestInt();
    auto terrySolverRowBounds = terryFlexBox.items[5].currentBounds.toNearestInt();
    auto terrySourceRowBounds = terryFlexBox.items[6].currentBounds.toNearestInt();

    // Layout variation controls (horizontal FlexBox)
    juce::FlexBox terryVariationFlexBox;
    terryVariationFlexBox.flexDirection = juce::FlexBox::Direction::row;
    terryVariationFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    juce::FlexItem terryVariationLabelItem(terryVariationLabel);
    terryVariationLabelItem.width = 80;
    terryVariationLabelItem.margin = juce::FlexItem::Margin(0, 5, 0, 0);

    juce::FlexItem terryVariationComboItem(terryVariationComboBox);
    terryVariationComboItem.flexGrow = 1;
    terryVariationComboItem.margin = juce::FlexItem::Margin(0, 0, 0, 5);

    terryVariationFlexBox.items.add(terryVariationLabelItem);
    terryVariationFlexBox.items.add(terryVariationComboItem);
    terryVariationFlexBox.performLayout(terryVariationRowBounds);

    // Layout flowstep controls (horizontal FlexBox)
    juce::FlexBox terryFlowstepFlexBox;
    terryFlowstepFlexBox.flexDirection = juce::FlexBox::Direction::row;
    terryFlowstepFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    juce::FlexItem terryFlowstepLabelItem(terryFlowstepLabel);
    terryFlowstepLabelItem.width = 80;
    terryFlowstepLabelItem.margin = juce::FlexItem::Margin(0, 5, 0, 0);

    juce::FlexItem terryFlowstepSliderItem(terryFlowstepSlider);
    terryFlowstepSliderItem.flexGrow = 1;
    terryFlowstepSliderItem.margin = juce::FlexItem::Margin(0, 0, 0, 5);

    terryFlowstepFlexBox.items.add(terryFlowstepLabelItem);
    terryFlowstepFlexBox.items.add(terryFlowstepSliderItem);
    terryFlowstepFlexBox.performLayout(terryFlowstepRowBounds);

    // Layout solver controls (horizontal FlexBox)
    juce::FlexBox terrySolverFlexBox;
    terrySolverFlexBox.flexDirection = juce::FlexBox::Direction::row;
    terrySolverFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    juce::FlexItem terrySolverLabelItem(terrySolverLabel);
    terrySolverLabelItem.width = 80;
    terrySolverLabelItem.margin = juce::FlexItem::Margin(0, 5, 0, 0);

    juce::FlexItem terrySolverToggleItem(terrySolverToggle);
    terrySolverToggleItem.flexGrow = 1;
    terrySolverToggleItem.margin = juce::FlexItem::Margin(0, 0, 0, 5);

    terrySolverFlexBox.items.add(terrySolverLabelItem);
    terrySolverFlexBox.items.add(terrySolverToggleItem);
    terrySolverFlexBox.performLayout(terrySolverRowBounds);

    // Layout audio source controls (horizontal FlexBox)
    juce::FlexBox terrySourceFlexBox;
    terrySourceFlexBox.flexDirection = juce::FlexBox::Direction::row;
    terrySourceFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    juce::FlexItem terrySourceLabelItem(terrySourceLabel);
    terrySourceLabelItem.width = 80;
    terrySourceLabelItem.margin = juce::FlexItem::Margin(0, 5, 0, 0);

    juce::FlexItem transformRecordingItem(transformRecordingButton);
    transformRecordingItem.width = 80;
    transformRecordingItem.margin = juce::FlexItem::Margin(0, 5, 0, 5);

    juce::FlexItem transformOutputItem(transformOutputButton);
    transformOutputItem.width = 80;
    transformOutputItem.margin = juce::FlexItem::Margin(0, 0, 0, 0);

    terrySourceFlexBox.items.add(terrySourceLabelItem);
    terrySourceFlexBox.items.add(transformRecordingItem);
    terrySourceFlexBox.items.add(transformOutputItem);
    terrySourceFlexBox.performLayout(terrySourceRowBounds);

    DBG("Switched to tab: " + juce::String(showGary ? "Gary" : (showJerry ? "Jerry" : "Terry")));

    repaint();
}

void Gary4juceAudioProcessorEditor::updateTabButtonStates()
{
    // Update tab button appearances
    garyTabButton.setColour(juce::TextButton::buttonColourId,
        currentTab == ModelTab::Gary ? juce::Colours::darkred : juce::Colours::darkgrey);
    garyTabButton.setColour(juce::TextButton::textColourOffId,
        currentTab == ModelTab::Gary ? juce::Colours::white : juce::Colours::lightgrey);

    jerryTabButton.setColour(juce::TextButton::buttonColourId,
        currentTab == ModelTab::Jerry ? juce::Colours::darkgreen : juce::Colours::darkgrey);
    jerryTabButton.setColour(juce::TextButton::textColourOffId,
        currentTab == ModelTab::Jerry ? juce::Colours::white : juce::Colours::lightgrey);

    terryTabButton.setColour(juce::TextButton::buttonColourId,
        currentTab == ModelTab::Terry ? juce::Colours::darkblue : juce::Colours::darkgrey);
    terryTabButton.setColour(juce::TextButton::textColourOffId,
        currentTab == ModelTab::Terry ? juce::Colours::white : juce::Colours::lightgrey);
}

//==============================================================================
// Updated timerCallback() with smooth progress animation:
void Gary4juceAudioProcessorEditor::timerCallback()
{
    updateRecordingStatus();

    // Update Jerry BPM display with current DAW BPM
    double currentBPM = audioProcessor.getCurrentBPM();
    jerryBpmLabel.setText("BPM: " + juce::String((int)currentBPM) + " (from DAW)",
        juce::dontSendNotification);

    // Check playback status every timer tick when playing (every 50ms for smooth cursor)
    if (isPlayingOutput)
    {
        checkPlaybackStatus();
    }

    // Smooth progress animation for generation (every 50ms for smooth animation)
    if (isGenerating && smoothProgressAnimation)
    {
        updateSmoothProgress();
    }

    // Poll backend every 60 timer ticks (every 3 seconds since timer runs every 50ms)
    static int pollCounter = 0;
    if (isPolling)
    {
        pollCounter++;
        if (pollCounter >= 60) // 60 * 50ms = 3 seconds - reasonable backend polling
        {
            pollCounter = 0;
            pollForResults();
        }
    }
}

// New function: Update smooth progress animation between server updates
void Gary4juceAudioProcessorEditor::updateSmoothProgress()
{
    auto currentTime = juce::Time::getCurrentTime().toMilliseconds();
    auto timeSinceUpdate = currentTime - lastProgressUpdateTime;

    // Animate over 3 seconds (time between polls)
    const int animationDuration = 3000; // 3 seconds

    if (timeSinceUpdate < animationDuration && targetProgress > lastKnownProgress)
    {
        // Calculate smooth interpolation progress (0.0 to 1.0)
        float animationProgress = (float)timeSinceUpdate / (float)animationDuration;
        animationProgress = juce::jlimit(0.0f, 1.0f, animationProgress);

        // Apply easing function for smoother animation (ease-out)
        float easedProgress = 1.0f - (1.0f - animationProgress) * (1.0f - animationProgress);

        // Interpolate between last known and target progress
        int interpolatedProgress = lastKnownProgress +
            (int)(easedProgress * (targetProgress - lastKnownProgress));

        generationProgress = juce::jlimit(0, 100, interpolatedProgress);
        repaint(); // Smooth visual update
    }
    else if (timeSinceUpdate >= animationDuration)
    {
        // Animation complete - set to exact target
        generationProgress = targetProgress;
        smoothProgressAnimation = false;
        repaint();
    }
}

void Gary4juceAudioProcessorEditor::updateRecordingStatus()
{
    bool wasRecording = isRecording;
    float wasProgress = recordingProgress;
    int wasSamples = recordedSamples;

    // Get current status from processor
    isRecording = audioProcessor.isRecording();
    recordingProgress = audioProcessor.getRecordingProgress();
    recordedSamples = audioProcessor.getRecordedSamples();

    // Update save button state
    saveBufferButton.setEnabled(recordedSamples > 0);

    // Update Gary button state - need saved audio to send to Gary
    sendToGaryButton.setEnabled(savedSamples > 0 && isConnected);

    // Update Terry button states
    updateTerrySourceButtons();

    // Check if status message has expired
    if (hasStatusMessage)
    {
        auto currentTime = juce::Time::getCurrentTime().toMilliseconds();
        if (currentTime - statusMessageTime > 3000) // 3 seconds
        {
            hasStatusMessage = false;
            statusMessage = "";
        }
    }

    // Repaint if status changed
    if (wasRecording != isRecording ||
        std::abs(wasProgress - recordingProgress) > 0.01f ||
        wasSamples != recordedSamples)
    {
        repaint();
    }
}

void Gary4juceAudioProcessorEditor::showStatusMessage(const juce::String& message, int durationMs)
{
    statusMessage = message;
    statusMessageTime = juce::Time::getCurrentTime().toMilliseconds();
    hasStatusMessage = true;
    repaint();
}

void Gary4juceAudioProcessorEditor::drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    // Black background
    g.setColour(juce::Colours::black);
    g.fillRect(area);

    // Draw border
    g.setColour(juce::Colour(0x40, 0x40, 0x40));
    g.drawRect(area, 1);

    if (recordedSamples <= 0)
    {
        // Show "waiting" state
        g.setFont(juce::FontOptions(14.0f));
        g.setColour(juce::Colours::darkgrey);
        g.drawText("Press PLAY in DAW to start recording", area, juce::Justification::centred);
        return;
    }

    // Get the recording buffer and validate it
    const auto& recordingBuffer = audioProcessor.getRecordingBuffer();
    if (recordingBuffer.getNumSamples() <= 0 || recordingBuffer.getNumChannels() <= 0)
        return;

    // Safe calculations with proper bounds checking
    const int waveWidth = juce::jmax(1, area.getWidth() - 2); // Account for border, minimum 1
    const int waveHeight = juce::jmax(1, area.getHeight() - 2);
    const int centerY = area.getCentreY();

    // Time calculations
    const double totalDuration = 30.0; // 30 seconds max
    const double recordedDuration = juce::jmax(0.0, (double)recordedSamples / 44100.0);
    const double savedDuration = juce::jmax(0.0, (double)savedSamples / 44100.0);

    // Pixel calculations with safety checks
    const int recordedPixels = juce::jmax(0, juce::jmin(waveWidth, (int)((recordedDuration / totalDuration) * waveWidth)));
    const int savedPixels = juce::jmax(0, juce::jmin(recordedPixels, (int)((savedDuration / totalDuration) * waveWidth)));

    // Early exit if nothing to draw
    if (recordedPixels <= 0)
        return;

    // Safe samples per pixel calculation - avoid division by zero
    const int samplesPerPixel = (recordedPixels > 0) ? juce::jmax(1, recordedSamples / recordedPixels) : 1;

    // Draw saved portion (solid red)
    if (savedPixels > 0)
    {
        g.setColour(juce::Colours::red);

        for (int x = 0; x < savedPixels; ++x)
        {
            const int startSample = x * samplesPerPixel;
            const int endSample = juce::jmin(startSample + samplesPerPixel, savedSamples);

            if (endSample > startSample && startSample < recordingBuffer.getNumSamples())
            {
                // Find min/max in this pixel's worth of samples
                float minVal = 0.0f, maxVal = 0.0f;

                for (int sample = startSample; sample < endSample && sample < recordingBuffer.getNumSamples(); ++sample)
                {
                    // Average across channels safely
                    float sampleValue = 0.0f;
                    for (int ch = 0; ch < recordingBuffer.getNumChannels(); ++ch)
                    {
                        sampleValue += recordingBuffer.getSample(ch, sample);
                    }
                    sampleValue /= recordingBuffer.getNumChannels();

                    minVal = juce::jmin(minVal, sampleValue);
                    maxVal = juce::jmax(maxVal, sampleValue);
                }

                // Scale to display area with clamping
                const int minY = juce::jlimit(area.getY(), area.getBottom(),
                    centerY - (int)(minVal * waveHeight * 0.4f));
                const int maxY = juce::jlimit(area.getY(), area.getBottom(),
                    centerY - (int)(maxVal * waveHeight * 0.4f));

                const int drawX = area.getX() + 1 + x;

                // Draw thicker, smoother lines
                if (maxY != minY)
                {
                    // Main line
                    g.drawVerticalLine(drawX, (float)maxY, (float)minY);
                    // Add thickness by drawing adjacent pixels with slight transparency
                    if (x > 0) // Left side
                    {
                        g.setColour(juce::Colours::red.withAlpha(0.6f));
                        g.drawVerticalLine(drawX - 1, (float)maxY, (float)minY);
                        g.setColour(juce::Colours::red); // Reset for next iteration
                    }
                    if (x < savedPixels - 1) // Right side  
                    {
                        g.setColour(juce::Colours::red.withAlpha(0.6f));
                        g.drawVerticalLine(drawX + 1, (float)maxY, (float)minY);
                        g.setColour(juce::Colours::red); // Reset for next iteration
                    }
                }
                else
                {
                    // For silent parts, draw a thicker center line
                    g.fillRect(drawX - 1, centerY - 1, 3, 2);
                }
            }
        }
    }

    // Draw unsaved portion (semi-transparent red)
    if (recordedPixels > savedPixels)
    {
        g.setColour(juce::Colours::red.withAlpha(0.5f));

        for (int x = savedPixels; x < recordedPixels; ++x)
        {
            const int startSample = x * samplesPerPixel;
            const int endSample = juce::jmin(startSample + samplesPerPixel, recordedSamples);

            if (endSample > startSample && startSample < recordingBuffer.getNumSamples())
            {
                // Find min/max in this pixel's worth of samples
                float minVal = 0.0f, maxVal = 0.0f;

                for (int sample = startSample; sample < endSample && sample < recordingBuffer.getNumSamples(); ++sample)
                {
                    // Average across channels safely
                    float sampleValue = 0.0f;
                    for (int ch = 0; ch < recordingBuffer.getNumChannels(); ++ch)
                    {
                        sampleValue += recordingBuffer.getSample(ch, sample);
                    }
                    sampleValue /= recordingBuffer.getNumChannels();

                    minVal = juce::jmin(minVal, sampleValue);
                    maxVal = juce::jmax(maxVal, sampleValue);
                }

                // Scale to display area with clamping
                const int minY = juce::jlimit(area.getY(), area.getBottom(),
                    centerY - (int)(minVal * waveHeight * 0.4f));
                const int maxY = juce::jlimit(area.getY(), area.getBottom(),
                    centerY - (int)(maxVal * waveHeight * 0.4f));

                const int drawX = area.getX() + 1 + x;

                // Draw thicker, smoother lines for unsaved portion
                if (maxY != minY)
                {
                    // Main line
                    g.drawVerticalLine(drawX, (float)maxY, (float)minY);
                    // Add thickness by drawing adjacent pixels with slight transparency
                    if (x > savedPixels) // Left side (don't overlap with saved portion)
                    {
                        g.setColour(juce::Colours::red.withAlpha(0.3f)); // Even more transparent for thickness
                        g.drawVerticalLine(drawX - 1, (float)maxY, (float)minY);
                        g.setColour(juce::Colours::red.withAlpha(0.5f)); // Reset for next iteration
                    }
                    if (x < recordedPixels - 1) // Right side
                    {
                        g.setColour(juce::Colours::red.withAlpha(0.3f));
                        g.drawVerticalLine(drawX + 1, (float)maxY, (float)minY);
                        g.setColour(juce::Colours::red.withAlpha(0.5f)); // Reset for next iteration
                    }
                }
                else
                {
                    // For silent parts, draw a thicker center line with transparency
                    g.fillRect(drawX - 1, centerY - 1, 3, 2);
                }
            }
        }
    }

    // Add recording indicator if currently recording
    if (isRecording && recordedPixels > 0)
    {
        // Animated recording line at the current position
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        const int recordingX = area.getX() + 1 + recordedPixels;
        if (recordingX >= area.getX() && recordingX <= area.getRight())
        {
            g.drawVerticalLine(recordingX, (float)area.getY(), (float)area.getBottom());

            // Optional: pulsing effect
            auto time = juce::Time::getCurrentTime().toMilliseconds();
            float pulse = (std::sin(time * 0.01f) + 1.0f) * 0.5f; // 0 to 1
            g.setColour(juce::Colours::red.withAlpha(0.3f + pulse * 0.4f));
            g.fillRect(recordingX, area.getY(), 2, area.getHeight());
        }
    }
}

void Gary4juceAudioProcessorEditor::saveRecordingBuffer()
{
    if (recordedSamples <= 0)
    {
        showStatusMessage("No recording to save - press PLAY in DAW first");
        return;
    }

    DBG("Save buffer button clicked with " + juce::String(recordedSamples) + " samples");

    // Create gary4juce directory in Documents
    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");

    // Create directory if it doesn't exist
    if (!garyDir.exists())
    {
        auto result = garyDir.createDirectory();
        DBG("Created gary4juce directory: " + juce::String(result ? "success" : "failed"));
    }

    // Always save to the same filename (overwrite each time)
    auto recordingFile = garyDir.getChildFile("myBuffer.wav");

    DBG("Saving to: " + recordingFile.getFullPathName());
    audioProcessor.saveRecordingToFile(recordingFile);

    // Update saved samples to current recorded samples
    savedSamples = recordedSamples;

    // Show success message in UI instead of popup
    double recordedSeconds = (double)recordedSamples / 44100.0;
    showStatusMessage(juce::String::formatted("? Saved %.1fs to myBuffer.wav", recordedSeconds), 4000);

    // Force waveform redraw to show the solidified state
    repaint();
}

void Gary4juceAudioProcessorEditor::startPollingForResults(const juce::String& sessionId)
{
    currentSessionId = sessionId;
    isPolling = true;
    isGenerating = true;
    generationProgress = 0;
    repaint(); // Start showing progress visualization
    DBG("Started polling for session: " + sessionId);
}

void Gary4juceAudioProcessorEditor::stopPolling()
{
    isPolling = false;
    //currentSessionId = "";
    DBG("Stopped polling");
}

void Gary4juceAudioProcessorEditor::pollForResults()
{
    if (!isPolling || currentSessionId.isEmpty())
        return;

    // Create polling request in background thread
    juce::Thread::launch([this]() {
        juce::URL pollUrl("https://g4l.thecollabagepatch.com/api/juce/poll_status/" + currentSessionId);

        try
        {
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(10000)
                .withExtraHeaders("Content-Type: application/json");

            auto stream = pollUrl.createInputStream(options);

            if (stream != nullptr)
            {
                auto responseText = stream->readEntireStreamAsString();

                // Handle response on main thread
                juce::MessageManager::callAsync([this, responseText]() {
                    handlePollingResponse(responseText);
                    });
            }
            else
            {
                DBG("Failed to create polling stream");
            }
        }
        catch (const std::exception& e)
        {
            DBG("Polling exception: " + juce::String(e.what()));
        }
        catch (...)
        {
            DBG("Unknown polling exception");
        }
        });
}

void Gary4juceAudioProcessorEditor::handlePollingResponse(const juce::String& responseText)
{
    if (responseText.isEmpty())
    {
        DBG("Empty polling response");
        return;
    }

    try
    {
        auto responseVar = juce::JSON::parse(responseText);
        if (auto* responseObj = responseVar.getDynamicObject())
        {
            bool success = responseObj->getProperty("success");
            if (!success)
            {
                DBG("Polling error: " + responseObj->getProperty("error").toString());
                stopPolling();
                showStatusMessage("Processing failed", 3000);
                continueButton.setEnabled(hasOutputAudio); // Re-enable continue button
                return;
            }

            // Check if still in progress
            bool generationInProgress = responseObj->getProperty("generation_in_progress");
            bool transformInProgress = responseObj->getProperty("transform_in_progress");

            if (generationInProgress || transformInProgress)
            {
                // Get real progress from server
                int serverProgress = responseObj->getProperty("progress");
                serverProgress = juce::jlimit(0, 100, serverProgress);

                // Set up smooth animation to new target
                lastKnownProgress = generationProgress;  // Where we are now (visually)
                targetProgress = serverProgress;         // Where server says we should be
                lastProgressUpdateTime = juce::Time::getCurrentTime().toMilliseconds();
                smoothProgressAnimation = true;          // Start smooth animation

                // FIXED: Check current tab instead of server transformInProgress flag
                if (currentTab == ModelTab::Terry)
                {
                    showStatusMessage("Transforming: " + juce::String(serverProgress) + "%", 1000);
                    DBG("Transform progress: " + juce::String(serverProgress) + "%, animating from " +
                        juce::String(lastKnownProgress));
                }
                else
                {
                    showStatusMessage("Generating: " + juce::String(serverProgress) + "%", 1000);
                    DBG("Generation progress: " + juce::String(serverProgress) + "%, animating from " +
                        juce::String(lastKnownProgress));
                }
                return;
            }

            // COMPLETED - Check what TYPE of completion this is FIRST
            auto audioData = responseObj->getProperty("audio_data").toString();
            auto status = responseObj->getProperty("status").toString();

            if (audioData.isNotEmpty())
            {
                stopPolling();

                // DIFFERENTIATE: Check if we're completing a transform vs generation
                if (transformInProgress || currentTab == ModelTab::Terry)
                {
                    // TRANSFORM COMPLETION - Keep session ID for undo
                    showStatusMessage("Transform complete!", 3000);
                    saveGeneratedAudio(audioData);
                    DBG("Successfully received transformed audio: " + juce::String(audioData.length()) + " chars");

                    // Enable undo button now that transform is complete
                    undoTransformButton.setEnabled(true);
                    // DON'T clear currentSessionId - we need it for undo!
                }
                else
                {
                    // GENERATION COMPLETION (Gary/Jerry) - Clear session ID since no undo needed
                    showStatusMessage("Audio generation complete!", 3000);
                    saveGeneratedAudio(audioData);
                    DBG("Successfully received generated audio: " + juce::String(audioData.length()) + " chars");

                    // Clear session ID for generation since we don't need it anymore
                    currentSessionId = "";  // Clear here for generation
                    continueButton.setEnabled(hasOutputAudio);
                }
            }
            else
            {
                // COMPLETED but no audio data
                if (status == "failed")
                {
                    auto error = responseObj->getProperty("error").toString();
                    stopPolling();

                    if (transformInProgress || currentTab == ModelTab::Terry)
                    {
                        showStatusMessage("Transform failed: " + error, 5000);
                    }
                    else
                    {
                        showStatusMessage("Generation failed: " + error, 5000);
                        continueButton.setEnabled(hasOutputAudio); // Re-enable continue button
                    }
                }
                else if (status == "completed")
                {
                    stopPolling();

                    if (transformInProgress || currentTab == ModelTab::Terry)
                    {
                        showStatusMessage("Transform completed but no audio received", 3000);
                    }
                    else
                    {
                        showStatusMessage("Generation completed but no audio received", 3000);
                        continueButton.setEnabled(hasOutputAudio); // Re-enable continue button
                    }
                }
            }
        }
    }
    catch (...)
    {
        DBG("Failed to parse polling response");
    }
}

void Gary4juceAudioProcessorEditor::saveGeneratedAudio(const juce::String& base64Audio)
{
    try
    {
        // Decode base64 audio using JUCE's proper API
        juce::MemoryOutputStream outputStream;

        if (!juce::Base64::convertFromBase64(outputStream, base64Audio))
        {
            DBG("Failed to decode base64 audio");
            return;
        }

        // Get the decoded data
        const juce::MemoryBlock& audioData = outputStream.getMemoryBlock();

        // Save to gary4juce directory as myOutput.wav (overwrite each time)
        auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        auto garyDir = documentsDir.getChildFile("gary4juce");

        // FIXED: Always save as myOutput.wav
        outputAudioFile = garyDir.getChildFile("myOutput.wav");

        // Write to file
        if (outputAudioFile.replaceWithData(audioData.getData(), audioData.getSize()))
        {
            showStatusMessage("Generated audio ready!", 3000);
            DBG("Generated audio saved to: " + outputAudioFile.getFullPathName());

            // STOP any current playback before loading new audio
            // This prevents confusion where old audio plays but new waveform shows
            if (isPlayingOutput || isPausedOutput)
            {
                stopOutputPlayback(); // Full stop - reset to beginning
                showStatusMessage("New audio ready! Press play to hear it.", 3000);
            }

            // Load the audio file into our buffer for waveform display
            loadOutputAudioFile();

            // FIXED: Reset continue button text after successful generation
            continueButton.setButtonText("Continue");

            // Reset generation state
            isGenerating = false;
            generationProgress = 0;

            // Update UI
            repaint();
        }
        else
        {
            DBG("Failed to save generated audio file");
        }
    }
    catch (...)
    {
        DBG("Exception saving generated audio");
    }
}

void Gary4juceAudioProcessorEditor::sendToGary()
{
    // Reset generation state immediately
    isGenerating = true;
    generationProgress = 0;
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;
    repaint(); // Force immediate UI update
    
    if (savedSamples <= 0)
    {
        isGenerating = false; // Reset if we're returning early
        showStatusMessage("Please save your recording first!");
        return;
    }

    if (!isConnected)
    {
        isGenerating = false; // Reset if we're returning early
        showStatusMessage("Backend not connected - check connection first");
        return;
    }

    // Get the model names
    const juce::StringArray modelNames = {
        "thepatch/vanya_ai_dnb_0.1",
        "thepatch/bleeps-medium",
        "thepatch/gary_orchestra_2",
        "thepatch/hoenn_lofi"
    };

    auto selectedModel = modelNames[currentModelIndex];

    // Debug current values
    DBG("Current prompt duration value: " + juce::String(currentPromptDuration) + " (will be cast to: " + juce::String((int)currentPromptDuration) + ")");

    // Read the saved audio file
    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");
    auto audioFile = garyDir.getChildFile("myBuffer.wav");

    if (!audioFile.exists())
    {
        showStatusMessage("Audio file not found - save recording first");
        return;
    }

    // Read and encode audio file
    juce::MemoryBlock audioData;
    if (!audioFile.loadFileAsData(audioData))
    {
        showStatusMessage("Failed to read audio file");
        return;
    }

    // Verify we have audio data
    if (audioData.getSize() == 0)
    {
        showStatusMessage("Audio file is empty");
        return;
    }

    auto base64Audio = juce::Base64::toBase64(audioData.getData(), audioData.getSize());

    // Debug: Log audio data size
    DBG("Audio file size: " + juce::String(audioData.getSize()) + " bytes");
    DBG("Base64 length: " + juce::String(base64Audio.length()) + " chars");

    // Disable button and show processing state
    sendToGaryButton.setEnabled(false);
    sendToGaryButton.setButtonText("Sending...");
    showStatusMessage("Sending audio to Gary...");

    // Create HTTP request in background thread
    juce::Thread::launch([this, selectedModel, base64Audio]() {

        auto startTime = juce::Time::getCurrentTime();

        // Create JSON payload - Clean construction without double encoding
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("model_name", selectedModel);
        jsonRequest->setProperty("prompt_duration", (int)currentPromptDuration);
        jsonRequest->setProperty("audio_data", base64Audio);
        jsonRequest->setProperty("top_k", 250);
        jsonRequest->setProperty("temperature", 1.0);
        jsonRequest->setProperty("cfg_coef", 3.0);
        jsonRequest->setProperty("description", "");

        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        // Debug: Log the JSON structure (not content due to size)
        DBG("JSON payload size: " + juce::String(jsonString.length()) + " characters");
        DBG("JSON preview: " + jsonString.substring(0, 100) + "...");

        juce::URL url("https://g4l.thecollabagepatch.com/api/juce/process_audio");

        juce::String responseText;
        int statusCode = 0;

        try
        {
            // JUCE 8.0.8 CORRECT HYBRID APPROACH: 
            // Use URL.withPOSTData for POST body, InputStreamOptions for modern settings
            juce::URL postUrl = url.withPOSTData(jsonString);

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(30000)
                .withExtraHeaders("Content-Type: application/json");

            auto stream = postUrl.createInputStream(options);

            auto requestTime = juce::Time::getCurrentTime() - startTime;
            DBG("HTTP connection established in " + juce::String(requestTime.inMilliseconds()) + "ms");

            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();

                auto totalTime = juce::Time::getCurrentTime() - startTime;
                DBG("HTTP request completed in " + juce::String(totalTime.inMilliseconds()) + "ms");
                DBG("Response length: " + juce::String(responseText.length()) + " characters");

                statusCode = 200; // Assume success if we got data
            }
            else
            {
                DBG("Failed to create input stream for HTTP request");
                responseText = "";
                statusCode = 0;
            }
        }
        catch (const std::exception& e)
        {
            DBG("HTTP request exception: " + juce::String(e.what()));
            responseText = "";
            statusCode = 0;
        }
        catch (...)
        {
            DBG("Unknown HTTP request exception");
            responseText = "";
            statusCode = 0;
        }

        // Handle response on main thread
        juce::MessageManager::callAsync([this, responseText, statusCode, startTime]() {

            auto totalTime = juce::Time::getCurrentTime() - startTime;
            DBG("Total request time: " + juce::String(totalTime.inMilliseconds()) + "ms");

            if (statusCode != 0)
            {
                DBG("HTTP Status: " + juce::String(statusCode));
            }

            if (responseText.isNotEmpty())
            {
                DBG("Response preview: " + responseText.substring(0, 200) + (responseText.length() > 200 ? "..." : ""));

                // Parse response
                juce::var responseVar = juce::JSON::parse(responseText);
                if (auto* responseObj = responseVar.getDynamicObject())
                {
                    bool success = responseObj->getProperty("success");
                    if (success)
                    {
                        juce::String sessionId = responseObj->getProperty("session_id").toString();
                        showStatusMessage("Sent to Gary! Processing...", 2000);
                        DBG("Session ID: " + sessionId);

                        // START POLLING FOR RESULTS
                        startPollingForResults(sessionId);
                    }
                    else
                    {
                        juce::String error = responseObj->getProperty("error").toString();
                        showStatusMessage("Error: " + error, 5000);
                        DBG("Server error: " + error);
                    }
                }
                else
                {
                    showStatusMessage("Invalid JSON response from server", 4000);
                    DBG("Failed to parse JSON response: " + responseText.substring(0, 100));
                }
            }
            else
            {
                juce::String errorMsg;
                if (statusCode == 0)
                    errorMsg = "Failed to connect to server";
                else if (statusCode >= 400)
                    errorMsg = "Server error (HTTP " + juce::String(statusCode) + ")";
                else
                    errorMsg = "Empty response from server";

                showStatusMessage(errorMsg, 4000);
                DBG(errorMsg);
            }

            // Re-enable button
            sendToGaryButton.setEnabled(savedSamples > 0 && isConnected);
            sendToGaryButton.setButtonText("Send to Gary");
            });
        });
}

void Gary4juceAudioProcessorEditor::styleSmartLoopButton()
{
    // Remove rounded corners and make rectangular
    generateAsLoopButton.setRadioGroupId(0);  // Clear any radio group

    if (generateAsLoop)
    {
        // Active state - enabled smart loop
        generateAsLoopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
        generateAsLoopButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        generateAsLoopButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }
    else
    {
        // Inactive state - regular generation
        generateAsLoopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        generateAsLoopButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        generateAsLoopButton.setColour(juce::TextButton::textColourOnId, juce::Colours::lightgrey);
    }
}

void Gary4juceAudioProcessorEditor::styleLoopTypeButton(juce::TextButton& button, bool selected)
{
    // Make buttons perfectly rectangular (no rounded corners)
    button.setRadioGroupId(0);  // Clear any radio group

    if (selected)
    {
        // Selected state - matches smart loop button when active
        button.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
        button.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        button.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    }
    else
    {
        // Unselected state - darker to contrast with selected
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0x50, 0x50, 0x50));  // Medium grey
        button.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        button.setColour(juce::TextButton::textColourOnId, juce::Colours::lightgrey);
    }
}

void Gary4juceAudioProcessorEditor::setLoopType(const juce::String& type)
{
    currentLoopType = type;

    // Update button styling to show selection
    styleLoopTypeButton(loopTypeAutoButton, type == "auto");
    styleLoopTypeButton(loopTypeDrumsButton, type == "drums");
    styleLoopTypeButton(loopTypeInstrumentsButton, type == "instruments");

    DBG("Loop type set to: " + type);
}

// Replace your entire sendToJerry() function with this updated version:

void Gary4juceAudioProcessorEditor::sendToJerry()
{
    if (!isConnected)
    {
        showStatusMessage("Backend not connected - check connection first");
        return;
    }

    if (currentJerryPrompt.trim().isEmpty())
    {
        showStatusMessage("Please enter a text prompt for Jerry");
        return;
    }

    // 1. Get DAW BPM from processor
    double bpm = audioProcessor.getCurrentBPM();

    // 2. Append BPM to prompt (same pattern as Max for Live)
    juce::String fullPrompt = currentJerryPrompt + " " + juce::String((int)bpm) + "bpm";

    // 3. Determine endpoint based on smart loop toggle
    juce::String endpoint = generateAsLoop ? "/audio/generate/loop" : "/audio/generate";
    juce::String statusText = generateAsLoop ?
        "Generating smart loop with Jerry..." : "Generating with Jerry...";

    DBG("Jerry generating with prompt: " + fullPrompt);
    DBG("Endpoint: " + endpoint);
    DBG("CFG: " + juce::String(currentJerryCfg, 1) + ", Steps: " + juce::String(currentJerrySteps));
    if (generateAsLoop) {
        DBG("Loop Type: " + currentLoopType);
    }

    // Disable button and show processing state
    generateWithJerryButton.setEnabled(false);
    generateWithJerryButton.setButtonText("Generating...");
    showStatusMessage(statusText, 2000);

    // Create HTTP request in background thread
    juce::Thread::launch([this, fullPrompt, endpoint]() {

        auto startTime = juce::Time::getCurrentTime();

        // Create JSON payload - different structure based on endpoint
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("prompt", fullPrompt);
        jsonRequest->setProperty("steps", currentJerrySteps);
        jsonRequest->setProperty("cfg_scale", currentJerryCfg);
        jsonRequest->setProperty("return_format", "base64");
        jsonRequest->setProperty("seed", -1);  // Random seed

        // Add loop-specific parameters if using smart loop
        if (generateAsLoop) {
            jsonRequest->setProperty("loop_type", currentLoopType);
            // Note: bars parameter is omitted - backend will auto-calculate based on BPM
        }

        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        DBG("Jerry JSON payload: " + jsonString);

        // Use the appropriate endpoint
        juce::URL url("https://g4l.thecollabagepatch.com" + endpoint);

        juce::String responseText;
        int statusCode = 0;

        try
        {
            // Use JUCE 8.0.8 pattern: withPOSTData + InputStreamOptions
            juce::URL postUrl = url.withPOSTData(jsonString);

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(30000)
                .withExtraHeaders("Content-Type: application/json");

            auto stream = postUrl.createInputStream(options);

            auto requestTime = juce::Time::getCurrentTime() - startTime;
            DBG("Jerry HTTP connection established in " + juce::String(requestTime.inMilliseconds()) + "ms");

            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();

                auto totalTime = juce::Time::getCurrentTime() - startTime;
                DBG("Jerry HTTP request completed in " + juce::String(totalTime.inMilliseconds()) + "ms");
                DBG("Jerry response length: " + juce::String(responseText.length()) + " characters");

                statusCode = 200; // Assume success if we got data
            }
            else
            {
                DBG("Failed to create input stream for Jerry request");
                responseText = "";
                statusCode = 0;
            }
        }
        catch (const std::exception& e)
        {
            DBG("Jerry HTTP request exception: " + juce::String(e.what()));
            responseText = "";
            statusCode = 0;
        }
        catch (...)
        {
            DBG("Unknown Jerry HTTP request exception");
            responseText = "";
            statusCode = 0;
        }

        // Handle response on main thread
        juce::MessageManager::callAsync([this, responseText, statusCode, startTime]() {

            auto totalTime = juce::Time::getCurrentTime() - startTime;
            DBG("Total Jerry request time: " + juce::String(totalTime.inMilliseconds()) + "ms");

            if (statusCode == 200 && responseText.isNotEmpty())
            {
                DBG("Jerry response: " + responseText.substring(0, 200) + "...");

                // Parse response JSON
                auto responseVar = juce::JSON::parse(responseText);
                if (auto* responseObj = responseVar.getDynamicObject())
                {
                    bool success = responseObj->getProperty("success");
                    if (success)
                    {
                        // Get the base64 audio data
                        auto audioBase64 = responseObj->getProperty("audio_base64").toString();

                        if (audioBase64.isNotEmpty())
                        {
                            // Save generated audio (same as Gary's saveGeneratedAudio function)
                            saveGeneratedAudio(audioBase64);

                            // Get metadata if available
                            if (auto* metadata = responseObj->getProperty("metadata").getDynamicObject())
                            {
                                auto genTime = metadata->getProperty("generation_time").toString();
                                auto rtFactor = metadata->getProperty("realtime_factor").toString();

                                // Show different success message based on generation type
                                if (generateAsLoop) {
                                    auto bars = (int)metadata->getProperty("bars");
                                    auto loopDuration = (double)metadata->getProperty("loop_duration_seconds");
                                    showStatusMessage("Smart loop complete! " + juce::String(bars) + " bars (" +
                                        juce::String(loopDuration, 1) + "s) " + genTime + "s", 5000);
                                    DBG("Jerry loop metadata - Bars: " + juce::String(bars) +
                                        ", Duration: " + juce::String(loopDuration, 1) + "s");
                                }
                                else {
                                    showStatusMessage("Jerry complete! " + genTime + "s (" + rtFactor + "x RT)", 4000);
                                }

                                DBG("Jerry metadata - Generation time: " + genTime + "s, RT factor: " + rtFactor + "x");
                            }
                            else
                            {
                                juce::String successMsg = generateAsLoop ?
                                    "Smart loop generation complete!" : "Jerry generation complete!";
                                showStatusMessage(successMsg, 3000);
                            }
                        }
                        else
                        {
                            showStatusMessage("Jerry completed but no audio received", 3000);
                            DBG("Jerry success but missing audio_base64");
                        }
                    }
                    else
                    {
                        auto error = responseObj->getProperty("error").toString();
                        showStatusMessage("Jerry error: " + error, 5000);
                        DBG("Jerry server error: " + error);
                    }
                }
                else
                {
                    showStatusMessage("Invalid JSON response from Jerry", 4000);
                    DBG("Failed to parse Jerry JSON response");
                }
            }
            else
            {
                juce::String errorMsg;
                if (statusCode == 0)
                    errorMsg = "Failed to connect to Jerry";
                else if (statusCode >= 400)
                    errorMsg = "Jerry server error (HTTP " + juce::String(statusCode) + ")";
                else
                    errorMsg = "Empty response from Jerry";

                showStatusMessage(errorMsg, 4000);
                DBG("Jerry request failed: " + errorMsg);
            }

            // Re-enable button
            generateWithJerryButton.setEnabled(isConnected && !currentJerryPrompt.trim().isEmpty());
            generateWithJerryButton.setButtonText("Generate with Jerry");
            });
        });
}

void Gary4juceAudioProcessorEditor::updateLoopTypeVisibility()
{
    // In the horizontal layout, the type buttons are only visible when:
    // 1. We're on Jerry tab (handled by switchToTab)
    // 2. Smart loop toggle is enabled
    bool showLoopType = generateAsLoop;

    DBG("updateLoopTypeVisibility() - Smart loop enabled: " + juce::String(showLoopType ? "true" : "false"));

    loopTypeAutoButton.setVisible(showLoopType);
    loopTypeDrumsButton.setVisible(showLoopType);
    loopTypeInstrumentsButton.setVisible(showLoopType);

    // Since we're in a horizontal layout, we need to trigger a layout update
    // when visibility changes to ensure proper spacing
    repaint();
}

void Gary4juceAudioProcessorEditor::setTerryAudioSource(bool useRecording)
{
    transformRecording = useRecording;
    DBG("Terry audio source set to: " + juce::String(useRecording ? "Recording" : "Output"));
    updateTerrySourceButtons();
}

void Gary4juceAudioProcessorEditor::updateTerrySourceButtons()
{
    // Update transform button availability
    bool canTransform = false;

    if (transformRecording)
    {
        canTransform = (savedSamples > 0 && isConnected);
    }
    else
    {
        canTransform = (hasOutputAudio && isConnected);
    }

    transformWithTerryButton.setEnabled(canTransform && !isGenerating);

    // Enable/disable radio buttons
    transformRecordingButton.setEnabled(savedSamples > 0);
    transformOutputButton.setEnabled(hasOutputAudio);

    // REMOVE THIS LINE - let transform operations handle undo button:
    // undoTransformButton.setEnabled(!currentSessionId.isEmpty() && !isGenerating);
}

void Gary4juceAudioProcessorEditor::sendToTerry()
{
    // Reset generation state immediately (like Gary)
    isGenerating = true;
    generationProgress = 0;
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;

    // Clear any previous session ID since we're starting fresh
    currentSessionId = "";  // Clear previous session

    repaint(); // Force immediate UI update

    // Basic validation checks (like Gary and Jerry)
    if (!isConnected)
    {
        isGenerating = false;
        showStatusMessage("Backend not connected - check connection first");
        return;
    }

    // Check that we have either a variation selected OR a custom prompt
    bool hasVariation = (currentTerryVariation >= 0);
    bool hasCustomPrompt = !currentTerryCustomPrompt.trim().isEmpty();

    if (!hasVariation && !hasCustomPrompt)
    {
        isGenerating = false;
        showStatusMessage("Please select a variation OR enter a custom prompt");
        return;
    }

    // Check that we have the selected audio source
    if (transformRecording)
    {
        if (savedSamples <= 0)
        {
            isGenerating = false;
            showStatusMessage("No recording available - save your recording first");
            return;
        }
    }
    else // transforming output
    {
        if (!hasOutputAudio)
        {
            isGenerating = false;
            showStatusMessage("No output audio available - generate with Gary or Jerry first");
            return;
        }
    }

    // Get the variation names array (matches your backend list)
    const juce::StringArray variationNames = {
        "accordion_folk", "banjo_bluegrass", "piano_classical", "celtic",
        "strings_quartet", "synth_retro", "synth_modern", "synth_edm",
        "lofi_chill", "synth_bass", "rock_band", "cinematic_epic",
        "retro_rpg", "chiptune", "steel_drums", "gamelan_fusion",
        "music_box", "trap_808", "lo_fi_drums", "boom_bap",
        "percussion_ensemble", "future_bass", "synthwave_retro", "melodic_techno",
        "dubstep_wobble", "glitch_hop", "digital_disruption", "circuit_bent",
        "orchestral_glitch", "vapor_drums", "industrial_textures", "jungle_breaks"
    };

    // Determine which audio file to read
    juce::File audioFile;
    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");

    if (transformRecording)
    {
        audioFile = garyDir.getChildFile("myBuffer.wav");
        DBG("Terry transforming recording: myBuffer.wav");
    }
    else
    {
        audioFile = garyDir.getChildFile("myOutput.wav");
        DBG("Terry transforming output: myOutput.wav");
    }

    if (!audioFile.exists())
    {
        isGenerating = false;
        showStatusMessage("Audio file not found - " + audioFile.getFileName());
        return;
    }

    // Read and encode audio file (same as Gary)
    juce::MemoryBlock audioData;
    if (!audioFile.loadFileAsData(audioData))
    {
        isGenerating = false;
        showStatusMessage("Failed to read audio file");
        return;
    }

    if (audioData.getSize() == 0)
    {
        isGenerating = false;
        showStatusMessage("Audio file is empty");
        return;
    }

    auto base64Audio = juce::Base64::toBase64(audioData.getData(), audioData.getSize());

    // Debug: Log audio data size
    DBG("Terry audio file size: " + juce::String(audioData.getSize()) + " bytes");
    DBG("Terry base64 length: " + juce::String(base64Audio.length()) + " chars");
    DBG("Terry flowstep: " + juce::String(currentTerryFlowstep, 3));
    DBG("Terry solver: " + juce::String(useMidpointSolver ? "midpoint" : "euler"));

    // Disable button and show processing state (like Gary and Jerry)
    transformWithTerryButton.setEnabled(false);
    transformWithTerryButton.setButtonText("Transforming...");
    showStatusMessage("Sending audio to Terry for transformation...");

    // Create HTTP request in background thread (same pattern as Gary and Jerry)
    juce::Thread::launch([this, base64Audio, variationNames, hasVariation, hasCustomPrompt]() {

        auto startTime = juce::Time::getCurrentTime();

        // Create JSON payload
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("audio_data", base64Audio);
        jsonRequest->setProperty("flowstep", currentTerryFlowstep);
        jsonRequest->setProperty("solver", useMidpointSolver ? "midpoint" : "euler");

        // Add either variation or custom_prompt (not both)
        if (hasVariation && currentTerryVariation < variationNames.size())
        {
            jsonRequest->setProperty("variation", variationNames[currentTerryVariation]);
            DBG("Terry using variation: " + variationNames[currentTerryVariation]);
        }
        else if (hasCustomPrompt)
        {
            jsonRequest->setProperty("custom_prompt", currentTerryCustomPrompt);
            DBG("Terry using custom prompt: " + currentTerryCustomPrompt);
        }

        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        DBG("Terry JSON payload size: " + juce::String(jsonString.length()) + " characters");

        juce::URL url("https://g4l.thecollabagepatch.com/api/juce/transform_audio");

        juce::String responseText;
        int statusCode = 0;

        try
        {
            // Use JUCE 8.0.8 pattern: withPOSTData + InputStreamOptions (same as Gary/Jerry)
            juce::URL postUrl = url.withPOSTData(jsonString);

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(30000)
                .withExtraHeaders("Content-Type: application/json");

            auto stream = postUrl.createInputStream(options);

            auto requestTime = juce::Time::getCurrentTime() - startTime;
            DBG("Terry HTTP connection established in " + juce::String(requestTime.inMilliseconds()) + "ms");

            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();

                auto totalTime = juce::Time::getCurrentTime() - startTime;
                DBG("Terry HTTP request completed in " + juce::String(totalTime.inMilliseconds()) + "ms");
                DBG("Terry response length: " + juce::String(responseText.length()) + " characters");

                statusCode = 200; // Assume success if we got data
            }
            else
            {
                DBG("Failed to create input stream for Terry request");
                responseText = "";
                statusCode = 0;
            }
        }
        catch (const std::exception& e)
        {
            DBG("Terry HTTP request exception: " + juce::String(e.what()));
            responseText = "";
            statusCode = 0;
        }
        catch (...)
        {
            DBG("Unknown Terry HTTP request exception");
            responseText = "";
            statusCode = 0;
        }

        // Handle response on main thread (same pattern as Gary)
        juce::MessageManager::callAsync([this, responseText, statusCode, startTime]() {

            auto totalTime = juce::Time::getCurrentTime() - startTime;
            DBG("Total Terry request time: " + juce::String(totalTime.inMilliseconds()) + "ms");

            if (statusCode == 200 && responseText.isNotEmpty())
            {
                DBG("Terry response preview: " + responseText.substring(0, 200) + (responseText.length() > 200 ? "..." : ""));

                // Parse response JSON (same as Gary)
                auto responseVar = juce::JSON::parse(responseText);
                if (auto* responseObj = responseVar.getDynamicObject())
                {
                    bool success = responseObj->getProperty("success");
                    if (success)
                    {
                        juce::String sessionId = responseObj->getProperty("session_id").toString();
                        showStatusMessage("Sent to Terry! Processing...", 2000);
                        DBG("Terry session ID: " + sessionId);

                        // START POLLING FOR RESULTS (same as Gary)
                        startPollingForResults(sessionId);  // This sets currentSessionId = sessionId

                        // Disable undo button until transform completes
                        undoTransformButton.setEnabled(false);
                    }
                    else
                    {
                        juce::String error = responseObj->getProperty("error").toString();
                        showStatusMessage("Terry error: " + error, 5000);
                        DBG("Terry server error: " + error);
                    }
                }
                else
                {
                    showStatusMessage("Invalid JSON response from Terry", 4000);
                    DBG("Failed to parse Terry JSON response: " + responseText.substring(0, 100));
                }
            }
            else
            {
                juce::String errorMsg;
                if (statusCode == 0)
                    errorMsg = "Failed to connect to Terry";
                else if (statusCode >= 400)
                    errorMsg = "Terry server error (HTTP " + juce::String(statusCode) + ")";
                else
                    errorMsg = "Empty response from Terry";

                showStatusMessage(errorMsg, 4000);
                DBG("Terry request failed: " + errorMsg);
            }

            // Re-enable transform button (same pattern as Gary/Jerry)
            updateTerrySourceButtons(); // This will set the right enabled state
            transformWithTerryButton.setButtonText("Transform with Terry");
            });
        });
}

void Gary4juceAudioProcessorEditor::undoTerryTransform()
{
    // We need a session ID to undo - this should be the current session
    if (currentSessionId.isEmpty())
    {
        showStatusMessage("No transform session to undo", 3000);
        return;
    }

    DBG("Attempting to undo Terry transform for session: " + currentSessionId);
    showStatusMessage("Undoing transform...", 2000);

    // Disable undo button during request
    undoTransformButton.setEnabled(false);
    undoTransformButton.setButtonText("Undoing...");

    // Create HTTP request in background thread (same pattern as other requests)
    juce::Thread::launch([this]() {

        auto startTime = juce::Time::getCurrentTime();

        // Create JSON payload - just needs session_id
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("session_id", currentSessionId);

        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        DBG("Terry undo JSON payload: " + jsonString);

        juce::URL url("https://g4l.thecollabagepatch.com/api/juce/undo_transform");

        juce::String responseText;
        int statusCode = 0;

        try
        {
            // Use JUCE 8.0.8 pattern: withPOSTData + InputStreamOptions
            juce::URL postUrl = url.withPOSTData(jsonString);

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(15000)  // Shorter timeout for undo - should be quick
                .withExtraHeaders("Content-Type: application/json");

            auto stream = postUrl.createInputStream(options);

            auto requestTime = juce::Time::getCurrentTime() - startTime;
            DBG("Terry undo HTTP connection established in " + juce::String(requestTime.inMilliseconds()) + "ms");

            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();

                auto totalTime = juce::Time::getCurrentTime() - startTime;
                DBG("Terry undo HTTP request completed in " + juce::String(totalTime.inMilliseconds()) + "ms");

                statusCode = 200; // Assume success if we got data
            }
            else
            {
                DBG("Failed to create input stream for Terry undo request");
                responseText = "";
                statusCode = 0;
            }
        }
        catch (const std::exception& e)
        {
            DBG("Terry undo HTTP request exception: " + juce::String(e.what()));
            responseText = "";
            statusCode = 0;
        }
        catch (...)
        {
            DBG("Unknown Terry undo HTTP request exception");
            responseText = "";
            statusCode = 0;
        }

        // Handle response on main thread
        juce::MessageManager::callAsync([this, responseText, statusCode]() {

            if (statusCode == 200 && responseText.isNotEmpty())
            {
                DBG("Terry undo response: " + responseText);

                // Parse response JSON
                auto responseVar = juce::JSON::parse(responseText);
                if (auto* responseObj = responseVar.getDynamicObject())
                {
                    bool success = responseObj->getProperty("success");
                    if (success)
                    {
                        // Get the restored audio data
                        auto audioData = responseObj->getProperty("audio_data").toString();
                        if (audioData.isNotEmpty())
                        {
                            // Save the restored audio
                            saveGeneratedAudio(audioData);
                            showStatusMessage("Transform undone - audio restored!", 3000);
                            DBG("Terry undo successful - audio restored");

                            // Clear session ID since undo is complete
                            currentSessionId = "";
                            undoTransformButton.setEnabled(false);  // Disable undo button
                        }
                        else
                        {
                            showStatusMessage("Undo completed but no audio data received", 3000);
                            DBG("Terry undo success but missing audio data");
                        }
                    }
                    else
                    {
                        auto error = responseObj->getProperty("error").toString();
                        showStatusMessage("Undo failed: " + error, 4000);
                        DBG("Terry undo server error: " + error);
                    }
                }
                else
                {
                    showStatusMessage("Invalid undo response format", 3000);
                    DBG("Failed to parse Terry undo JSON response");
                }
            }
            else
            {
                juce::String errorMsg;
                if (statusCode == 0)
                    errorMsg = "Failed to connect for undo";
                else if (statusCode >= 400)
                    errorMsg = "Undo server error (HTTP " + juce::String(statusCode) + ")";
                else
                    errorMsg = "Empty undo response";

                showStatusMessage(errorMsg, 4000);
                DBG("Terry undo request failed: " + errorMsg);
            }

            // Re-enable undo button
            undoTransformButton.setEnabled(true);
            undoTransformButton.setButtonText("Undo Transform");
            });
        });
}

void Gary4juceAudioProcessorEditor::clearRecordingBuffer()
{
    audioProcessor.clearRecordingBuffer();
    savedSamples = 0;  // Reset saved samples too
    updateRecordingStatus();
}

// ========== UPDATED CONNECTION STATUS METHOD ==========
void Gary4juceAudioProcessorEditor::updateConnectionStatus(bool connected)
{
    if (isConnected != connected)
    {
        isConnected = connected;
        DBG("Backend connection status updated: " + juce::String(connected ? "Connected" : "Disconnected"));

        // Update Gary button state when connection changes
        sendToGaryButton.setEnabled(savedSamples > 0 && isConnected);

        // Update Jerry button state when connection changes
        generateWithJerryButton.setEnabled(isConnected && !currentJerryPrompt.trim().isEmpty());

        // Update Terry button state when connection changes
        updateTerrySourceButtons();

        repaint(); // Trigger a redraw to update tab section border

        // Re-enable check button if it was disabled
        if (!checkConnectionButton.isEnabled())
        {
            checkConnectionButton.setButtonText("Check Backend Connection");
            checkConnectionButton.setEnabled(true);
        }
    }
}

void Gary4juceAudioProcessorEditor::loadOutputAudioFile()
{
    if (!outputAudioFile.exists())
    {
        hasOutputAudio = false;
        playOutputButton.setEnabled(false);
        stopOutputButton.setEnabled(false); 
        clearOutputButton.setEnabled(false);
        cropButton.setEnabled(false);
        continueButton.setEnabled(false);
        totalAudioDuration = 0.0;
        return;
    }

    // Read the audio file
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(outputAudioFile));

    if (reader != nullptr)
    {
        // Resize buffer to fit the audio
        outputAudioBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);

        // Read the audio data
        reader->read(&outputAudioBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

        // Calculate total duration
        totalAudioDuration = (double)reader->lengthInSamples / reader->sampleRate;

        hasOutputAudio = true;
        playOutputButton.setEnabled(true);
        stopOutputButton.setEnabled(true);  // Always enabled when we have audio
        clearOutputButton.setEnabled(true);
        cropButton.setEnabled(true);
        continueButton.setEnabled(true);

        DBG("Loaded output audio: " + juce::String(reader->lengthInSamples) + " samples, " +
            juce::String(reader->numChannels) + " channels, " +
            juce::String(totalAudioDuration, 2) + " seconds");
    }
    else
    {
        DBG("Failed to read output audio file");
        hasOutputAudio = false;
        playOutputButton.setEnabled(false);
        clearOutputButton.setEnabled(false);
        cropButton.setEnabled(false);
        continueButton.setEnabled(false);
        totalAudioDuration = 0.0;
    }
}

void Gary4juceAudioProcessorEditor::drawOutputWaveform(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    // Black background
    g.setColour(juce::Colours::black);
    g.fillRect(area);

    // Draw border
    g.setColour(juce::Colour(0x40, 0x40, 0x40));
    g.drawRect(area, 1);

    if (isGenerating)
    {
        // PROGRESS VISUALIZATION during generation

        // If we have existing output, draw it first (dimmed)
        if (hasOutputAudio && outputAudioBuffer.getNumSamples() > 0)
        {
            drawExistingOutput(g, area, 0.3f); // 30% opacity
        }

        // Draw progress overlay
        const int progressWidth = (area.getWidth() - 2) * generationProgress / 100;

        if (progressWidth > 0)
        {
            // Progress background
            g.setColour(juce::Colours::red.withAlpha(0.4f));
            g.fillRect(area.getX() + 1, area.getY() + 1, progressWidth, area.getHeight() - 2);

            // Progress border (growing edge)
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.drawVerticalLine(area.getX() + 1 + progressWidth, (float)area.getY(), (float)area.getBottom());
        }

        // Progress text
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.setColour(juce::Colours::white);
        g.drawText("Generating: " + juce::String(generationProgress) + "%",
            area, juce::Justification::centred);
    }
    else if (hasOutputAudio && outputAudioBuffer.getNumSamples() > 0)
    {
        // NORMAL OUTPUT WAVEFORM display
        drawExistingOutput(g, area, 1.0f); // Full opacity
        
        // Add drag feedback overlay
        if (isDragging)
        {
            // Drag highlight overlay
            g.setColour(juce::Colours::yellow.withAlpha(0.3f));
            g.fillRoundedRectangle(area.toFloat(), 4.0f);
            
            // Drag border
            g.setColour(juce::Colours::yellow);
            g.drawRoundedRectangle(area.toFloat(), 4.0f, 2.0f);
            
            // Drag text
            g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            g.setColour(juce::Colours::white);
            g.drawText("Dragging to DAW...", area, juce::Justification::centred);
        }
    }
    else
    {
        // NO OUTPUT state
        g.setFont(juce::FontOptions(14.0f));
        g.setColour(juce::Colours::darkgrey);
        g.drawText("Generated audio will appear here", area, juce::Justification::centred);
    }
}

void Gary4juceAudioProcessorEditor::drawExistingOutput(juce::Graphics& g, const juce::Rectangle<int>& area, float opacity)
{
    const int waveWidth = area.getWidth() - 2;
    const int waveHeight = area.getHeight() - 2;
    const int centerY = area.getCentreY();

    if (waveWidth <= 0 || outputAudioBuffer.getNumSamples() <= 0)
        return;

    // Calculate samples per pixel
    const int samplesPerPixel = juce::jmax(1, outputAudioBuffer.getNumSamples() / waveWidth);

    // Draw waveform in brand red color
    g.setColour(juce::Colours::red.withAlpha(opacity));

    for (int x = 0; x < waveWidth; ++x)
    {
        const int startSample = x * samplesPerPixel;
        const int endSample = juce::jmin(startSample + samplesPerPixel, outputAudioBuffer.getNumSamples());

        if (endSample > startSample)
        {
            // Find min/max in this pixel's worth of samples
            float minVal = 0.0f, maxVal = 0.0f;

            for (int sample = startSample; sample < endSample; ++sample)
            {
                // Average across channels
                float sampleValue = 0.0f;
                for (int ch = 0; ch < outputAudioBuffer.getNumChannels(); ++ch)
                {
                    sampleValue += outputAudioBuffer.getSample(ch, sample);
                }
                sampleValue /= outputAudioBuffer.getNumChannels();

                minVal = juce::jmin(minVal, sampleValue);
                maxVal = juce::jmax(maxVal, sampleValue);
            }

            // Scale to display area
            const int minY = juce::jlimit(area.getY(), area.getBottom(),
                centerY - (int)(minVal * waveHeight * 0.4f));
            const int maxY = juce::jlimit(area.getY(), area.getBottom(),
                centerY - (int)(maxVal * waveHeight * 0.4f));

            const int drawX = area.getX() + 1 + x;

            // Draw waveform line
            if (maxY != minY)
            {
                g.drawVerticalLine(drawX, (float)maxY, (float)minY);
            }
            else
            {
                g.fillRect(drawX, centerY - 1, 1, 2);
            }
        }
    }

    // Draw playback cursor when playing, paused, OR when we have a seek position
    if ((isPlayingOutput || isPausedOutput || currentPlaybackPosition > 0.0) && totalAudioDuration > 0.0)
    {
        // Calculate cursor position as a percentage of total duration
        double progressPercent = currentPlaybackPosition / totalAudioDuration;
        progressPercent = juce::jlimit(0.0, 1.0, progressPercent);

        // Convert to pixel position
        int cursorX = area.getX() + 1 + (int)(progressPercent * waveWidth);

        // Different cursor appearance for different states
        if (isPlayingOutput)
        {
            // Playing cursor - bright white
            g.setColour(juce::Colours::white.withAlpha(0.9f));
        }
        else if (isPausedOutput)
        {
            // Paused cursor - slightly dimmer but still visible
            g.setColour(juce::Colours::white.withAlpha(0.7f));
        }
        else
        {
            // Seek position cursor - dimmer still but visible
            g.setColour(juce::Colours::white.withAlpha(0.5f));
        }

        g.drawVerticalLine(cursorX, (float)area.getY() + 1, (float)area.getBottom() - 1);

        // Add glow effect
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        if (cursorX > area.getX() + 1)
            g.drawVerticalLine(cursorX - 1, (float)area.getY() + 1, (float)area.getBottom() - 1);
        if (cursorX < area.getRight() - 1)
            g.drawVerticalLine(cursorX + 1, (float)area.getY() + 1, (float)area.getBottom() - 1);
    }
}

void Gary4juceAudioProcessorEditor::playOutputAudio()
{
    if (!hasOutputAudio || !outputAudioFile.exists())
    {
        showStatusMessage("No output audio to play");
        return;
    }

    // If currently playing, pause it
    if (isPlayingOutput)
    {
        pauseOutputPlayback();
        return;
    }

    // If paused, resume from paused position
    if (isPausedOutput)
    {
        resumeOutputPlayback();
        return;
    }

    // Starting fresh playback
    startOutputPlayback();
}

void Gary4juceAudioProcessorEditor::startOutputPlayback()
{
    // Stop any current playback (but don't clear everything)
    if (transportSource && isPlayingOutput)
    {
        transportSource->stop();
    }

    // Initialize audio playback if needed
    if (!audioDeviceManager)
    {
        initializeAudioPlayback();
    }

    // Create a new reader for the output file
    if (audioFormatManager)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(
            audioFormatManager->createReaderFor(outputAudioFile));

        if (reader != nullptr)
        {
            // Get the sample rate BEFORE releasing the reader
            double sampleRate = reader->sampleRate;

            // Create reader source (this takes ownership of the reader)
            readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);

            // Set up transport source
            if (!transportSource)
                transportSource = std::make_unique<juce::AudioTransportSource>();

            transportSource->setSource(readerSource.get(), 0, nullptr, sampleRate);

            // Start from seek position if we have one, otherwise from beginning
            double startPosition = (isPausedOutput || currentPlaybackPosition > 0.0) ?
                currentPlaybackPosition : 0.0;

            transportSource->setPosition(startPosition);
            transportSource->start();

            isPlayingOutput = true;
            isPausedOutput = false;

            // Update button text
            playOutputButton.setButtonText("Pause");

            showStatusMessage("Playing output...", 2000);
            DBG("Started output playback from " + juce::String(startPosition, 2) + "s");
        }
        else
        {
            showStatusMessage("Failed to read output audio");
        }
    }
}

// New function: Pause playback (keep position)
void Gary4juceAudioProcessorEditor::pauseOutputPlayback()
{
    if (transportSource && isPlayingOutput)
    {
        // Save current position before stopping
        pausedPosition = transportSource->getCurrentPosition();
        currentPlaybackPosition = pausedPosition;

        transportSource->stop();

        isPlayingOutput = false;
        isPausedOutput = true;

        playOutputButton.setButtonText("Resume");

        showStatusMessage("Paused", 1500);
        DBG("Paused output playback at " + juce::String(pausedPosition, 2) + "s");

        repaint(); // Keep cursor visible at paused position
    }
}

// New function: Resume from paused position
// Updated resumeOutputPlayback() - remove the callAfterDelay
// Updated resumeOutputPlayback() - reinitialize if needed
void Gary4juceAudioProcessorEditor::resumeOutputPlayback()
{
    if (isPausedOutput)
    {
        // Check if transport source still has audio loaded
        if (!transportSource || !readerSource)
        {
            // Need to reinitialize - transport was cleared
            if (!audioDeviceManager)
            {
                initializeAudioPlayback();
            }

            if (audioFormatManager)
            {
                std::unique_ptr<juce::AudioFormatReader> reader(
                    audioFormatManager->createReaderFor(outputAudioFile));

                if (reader != nullptr)
                {
                    double sampleRate = reader->sampleRate;
                    readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);

                    if (!transportSource)
                        transportSource = std::make_unique<juce::AudioTransportSource>();

                    transportSource->setSource(readerSource.get(), 0, nullptr, sampleRate);
                }
                else
                {
                    showStatusMessage("Failed to read output audio");
                    return;
                }
            }
        }

        // Resume from saved position
        transportSource->setPosition(pausedPosition);
        transportSource->start();

        isPlayingOutput = true;
        isPausedOutput = false;

        playOutputButton.setButtonText("Pause");

        showStatusMessage("Resumed from " + juce::String(pausedPosition, 1) + "s", 1500);
        DBG("Resumed output playback from " + juce::String(pausedPosition, 2) + "s");
    }
}

// Updated stopOutputPlayback() - distinguish between pause and full stop
void Gary4juceAudioProcessorEditor::stopOutputPlayback()
{
    if (transportSource)
    {
        transportSource->stop();
        transportSource->setSource(nullptr);
        readerSource.reset();
    }

    isPlayingOutput = false;
    isPausedOutput = false;

    playOutputButton.setButtonText("Play Output");

    // Reset to beginning
    currentPlaybackPosition = 0.0;
    pausedPosition = 0.0;

    repaint();

    DBG("Stopped output playback completely");
}

// New function: Full stop (different from pause)
void Gary4juceAudioProcessorEditor::fullStopOutputPlayback()
{
    stopOutputPlayback(); // This does everything we need for a full stop
}

void Gary4juceAudioProcessorEditor::initializeAudioPlayback()
{
    // Initialize audio format manager
    audioFormatManager = std::make_unique<juce::AudioFormatManager>();
    audioFormatManager->registerBasicFormats();

    // Initialize audio device manager
    audioDeviceManager = std::make_unique<juce::AudioDeviceManager>();

    // Use default device with common settings
    juce::String error = audioDeviceManager->initialise(
        0,    // numInputChannelsNeeded
        2,    // numOutputChannelsNeeded  
        nullptr, // savedState
        true  // selectDefaultDeviceOnFailure
    );

    if (error.isNotEmpty())
    {
        DBG("Audio device manager error: " + error);
        // Continue anyway - might still work
    }

    // Create transport source
    transportSource = std::make_unique<juce::AudioTransportSource>();

    // Create audio source player (this is the AudioIODeviceCallback)
    audioSourcePlayer = std::make_unique<juce::AudioSourcePlayer>();

    // Set the transport source as the audio source for the player
    audioSourcePlayer->setSource(transportSource.get());

    // Add the audio source player as audio callback (not the transport source directly)
    audioDeviceManager->addAudioCallback(audioSourcePlayer.get());

    DBG("Audio playback initialized");
}

// Updated checkPlaybackStatus() - full stop when audio finishes naturally
void Gary4juceAudioProcessorEditor::checkPlaybackStatus()
{
    if (transportSource && isPlayingOutput)
    {
        if (!transportSource->isPlaying())
        {
            // Playback finished naturally - do a full stop and reset to beginning
            stopOutputPlayback(); // This resets everything to beginning
            showStatusMessage("Playback finished", 1500);
        }
        else
        {
            // Update current playback position
            currentPlaybackPosition = transportSource->getCurrentPosition();
            repaint();
        }
    }
}

void Gary4juceAudioProcessorEditor::clearOutputAudio()
{
    hasOutputAudio = false;
    outputAudioBuffer.clear();
    playOutputButton.setEnabled(false);
    clearOutputButton.setEnabled(false);
    cropButton.setEnabled(false);
    continueButton.setEnabled(false);

    // Reset playback tracking
    currentPlaybackPosition = 0.0;
    totalAudioDuration = 0.0;

    // Optionally delete the file
    if (outputAudioFile.exists())
    {
        outputAudioFile.deleteFile();
    }

    showStatusMessage("Output cleared", 2000);
    repaint();
}

void Gary4juceAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    // Reset drag state
    isDragging = false;
    dragStarted = false;
    dragStartPosition = event.getPosition();

    // Check if click is within the output waveform area
    if (outputWaveformArea.contains(event.getPosition()) && hasOutputAudio && totalAudioDuration > 0.0)
    {
        // Calculate click position relative to waveform area
        auto clickPos = event.getPosition();
        auto relativeX = clickPos.x - outputWaveformArea.getX() - 1; // Account for border
        auto waveformWidth = outputWaveformArea.getWidth() - 2; // Account for borders

        // Convert click position to percentage (0.0 to 1.0)
        double clickPercent = (double)relativeX / (double)waveformWidth;
        clickPercent = juce::jlimit(0.0, 1.0, clickPercent);

        // Convert percentage to time in seconds
        double seekTime = clickPercent * totalAudioDuration;

        // Store click for potential drag - only seek if it's not a drag
        // The actual seek will happen in mouseUp if no drag occurred
        
        return; // We handled this click
    }

    // If we didn't handle it, pass to parent
    juce::Component::mouseDown(event);
}

void Gary4juceAudioProcessorEditor::mouseDrag(const juce::MouseEvent& event)
{
    // Check if we're dragging from the output waveform area
    if (isMouseOverOutputWaveform(dragStartPosition) && hasOutputAudio && outputAudioFile.existsAsFile())
    {
        // Calculate drag distance to determine if this is a drag operation
        auto dragDistance = dragStartPosition.getDistanceFrom(event.getPosition());
        
        if (dragDistance > 10 && !dragStarted) // 10 pixel threshold for drag
        {
            isDragging = true;
            repaint(); // Update visual feedback
            
            startAudioDrag();
            
            dragStarted = true;
            isDragging = false;
            repaint(); // Remove visual feedback
            
            return;
        }
    }
    
    juce::Component::mouseDrag(event);
}

void Gary4juceAudioProcessorEditor::mouseUp(const juce::MouseEvent& event)
{
    // If we didn't drag, and we're in the output waveform area, perform seek
    if (!dragStarted && isMouseOverOutputWaveform(event.getPosition()) && 
        hasOutputAudio && totalAudioDuration > 0.0)
    {
        // Calculate click position relative to waveform area (same as original logic)
        auto clickPos = event.getPosition();
        auto relativeX = clickPos.x - outputWaveformArea.getX() - 1;
        auto waveformWidth = outputWaveformArea.getWidth() - 2;

        double clickPercent = (double)relativeX / (double)waveformWidth;
        clickPercent = juce::jlimit(0.0, 1.0, clickPercent);

        double seekTime = clickPercent * totalAudioDuration;
        seekToPosition(seekTime);

        DBG("Click-to-seek: " + juce::String(clickPercent * 100.0, 1) + "% = " +
            juce::String(seekTime, 2) + "s");
    }
    
    // Reset drag state
    isDragging = false;
    dragStarted = false;
    repaint();
    
    juce::Component::mouseUp(event);
}

void Gary4juceAudioProcessorEditor::seekToPosition(double timeInSeconds)
{
    // Clamp time to valid range
    timeInSeconds = juce::jlimit(0.0, totalAudioDuration, timeInSeconds);

    if (isPlayingOutput)
    {
        // Currently playing - seek and continue playing
        if (transportSource)
        {
            transportSource->setPosition(timeInSeconds);
            currentPlaybackPosition = timeInSeconds;

            showStatusMessage("Seek to " + juce::String(timeInSeconds, 1) + "s", 1500);
            repaint(); // Update cursor position immediately
        }
    }
    else if (isPausedOutput)
    {
        // Currently paused - move cursor and stay paused
        pausedPosition = timeInSeconds;
        currentPlaybackPosition = timeInSeconds;

        showStatusMessage("Seek to " + juce::String(timeInSeconds, 1) + "s (paused)", 1500);
        repaint(); // Update cursor position immediately
    }
    else if (hasOutputAudio)
    {
        // Not playing but we have audio - move cursor and prepare for playback
        currentPlaybackPosition = timeInSeconds;
        pausedPosition = timeInSeconds;
        isPausedOutput = true; // Set paused state so next play resumes from here

        showStatusMessage("Seek to " + juce::String(timeInSeconds, 1) + "s", 1500);
        repaint(); // Update cursor position immediately
    }
}


// ========== UPDATED PAINT METHOD ==========
void Gary4juceAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colours::black);

    // Title (using reserved area)
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawFittedText("Gary4JUCE", titleArea, juce::Justification::centred, 1);

    // Connection status display (using reserved area)
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    if (isConnected)
    {
        g.setColour(juce::Colours::lightgreen);
        g.drawFittedText("CONNECTED", connectionStatusArea, juce::Justification::centred, 1);
    }
    else
    {
        g.setColour(juce::Colours::orange);
        g.drawFittedText("DISCONNECTED", connectionStatusArea, juce::Justification::centred, 1);
    }

    // Recording status section header (using reserved area)
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawFittedText("Recording Buffer", recordingLabelArea, juce::Justification::centred, 1);

    // Draw the INPUT waveform
    drawWaveform(g, waveformArea);

    // Status text below input waveform (using reserved area)
    g.setFont(juce::FontOptions(12.0f));

    if (hasStatusMessage && !statusMessage.isEmpty())
    {
        g.setColour(juce::Colours::lightgreen);
        g.drawText(statusMessage, inputStatusArea, juce::Justification::centred);
    }
    else
    {
        juce::String statusText;
        if (isRecording)
        {
            statusText = "RECORDING";
            g.setColour(juce::Colours::red);
        }
        else if (recordedSamples > 0)
        {
            statusText = "READY";
            g.setColour(juce::Colours::green);
        }
        else
        {
            statusText = "Press PLAY in DAW to start recording";
            g.setColour(juce::Colours::grey);
        }
        g.drawText(statusText, inputStatusArea, juce::Justification::centred);
    }

    if (recordedSamples > 0)
    {
        double recordedSeconds = (double)recordedSamples / 44100.0;
        double savedSeconds = (double)savedSamples / 44100.0;

        juce::String infoText;
        if (savedSamples < recordedSamples)
        {
            infoText = juce::String::formatted("%.1fs recorded (%.1fs saved) - %d samples",
                recordedSeconds, savedSeconds, recordedSamples);
        }
        else
        {
            infoText = juce::String::formatted("%.1fs - %d samples - Saved",
                recordedSeconds, recordedSamples);
        }

        g.setFont(juce::FontOptions(11.0f));
        g.setColour(juce::Colours::lightgrey);
        // Remove this line: auto infoArea = juce::Rectangle<int>(0, statusArea.getBottom() + 5, getWidth(), 15);
        g.drawText(infoText, inputInfoArea, juce::Justification::centred);
    }

    // ========== DRAW TAB SECTION BACKGROUND ==========
    auto fullTabArea = fullTabAreaRect;
    g.setColour(juce::Colour(0x15, 0x15, 0x15));
    g.fillRoundedRectangle(fullTabArea.toFloat(), 5.0f);

    // Tab section border (color depends on current tab and connection)
    if (isConnected)
    {
        switch (currentTab)
        {
        case ModelTab::Gary:
            g.setColour(juce::Colours::darkred.withAlpha(0.6f));
            break;
        case ModelTab::Jerry:
            g.setColour(juce::Colours::darkgreen.withAlpha(0.6f));
            break;
        case ModelTab::Terry:
            g.setColour(juce::Colours::darkblue.withAlpha(0.6f));
            break;
        }
    }
    else
    {
        g.setColour(juce::Colour(0x30, 0x30, 0x30));
    }
    g.drawRoundedRectangle(fullTabArea.toFloat(), 5.0f, 1.0f);

    // Draw the OUTPUT waveform
    drawOutputWaveform(g, outputWaveformArea);

    // Output info below output waveform
    if (hasOutputAudio && outputAudioBuffer.getNumSamples() > 0)
    {
        double outputSeconds = (double)outputAudioBuffer.getNumSamples() / 44100.0;
        juce::String outputInfo = juce::String::formatted("Output: %.1fs - %d samples",
            outputSeconds, outputAudioBuffer.getNumSamples());

        g.setFont(juce::FontOptions(11.0f));
        g.setColour(juce::Colours::lightgrey);
       // auto outputInfoArea = juce::Rectangle<int>(0, outputWaveformArea.getBottom() + 5, getWidth(), 15);
        g.drawText(outputInfo, outputInfoArea, juce::Justification::centred);
    }

    // Draw crop icon overlay
    if (cropIcon && hasOutputAudio)
    {
        auto cropBounds = cropButton.getBounds();

        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(cropBounds.toFloat(), 3.0f);

        if (cropButton.isOver() || cropButton.isDown())
        {
            g.setColour(juce::Colours::orange.withAlpha(0.8f));
            g.drawRoundedRectangle(cropBounds.toFloat(), 3.0f, 1.5f);
        }

        auto iconArea = cropBounds.reduced(6);
        cropIcon->drawWithin(g, iconArea.toFloat(),
            juce::RectanglePlacement::centred, 1.0f);
    }
}

// ========== UPDATED RESIZED METHOD ==========
void Gary4juceAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // ========== TOP SECTION (FlexBox Implementation) ==========
// This replaces the manual "removeFromTop(300)" approach

// Create main FlexBox for entire top section (vertical layout)
    juce::FlexBox topSectionFlexBox;
    topSectionFlexBox.flexDirection = juce::FlexBox::Direction::column;
    topSectionFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    // 1. Title area (fixed height)
    juce::Component titleComponent;
    juce::FlexItem titleItem(titleComponent);
    titleItem.height = 40;  // Title + some padding
    titleItem.margin = juce::FlexItem::Margin(8, 0, 0, 0);  // 10px top margin

    // 2. Connection status area (fixed height)
    juce::Component connectionComponent;
    juce::FlexItem connectionItem(connectionComponent);
    connectionItem.height = 25;  // Connection status height
    connectionItem.margin = juce::FlexItem::Margin(0, 0, 0, 0);

    // 3. Recording label area (fixed height)
    juce::Component recordingLabelComponent;
    juce::FlexItem recordingLabelItem(recordingLabelComponent);
    recordingLabelItem.height = 20;  // "Recording Buffer" label
    recordingLabelItem.margin = juce::FlexItem::Margin(8, 0, 0, 0);  // 10px top margin

    // 4. Waveform area (flexible - but with constraints)
    juce::Component waveformDisplayComponent;
    juce::FlexItem waveformDisplayItem(waveformDisplayComponent);
    waveformDisplayItem.flexGrow = 1;  // Can grow
    waveformDisplayItem.minHeight = 80;  // Minimum waveform height
    waveformDisplayItem.maxHeight = 180;  // Maximum waveform height
    waveformDisplayItem.margin = juce::FlexItem::Margin(8, 20, 8, 20);  // Margins: top, right, bottom, left

    // 5. Status message area (fixed height - for "RECORDING", "READY", etc.)
    juce::Component statusComponent;
    juce::FlexItem statusItem(statusComponent);
    statusItem.height = 25;  // Status text height
    statusItem.margin = juce::FlexItem::Margin(0, 0, 0, 0);

    // 6. Input info area (fixed height - for sample count info)
    juce::Component inputInfoComponent;
    juce::FlexItem inputInfoItem(inputInfoComponent);
    inputInfoItem.height = 20;  // Sample info text height
    inputInfoItem.margin = juce::FlexItem::Margin(3, 0, 8, 0);  // 5px top, 10px bottom

    // 7. Buffer control buttons area (fixed height)
    juce::Component bufferControlsComponent;
    juce::FlexItem bufferControlsItem(bufferControlsComponent);
    bufferControlsItem.height = 70;  // Height for 2 rows of buttons
    bufferControlsItem.margin = juce::FlexItem::Margin(0, 20, 15, 20);  // Margins around buttons

    // Add all items to the top section FlexBox
    topSectionFlexBox.items.add(titleItem);
    topSectionFlexBox.items.add(connectionItem);
    topSectionFlexBox.items.add(recordingLabelItem);
    topSectionFlexBox.items.add(waveformDisplayItem);
    topSectionFlexBox.items.add(statusItem);
    topSectionFlexBox.items.add(inputInfoItem);
    topSectionFlexBox.items.add(bufferControlsItem);

    // Calculate how much space the top section needs (flexible based on content)
    auto availableHeight = bounds.getHeight();
    auto estimatedTopSectionHeight = juce::jmin(320, availableHeight - 380);  // Leave 380px for tabs+output

    auto topSectionBounds = bounds.removeFromTop(estimatedTopSectionHeight);

    // Perform the layout for top section
    topSectionFlexBox.performLayout(topSectionBounds);

    // Extract the calculated bounds for each area
    titleArea = topSectionFlexBox.items[0].currentBounds.toNearestInt();
    connectionStatusArea = topSectionFlexBox.items[1].currentBounds.toNearestInt();
    recordingLabelArea = topSectionFlexBox.items[2].currentBounds.toNearestInt();
    waveformArea = topSectionFlexBox.items[3].currentBounds.toNearestInt();
    inputStatusArea = topSectionFlexBox.items[4].currentBounds.toNearestInt();
    inputInfoArea = topSectionFlexBox.items[5].currentBounds.toNearestInt();

    // Layout buffer control buttons within their allocated space
    auto bufferControlsBounds = topSectionFlexBox.items[6].currentBounds.toNearestInt();

    // Create FlexBox for buffer control buttons (2 rows)
    juce::FlexBox bufferButtonsFlexBox;
    bufferButtonsFlexBox.flexDirection = juce::FlexBox::Direction::column;
    bufferButtonsFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;

    // Row 1: Save and Clear buttons (horizontal)
    juce::Component row1Component;
    juce::FlexItem row1Item(row1Component);
    row1Item.height = 35;
    row1Item.margin = juce::FlexItem::Margin(5, 0, 5, 0);

    // Row 2: Check Connection button (horizontal)
    juce::Component row2Component;
    juce::FlexItem row2Item(row2Component);
    row2Item.height = 35;
    row2Item.margin = juce::FlexItem::Margin(5, 0, 5, 0);

    bufferButtonsFlexBox.items.add(row1Item);
    bufferButtonsFlexBox.items.add(row2Item);

    // Layout the button rows
    bufferButtonsFlexBox.performLayout(bufferControlsBounds);

    auto buttonRow1Bounds = bufferButtonsFlexBox.items[0].currentBounds.toNearestInt();
    auto buttonRow2Bounds = bufferButtonsFlexBox.items[1].currentBounds.toNearestInt();

    // Layout buttons within Row 1 (Save and Clear side by side)
    juce::FlexBox row1FlexBox;
    row1FlexBox.flexDirection = juce::FlexBox::Direction::row;
    row1FlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;

    juce::FlexItem saveItem(saveBufferButton);
    saveItem.flexGrow = 1;
    saveItem.margin = juce::FlexItem::Margin(0, 5, 0, 5);

    juce::FlexItem clearBufferItem(clearBufferButton);
    clearBufferItem.flexGrow = 1;
    clearBufferItem.margin = juce::FlexItem::Margin(0, 5, 0, 5);

    row1FlexBox.items.add(saveItem);
    row1FlexBox.items.add(clearBufferItem);  // was clearItem
    row1FlexBox.performLayout(buttonRow1Bounds);

    // Layout Check Connection button in Row 2 (centered)
    checkConnectionButton.setBounds(buttonRow2Bounds.reduced(5, 0));

    // ========== TAB AREA ==========
    auto tabSectionBounds = bounds.removeFromTop(320).reduced(20, 10);
    fullTabAreaRect = tabSectionBounds.expanded(20, 10); // Expand back to account for the reduced margins

    // Tab buttons area
    tabArea = tabSectionBounds.removeFromTop(35);
    auto tabButtonWidth = tabArea.getWidth() / 3;

    garyTabButton.setBounds(tabArea.removeFromLeft(tabButtonWidth).reduced(2, 2));
    jerryTabButton.setBounds(tabArea.removeFromLeft(tabButtonWidth).reduced(2, 2));
    terryTabButton.setBounds(tabArea.reduced(2, 2));

    // Model controls area (below tabs)
    modelControlsArea = tabSectionBounds.reduced(0, 5);

    // ========== GARY CONTROLS LAYOUT (FlexBox Implementation) ==========
    auto garyBounds = modelControlsArea;

    // Create main FlexBox for Gary controls (vertical layout)
    juce::FlexBox garyFlexBox;
    garyFlexBox.flexDirection = juce::FlexBox::Direction::column;
    garyFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    // 1. Gary section title (fixed height)
    juce::FlexItem garyTitleItem(garyLabel);
    garyTitleItem.height = 35;  // Consistent height
    garyTitleItem.margin = juce::FlexItem::Margin(5, 0, 5, 0);  // 5px top/bottom margins

    // 2. Prompt duration row (fixed height)
    juce::Component promptRowComponent;
    juce::FlexItem promptRowItem(promptRowComponent);
    promptRowItem.height = 35;  // Consistent height for control rows
    promptRowItem.margin = juce::FlexItem::Margin(5, 0, 5, 0);

    // 3. Model selection row (fixed height)
    juce::Component modelRowComponent;
    juce::FlexItem modelRowItem(modelRowComponent);
    modelRowItem.height = 35;  // Same height as prompt row
    modelRowItem.margin = juce::FlexItem::Margin(5, 0, 5, 0);

    // 4. Send to Gary button (fixed height)
    juce::FlexItem sendToGaryItem(sendToGaryButton);
    sendToGaryItem.height = 40;  // Standard button height
    sendToGaryItem.margin = juce::FlexItem::Margin(10, 50, 5, 50);  // 10px top, 50px left/right, 5px bottom

    // 5. Continue button (fixed height - SAME as Send to Gary)
    juce::FlexItem continueItem(continueButton);
    continueItem.height = 40;  // SAME height as Send to Gary button
    continueItem.margin = juce::FlexItem::Margin(5, 50, 10, 50);  // 5px top, 50px left/right, 10px bottom

    // Add all items to Gary FlexBox
    garyFlexBox.items.add(garyTitleItem);
    garyFlexBox.items.add(promptRowItem);
    garyFlexBox.items.add(modelRowItem);
    garyFlexBox.items.add(sendToGaryItem);
    garyFlexBox.items.add(continueItem);

    // Perform layout for Gary controls
    garyFlexBox.performLayout(garyBounds);

    // Extract calculated bounds for control rows
    auto promptRowBounds = garyFlexBox.items[1].currentBounds.toNearestInt();
    auto modelRowBounds = garyFlexBox.items[2].currentBounds.toNearestInt();

    // Layout prompt duration controls (horizontal FlexBox)
    juce::FlexBox promptFlexBox;
    promptFlexBox.flexDirection = juce::FlexBox::Direction::row;
    promptFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    juce::FlexItem promptLabelItem(promptDurationLabel);
    promptLabelItem.width = 100;  // Fixed width for label
    promptLabelItem.margin = juce::FlexItem::Margin(0, 5, 0, 0);  // 5px right margin

    juce::FlexItem promptSliderItem(promptDurationSlider);
    promptSliderItem.flexGrow = 1;  // Fill remaining space
    promptSliderItem.margin = juce::FlexItem::Margin(0, 0, 0, 5);  // 5px left margin

    promptFlexBox.items.add(promptLabelItem);
    promptFlexBox.items.add(promptSliderItem);
    promptFlexBox.performLayout(promptRowBounds);

    // Layout model selection controls (horizontal FlexBox)
    juce::FlexBox modelFlexBox;
    modelFlexBox.flexDirection = juce::FlexBox::Direction::row;
    modelFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    juce::FlexItem modelLabelItem(modelLabel);
    modelLabelItem.width = 100;  // Same width as prompt label
    modelLabelItem.margin = juce::FlexItem::Margin(0, 5, 0, 0);  // 5px right margin

    juce::FlexItem modelComboItem(modelComboBox);
    modelComboItem.flexGrow = 1;  // Fill remaining space
    modelComboItem.margin = juce::FlexItem::Margin(0, 0, 0, 5);  // 5px left margin

    modelFlexBox.items.add(modelLabelItem);
    modelFlexBox.items.add(modelComboItem);
    modelFlexBox.performLayout(modelRowBounds);

    // Replace the JERRY CONTROLS LAYOUT section in your resized() method with this:

// ========== JERRY CONTROLS LAYOUT (WITH HORIZONTAL SMART LOOP ROW) ==========
    auto jerryBounds = modelControlsArea;

    // Create main FlexBox for Jerry controls (vertical layout)
    juce::FlexBox jerryFlexBox;
    jerryFlexBox.flexDirection = juce::FlexBox::Direction::column;
    jerryFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    // 1. Jerry section title
    juce::FlexItem jerryTitleItem(jerryLabel);
    jerryTitleItem.height = 30;
    jerryTitleItem.margin = juce::FlexItem::Margin(5, 0, 5, 0);

    // 2. Text prompt label
    juce::FlexItem jerryPromptLabelItem(jerryPromptLabel);
    jerryPromptLabelItem.height = 20;
    jerryPromptLabelItem.margin = juce::FlexItem::Margin(2, 0, 2, 0);

    // 3. Text prompt editor
    juce::FlexItem jerryPromptEditorItem(jerryPromptEditor);
    jerryPromptEditorItem.height = 45;
    jerryPromptEditorItem.margin = juce::FlexItem::Margin(0, 5, 8, 5);

    // 4. CFG controls row
    juce::Component jerryCfgRowComponent;
    juce::FlexItem jerryCfgRowItem(jerryCfgRowComponent);
    jerryCfgRowItem.height = 30;
    jerryCfgRowItem.margin = juce::FlexItem::Margin(3, 0, 3, 0);

    // 5. Steps controls row
    juce::Component jerryStepsRowComponent;
    juce::FlexItem jerryStepsRowItem(jerryStepsRowComponent);
    jerryStepsRowItem.height = 30;
    jerryStepsRowItem.margin = juce::FlexItem::Margin(3, 0, 3, 0);

    // 6. COMBINED smart loop row (toggle + type buttons in one horizontal row)
    juce::Component smartLoopRowComponent;
    juce::FlexItem smartLoopRowItem(smartLoopRowComponent);
    smartLoopRowItem.height = 32;
    smartLoopRowItem.margin = juce::FlexItem::Margin(5, 10, 5, 10);

    // 7. BPM info label
    juce::FlexItem jerryBpmItem(jerryBpmLabel);
    jerryBpmItem.height = 20;
    jerryBpmItem.margin = juce::FlexItem::Margin(3, 0, 8, 0);

    // 8. Generate button
    juce::FlexItem generateJerryItem(generateWithJerryButton);
    generateJerryItem.height = 35;
    generateJerryItem.margin = juce::FlexItem::Margin(5, 50, 5, 50);

    // Add all items to Jerry FlexBox
    jerryFlexBox.items.add(jerryTitleItem);
    jerryFlexBox.items.add(jerryPromptLabelItem);
    jerryFlexBox.items.add(jerryPromptEditorItem);
    jerryFlexBox.items.add(jerryCfgRowItem);
    jerryFlexBox.items.add(jerryStepsRowItem);
    jerryFlexBox.items.add(smartLoopRowItem);  // Combined row
    jerryFlexBox.items.add(jerryBpmItem);
    jerryFlexBox.items.add(generateJerryItem);

    // Perform layout for Jerry controls
    jerryFlexBox.performLayout(jerryBounds);

    // Extract calculated bounds for control rows
    auto jerryCfgRowBounds = jerryFlexBox.items[3].currentBounds.toNearestInt();
    auto jerryStepsRowBounds = jerryFlexBox.items[4].currentBounds.toNearestInt();
    auto smartLoopRowBounds = jerryFlexBox.items[5].currentBounds.toNearestInt();  // Combined row

    // Layout CFG controls (unchanged)
    juce::FlexBox jerryCfgFlexBox;
    jerryCfgFlexBox.flexDirection = juce::FlexBox::Direction::row;
    jerryCfgFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    juce::FlexItem jerryCfgLabelItem(jerryCfgLabel);
    jerryCfgLabelItem.width = 80;
    jerryCfgLabelItem.margin = juce::FlexItem::Margin(0, 5, 0, 0);

    juce::FlexItem jerryCfgSliderItem(jerryCfgSlider);
    jerryCfgSliderItem.flexGrow = 1;
    jerryCfgSliderItem.margin = juce::FlexItem::Margin(0, 0, 0, 5);

    jerryCfgFlexBox.items.add(jerryCfgLabelItem);
    jerryCfgFlexBox.items.add(jerryCfgSliderItem);
    jerryCfgFlexBox.performLayout(jerryCfgRowBounds);

    // Layout Steps controls (unchanged)
    juce::FlexBox jerryStepsFlexBox;
    jerryStepsFlexBox.flexDirection = juce::FlexBox::Direction::row;
    jerryStepsFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    juce::FlexItem jerryStepsLabelItem(jerryStepsLabel);
    jerryStepsLabelItem.width = 80;
    jerryStepsLabelItem.margin = juce::FlexItem::Margin(0, 5, 0, 0);

    juce::FlexItem jerryStepsSliderItem(jerryStepsSlider);
    jerryStepsSliderItem.flexGrow = 1;
    jerryStepsSliderItem.margin = juce::FlexItem::Margin(0, 0, 0, 5);

    jerryStepsFlexBox.items.add(jerryStepsLabelItem);
    jerryStepsFlexBox.items.add(jerryStepsSliderItem);
    jerryStepsFlexBox.performLayout(jerryStepsRowBounds);

    // NEW: Layout the combined smart loop row (horizontal layout)
    juce::FlexBox smartLoopFlexBox;
    smartLoopFlexBox.flexDirection = juce::FlexBox::Direction::row;
    smartLoopFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    // Smart loop toggle button (left side)
    juce::FlexItem smartLoopToggleItem(generateAsLoopButton);
    smartLoopToggleItem.width = 140;  // Fixed width for "Generate as smart loop"
    smartLoopToggleItem.margin = juce::FlexItem::Margin(0, 0, 0, 0);  // No margins - will touch

    // Auto button (only visible when smart loop is enabled)
    juce::FlexItem loopTypeAutoItem(loopTypeAutoButton);
    loopTypeAutoItem.width = 50;  // Compact width
    loopTypeAutoItem.margin = juce::FlexItem::Margin(0, 0, 0, 0);  // No margins - will touch

    // Drums button
    juce::FlexItem loopTypeDrumsItem(loopTypeDrumsButton);
    loopTypeDrumsItem.width = 55;  // Compact width
    loopTypeDrumsItem.margin = juce::FlexItem::Margin(0, 0, 0, 0);  // No margins - will touch

    // Instruments button
    juce::FlexItem loopTypeInstrumentsItem(loopTypeInstrumentsButton);
    loopTypeInstrumentsItem.width = 80;  // Compact width
    loopTypeInstrumentsItem.margin = juce::FlexItem::Margin(0, 0, 0, 0);  // No margins - will touch

    // Add items to horizontal FlexBox
    smartLoopFlexBox.items.add(smartLoopToggleItem);
    smartLoopFlexBox.items.add(loopTypeAutoItem);
    smartLoopFlexBox.items.add(loopTypeDrumsItem);
    smartLoopFlexBox.items.add(loopTypeInstrumentsItem);

    // Layout the combined smart loop row
    smartLoopFlexBox.performLayout(smartLoopRowBounds);

    
    // Replace just the OUTPUT SECTION part in your resized() method with this:

// ========== OUTPUT SECTION (FlexBox Implementation) ==========
    auto outputSection = bounds.removeFromTop(200).reduced(20, 10);

    // Create main FlexBox for output section (vertical layout)
    juce::FlexBox outputFlexBox;
    outputFlexBox.flexDirection = juce::FlexBox::Direction::column;
    outputFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    // 1. Output label (fixed height)
    juce::FlexItem labelItem(outputLabel);
    labelItem.height = 25;  // Fixed height for the label
    labelItem.margin = juce::FlexItem::Margin(0, 0, 5, 0);  // 5px bottom margin

    // 2. Waveform area (flexible - grows to fill available space)
    juce::Component waveformComponent;  // Dummy component for sizing
    juce::FlexItem waveformItem(waveformComponent);
    waveformItem.flexGrow = 1;  // This will grow to fill available space
    waveformItem.minHeight = 80;  // Minimum height so it's always visible
    waveformItem.margin = juce::FlexItem::Margin(0, 0, 10, 0);  // 10px bottom margin

    // 3. Output info text area (fixed height)
    juce::Component outputInfoComponent;  // Dummy component for text area
    juce::FlexItem outputInfoItem(outputInfoComponent);
    outputInfoItem.height = 20;  // Fixed height for info text
    outputInfoItem.margin = juce::FlexItem::Margin(0, 0, 5, 0);  // 5px bottom margin

    // 4. Button container (fixed height)
    juce::Component buttonContainer;  // Dummy component for button area
    juce::FlexItem buttonItem(buttonContainer);
    buttonItem.height = 35;  // Fixed height for buttons

    // Add items to main FlexBox
    outputFlexBox.items.add(labelItem);
    outputFlexBox.items.add(waveformItem);
    outputFlexBox.items.add(outputInfoItem);
    outputFlexBox.items.add(buttonItem);

    // Perform the layout
    outputFlexBox.performLayout(outputSection);

    // Extract the calculated bounds
    auto labelBounds = outputFlexBox.items[0].currentBounds.toNearestInt();
    auto waveformBounds = outputFlexBox.items[1].currentBounds.toNearestInt();
    auto outputInfoBounds = outputFlexBox.items[2].currentBounds.toNearestInt();
    auto buttonContainerBounds = outputFlexBox.items[3].currentBounds.toNearestInt();

    // Set component bounds based on FlexBox calculations
    outputLabel.setBounds(labelBounds);
    outputWaveformArea = waveformBounds;
    outputInfoArea = outputInfoBounds;  // Store this for use in paint()

    // Create FlexBox for the three buttons (horizontal layout)
    juce::FlexBox buttonFlexBox;
    buttonFlexBox.flexDirection = juce::FlexBox::Direction::row;
    buttonFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;

    // Create flex items for the three buttons (equal width)
    juce::FlexItem playItem(playOutputButton);
    playItem.flexGrow = 1;
    playItem.margin = juce::FlexItem::Margin(0, 2, 0, 2);  // 2px left/right margins

    juce::FlexItem stopItem(stopOutputButton);
    stopItem.flexGrow = 1;
    stopItem.margin = juce::FlexItem::Margin(0, 2, 0, 2);

    juce::FlexItem clearItem(clearOutputButton);
    clearItem.flexGrow = 1;
    clearItem.margin = juce::FlexItem::Margin(0, 2, 0, 2);

    // Add button items to button FlexBox
    buttonFlexBox.items.add(playItem);
    buttonFlexBox.items.add(stopItem);
    buttonFlexBox.items.add(clearItem);

    // Layout the buttons within their container
    buttonFlexBox.performLayout(buttonContainerBounds);

    // The buttons will automatically get their bounds set by FlexBox

    // Crop button as overlay (keep existing logic - positioned relative to waveform)
    auto cropOverlayArea = juce::Rectangle<int>(
        outputWaveformArea.getRight() - 50,
        outputWaveformArea.getY() + 5,
        45, 25
    );
    cropButton.setBounds(cropOverlayArea);
}

void Gary4juceAudioProcessorEditor::startAudioDrag()
{
    if (!outputAudioFile.existsAsFile())
    {
        DBG("No output audio file to drag");
        return;
    }
    
    // Create dragged_audio folder inside Documents/gary4juce
    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");
    auto draggedAudioDir = garyDir.getChildFile("dragged_audio");
    
    // Ensure the dragged_audio directory exists
    if (!draggedAudioDir.exists())
    {
        auto result = draggedAudioDir.createDirectory();
        if (!result.wasOk())
        {
            DBG("Failed to create dragged_audio directory: " + result.getErrorMessage());
            showStatusMessage("Drag failed - folder creation error", 2000);
            return;
        }
    }
    
    // Create a unique filename with timestamp for the drag
    auto timestamp = juce::String(juce::Time::getCurrentTime().toMilliseconds());
    auto uniqueFileName = "gary4juce_" + timestamp + ".wav";
    auto uniqueDragFile = draggedAudioDir.getChildFile(uniqueFileName);
    
    // Copy the current output file to the unique drag file
    if (!outputAudioFile.copyFileTo(uniqueDragFile))
    {
        DBG("Failed to create unique copy for dragging");
        showStatusMessage("Drag failed - file copy error", 2000);
        return;
    }
    
    // Create array of files to drag
    juce::StringArray filesToDrag;
    filesToDrag.add(uniqueDragFile.getFullPathName());
    
    DBG("Starting drag for persistent file: " + uniqueDragFile.getFullPathName());
    
    // Perform external drag to DAW/other applications
    auto success = performExternalDragDropOfFiles(filesToDrag, true, this, [this]()
    {
        DBG("Drag operation completed");
        showStatusMessage("Audio dragged successfully!", 2000);
    });
    
    if (!success)
    {
        DBG("Failed to start drag operation");
        showStatusMessage("Drag failed - try again", 2000);
        // Clean up the file if drag failed
        uniqueDragFile.deleteFile();
    }
}

bool Gary4juceAudioProcessorEditor::isMouseOverOutputWaveform(const juce::Point<int>& position) const
{
    return outputWaveformArea.contains(position);
}

void Gary4juceAudioProcessorEditor::createCropIcon()
{
    // Fixed SVG with explicit white color instead of currentColor
    const char* cropSvg = R"(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
<path d="M6.13 1L6 16a2 2 0 0 0 2 2h15"></path>
<path d="M1 6.13L16 6a2 2 0 0 1 2 2v15"></path>
</svg>)";

    // Create drawable from SVG
    cropIcon = juce::Drawable::createFromImageData(cropSvg, strlen(cropSvg));

    if (cropIcon)
    {
        DBG("Crop icon created successfully from SVG");
    }
    else
    {
        DBG("Failed to create crop icon from SVG");
    }
}

void Gary4juceAudioProcessorEditor::cropAudioAtCurrentPosition()
{
    if (!hasOutputAudio || totalAudioDuration <= 0.0)
    {
        showStatusMessage("No audio to crop", 2000);
        return;
    }

    // Get crop position
    double cropPosition = 0.0;

    if (isPlayingOutput)
    {
        cropPosition = currentPlaybackPosition;
        DBG("Cropping at playing position: " + juce::String(cropPosition, 2));
    }
    else if (isPausedOutput)
    {
        cropPosition = pausedPosition;
        DBG("Cropping at paused position: " + juce::String(cropPosition, 2));
    }
    else if (currentPlaybackPosition > 0.0)
    {
        cropPosition = currentPlaybackPosition;
        DBG("Cropping at seek position: " + juce::String(cropPosition, 2));
    }
    else
    {
        // We're in stopped state at beginning - can't crop here
        showStatusMessage("Cannot crop at beginning - play or seek to position first", 4000);
        DBG("Cannot crop: audio is stopped at beginning position");
        return;
    }

    // Validate crop position
    if (cropPosition <= 0.1)
    {
        showStatusMessage("Cannot crop at very beginning - seek forward first", 3000);
        return;
    }

    if (cropPosition >= (totalAudioDuration - 0.1))
    {
        showStatusMessage("Cannot crop at end - seek backward first", 3000);
        return;
    }

    // CRITICAL: Stop ALL audio playback and DISCONNECT from file
    fullStopOutputPlayback();

    // CRITICAL: Additional cleanup to ensure file is released
    if (transportSource)
    {
        transportSource->setSource(nullptr);  // Disconnect from file
    }
    if (readerSource)
    {
        readerSource.reset();  // Release file reader
    }

    // Give the system a moment to release file handles
    juce::Thread::sleep(100);

    DBG("Starting crop operation at " + juce::String(cropPosition, 2) + "s");

    // Read the CURRENT file into memory first
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    // Create a COPY of the file path to avoid any reference issues
    juce::File sourceFile = outputAudioFile;

    DBG("Reading source file: " + sourceFile.getFullPathName());
    DBG("File exists: " + juce::String(sourceFile.exists() ? "yes" : "no"));
    DBG("File size: " + juce::String(sourceFile.getSize()) + " bytes");

    auto reader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(sourceFile));
    if (!reader)
    {
        showStatusMessage("Failed to read audio file for cropping", 3000);
        DBG("ERROR: Could not create reader for file");
        return;
    }

    DBG("Reader created successfully");
    DBG("Reader length: " + juce::String(reader->lengthInSamples) + " samples");
    DBG("Reader sample rate: " + juce::String(reader->sampleRate) + " Hz");
    DBG("Reader channels: " + juce::String(reader->numChannels));

    // Calculate samples to keep
    int samplesToKeep = (int)(cropPosition * reader->sampleRate);

    DBG("Crop position: " + juce::String(cropPosition, 2) + "s");
    DBG("Samples to keep: " + juce::String(samplesToKeep));
    DBG("Original samples: " + juce::String(reader->lengthInSamples));

    if (samplesToKeep <= 0 || samplesToKeep >= reader->lengthInSamples)
    {
        showStatusMessage("Invalid crop position", 3000);
        DBG("ERROR: Invalid samples to keep: " + juce::String(samplesToKeep));
        return;
    }

    // Read the cropped portion into buffer
    juce::AudioBuffer<float> croppedBuffer((int)reader->numChannels, samplesToKeep);

    DBG("Created buffer: " + juce::String(croppedBuffer.getNumChannels()) + " channels, " +
        juce::String(croppedBuffer.getNumSamples()) + " samples");

    if (!reader->read(&croppedBuffer, 0, samplesToKeep, 0, true, true))
    {
        showStatusMessage("Failed to read audio data", 3000);
        DBG("ERROR: Failed to read audio data into buffer");
        return;
    }

    DBG("Successfully read audio data into buffer");

    // Create a TEMPORARY file for writing (avoid file locking issues)
    auto tempFile = sourceFile.getSiblingFile("temp_crop_" + juce::String(juce::Time::getCurrentTime().toMilliseconds()) + ".wav");

    DBG("Writing to temporary file: " + tempFile.getFullPathName());

    // Write to temporary file
    auto wavFormat = formatManager.findFormatForFileExtension("wav");
    if (!wavFormat)
    {
        showStatusMessage("WAV format not available", 3000);
        DBG("ERROR: WAV format not found");
        return;
    }

    auto fileStream = std::unique_ptr<juce::FileOutputStream>(tempFile.createOutputStream());
    if (!fileStream)
    {
        showStatusMessage("Failed to create temp file", 3000);
        DBG("ERROR: Could not create output stream for temp file");
        return;
    }

    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        wavFormat->createWriterFor(fileStream.get(), reader->sampleRate,
            (unsigned int)reader->numChannels, 24, {}, 0));

    if (!writer)
    {
        showStatusMessage("Failed to create audio writer", 3000);
        DBG("ERROR: Could not create audio writer");
        return;
    }

    fileStream.release(); // Writer takes ownership

    DBG("Writing " + juce::String(croppedBuffer.getNumSamples()) + " samples to file");

    bool writeSuccess = writer->writeFromAudioSampleBuffer(croppedBuffer, 0, croppedBuffer.getNumSamples());

    // CRITICAL: Ensure writer is properly closed and flushed
    writer.reset();

    if (!writeSuccess)
    {
        showStatusMessage("Failed to write cropped audio", 3000);
        DBG("ERROR: writeFromAudioSampleBuffer failed");
        tempFile.deleteFile(); // Clean up temp file
        return;
    }

    DBG("Successfully wrote audio to temp file");
    DBG("Temp file size: " + juce::String(tempFile.getSize()) + " bytes");

    // Verify temp file was written correctly
    if (!tempFile.exists() || tempFile.getSize() < 1000) // Should be at least 1KB for any real audio
    {
        showStatusMessage("Temp file write failed", 3000);
        DBG("ERROR: Temp file doesn't exist or is too small");
        tempFile.deleteFile();
        return;
    }

    // Replace original file with temp file
    if (!sourceFile.deleteFile())
    {
        showStatusMessage("Failed to delete original file", 3000);
        DBG("ERROR: Could not delete original file");
        tempFile.deleteFile();
        return;
    }

    DBG("Deleted original file");

    if (!tempFile.moveFileTo(sourceFile))
    {
        showStatusMessage("Failed to move temp file", 3000);
        DBG("ERROR: Could not move temp file to original location");
        return;
    }

    DBG("Moved temp file to original location");
    DBG("Final file size: " + juce::String(sourceFile.getSize()) + " bytes");

    // Reload the cropped audio
    loadOutputAudioFile();

    // Reset playback position
    currentPlaybackPosition = 0.0;
    pausedPosition = 0.0;
    isPausedOutput = false;

    // Verify the reload worked
    double newDuration = (double)outputAudioBuffer.getNumSamples() / 44100.0;
    DBG("New audio duration after reload: " + juce::String(newDuration, 2) + "s");

    showStatusMessage("Audio cropped at " + juce::String(cropPosition, 1) + "s", 3000);
    DBG("Crop operation completed successfully");

    // Force a repaint to update the waveform display
    repaint();
}

void Gary4juceAudioProcessorEditor::continueMusic()
{
    if (!hasOutputAudio)
    {
        showStatusMessage("No audio to continue", 2000);
        return;
    }

    // Encode current output audio as base64
    if (!outputAudioFile.exists())
    {
        showStatusMessage("Output file not found", 2000);
        return;
    }

    // Read the audio file as binary data
    juce::MemoryBlock audioData;
    if (!outputAudioFile.loadFileAsData(audioData))
    {
        showStatusMessage("Failed to read audio file", 3000);
        return;
    }

    // Convert to base64
    auto base64Audio = juce::Base64::toBase64(audioData.getData(), audioData.getSize());
    
    sendContinueRequest(base64Audio);
}

void Gary4juceAudioProcessorEditor::sendContinueRequest(const juce::String& audioData)
{
    DBG("Sending continue request with " + juce::String(audioData.length()) + " chars of audio data");
    showStatusMessage("Requesting continuation...", 3000);

    // Disable continue button during processing
    continueButton.setEnabled(false);
    continueButton.setButtonText("Continuing...");

    // Create HTTP request in background thread (same pattern as sendToGary)
    juce::Thread::launch([this, audioData]() {

        auto startTime = juce::Time::getCurrentTime();

        // Create JSON payload - same structure as sendToGary
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("audio_data", audioData);
        jsonRequest->setProperty("prompt_duration", (int)currentPromptDuration);
        jsonRequest->setProperty("model_name", "thepatch/vanya_ai_dnb_0.1"); // Use same model as Gary
        jsonRequest->setProperty("top_k", 250);
        jsonRequest->setProperty("temperature", 1.0);
        jsonRequest->setProperty("cfg_coef", 3.0);
        jsonRequest->setProperty("description", "");

        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        DBG("Continue JSON payload size: " + juce::String(jsonString.length()) + " characters");

        juce::URL url("https://g4l.thecollabagepatch.com/api/juce/continue_music");

        juce::String responseText;
        int statusCode = 0;

        try
        {
            // Use JUCE 8.0.8 pattern: withPOSTData + InputStreamOptions
            juce::URL postUrl = url.withPOSTData(jsonString);

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(30000)
                .withExtraHeaders("Content-Type: application/json");

            auto stream = postUrl.createInputStream(options);

            auto requestTime = juce::Time::getCurrentTime() - startTime;
            DBG("Continue HTTP connection established in " + juce::String(requestTime.inMilliseconds()) + "ms");

            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();

                auto totalTime = juce::Time::getCurrentTime() - startTime;
                DBG("Continue HTTP request completed in " + juce::String(totalTime.inMilliseconds()) + "ms");
                DBG("Continue response length: " + juce::String(responseText.length()) + " characters");

                statusCode = 200; // Assume success if we got data
            }
            else
            {
                DBG("Failed to create input stream for continue request");
                responseText = "";
                statusCode = 0;
            }
        }
        catch (const std::exception& e)
        {
            DBG("Continue HTTP request exception: " + juce::String(e.what()));
            responseText = "";
            statusCode = 0;
        }
        catch (...)
        {
            DBG("Unknown continue HTTP request exception");
            responseText = "";
            statusCode = 0;
        }

        // Handle response on main thread
        juce::MessageManager::callAsync([this, responseText, statusCode]() {
            if (statusCode == 200 && responseText.isNotEmpty())
            {
                DBG("Continue response: " + responseText);

                // Parse response JSON
                auto responseVar = juce::JSON::parse(responseText);
                if (auto* obj = responseVar.getDynamicObject())
                {
                    bool requestSuccess = obj->getProperty("success");
                    if (requestSuccess)
                    {
                        auto sessionId = obj->getProperty("session_id").toString();
                        DBG("Continue request queued, session ID: " + sessionId);
                        showStatusMessage("Continuation queued...", 2000);
                        
                        // Start polling for results (will replace current output audio when complete)
                        startPollingForResults(sessionId);
                    }
                    else
                    {
                        auto error = obj->getProperty("error").toString();
                        showStatusMessage("Continue failed: " + error, 5000);
                        continueButton.setEnabled(hasOutputAudio);
                        continueButton.setButtonText("Continue");
                    }
                }
                else
                {
                    showStatusMessage("Invalid response format", 3000);
                    continueButton.setEnabled(hasOutputAudio);
                    continueButton.setButtonText("Continue");
                }
            }
            else
            {
                DBG("Continue request failed - HTTP " + juce::String(statusCode));
                showStatusMessage("Connection failed", 3000);
                continueButton.setEnabled(hasOutputAudio);
                continueButton.setButtonText("Continue");
            }
        });
    });
}