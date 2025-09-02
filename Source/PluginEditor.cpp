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
    cropButton("Crop", juce::DrawableButton::ImageFitted),
    garyHelpButton("gary help", juce::DrawableButton::ImageFitted),
    jerryHelpButton("jerry help", juce::DrawableButton::ImageFitted),
    terryHelpButton("terry help", juce::DrawableButton::ImageFitted)

{
    setSize(400, 850);  // Made taller to accommodate controls

    // Check initial backend connection status
    isConnected = audioProcessor.isBackendConnected();
    DBG("Editor created, backend connection status: " + juce::String(isConnected ? "Connected" : "Disconnected"));

    // ========== TAB BUTTONS SETUP ==========
    garyTabButton.setButtonText("gary");
    garyTabButton.setButtonStyle(CustomButton::ButtonStyle::Gary);
    garyTabButton.onClick = [this]() { switchToTab(ModelTab::Gary); };
    addAndMakeVisible(garyTabButton);

    jerryTabButton.setButtonText("jerry");
    jerryTabButton.setButtonStyle(CustomButton::ButtonStyle::Jerry);
    jerryTabButton.onClick = [this]() { switchToTab(ModelTab::Jerry); };
    addAndMakeVisible(jerryTabButton);

    terryTabButton.setButtonText("terry");
    terryTabButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
    terryTabButton.onClick = [this]() { switchToTab(ModelTab::Terry); };
    // terryTabButton.setEnabled(false);  // Disabled until implemented
    addAndMakeVisible(terryTabButton);

    // ========== GARY CONTROLS SETUP ==========
    garyLabel.setText("gary (musicgen)", juce::dontSendNotification);
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
        
        // Update tooltips with new duration
        sendToGaryButton.setTooltip("have gary extend the recorded audio using the first " + 
            juce::String((int)currentPromptDuration) + " seconds as audio prompt");
        continueButton.setTooltip("have gary extend the output audio using the last " + 
            juce::String((int)currentPromptDuration) + " seconds as audio prompt");
        retryButton.setTooltip("have gary retry that last continuation using different prompt duration or model if you want, or just have him do it over");
    };
    addAndMakeVisible(promptDurationSlider);

    promptDurationLabel.setText("prompt duration", juce::dontSendNotification);
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

    updateModelAvailability();

    modelLabel.setText("model", juce::dontSendNotification);
    modelLabel.setFont(juce::FontOptions(12.0f));
    modelLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    modelLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(modelLabel);

    // Send to Gary button
    sendToGaryButton.setButtonText("send to gary");
    sendToGaryButton.setButtonStyle(CustomButton::ButtonStyle::Gary);
    sendToGaryButton.onClick = [this]() { sendToGary(); };
    sendToGaryButton.setEnabled(false); // Initially disabled until we have audio
    sendToGaryButton.setTooltip("have gary extend the recorded audio using the first " + 
        juce::String((int)currentPromptDuration) + " seconds as audio prompt");
    addAndMakeVisible(sendToGaryButton);

    // Continue button for Gary
    continueButton.setButtonText("continue");
    continueButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    continueButton.onClick = [this]() { continueMusic(); };
    continueButton.setEnabled(false); // Initially disabled
    continueButton.setTooltip("have gary extend the output audio using the last " + 
        juce::String((int)currentPromptDuration) + " seconds as audio prompt");
    addAndMakeVisible(continueButton);

    // Retry button for Gary
    retryButton.setButtonText("retry");
    retryButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    retryButton.onClick = [this]() { retryLastContinuation(); };
    retryButton.setEnabled(false); // Initially disabled
    retryButton.setTooltip("have gary retry that last continuation using different prompt duration or model if you want, or just have him do it over");
    addAndMakeVisible(retryButton);

    // ========== JERRY CONTROLS SETUP ==========
    jerryLabel.setText("jerry (stable audio open small)", juce::dontSendNotification);
    jerryLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    jerryLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    jerryLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(jerryLabel);

    // Text prompt editor
    jerryPromptEditor.setMultiLine(true);
    jerryPromptEditor.setReturnKeyStartsNewLine(true);
    jerryPromptEditor.setTextToShowWhenEmpty("enter your audio generation prompt here...", juce::Colours::darkgrey);
    jerryPromptEditor.onTextChange = [this]() {
        currentJerryPrompt = jerryPromptEditor.getText();
        // Update button state whenever prompt changes
        updateAllGenerationButtonStates();
        };
    addAndMakeVisible(jerryPromptEditor);

    jerryPromptLabel.setText("text prompt", juce::dontSendNotification);
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

    jerryCfgLabel.setText("cfg scale", juce::dontSendNotification);
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

    jerryStepsLabel.setText("steps", juce::dontSendNotification);
    jerryStepsLabel.setFont(juce::FontOptions(12.0f));
    jerryStepsLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    jerryStepsLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(jerryStepsLabel);

    // BPM info label (will show DAW BPM)
    jerryBpmLabel.setText("bpm: " + juce::String((int)audioProcessor.getCurrentBPM()),
        juce::dontSendNotification);
    jerryBpmLabel.setFont(juce::FontOptions(11.0f));
    jerryBpmLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    jerryBpmLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(jerryBpmLabel);

    // Generate with Jerry button
    generateWithJerryButton.setButtonText("generate with jerry");
    generateWithJerryButton.setButtonStyle(CustomButton::ButtonStyle::Jerry);
    generateWithJerryButton.onClick = [this]() { sendToJerry(); };
    generateWithJerryButton.setEnabled(false); // Initially disabled until connected
    generateWithJerryButton.setTooltip("generate audio from text prompt with current daw bpm");
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
    loopTypeAutoButton.setButtonText("auto");
    loopTypeAutoButton.onClick = [this]() { setLoopType("auto"); };

    loopTypeDrumsButton.setButtonText("drums");
    loopTypeDrumsButton.onClick = [this]() { setLoopType("drums"); };

    loopTypeInstrumentsButton.setButtonText("instr");  // SHORTENED for compact layout
    loopTypeInstrumentsButton.onClick = [this]() { setLoopType("instruments"); };

    addAndMakeVisible(loopTypeAutoButton);
    addAndMakeVisible(loopTypeDrumsButton);
    addAndMakeVisible(loopTypeInstrumentsButton);

    // Initial styling and state
    styleSmartLoopButton();
    setLoopType("auto");  // This will style the segmented control
    updateLoopTypeVisibility();  // Initially hide loop type controls

    // Style loop buttons with theme
    generateAsLoopButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    loopTypeAutoButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    loopTypeDrumsButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    loopTypeInstrumentsButton.setButtonStyle(CustomButton::ButtonStyle::Standard);

    // ========== TERRY CONTROLS SETUP ==========
    terryLabel.setText("terry (melodyflow)", juce::dontSendNotification);
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

    terryVariationLabel.setText("variation", juce::dontSendNotification);
    terryVariationLabel.setFont(juce::FontOptions(12.0f));
    terryVariationLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    terryVariationLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terryVariationLabel);

    // Custom prompt editor (alternative to variations)
    terryCustomPromptEditor.setMultiLine(false);
    terryCustomPromptEditor.setReturnKeyStartsNewLine(false);
    terryCustomPromptEditor.setTextToShowWhenEmpty("or enter custom prompt...", juce::Colours::darkgrey);
    terryCustomPromptEditor.onTextChange = [this]() {
        currentTerryCustomPrompt = terryCustomPromptEditor.getText();
        // Clear variation selection when custom prompt is used
        if (!currentTerryCustomPrompt.trim().isEmpty()) {
            terryVariationComboBox.setSelectedId(0); // Deselect
            currentTerryVariation = -1;
        }
        };
    addAndMakeVisible(terryCustomPromptEditor);

    terryCustomPromptLabel.setText("custom prompt", juce::dontSendNotification);
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

    terryFlowstepLabel.setText("flowstep", juce::dontSendNotification);
    terryFlowstepLabel.setFont(juce::FontOptions(12.0f));
    terryFlowstepLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    terryFlowstepLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terryFlowstepLabel);

    // Solver toggle (euler/midpoint)
    terrySolverToggle.setButtonText("use midpoint solver");
    terrySolverToggle.setToggleState(false, juce::dontSendNotification); // Default to euler
    terrySolverToggle.onClick = [this]() {
        useMidpointSolver = terrySolverToggle.getToggleState();
        DBG("Solver changed to: " + juce::String(useMidpointSolver ? "midpoint" : "euler"));
        };
    addAndMakeVisible(terrySolverToggle);

    terrySolverLabel.setText("solver", juce::dontSendNotification);
    terrySolverLabel.setFont(juce::FontOptions(12.0f));
    terrySolverLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    terrySolverLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terrySolverLabel);

    // Audio source selection
    terrySourceLabel.setText("transform", juce::dontSendNotification);
    terrySourceLabel.setFont(juce::FontOptions(12.0f));
    terrySourceLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    terrySourceLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terrySourceLabel);

    transformRecordingButton.setButtonText("recording");
    transformRecordingButton.setRadioGroupId(1001); // Same group for mutual exclusion
    transformRecordingButton.onClick = [this]() { setTerryAudioSource(true); };
    addAndMakeVisible(transformRecordingButton);

    transformOutputButton.setButtonText("output");
    transformOutputButton.setRadioGroupId(1001); // Same group for mutual exclusion
    transformOutputButton.setToggleState(true, juce::dontSendNotification); // Default selection
    transformOutputButton.onClick = [this]() { setTerryAudioSource(false); };
    addAndMakeVisible(transformOutputButton);

    // Transform button
    transformWithTerryButton.setButtonText("transform with terry");
    transformWithTerryButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
    transformWithTerryButton.onClick = [this]() { sendToTerry(); };
    transformWithTerryButton.setEnabled(false); // Initially disabled until we have audio
    transformWithTerryButton.setTooltip("transform selected audio source according to variation or custom prompt. max: ~40 seconds");
    addAndMakeVisible(transformWithTerryButton);

    // Undo button  
    undoTransformButton.setButtonText("undo transform");
    undoTransformButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    undoTransformButton.onClick = [this]() { undoTerryTransform(); };
    // Enable if we have a session ID stored from previous session
    undoTransformButton.setEnabled(false);
    undoTransformButton.setTooltip("restore audio to state before last transformation");
    addAndMakeVisible(undoTransformButton);

    // ========== REMAINING SETUP (unchanged) ==========
    // Set up the "Check Connection" button
    checkConnectionIcon = IconFactory::createCheckConnectionIcon();
    if (checkConnectionIcon)
        checkConnectionButton.setIcon(checkConnectionIcon->createCopy());
    checkConnectionButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    checkConnectionButton.setTooltip("check backend connection");
    checkConnectionButton.onClick = [this]() {
        DBG("Manual backend health check requested");
        audioProcessor.checkBackendHealth();
        checkConnectionButton.setEnabled(false);
        
        juce::Timer::callAfterDelay(6000, [this]() {
            checkConnectionButton.setEnabled(true);
            
            // Show popup if remote backend check failed, but throttle it
            if (!audioProcessor.isBackendConnected() && !audioProcessor.getIsUsingLocalhost())
            {
                auto currentTime = juce::Time::getCurrentTime();
                auto timeSinceLastPopup = currentTime - lastBackendDisconnectionPopupTime;
                
                // Only show popup if it hasn't been shown in the last 10 minutes
                if (timeSinceLastPopup.inMinutes() >= 10)
                {
                    handleBackendDisconnection();
                    lastBackendDisconnectionPopupTime = currentTime;
                }
                else
                {
                    // Just show a status message instead of the popup
                    showStatusMessage("remote backend not responding", 4000);
                    DBG("Manual check failed but popup throttled (last shown " + 
                        juce::String(timeSinceLastPopup.inMinutes()) + " minutes ago)");
                }
            }
        });
    };

    // Backend toggle button setup
    isUsingLocalhost = audioProcessor.getIsUsingLocalhost(); // Sync with processor
    backendToggleButton.setButtonText("remote");
    backendToggleButton.onClick = [this]() { toggleBackend(); };
    updateBackendToggleButton(); // Set initial state

    // Set up the "Save Buffer" button
    saveBufferButton.setButtonText("save buffer");
    saveBufferButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    saveBufferButton.onClick = [this]() { saveRecordingBuffer(); };
    saveBufferButton.setEnabled(false); // Initially disabled

    // Set up the "Clear Buffer" button
    trashIcon = IconFactory::createTrashIcon();
    if (trashIcon)
        clearBufferButton.setIcon(trashIcon->createCopy());
    clearBufferButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    clearBufferButton.setTooltip("clear buffer");
    clearBufferButton.onClick = [this]() { clearRecordingBuffer(); };

    addAndMakeVisible(checkConnectionButton);
    addAndMakeVisible(backendToggleButton);
    addAndMakeVisible(saveBufferButton);
    addAndMakeVisible(clearBufferButton);

    // Start timer to update recording status (refresh every 50ms for smooth waveform)
    startTimer(50);

    // Initial status update
    updateRecordingStatus();

    // Set up output audio controls
    outputLabel.setText("output", juce::dontSendNotification);
    outputLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    outputLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    outputLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputLabel);

    // Play output button
    playIcon = IconFactory::createPlayIcon();
    pauseIcon = IconFactory::createPauseIcon();
    playOutputButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    updatePlayButtonIcon(); // Set initial icon
    playOutputButton.onClick = [this]() { playOutputAudio(); };
    playOutputButton.setEnabled(false);
    addAndMakeVisible(playOutputButton);

    // Clear output button
    if (trashIcon)
        clearOutputButton.setIcon(trashIcon->createCopy());
    clearOutputButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    clearOutputButton.setTooltip("clear output");
    clearOutputButton.onClick = [this]() { clearOutputAudio(); };
    clearOutputButton.setEnabled(false); // Initially disabled
    addAndMakeVisible(clearOutputButton);

    // Stop output button  
    stopIcon = IconFactory::createStopIcon();
    if (stopIcon)
        stopOutputButton.setIcon(stopIcon->createCopy());
    stopOutputButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    stopOutputButton.setTooltip("stop playback");
    stopOutputButton.onClick = [this]() { fullStopOutputPlayback(); };
    stopOutputButton.setEnabled(false); // Initially disabled
    addAndMakeVisible(stopOutputButton);

    // Crop button - simple approach with custom drawing
    cropIcon = IconFactory::createCropIcon();
    
    // Load the logo image for the title
    logoImage = IconFactory::loadLogoImage();
    
    cropButton.setTooltip("crop audio at current playback position");
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
    outputLabel.setTooltip("drag generated audio to your DAW timeline");
    setInterceptsMouseClicks(true, true);

    // Set initial tab state
    updateTabButtonStates();
    
    // CRITICAL: Set initial visibility state for tabs
    switchToTab(ModelTab::Gary);
    
    // Initialize tooltip window
    tooltipWindow = std::make_unique<juce::TooltipWindow>(this);
    
    // Setup help icons and social media icons
    helpIcon = IconFactory::createHelpIcon();
    discordIcon = IconFactory::createDiscordIcon();
    xIcon = IconFactory::createXIcon();
    
    if (helpIcon)
    {
        // gary help button
        garyHelpButton.setImages(helpIcon.get());
        garyHelpButton.setTooltip("learn more about musicgen");
        garyHelpButton.onClick = [this]() {
            juce::URL("https://github.com/facebookresearch/audiocraft").launchInDefaultBrowser();
        };
        addAndMakeVisible(garyHelpButton);
        
        // jerry help button  
        jerryHelpButton.setImages(helpIcon.get());
        jerryHelpButton.setTooltip("learn more about stable audio open small");
        jerryHelpButton.onClick = [this]() {
            juce::URL("https://huggingface.co/stabilityai/stable-audio-open-small").launchInDefaultBrowser();
        };
        addAndMakeVisible(jerryHelpButton);
        
        // terry help button
        terryHelpButton.setImages(helpIcon.get());
        terryHelpButton.setTooltip("learn more about melodyflow");
        terryHelpButton.onClick = [this]() {
            juce::URL("https://huggingface.co/spaces/facebook/MelodyFlow").launchInDefaultBrowser();
        };
        addAndMakeVisible(terryHelpButton);
    }
    
    // ========== CRITICAL: STATE RESTORATION AFTER COMPONENT CREATION ==========
    
    DBG("=== RESTORING PLUGIN STATE AFTER WINDOW RECREATION ===");
    
    // 1. Restore persistent state from processor
    savedSamples = audioProcessor.getSavedSamples();
    isConnected = audioProcessor.isBackendConnected();
    transformRecording = audioProcessor.getTransformRecording();
    
    DBG("Restored savedSamples: " + juce::String(savedSamples));
    DBG("Restored connection status: " + juce::String(isConnected ? "connected" : "disconnected"));
    
    // 2. Restore output audio state by checking file existence
    if (outputAudioFile.exists())
    {
        loadOutputAudioFile();  // This sets hasOutputAudio = true
        DBG("Output audio file found and loaded: " + outputAudioFile.getFullPathName());
    }
    else
    {
        DBG("No output audio file found");
    }


    
    // 3. CRITICAL: Sync Terry radio button logic with visual state
    // The transformOutputButton is visually checked by default, so sync the logic
    bool restoredTransformRecording = audioProcessor.getTransformRecording();
    
    if (transformOutputButton.getToggleState() && restoredTransformRecording)
    {
        // Visual says "output" but logic says "recording" - fix the mismatch
        DBG("Syncing Terry state: UI shows output selected, fixing logic state");
        setTerryAudioSource(false);  // This sets transformRecording = false
    }
    else if (transformRecordingButton.getToggleState() && !restoredTransformRecording)
    {
        // Visual says "recording" but logic says "output" - fix the mismatch  
        DBG("Syncing Terry state: UI shows recording selected, fixing logic state");
        setTerryAudioSource(true);   // This sets transformRecording = true
    }
    else
    {
        // States match, just update the processor
        audioProcessor.setTransformRecording(transformRecording);
        DBG("Terry states already in sync: " + juce::String(transformRecording ? "recording" : "output"));
    }

    // 3. CRITICAL: Restore session ID and validate undo/retry state
    juce::String restoredSessionId = audioProcessor.getCurrentSessionId();
    bool undoAvailable = audioProcessor.getUndoTransformAvailable();
    bool retryAvailable = audioProcessor.getRetryAvailable();  // ADD THIS

    DBG("=== SESSION STATE RESTORATION ===");
    DBG("Restored session ID: '" + restoredSessionId + "'");
    DBG("Session ID length: " + juce::String(restoredSessionId.length()));
    DBG("Undo transform available: " + juce::String(undoAvailable ? "true" : "false"));
    DBG("Retry available: " + juce::String(retryAvailable ? "true" : "false"));  // ADD THIS

    // Validate state consistency - should only have one operation available at a time
    if (undoAvailable && retryAvailable)
    {
        DBG("WARNING: Both undo and retry are available! This shouldn't happen. Prioritizing undo...");
        audioProcessor.setRetryAvailable(false);
        retryAvailable = false;
    }

    if ((undoAvailable || retryAvailable) && restoredSessionId.isEmpty())
    {
        DBG("WARNING: Operation available but session ID is empty! Clearing operation flags...");
        audioProcessor.setUndoTransformAvailable(false);
        audioProcessor.setRetryAvailable(false);
        undoAvailable = false;
        retryAvailable = false;
    }

    if (!undoAvailable && !retryAvailable && !restoredSessionId.isEmpty())
    {
        DBG("INFO: Session ID exists but no operations available. This might be from an initial generation.");
    }

    undoTransformButton.setEnabled(undoAvailable && !restoredSessionId.isEmpty() && !isGenerating);
    retryButton.setEnabled(retryAvailable && !restoredSessionId.isEmpty() && !isGenerating && isConnected);
    
    // 5. Update all button states after restoration
    updateAllGenerationButtonStates();
    updateRetryButtonState();        
    updateContinueButtonState();     
    updateTerrySourceButtons(); 

    // 6. Double-check button states after connection stabilizes
    juce::Timer::callAfterDelay(2000, [this]() {
        updateRetryButtonState();
        updateTerrySourceButtons();
        DBG("Button states updated after connection check");
        });
    
    DBG("All button states updated after restoration");
    
    // 5. Set initial tab and visibility
    switchToTab(ModelTab::Gary);
    
    // Force initial layout now that help icons are created
    resized();
}

void Gary4juceAudioProcessorEditor::stopAllBackgroundOperations()
{
    DBG("=== STOPPING ALL BACKGROUND OPERATIONS ===");

    // Stop polling and generation immediately
    isPolling = false;
    isGenerating = false;
    continueInProgress = false;

    // Reset progress tracking
    generationProgress = 0;
    lastProgressUpdateTime = 0;
    lastKnownServerProgress = 0;
    hasDetectedStall = false;

    // REMOVED: Don't clear session to prevent new requests
    // audioProcessor.clearCurrentSessionId();  // <-- REMOVE THIS LINE!

    // Stop any health checks in processor
    audioProcessor.stopHealthChecks();

    // Wait for ongoing requests to notice the flags and abort
    juce::Thread::sleep(150);

    DBG("Background operations stopped - threads should abort");
    DBG("Session ID preserved: '" + audioProcessor.getCurrentSessionId() + "'");
}

Gary4juceAudioProcessorEditor::~Gary4juceAudioProcessorEditor()
{
    isEditorValid.store(false);
    // NEW: Stop all background operations FIRST
    stopAllBackgroundOperations();
    
    // Ensure tooltip window is cleaned up first
    tooltipWindow.reset();
    
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
    retryButton.setVisible(showGary);

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

    // Help button visibility
    if (helpIcon)
    {
        garyHelpButton.setVisible(showGary);
        jerryHelpButton.setVisible(showJerry);
        terryHelpButton.setVisible(showTerry);
    }

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

    // Position Terry help icon next to title text (only if Terry tab is active)
    if (helpIcon && currentTab == ModelTab::Terry)
    {
        auto terryLabelBounds = terryFlexBox.items[0].currentBounds.toNearestInt(); // Title item
        auto textWidth = terryLabel.getFont().getStringWidth(terryLabel.getText());
        auto textStartX = terryLabelBounds.getX() + (terryLabelBounds.getWidth() - textWidth) / 2; // Center-aligned text start
        auto terryHelpArea = juce::Rectangle<int>(
            textStartX + textWidth + 5,  // 5px to the right of actual text
            terryLabelBounds.getY() + (terryLabelBounds.getHeight() - 20) / 2,
            20, 20  
        );
        terryHelpButton.setBounds(terryHelpArea);
    }

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

    // Force a complete relayout to position help icons correctly
    resized();
    repaint();
}

void Gary4juceAudioProcessorEditor::updateTabButtonStates()
{
    // Update tab button appearances using CustomButton styles
    garyTabButton.setButtonStyle(currentTab == ModelTab::Gary ? 
        CustomButton::ButtonStyle::Gary : CustomButton::ButtonStyle::Inactive);
    
    jerryTabButton.setButtonStyle(currentTab == ModelTab::Jerry ? 
        CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
    
    terryTabButton.setButtonStyle(currentTab == ModelTab::Terry ? 
        CustomButton::ButtonStyle::Terry : CustomButton::ButtonStyle::Inactive);
}

void Gary4juceAudioProcessorEditor::updateAllGenerationButtonStates()
{
    // Gary buttons - all should be disabled during ANY generation
    sendToGaryButton.setEnabled(savedSamples > 0 && isConnected && !isGenerating);
    if (!isGenerating) {
        sendToGaryButton.setButtonText("send to gary");  // Only reset text when not generating
    }
    continueButton.setEnabled(hasOutputAudio && isConnected && !isGenerating);

    updateRetryButtonState();

    // REMOVED OLD RETRY LOGIC - Let updateRetryButtonState() handle it properly!
    // OLD BUGGY CODE (was using implicit logic):
    // juce::String sessionId = audioProcessor.getCurrentSessionId();
    // bool hasValidSession = !sessionId.isEmpty();
    // retryButton.setEnabled(hasValidSession && isConnected && !isGenerating);

    // Jerry buttons - should be disabled during ANY generation
    generateWithJerryButton.setEnabled(isConnected && !currentJerryPrompt.trim().isEmpty() && !isGenerating);

    // Terry buttons - already have the logic, but let's be explicit
    updateTerrySourceButtons(); // This already checks !isGenerating
}


//==============================================================================
// Updated timerCallback() with smooth progress animation:
void Gary4juceAudioProcessorEditor::timerCallback()
{
    updateRecordingStatus();

    // Update Jerry BPM display with current DAW BPM
    double currentBPM = audioProcessor.getCurrentBPM();
    jerryBpmLabel.setText("bpm: " + juce::String((int)currentBPM) + " (from daw)",
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

    // Connection status flash animation (every 20 ticks = ~1 second)
    static int flashCounter = 0;
    if (isConnected)
    {
        flashCounter++;
        if (flashCounter >= 20) // 20 * 50ms = 1 second flash interval
        {
            flashCounter = 0;
            connectionFlashState = !connectionFlashState;
            repaint(); // Repaint to show flash change
        }
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
    bool wasConnected = isConnected; // Add this to track connection changes

    // Get current status from processor
    isRecording = audioProcessor.isRecording();
    recordingProgress = audioProcessor.getRecordingProgress();
    recordedSamples = audioProcessor.getRecordedSamples();

    // Update save button state
    saveBufferButton.setEnabled(recordedSamples > 0);

    // ONLY update generation button states when something relevant has changed
    bool needsButtonUpdate = (wasRecording != isRecording) ||
        (wasSamples != recordedSamples) ||
        (wasConnected != isConnected);

    if (needsButtonUpdate) {
        updateAllGenerationButtonStates();
    }

    // Check if status message has expired
    if (hasStatusMessage)
    {
        auto currentTime = juce::Time::getCurrentTime().toMilliseconds();
        if (currentTime - statusMessageTime > statusMessageDuration) // Use custom duration
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
    statusMessageDuration = durationMs;  // Store custom duration
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
        g.drawText("press PLAY in DAW to start recording", area, juce::Justification::centred);
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

    // FIXED: Use actual sample rate from processor instead of hardcoded 44100
    const double currentSampleRate = audioProcessor.getCurrentSampleRate();

    // Time calculations
    const double totalDuration = 30.0; // 30 seconds max
    const double recordedDuration = juce::jmax(0.0, (double)recordedSamples / currentSampleRate);
    const double savedDuration = juce::jmax(0.0, (double)savedSamples / currentSampleRate);

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
        showStatusMessage("no recording to save - press play in daw first");
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

    // Get the saved samples from processor (source of truth)
    savedSamples = audioProcessor.getSavedSamples();

    // FIXED: Use actual sample rate instead of hardcoded 44100
    const double currentSampleRate = audioProcessor.getCurrentSampleRate();
    double recordedSeconds = (double)recordedSamples / currentSampleRate;
    showStatusMessage(juce::String::formatted("? saved %.1fs to myBuffer.wav", recordedSeconds), 4000);

    // CRITICAL FIX: Explicitly update button states after saving
    // This ensures Terry radio buttons are updated immediately when savedSamples changes
    updateAllGenerationButtonStates();

    // Force waveform redraw to show the solidified state
    repaint();
}

void Gary4juceAudioProcessorEditor::startPollingForResults(const juce::String& sessionId)
{
    audioProcessor.setCurrentSessionId(sessionId);
    isPolling = true;
    isGenerating = true;
    generationProgress = 0;
    resetStallDetection();
    pollingStartTimeMs = juce::Time::getCurrentTime().toMilliseconds();
    updateAllGenerationButtonStates();
    repaint(); // Start showing progress visualization
    DBG("Started polling for session: " + sessionId);
}

void Gary4juceAudioProcessorEditor::stopPolling()
{
    isPolling = false;
    
    // Wait briefly for any ongoing poll requests to notice the flag
    juce::Thread::sleep(50);
    
    DBG("Stopped polling - ongoing requests should abort");
}

void Gary4juceAudioProcessorEditor::pollForResults()
{
    const juce::String sessionId = audioProcessor.getCurrentSessionId();
    if (!isPolling || sessionId.isEmpty())
        return;

    // If we�re in warmup, keep the stall detector quiet.
    // (We still want the stall detector for real backend drops.)
    if (withinWarmup)
        lastProgressUpdateTime = juce::Time::getCurrentTime().toMilliseconds();

    // Don�t start another poll while one is in flight
    if (pollInFlight.exchange(true))
        return;

    // Optional: if you have a very fast poll timer, add a micro-backoff while warming
    const bool softBackoff = withinWarmup || isCurrentlyQueued;
    if (softBackoff)
        juce::Thread::sleep(60); // tiny spacing to avoid hammering during cold start

    // Only trip �stall� when not warming up
    if (!withinWarmup && checkForGenerationStall())
    {
        pollInFlight = false;
        handleGenerationStall();
        return;
    }

    juce::Thread::launch([this, sessionId]()
        {
            auto clearInFlight = [this]() { pollInFlight = false; };

            // SAFETY: Exit if polling stopped
            if (!isPolling || sessionId.isEmpty())
            {
                clearInFlight();
                DBG("Polling aborted - no longer active");
                return;
            }

            juce::URL pollUrl(getServiceUrl(ServiceType::Gary, "/api/juce/poll_status/" + sessionId));

            try
            {
                int httpStatus = 0;
                juce::StringPairArray responseHeaders;

                auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(withinWarmup ? 4000 : 8000) // shorter during warmup
                    .withNumRedirectsToFollow(3)
                    .withExtraHeaders("Accept: application/json\r\nContent-Type: application/json")
                    .withResponseHeaders(&responseHeaders)
                    .withStatusCode(&httpStatus);

                // Attempt #1
                std::unique_ptr<juce::InputStream> stream(pollUrl.createInputStream(options));

                // If we didn�t get a stream while warming, quick retry once
                if (stream == nullptr && (withinWarmup || isCurrentlyQueued || isGenerating))
                {
                    DBG("Polling: null stream during warmup/active; quick retry");
                    juce::Thread::sleep(150);

                    // Move-assign the new unique_ptr result into our existing one
                    auto retryStream = pollUrl.createInputStream(options); // returns std::unique_ptr<InputStream>
                    if (retryStream)
                        stream = std::move(retryStream); // or simply: stream = pollUrl.createInputStream(options);
                }

                if (stream != nullptr)
                {
                    const auto responseText = stream->readEntireStreamAsString();
                    lastGoodPollMs = juce::Time::getCurrentTime().toMilliseconds();
                    clearInFlight();

                    juce::MessageManager::callAsync([this, responseText]()
                        {
                            if (!isPolling) { DBG("Polling callback aborted"); return; }
                            handlePollingResponse(responseText);
                        });
                    return;
                }

                // No stream: consider whether this is a transient warmup hiccup
                clearInFlight();

                const bool treatAsTransient =
                    withinWarmup || isCurrentlyQueued || isGenerating;

                if (treatAsTransient)
                {
                    // Keep UI alive; don�t escalate to failure.
                    juce::MessageManager::callAsync([this]()
                        {
                            // keep user informed but gentle
                            showStatusMessage("warming up� (network jitter)", 2500);
                            // also reset stall timer so we don�t trip
                            lastProgressUpdateTime = juce::Time::getCurrentTime().toMilliseconds();
                        });
                    return; // simply wait for next timer tick to poll again
                }

                // Not warming/queued and no stream: do your existing health check path
                DBG("Polling failed - checking backend health (no stream; status unknown)");
                juce::MessageManager::callAsync([this]()
                    {
                        if (!isPolling) { DBG("Polling health check callback aborted"); return; }
                        audioProcessor.checkBackendHealth();

                        juce::Timer::callAfterDelay(6000, [this]()
                            {
                                if (!audioProcessor.isBackendConnected())
                                    handleBackendDisconnection();
                                else
                                    handleGenerationFailure("polling failed - try again");
                            });
                    });
            }
            catch (const std::exception& e)
            {
                clearInFlight();
                DBG("Polling exception: " + juce::String(e.what()));

                // During warmup, treat as transient
                if (withinWarmup || isCurrentlyQueued || isGenerating)
                {
                    juce::MessageManager::callAsync([this]()
                        {
                            showStatusMessage("warming up� (transient)", 2500);
                            lastProgressUpdateTime = juce::Time::getCurrentTime().toMilliseconds();
                        });
                    return;
                }

                juce::MessageManager::callAsync([this]() {
                    handleGenerationFailure("network error during generation");
                    });
            }
            catch (...)
            {
                clearInFlight();
                DBG("Unknown polling exception");

                if (withinWarmup || isCurrentlyQueued || isGenerating)
                {
                    juce::MessageManager::callAsync([this]()
                        {
                            showStatusMessage("warming up� (transient)", 2500);
                            lastProgressUpdateTime = juce::Time::getCurrentTime().toMilliseconds();
                        });
                    return;
                }

                juce::MessageManager::callAsync([this]() {
                    handleGenerationFailure("network error during generation");
                    });
            }
        });
}


void Gary4juceAudioProcessorEditor::handlePollingResponse(const juce::String& responseText)
{
    if (responseText.isEmpty())
    {
        DBG("Empty polling response - backend likely down");
        stopPolling();

        // Empty response indicates backend failure - check health immediately
        audioProcessor.checkBackendHealth();

        juce::Timer::callAfterDelay(3000, [this]() {
            if (!audioProcessor.isBackendConnected())
            {
                handleBackendDisconnection();
                lastBackendDisconnectionPopupTime = juce::Time::getCurrentTime();
            }
            else
            {
                showStatusMessage("backend reachable but no response; retry", 3000);
            }
            });
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
                // --- WARMUP HANDLING (model download / cold start) ---
                // Some backends return success:false with a descriptive error while the model weights
                // are being downloaded/loaded on first use. Treat those as a non-fatal "warmup" state.
                {
                    juce::String errorMsg = responseObj->getProperty("error").toString().toLowerCase();
                    const bool looksLikeWarmup =
                        errorMsg.contains("download") ||
                        errorMsg.contains("downloading") ||
                        errorMsg.contains("loading model") ||
                        errorMsg.contains("loading weights") ||
                        errorMsg.contains("warmup") ||
                        errorMsg.contains("warming") ||
                        errorMsg.contains("huggingface") ||
                        errorMsg.contains("initializing");
                    if (looksLikeWarmup)
                    {
                        // Mark and keep polling without treating this as a failure
                        withinWarmup = true;
                        lastProgressUpdateTime = juce::Time::getCurrentTime().toMilliseconds(); // prevent stall detector
                        isCurrentlyQueued = true; // keep UI in "busy" state

                        // Short, friendly status � we avoid failing the run
                        showStatusMessage("warming up (downloading model)...", 4000);
                        DBG("Polling: backend in warmup/cold-start: " + errorMsg);

                        // Do NOT stopPolling(); just return so the timer continues polling.
                        return;
                    }
                }
                // --- END WARMUP HANDLING ---

                DBG("Polling error: " + responseObj->getProperty("error").toString());
                stopPolling();
                showStatusMessage("processing failed", 3000);
                return;
            }

            // Check if still in progress
            bool generationInProgress = responseObj->getProperty("generation_in_progress");
            bool transformInProgress = responseObj->getProperty("transform_in_progress");

            if (generationInProgress || transformInProgress)
            {
                // If we were previously in warmup, keep UI responsive and prevent stall
                if (withinWarmup)
                {
                    // Heuristics to exit warmup: any server progress > 0, or explicit queue ready
                    int warmProgressCheck = responseObj->getProperty("progress");
                    juce::String warmQueueStatus;
                    if (auto* warmQueueObj = responseObj->getProperty("queue_status").getDynamicObject())
                        warmQueueStatus = warmQueueObj->getProperty("status").toString();

                    if (warmProgressCheck > 0 || warmQueueStatus == "ready")
                    {
                        DBG("Exiting warmup state (progress or ready observed)");
                        withinWarmup = false;
                    }
                    else
                    {
                        // Stay in warmup: keep resetting the stall timer and show a gentle status
                        lastProgressUpdateTime = juce::Time::getCurrentTime().toMilliseconds();
                        showStatusMessage("warming up...", 3000);
                        // Don't early-return: allow the rest of the block to parse queue info etc.
                    }
                }

                // Get real progress from server
                int serverProgress = responseObj->getProperty("progress");
                serverProgress = juce::jlimit(0, 100, serverProgress);

                // NEW: Check for queue status information (from enhanced /task_notification)
                bool hasValidQueueStatus = false;
                bool isQueuedForProcessing = false;
                juce::String queueMessage;
                juce::String queueStatus;

                if (auto* queueStatusObj = responseObj->getProperty("queue_status").getDynamicObject())
                {
                    queueStatus = queueStatusObj->getProperty("status").toString();
                    queueMessage = queueStatusObj->getProperty("message").toString();
                    hasValidQueueStatus = !queueStatus.isEmpty();
                    isQueuedForProcessing = (queueStatus == "queued");

                    DBG("Queue status found - Status: " + queueStatus + ", Message: " + queueMessage);
                }

                // Update stall detection tracking
                auto currentTime = juce::Time::getCurrentTime().toMilliseconds();

                // ENHANCED: Reset stall detection on progress changes OR valid queue status
                if (serverProgress > lastKnownServerProgress || hasValidQueueStatus)
                {
                    lastProgressUpdateTime = currentTime;
                    if (serverProgress > lastKnownServerProgress)
                        lastKnownServerProgress = serverProgress;
                    hasDetectedStall = false;

                    DBG("Stall detection reset - Progress: " + juce::String(serverProgress) +
                        "%, Valid queue status: " + juce::String(hasValidQueueStatus ? "yes" : "no"));
                }

                // Set up smooth animation to new target (only if not queued)
                if (!isQueuedForProcessing)
                {
                    lastKnownProgress = generationProgress;  // Where we are now (visually)
                    targetProgress = serverProgress;         // Where server says we should be
                    smoothProgressAnimation = true;          // Start smooth animation
                }

                // Update isCurrentlyQueued state
                isCurrentlyQueued = isQueuedForProcessing;

                // ENHANCED: Show appropriate status messages based on queue state
                if (isQueuedForProcessing && hasValidQueueStatus)
                {
                    // Parse queue status data for concise message
                    if (auto* queueStatusObj = responseObj->getProperty("queue_status").getDynamicObject())
                    {
                        int position = (int)queueStatusObj->getProperty("position");
                        juce::String estimatedTime = queueStatusObj->getProperty("estimated_time").toString();
                        int estimatedSeconds = (int)queueStatusObj->getProperty("estimated_seconds");

                        // Create concise queue message
                        juce::String conciseMessage;

                        if (position > 0)
                        {
                            // Format: "busy rn - pos 2 - wait ~45s"
                            juce::String shortTime;
                            if (estimatedSeconds < 60)
                                shortTime = juce::String(estimatedSeconds) + "s";
                            else
                                shortTime = juce::String(estimatedSeconds / 60) + "m";

                            conciseMessage = "busy rn - queued -...position # " + juce::String(position) + " - wait ~" + shortTime;
                        }
                        else
                        {
                            // Position 0 or unknown - just show that we're queued
                            conciseMessage = "queued - starting soon...";
                        }

                        showStatusMessage(conciseMessage, 5000);
                        DBG("Displaying concise queue message: " + conciseMessage);
                        DBG("Full queue details - Position: " + juce::String(position) +
                            ", Estimated time: " + estimatedTime);
                    }
                    else
                    {
                        // Fallback if queue data is missing
                        showStatusMessage("queued for processing...", 5000);
                    }
                }
                else if (serverProgress > 0 || queueStatus == "ready")
                {
                    // Show progress-based messages only when actually processing (not queued)
                    if (currentTab == ModelTab::Terry)
                    {
                        showStatusMessage("transforming: " + juce::String(serverProgress) + "%", 5000);
                        DBG("Transform progress: " + juce::String(serverProgress) + "%, animating from " +
                            juce::String(lastKnownProgress));
                    }
                    else
                    {
                        showStatusMessage("cooking: " + juce::String(serverProgress) + "%", 5000);
                        DBG("Generation progress: " + juce::String(serverProgress) + "%, animating from " +
                            juce::String(lastKnownProgress));
                    }
                }
                else
                {
                    // Fallback message when we have no specific queue info but task is in progress
                    if (currentTab == ModelTab::Terry)
                    {
                        showStatusMessage("processing transform...", 5000);
                    }
                    else
                    {
                        showStatusMessage("processing audio...", 5000);
                    }
                }

                return;
            }

            // COMPLETED - Check what TYPE of completion this is FIRST
            auto audioData = responseObj->getProperty("audio_data").toString();
            auto status = responseObj->getProperty("status").toString();

            withinWarmup = false; // completed/terminal statuses should clear warmup

            DBG("=== POLLING RESPONSE ANALYSIS ===");
            DBG("Status: " + status);
            DBG("Audio data length: " + juce::String(audioData.length()));
            DBG("Audio data empty: " + juce::String(audioData.isEmpty() ? "YES" : "NO"));
            DBG("Full response: " + responseText.substring(0, 500) + "...");

            // Reset queue state when task completes
            isCurrentlyQueued = false;

            if (audioData.isNotEmpty() && status == "completed")
            {
                DBG("=== LEGITIMATE COMPLETION DETECTED ===");
                stopPolling();
                isGenerating = false;

                // DIFFERENTIATE: Check if we're completing a transform vs generation
                if (transformInProgress || currentTab == ModelTab::Terry)
                {
                    // TRANSFORM COMPLETION - Keep session ID for undo
                    transformWithTerryButton.setButtonText("transform with terry");
                    showStatusMessage("transform complete!", 3000);
                    saveGeneratedAudio(audioData);
                    DBG("Successfully received transformed audio: " + juce::String(audioData.length()) + " chars");

                    // Enable undo button now that transform is complete
                    audioProcessor.setUndoTransformAvailable(true); // ADD THIS
                    undoTransformButton.setEnabled(true);
                    audioProcessor.setRetryAvailable(false);
                    // DON'T clear currentSessionId - we need it for undo!

                }
                else
                {
                    // GENERATION COMPLETION (Gary/Jerry)
                    showStatusMessage("audio generation complete!", 3000);
                    audioProcessor.setUndoTransformAvailable(false);
                    audioProcessor.setRetryAvailable(true);
                    saveGeneratedAudio(audioData);
                    DBG("Successfully received generated audio: " + juce::String(audioData.length()) + " chars");

                    // Check if this was a continue operation using our flag
                    if (continueInProgress)
                    {
                        // This was a continue operation - keep session ID and enable retry
                        continueInProgress = false;  // Reset the flag
                        retryButton.setButtonText("retry");  // Reset retry button text
                        updateRetryButtonState();
                        DBG("Continue operation completed - retry button enabled");
                    }
                    else
                    {
                        // This was initial generation - clear session ID, disable retry
                        audioProcessor.setUndoTransformAvailable(false);
                        audioProcessor.setRetryAvailable(false);
                        audioProcessor.clearCurrentSessionId();
                        updateRetryButtonState();
                        DBG("Initial generation completed - retry button disabled");
                    }

                    updateContinueButtonState();
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
                        showStatusMessage("transform failed: " + error, 5000);
                        audioProcessor.setUndoTransformAvailable(false);
                        undoTransformButton.setEnabled(false);
                        audioProcessor.setRetryAvailable(false);
                        updateRetryButtonState();
                    }
                    else
                    {
                        showStatusMessage("generation failed: " + error, 5000);
                        audioProcessor.setRetryAvailable(true);
                        updateRetryButtonState();
                    }

                    isGenerating = false;
                    isCurrentlyQueued = false;
                    updateAllGenerationButtonStates();
                    repaint();
                }
                else if (status == "completed")
                {
                    stopPolling();

                    if (transformInProgress || currentTab == ModelTab::Terry)
                    {
                        showStatusMessage("transform completed but no audio received", 3000);
                    }
                    else
                    {
                        showStatusMessage("generation completed but no audio received", 3000);
                    }

                    isGenerating = false;
                    isCurrentlyQueued = false;
                    updateAllGenerationButtonStates();
                    repaint();
                }
            }
        }
        else
        {
            DBG("Failed to parse polling response as JSON - backend likely down");
            stopPolling();

            // JSON parsing failure indicates backend issues - check health
            audioProcessor.checkBackendHealth();

            juce::Timer::callAfterDelay(3000, [this]() {
                if (!audioProcessor.isBackendConnected())
                {
                    handleBackendDisconnection();
                    lastBackendDisconnectionPopupTime = juce::Time::getCurrentTime();
                }
                else
                {
                    showStatusMessage("bad response; retry", 3000);
                }
                });
        }
    }
    catch (...)
    {
        DBG("Exception parsing polling response - backend likely down");
        stopPolling();

        // Exception indicates backend issues - check health
        audioProcessor.checkBackendHealth();

        juce::Timer::callAfterDelay(3000, [this]() {
            if (!audioProcessor.isBackendConnected())
            {
                handleBackendDisconnection();
                lastBackendDisconnectionPopupTime = juce::Time::getCurrentTime();
            }
            else
            {
                showStatusMessage("parse error; retry", 3000);
            }
            });
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
            showStatusMessage("generated audio ready", 3000);
            DBG("Generated audio saved to: " + outputAudioFile.getFullPathName());

            // STOP any current playback before loading new audio
            // This prevents confusion where old audio plays but new waveform shows
            if (isPlayingOutput || isPausedOutput)
            {
                stopOutputPlayback(); // Full stop - reset to beginning
                showStatusMessage("new audio ready. press play to hear it.", 3000);
            }

            // Load the audio file into our buffer for waveform display
            loadOutputAudioFile();

            // Reset button text after successful generation
            sendToGaryButton.setButtonText("send to gary");
            continueButton.setButtonText("continue");

            // Reset generation state
            isGenerating = false;
            generationProgress = 0;

            // Update all button states centrally
            updateAllGenerationButtonStates();

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
    continueInProgress = false;  // This is NOT a continue operation
    generationProgress = 0;
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;
    resetStallDetection();

    
    if (savedSamples <= 0)
    {
        isGenerating = false; // Reset if we're returning early
        showStatusMessage("please save your recording first!");
        return;
    }

    if (!isConnected)
    {
        isGenerating = false; // Reset if we're returning early
        showStatusMessage("backend not connected - check connection first");
        return;
    }

    // Starting initial Gary generation - clear any previous session
    DBG("Starting initial Gary generation - clearing previous session");
    audioProcessor.clearCurrentSessionId();
    updateRetryButtonState();

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
        showStatusMessage("audio file not found - save recording first");
        return;
    }

    // Read and encode audio file
    juce::MemoryBlock audioData;
    if (!audioFile.loadFileAsData(audioData))
    {
        showStatusMessage("failed to read audio file");
        return;
    }

    // Verify we have audio data
    if (audioData.getSize() == 0)
    {
        showStatusMessage("audio file is empty");
        return;
    }

    auto base64Audio = juce::Base64::toBase64(audioData.getData(), audioData.getSize());

    // Debug: Log audio data size
    DBG("Audio file size: " + juce::String(audioData.getSize()) + " bytes");
    DBG("Base64 length: " + juce::String(base64Audio.length()) + " chars");

    // Button text feedback and status during processing (AFTER button state update)
    sendToGaryButton.setButtonText("sending...");
    showStatusMessage("sending audio to gary...");

    updateAllGenerationButtonStates();
    repaint(); // Force immediate UI update

    // Create HTTP request in background thread
    juce::Thread::launch([this, selectedModel, base64Audio]() {
        // SAFETY: Exit if generation stopped
        if (!isGenerating) {
            DBG("Gary request aborted - generation stopped");
            return;
        }

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

        juce::URL url(getServiceUrl(ServiceType::Gary, "/api/juce/process_audio"));

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
            // SAFETY: Don't process if generation stopped
            if (!isGenerating) {
                DBG("Gary callback aborted");
                return;
            }

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
                        showStatusMessage("sent to gary. processing...", 2000);
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
                bool shouldCheckHealth = false;
                
                if (statusCode == 0 && audioProcessor.getIsUsingLocalhost())
                    errorMsg = "Cannot connect to localhost - ensure Docker Compose is running";
                else if (statusCode == 0)
                {
                    errorMsg = "Failed to connect to remote backend";
                    shouldCheckHealth = true;  // Connection failure on remote - check if VM is down
                }
                else if (statusCode >= 400)
                {
                    errorMsg = "Server error (HTTP " + juce::String(statusCode) + ")";
                    shouldCheckHealth = true;  // Server error - might be backend issue
                }
                else
                    errorMsg = "Empty response from server";

                showStatusMessage(errorMsg, 4000);
                DBG("Gary request failed: " + errorMsg);
                
                // Check backend health if connection/server failure
                if (shouldCheckHealth)
                {
                    DBG("Gary failed - checking backend health");
                    audioProcessor.checkBackendHealth();
                    
                    // Give health check time, then handle result
                    juce::Timer::callAfterDelay(6000, [this]() {
                        if (!audioProcessor.isBackendConnected())
                        {
                            handleBackendDisconnection();
                            lastBackendDisconnectionPopupTime = juce::Time::getCurrentTime();
                        }
                    });
                }
                
                // Reset button text on error
                sendToGaryButton.setButtonText("send to gary");
            }


            });
        });
}

void Gary4juceAudioProcessorEditor::continueMusic()
{
    if (!hasOutputAudio)
    {
        showStatusMessage("no audio to continue", 2000);
        return;
    }

    // Encode current output audio as base64
    if (!outputAudioFile.exists())
    {
        showStatusMessage("output file not found", 2000);
        return;
    }

    // Read the audio file as binary data
    juce::MemoryBlock audioData;
    if (!outputAudioFile.loadFileAsData(audioData))
    {
        showStatusMessage("failed to read audio file", 3000);
        return;
    }

    // Convert to base64
    auto base64Audio = juce::Base64::toBase64(audioData.getData(), audioData.getSize());

    sendContinueRequest(base64Audio);
}

void Gary4juceAudioProcessorEditor::debugModelSelection(const juce::String& functionName)
{
    const juce::StringArray modelNames = {
        "thepatch/vanya_ai_dnb_0.1",
        "thepatch/bleeps-medium",
        "thepatch/gary_orchestra_2",
        "thepatch/hoenn_lofi"
    };

    DBG("=== MODEL SELECTION DEBUG (" + functionName + ") ===");
    DBG("ComboBox selectedId: " + juce::String(modelComboBox.getSelectedId()));
    DBG("ComboBox selectedText: " + modelComboBox.getText());
    DBG("currentModelIndex: " + juce::String(currentModelIndex));

    if (currentModelIndex >= 0 && currentModelIndex < modelNames.size())
    {
        DBG("Selected model: " + modelNames[currentModelIndex]);
    }
    else
    {
        DBG("ERROR: currentModelIndex out of bounds!");
    }
    DBG("=== END DEBUG ===");
}

// UPDATED: sendContinueRequest with proper debugging
void Gary4juceAudioProcessorEditor::sendContinueRequest(const juce::String& audioData)
{
    // Debug model selection BEFORE starting request
    debugModelSelection("sendContinueRequest");

    DBG("Sending continue request with " + juce::String(audioData.length()) + " chars of audio data");
    showStatusMessage("requesting continuation...", 3000);

    // Reset generation state immediately
    isGenerating = true;
    continueInProgress = true;  // Mark this as a continue operation
    generationProgress = 0;
    resetStallDetection();
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;
    updateAllGenerationButtonStates();
    repaint(); // Force immediate UI update

    // Button text feedback during processing
    continueButton.setButtonText("continuing...");

    // Capture current model index NOW (before thread launch)
    int capturedModelIndex = currentModelIndex;
    DBG("Captured model index for continue: " + juce::String(capturedModelIndex));

    // Create HTTP request in background thread
    juce::Thread::launch([this, audioData, capturedModelIndex]() {
        // SAFETY: Exit if generation stopped
        if (!isGenerating) {
            DBG("Continue request aborted - generation stopped");
            return;
        }

        auto startTime = juce::Time::getCurrentTime();

        // Get the model names array
        const juce::StringArray modelNames = {
            "thepatch/vanya_ai_dnb_0.1",
            "thepatch/bleeps-medium",
            "thepatch/gary_orchestra_2",
            "thepatch/hoenn_lofi"
        };

        // Safely get selected model with bounds checking
        juce::String selectedModel;
        if (capturedModelIndex >= 0 && capturedModelIndex < modelNames.size())
        {
            selectedModel = modelNames[capturedModelIndex];
        }
        else
        {
            DBG("ERROR: Invalid model index " + juce::String(capturedModelIndex) + ", defaulting to first model");
            selectedModel = modelNames[0];
        }

        DBG("Continue using model: " + selectedModel + " (index: " + juce::String(capturedModelIndex) + ")");

        // Create JSON payload - same structure as sendToGary
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("audio_data", audioData);
        jsonRequest->setProperty("prompt_duration", (int)currentPromptDuration);
        jsonRequest->setProperty("model_name", selectedModel); // Use captured selected model
        jsonRequest->setProperty("top_k", 250);
        jsonRequest->setProperty("temperature", 1.0);
        jsonRequest->setProperty("cfg_coef", 3.0);
        jsonRequest->setProperty("description", "");

        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        DBG("Continue JSON payload size: " + juce::String(jsonString.length()) + " characters");

        juce::URL url(getServiceUrl(ServiceType::Gary, "/api/juce/continue_music"));

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
            // SAFETY: Don't process if generation stopped
            if (!isGenerating) {
                DBG("Continue callback aborted");
                return;
            }
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
                        showStatusMessage("continuation queued...", 2000);

                        // Start polling for results (will replace current output audio when complete)
                        startPollingForResults(sessionId);
                    }
                    else
                    {
                        auto error = obj->getProperty("error").toString();
                        showStatusMessage("continue failed: " + error, 5000);
                        continueButton.setButtonText("continue");
                    }
                }
                else
                {
                    showStatusMessage("invalid response format", 3000);
                    continueButton.setButtonText("continue");
                }
            }
            else
            {
                juce::String errorMsg;
                bool shouldCheckHealth = false;
                
                if (statusCode == 0 && audioProcessor.getIsUsingLocalhost())
                    errorMsg = "Cannot connect to localhost - ensure Docker Compose is running";
                else if (statusCode == 0)
                {
                    errorMsg = "Failed to connect to remote backend";
                    shouldCheckHealth = true;  // Connection failure on remote - check if VM is down
                }
                else if (statusCode >= 400)
                {
                    errorMsg = "Server error (HTTP " + juce::String(statusCode) + ")";
                    shouldCheckHealth = true;  // Server error - might be backend issue
                }
                else
                    errorMsg = "Empty response from server";

                showStatusMessage(errorMsg, 4000);
                DBG("Continue request failed: " + errorMsg);
                
                // Check backend health if connection/server failure
                if (shouldCheckHealth)
                {
                    DBG("Continue failed - checking backend health");
                    audioProcessor.checkBackendHealth();
                    
                    // Give health check time, then handle result
                    juce::Timer::callAfterDelay(6000, [this]() {
                        if (!audioProcessor.isBackendConnected())
                        {
                            handleBackendDisconnection();
                            lastBackendDisconnectionPopupTime = juce::Time::getCurrentTime();
                        }
                    });
                }
                
                continueButton.setButtonText("continue");
            }
            });
        });
}

void Gary4juceAudioProcessorEditor::retryLastContinuation()
{
    // Check if we have a session ID from the last continue operation
    juce::String sessionId = audioProcessor.getCurrentSessionId();
    if (sessionId.isEmpty())
    {
        showStatusMessage("no previous continuation to retry", 3000);
        return;
    }

    if (!isConnected)
    {
        showStatusMessage("backend not connected - check connection first");
        return;
    }

    // ADD THIS: Stop any existing polling first to prevent race conditions
    if (isPolling) {
        DBG("Stopping existing polling before retry");
        stopPolling();
        // Give it a moment to clean up
        juce::Thread::sleep(50);
    }


    DBG("Retrying last continuation for session: " + sessionId);
    
    // Reset generation state immediately (like other operations)
    isGenerating = true;
    continueInProgress = true;  // Retry is also a continue-type operation
    generationProgress = 0;
    resetStallDetection();
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;
    updateAllGenerationButtonStates();
    repaint();

    // Button text feedback during processing
    retryButton.setButtonText("retrying...");
    
    showStatusMessage("retrying last continuation...", 2000);

    // Create HTTP request in background thread (same pattern as other requests)
    juce::Thread::launch([this, sessionId]() {
        // SAFETY: Exit if generation stopped
        if (!isGenerating) {
            DBG("Retry request aborted - generation stopped");
            return;
        }
        
        auto startTime = juce::Time::getCurrentTime();

        // Create JSON payload with current UI parameters
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("session_id", sessionId);
        jsonRequest->setProperty("prompt_duration", (int)currentPromptDuration);
        
        // Get current model selection
        const juce::StringArray modelNames = {
            "thepatch/vanya_ai_dnb_0.1",
            "thepatch/bleeps-medium", 
            "thepatch/gary_orchestra_2",
            "thepatch/hoenn_lofi"
        };
        jsonRequest->setProperty("model_name", modelNames[currentModelIndex]);
        jsonRequest->setProperty("top_k", 250);
        jsonRequest->setProperty("temperature", 1.0);
        jsonRequest->setProperty("cfg_coef", 3.0);
        jsonRequest->setProperty("description", "");

        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        DBG("Retry JSON payload: " + jsonString);

        juce::URL url(getServiceUrl(ServiceType::Gary, "/api/juce/retry_music"));

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

            if (stream != nullptr)
            {
                responseText = stream->readEntireStreamAsString();
                statusCode = 200;
                DBG("Retry HTTP request completed");
            }
            else
            {
                DBG("Failed to create input stream for retry request");
                statusCode = 0;
            }
        }
        catch (const std::exception& e)
        {
            DBG("Retry HTTP request exception: " + juce::String(e.what()));
            statusCode = 0;
        }

        // Handle response on main thread
        juce::MessageManager::callAsync([this, responseText, statusCode]() {
            // SAFETY: Don't process if generation stopped
            if (!isGenerating) {
                DBG("Retry callback aborted");
                return;
            }
            if (statusCode == 200 && responseText.isNotEmpty())
            {
                // Parse response JSON
                auto responseVar = juce::JSON::parse(responseText);
                if (auto* obj = responseVar.getDynamicObject())
                {
                    bool requestSuccess = obj->getProperty("success");
                    if (requestSuccess)
                    {
                        auto newSessionId = obj->getProperty("session_id").toString();
                        DBG("Retry request queued, session ID: " + newSessionId);
                        showStatusMessage("retry queued...", 2000);

                        // Start polling for results (will replace current output when complete)
                        startPollingForResults(newSessionId);
                    }
                    else
                    {
                        auto error = obj->getProperty("error").toString();
                        showStatusMessage("retry failed: " + error, 5000);
                        updateRetryButtonState();
                    }
                }
                else
                {
                    showStatusMessage("invalid retry response format", 3000);
                    updateRetryButtonState();
                }
            }
            else
            {
                juce::String errorMsg;
                bool shouldCheckHealth = false;
                
                if (statusCode == 0 && audioProcessor.getIsUsingLocalhost())
                    errorMsg = "Cannot connect to localhost - ensure Docker Compose is running";
                else if (statusCode == 0)
                {
                    errorMsg = "Failed to connect to remote backend";
                    shouldCheckHealth = true;  // Connection failure on remote - check if VM is down
                }
                else if (statusCode >= 400)
                {
                    errorMsg = "Server error (HTTP " + juce::String(statusCode) + ")";
                    shouldCheckHealth = true;  // Server error - might be backend issue
                }
                else
                    errorMsg = "Empty response from server";

                showStatusMessage(errorMsg, 4000);
                DBG("Retry request failed: " + errorMsg);
                
                // Check backend health if connection/server failure
                if (shouldCheckHealth)
                {
                    DBG("Retry failed - checking backend health");
                    audioProcessor.checkBackendHealth();
                    
                    // Give health check time, then handle result
                    juce::Timer::callAfterDelay(6000, [this]() {
                        if (!audioProcessor.isBackendConnected())
                        {
                            handleBackendDisconnection();
                            lastBackendDisconnectionPopupTime = juce::Time::getCurrentTime();
                        }
                    });
                }
                
                updateRetryButtonState();
            }
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

void Gary4juceAudioProcessorEditor::styleLoopTypeButton(CustomButton& button, bool selected)
{
    if (selected)
    {
        // Selected state - use Gary style for visual emphasis (red theme)
        button.setButtonStyle(CustomButton::ButtonStyle::Gary);
    }
    else
    {
        // Unselected state - standard button style
        button.setButtonStyle(CustomButton::ButtonStyle::Standard);
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
        showStatusMessage("backend not connected - check connection first");
        return;
    }

    if (currentJerryPrompt.trim().isEmpty())
    {
        showStatusMessage("please enter a text prompt for jerry");
        return;
    }

    // 1. Get DAW BPM from processor
    double bpm = audioProcessor.getCurrentBPM();

    // 2. Append BPM to prompt (same pattern as Max for Live)
    juce::String fullPrompt = currentJerryPrompt + " " + juce::String((int)bpm) + "bpm";

    // 3. Determine endpoint based on smart loop toggle
    juce::String endpoint;

    if (audioProcessor.getIsUsingLocalhost())
    {
        // Localhost endpoints do NOT have the /audio prefix
        endpoint = generateAsLoop ? "/generate/loop" : "/generate";
    }
    else
    {
        // Remote backend requires /audio prefix
        endpoint = generateAsLoop ? "/audio/generate/loop" : "/audio/generate";
    }
    juce::String statusText = generateAsLoop ?
        "cooking a smart loop with jerry..." : "baking with jerry...";

    DBG("Jerry generating with prompt: " + fullPrompt);
    DBG("Endpoint: " + endpoint);
    DBG("CFG: " + juce::String(currentJerryCfg, 1) + ", Steps: " + juce::String(currentJerrySteps));
    if (generateAsLoop) {
        DBG("Loop Type: " + currentLoopType);
    }

    // Button text feedback and status during processing
    generateWithJerryButton.setButtonText("generating...");
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
        juce::URL url(getServiceUrl(ServiceType::Jerry, endpoint));

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
                            // Reset button text on successful completion
                            generateWithJerryButton.setButtonText("generate with jerry");
                            
                            // Save generated audio (same as Gary's saveGeneratedAudio function)
                            saveGeneratedAudio(audioBase64);

                            audioProcessor.clearCurrentSessionId();
                            audioProcessor.setUndoTransformAvailable(false);  // ADD THIS
                            audioProcessor.setRetryAvailable(false);          // ADD THIS

                            // Get metadata if available
                            if (auto* metadata = responseObj->getProperty("metadata").getDynamicObject())
                            {
                                auto genTime = metadata->getProperty("generation_time").toString();
                                auto rtFactor = metadata->getProperty("realtime_factor").toString();

                                // Show different success message based on generation type
                                if (generateAsLoop) {
                                    auto bars = (int)metadata->getProperty("bars");
                                    auto loopDuration = (double)metadata->getProperty("loop_duration_seconds");
                                    showStatusMessage("smart loop rdy " + juce::String(bars) + " bars (" +
                                        juce::String(loopDuration, 1) + "s) " + genTime + "s", 5000);
                                    DBG("Jerry loop metadata - Bars: " + juce::String(bars) +
                                        ", Duration: " + juce::String(loopDuration, 1) + "s");
                                }
                                else {
                                    showStatusMessage("jerry's done already " + genTime + "s (" + rtFactor + "x RT)", 4000);
                                }

                                DBG("Jerry metadata - Generation time: " + genTime + "s, RT factor: " + rtFactor + "x");
                            }
                            else
                            {
                                juce::String successMsg = generateAsLoop ?
                                    "smart loop rdy" : "jerry's done already";
                                showStatusMessage(successMsg, 3000);
                            }
                        }
                        else
                        {
                            showStatusMessage("jerry finished but no audio received", 3000);
                            DBG("Jerry success but missing audio_base64");
                        }
                    }
                    else
                    {
                        auto error = responseObj->getProperty("error").toString();
                        showStatusMessage("jerry error: " + error, 5000);
                        DBG("Jerry server error: " + error);
                    }
                }
                else
                {
                    showStatusMessage("invalid JSON response from jerry", 4000);
                    DBG("Failed to parse Jerry JSON response");
                    
                    // Invalid JSON could indicate backend issues - check health
                    DBG("Jerry JSON parsing failed - checking backend health");
                    audioProcessor.checkBackendHealth();
                    
                    juce::Timer::callAfterDelay(6000, [this]() {
                        if (!audioProcessor.isBackendConnected())
                        {
                            handleBackendDisconnection();
                        }
                    });
                }
            }
            else
            {
                juce::String errorMsg;
                bool shouldCheckHealth = false;
                
                if (statusCode == 0 && audioProcessor.getIsUsingLocalhost())
                    errorMsg = "cannot connect to jerry on localhost - ensure docker compose is running";
                else if (statusCode == 0)
                {
                    errorMsg = "failed to connect to jerry on remote backend";
                    shouldCheckHealth = true;  // Connection failure on remote - check if VM is down
                }
                else if (statusCode >= 400)
                {
                    errorMsg = "jerry server error (HTTP " + juce::String(statusCode) + ")";
                    shouldCheckHealth = true;  // Server error - might be backend issue
                }
                else
                    errorMsg = "empty response from jerry";

                showStatusMessage(errorMsg, 4000);
                DBG("Jerry request failed: " + errorMsg);
                
                // Check backend health if connection/server failure
                if (shouldCheckHealth)
                {
                    DBG("Jerry failed - checking backend health");
                    audioProcessor.checkBackendHealth();
                    
                    // Give health check time, then handle result
                    juce::Timer::callAfterDelay(6000, [this]() {
                        if (!audioProcessor.isBackendConnected())
                        {
                            handleBackendDisconnection();
                            lastBackendDisconnectionPopupTime = juce::Time::getCurrentTime();
                        }
                    });
                }
            }

            // Reset button text on error
            generateWithJerryButton.setButtonText("generate with jerry");
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
    
    // CRITICAL: Persist this state in processor
    audioProcessor.setTransformRecording(useRecording);
    
    DBG("Terry audio source set to: " + juce::String(useRecording ? "Recording" : "Output"));
    updateTerrySourceButtons();
}

void Gary4juceAudioProcessorEditor::updateTerrySourceButtons()
{
    bool canTransform = false;
    
    DBG("=== TERRY BUTTON UPDATE ===");
    DBG("transformRecording: " + juce::String(transformRecording ? "true" : "false"));
    DBG("savedSamples: " + juce::String(savedSamples));
    DBG("hasOutputAudio: " + juce::String(hasOutputAudio ? "true" : "false"));
    DBG("isConnected: " + juce::String(isConnected ? "true" : "false"));
    DBG("isGenerating: " + juce::String(isGenerating ? "true" : "false"));

    if (transformRecording)
    {
        canTransform = (savedSamples > 0 && isConnected);
        DBG("Using recording source - canTransform: " + juce::String(canTransform ? "true" : "false"));
    }
    else
    {
        canTransform = (hasOutputAudio && isConnected);
        DBG("Using output source - canTransform: " + juce::String(canTransform ? "true" : "false"));
    }

    transformWithTerryButton.setEnabled(canTransform && !isGenerating);
    if (!isGenerating) {
        transformWithTerryButton.setButtonText("transform with terry");
    }

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
    resetStallDetection();
    targetProgress = 0;
    smoothProgressAnimation = false;

    // Clear any previous session ID since we're starting fresh
    // Clear any previous session ID since we're starting fresh transform
    audioProcessor.clearCurrentSessionId();
    audioProcessor.setRetryAvailable(false);  // ADD THIS - clear retry for new transform
    updateRetryButtonState();

    updateAllGenerationButtonStates();
    repaint(); // Force immediate UI update

    // Basic validation checks (like Gary and Jerry)
    if (!isConnected)
    {
        isGenerating = false;
        showStatusMessage("backend not connected - check connection first");
        return;
    }

    // Check that we have either a variation selected OR a custom prompt
    bool hasVariation = (currentTerryVariation >= 0);
    bool hasCustomPrompt = !currentTerryCustomPrompt.trim().isEmpty();

    if (!hasVariation && !hasCustomPrompt)
    {
        isGenerating = false;
        showStatusMessage("please select a variation OR enter a custom prompt");
        return;
    }

    // Check that we have the selected audio source
    if (transformRecording)
    {
        if (savedSamples <= 0)
        {
            isGenerating = false;
            showStatusMessage("no recording available - save your recording first");
            return;
        }
    }
    else // transforming output
    {
        if (!hasOutputAudio)
        {
            isGenerating = false;
            showStatusMessage("no output audio available - generate with gary or jerry first");
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
        showStatusMessage("audio file not found - " + audioFile.getFileName());
        return;
    }

    // Read and encode audio file (same as Gary)
    juce::MemoryBlock audioData;
    if (!audioFile.loadFileAsData(audioData))
    {
        isGenerating = false;
        showStatusMessage("failed to read audio file");
        return;
    }

    if (audioData.getSize() == 0)
    {
        isGenerating = false;
        showStatusMessage("audio file is empty");
        return;
    }

    auto base64Audio = juce::Base64::toBase64(audioData.getData(), audioData.getSize());

    // Debug: Log audio data size
    DBG("Terry audio file size: " + juce::String(audioData.getSize()) + " bytes");
    DBG("Terry base64 length: " + juce::String(base64Audio.length()) + " chars");
    DBG("Terry flowstep: " + juce::String(currentTerryFlowstep, 3));
    DBG("Terry solver: " + juce::String(useMidpointSolver ? "midpoint" : "euler"));

    // Button text feedback and status during processing
    transformWithTerryButton.setButtonText("transforming...");
    showStatusMessage("sending audio to terry for transformation...");

    // Create HTTP request in background thread (same pattern as Gary and Jerry)
    juce::Thread::launch([this, base64Audio, variationNames, hasVariation, hasCustomPrompt]() {
        // SAFETY: Exit if generation stopped
        if (!isGenerating) {
            DBG("Terry request aborted - generation stopped");
            return;
        }

        auto startTime = juce::Time::getCurrentTime();

        // Create JSON payload
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("audio_data", base64Audio);
        jsonRequest->setProperty("flowstep", currentTerryFlowstep);
        jsonRequest->setProperty("solver", useMidpointSolver ? "midpoint" : "euler");

        // FIXED: Always send a variation (required by backend), custom_prompt overrides it
        if (hasCustomPrompt)
        {
            // Send default variation + custom_prompt (custom_prompt will override)
            jsonRequest->setProperty("variation", "accordion_folk");  // Default/neutral variation
            jsonRequest->setProperty("custom_prompt", currentTerryCustomPrompt);
            DBG("Terry using custom prompt: " + currentTerryCustomPrompt + " (with default variation)");
        }
        else if (hasVariation && currentTerryVariation < variationNames.size())
        {
            // Send selected variation only
            jsonRequest->setProperty("variation", variationNames[currentTerryVariation]);
            DBG("Terry using variation: " + variationNames[currentTerryVariation]);
        }
        else
        {
            // Fallback - should not happen due to validation, but safety net
            jsonRequest->setProperty("variation", "accordion_folk");
            DBG("Terry fallback to default variation");
        }

        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        DBG("Terry JSON payload size: " + juce::String(jsonString.length()) + " characters");

        juce::URL url(getServiceUrl(ServiceType::Terry, "/api/juce/transform_audio"));

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
            // SAFETY: Don't process if generation stopped
            if (!isGenerating) {
                DBG("Terry callback aborted");
                return;
            }

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
                        showStatusMessage("sent to terry. processing...", 2000);
                        DBG("Terry session ID: " + sessionId);

                        // START POLLING FOR RESULTS (same as Gary)
                        startPollingForResults(sessionId);  // This sets currentSessionId = sessionId

                        // Disable undo button until transform completes
                        undoTransformButton.setEnabled(false);
                    }
                    else
                    {
                        juce::String error = responseObj->getProperty("error").toString();
                        showStatusMessage("terry error: " + error, 5000);
                        DBG("Terry server error: " + error);
                    }
                }
                else
                {
                    showStatusMessage("invalid JSON response from terry", 4000);
                    DBG("Failed to parse Terry JSON response: " + responseText.substring(0, 100));
                }
            }
            else
            {
                juce::String errorMsg;
                bool shouldCheckHealth = false;
                
                if (statusCode == 0 && audioProcessor.getIsUsingLocalhost())
                    errorMsg = "cannot connect to terry on localhost - ensure docker compose is running";
                else if (statusCode == 0)
                {
                    errorMsg = "failed to connect to Terry on remote backend";
                    shouldCheckHealth = true;  // Connection failure on remote - check if VM is down
                }
                else if (statusCode >= 400)
                {
                    errorMsg = "terry server error (HTTP " + juce::String(statusCode) + ")";
                    shouldCheckHealth = true;  // Server error - might be backend issue
                }
                else
                    errorMsg = "empty response from terry";

                showStatusMessage(errorMsg, 4000);
                DBG("Terry request failed: " + errorMsg);
                
                // Check backend health if connection/server failure
                if (shouldCheckHealth)
                {
                    DBG("Terry failed - checking backend health");
                    audioProcessor.checkBackendHealth();
                    
                    // Give health check time, then handle result
                    juce::Timer::callAfterDelay(6000, [this]() {
                        if (!audioProcessor.isBackendConnected())
                        {
                            handleBackendDisconnection();
                            lastBackendDisconnectionPopupTime = juce::Time::getCurrentTime();
                        }
                    });
                }
                
                transformWithTerryButton.setButtonText("transform with terry");
            }

            // Re-enable transform button (same pattern as Gary/Jerry)
            updateTerrySourceButtons(); // This will set the right enabled state

            });
        });
}

void Gary4juceAudioProcessorEditor::undoTerryTransform()
{
    // We need a session ID to undo - this should be the current session
    juce::String sessionId = audioProcessor.getCurrentSessionId();
    if (sessionId.isEmpty())
    {
        showStatusMessage("no transform session to undo", 3000);
        return;
    }

    DBG("Attempting to undo Terry transform for session: " + sessionId);
    showStatusMessage("undoing transform...", 2000);

    // Disable undo button during request
    audioProcessor.setUndoTransformAvailable(false);
    undoTransformButton.setEnabled(false);
    undoTransformButton.setButtonText("undoing...");

    // Create HTTP request in background thread
    juce::Thread::launch([this, sessionId]() {
        // REMOVED: Don't check isGenerating for undo operations!
        // Undo operations are not generation operations and should proceed regardless
        // if (!isGenerating) {
        //     DBG("Undo Terry request aborted - generation stopped");
        //     return;
        // }

        auto startTime = juce::Time::getCurrentTime();

        // Create JSON payload - just needs session_id
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("session_id", sessionId);

        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        DBG("Terry undo JSON payload: " + jsonString);

        juce::URL url(getServiceUrl(ServiceType::Terry, "/api/juce/undo_transform"));

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
            // REMOVED: Don't check isGenerating for undo callback either!
            // if (!isGenerating) {
            //     DBG("Undo Terry callback aborted");
            //     return;
            // }

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
                            showStatusMessage("transform undone - audio restored.", 3000);
                            DBG("Terry undo successful - audio restored");

                            // Clear session ID since undo is complete
                            audioProcessor.clearCurrentSessionId();
                            undoTransformButton.setEnabled(false);  // Disable undo button
                            audioProcessor.setRetryAvailable(false);          // ADD THIS
                            undoTransformButton.setButtonText("undo transform");  // Reset button text
                        }
                        else
                        {
                            showStatusMessage("undo completed but no audio data received", 3000);
                            DBG("Terry undo success but missing audio data");
                            // Re-enable undo button since this is an error case
                            audioProcessor.setUndoTransformAvailable(true);
                            undoTransformButton.setEnabled(true);
                            undoTransformButton.setButtonText("undo transform");
                        }
                    }
                    else
                    {
                        auto error = responseObj->getProperty("error").toString();
                        showStatusMessage("undo failed: " + error, 4000);
                        DBG("Terry undo server error: " + error);
                        // Re-enable undo button since this is an error case
                        audioProcessor.setUndoTransformAvailable(true);
                        undoTransformButton.setEnabled(true);
                        undoTransformButton.setButtonText("undo transform");
                    }
                }
                else
                {
                    showStatusMessage("invalid undo response format", 3000);
                    DBG("Failed to parse Terry undo JSON response");
                    // Re-enable undo button since this is an error case
                    audioProcessor.setUndoTransformAvailable(true);
                    undoTransformButton.setEnabled(true);
                    undoTransformButton.setButtonText("undo transform");
                }
            }
            else
            {
                juce::String errorMsg;
                if (statusCode == 0 && audioProcessor.getIsUsingLocalhost())
                    errorMsg = "cannot connect for undo on localhost - ensure docker compose is running";
                else if (statusCode == 0)
                    errorMsg = "failed to connect for undo on remote backend";
                else if (statusCode >= 400)
                    errorMsg = "undo server error (HTTP " + juce::String(statusCode) + ")";
                else
                    errorMsg = "empty undo response";

                showStatusMessage(errorMsg, 4000);
                DBG("Terry undo request failed: " + errorMsg);
                // Re-enable undo button since this is an error case
                audioProcessor.setUndoTransformAvailable(true);
                undoTransformButton.setEnabled(true);
                undoTransformButton.setButtonText("undo transform");
            }
            });
        });
}

void Gary4juceAudioProcessorEditor::clearRecordingBuffer()
{
    audioProcessor.clearRecordingBuffer();
    savedSamples = audioProcessor.getSavedSamples();  // Will be 0 after clear
    updateRecordingStatus();
}

// ========== UPDATED CONNECTION STATUS METHOD ==========
void Gary4juceAudioProcessorEditor::updateConnectionStatus(bool connected)
{
    if (isConnected != connected)
    {
        isConnected = connected;
        DBG("Backend connection status updated: " + juce::String(connected ? "Connected" : "Disconnected"));

        // Update ALL button states when connection changes
        updateAllGenerationButtonStates();

        repaint(); // Trigger a redraw to update tab section border

        // Re-enable check button if it was disabled
        if (!checkConnectionButton.isEnabled())
        {
            checkConnectionButton.setButtonText("check backend connection");
            checkConnectionButton.setEnabled(true);
        }
    }
}

// ========== BACKEND TOGGLE METHODS ==========
juce::String Gary4juceAudioProcessorEditor::getServiceUrl(ServiceType service, const juce::String& endpoint) const
{
    // Map editor ServiceType to processor ServiceType
    Gary4juceAudioProcessor::ServiceType processorService;
    switch (service)
    {
        case ServiceType::Gary:  processorService = Gary4juceAudioProcessor::ServiceType::Gary; break;
        case ServiceType::Jerry: processorService = Gary4juceAudioProcessor::ServiceType::Jerry; break;
        case ServiceType::Terry: processorService = Gary4juceAudioProcessor::ServiceType::Terry; break;
    }
    
    return audioProcessor.getServiceUrl(processorService, endpoint);
}

void Gary4juceAudioProcessorEditor::toggleBackend()
{
    isUsingLocalhost = !isUsingLocalhost;

    // Tell processor to switch backends
    audioProcessor.setUsingLocalhost(isUsingLocalhost);

    // ALSO update processor's connection state
    audioProcessor.setBackendConnectionStatus(false);

    // Update button text and styling
    updateBackendToggleButton();

    // NEW: Update model availability based on new backend
    updateModelAvailability();

    // Update connection status display (this can stay)
    updateConnectionStatus(false);

    DBG("Switched to " + audioProcessor.getCurrentBackendType() + " backend");
}

void Gary4juceAudioProcessorEditor::updateBackendToggleButton()
{
    if (isUsingLocalhost)
    {
        backendToggleButton.setButtonText("local");
        backendToggleButton.setButtonStyle(CustomButton::ButtonStyle::Gary); // Red style for local
        backendToggleButton.setTooltip("using localhost backend (ports 8000/8005) - click to switch to remote");
    }
    else
    {
        backendToggleButton.setButtonText("remote");
        backendToggleButton.setButtonStyle(CustomButton::ButtonStyle::Standard); // Standard style for remote
        backendToggleButton.setTooltip("using remote backend (g4l.thecollabagepatch.com) - click to switch to localhost");
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
        currentAudioSampleRate = 44100.0;  // Reset to default
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
        // ADDED: Store the file's sample rate
        currentAudioSampleRate = reader->sampleRate;
        hasOutputAudio = true;
        playOutputButton.setEnabled(true);
        stopOutputButton.setEnabled(true);  // Always enabled when we have audio
        clearOutputButton.setEnabled(true);
        cropButton.setEnabled(true);
        continueButton.setEnabled(true);
        DBG("Loaded output audio: " + juce::String(reader->lengthInSamples) + " samples, " +
            juce::String(reader->numChannels) + " channels, " +
            juce::String(totalAudioDuration, 2) + " seconds at " +
            juce::String(reader->sampleRate) + " Hz");
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
        currentAudioSampleRate = 44100.0;  // Reset to default
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

        // ENHANCED: Different visual treatment for queued vs processing
        if (isCurrentlyQueued)
        {
            // QUEUED STATE - Show pulsing queue indicator instead of progress bar
            auto time = juce::Time::getCurrentTime().toMilliseconds();
            float pulse = (std::sin(time * 0.003f) + 1.0f) * 0.5f; // Slower pulse for queue

            // Subtle pulsing background
            g.setColour(juce::Colours::orange.withAlpha(0.2f + pulse * 0.2f));
            g.fillRect(area.getX() + 1, area.getY() + 1, area.getWidth() - 2, area.getHeight() - 2);

            // Pulsing border
            g.setColour(juce::Colours::orange.withAlpha(0.5f + pulse * 0.3f));
            g.drawRect(area.getX() + 1, area.getY() + 1, area.getWidth() - 2, area.getHeight() - 2, 1);
        }
        else
        {
            // PROCESSING STATE - Show traditional progress bar
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
        }

        // ENHANCED: Smart progress text based on queue state
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.setColour(juce::Colours::white);

        juce::String displayText;
        if (isCurrentlyQueued)
        {
            // Show queue status instead of confusing "cooking: 0%"
            if (currentTab == ModelTab::Terry)
            {
                displayText = "queued for transform";
            }
            else
            {
                displayText = "queued for generation";
            }
        }
        else
        {
            // Show traditional progress when actually processing
            if (currentTab == ModelTab::Terry)
            {
                displayText = "transforming: " + juce::String(generationProgress) + "%";
            }
            else
            {
                displayText = "cooking: " + juce::String(generationProgress) + "%";
            }
        }

        g.drawText(displayText, area, juce::Justification::centred);
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
            g.drawText("dragging to DAW...", area, juce::Justification::centred);
        }
    }
    else
    {
        // NO OUTPUT state
        g.setFont(juce::FontOptions(14.0f));
        g.setColour(juce::Colours::darkgrey);
        g.drawText("output audio will appear here", area, juce::Justification::centred);
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
        showStatusMessage("no output audio to play");
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
        auto initStartTime = juce::Time::getCurrentTime();
        initializeAudioPlayback();
        auto initDuration = juce::Time::getCurrentTime() - initStartTime;

        // If initialization took too long or failed, likely due to exclusive ASIO
        if (initDuration.inMilliseconds() > 1000 || !audioDeviceManager->getCurrentAudioDevice())
        {
            DBG("Audio initialization slow/failed - likely exclusive ASIO blocking");
            showAsioDriverWarning("ASIO driver (possibly ASIO4ALL)");
            return;
        }
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

            // Check if it actually started
            juce::Timer::callAfterDelay(100, [this]() {
                if (transportSource && !transportSource->isPlaying()) {
                    DBG("Playback failed to start - likely ASIO blocking");
                    showAsioDriverWarning("ASIO driver");
                }
                });

            isPlayingOutput = true;
            isPausedOutput = false;

            // Update button icon
            updatePlayButtonIcon();

            showStatusMessage("playing output...", 2000);
            DBG("Started output playback from " + juce::String(startPosition, 2) + "s");
        }
        else
        {
            showStatusMessage("failed to read output audio");
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

        updatePlayButtonIcon();

        showStatusMessage("paused", 1500);
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
                    showStatusMessage("failed to read output audio");
                    return;
                }
            }
        }

        // Resume from saved position
        transportSource->setPosition(pausedPosition);
        transportSource->start();

        isPlayingOutput = true;
        isPausedOutput = false;

        updatePlayButtonIcon();

        showStatusMessage("resumed from " + juce::String(pausedPosition, 1) + "s", 1500);
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

    updatePlayButtonIcon();

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

void Gary4juceAudioProcessorEditor::showAsioDriverWarning(const juce::String& driverName)
{
    // Show immediate status message
    showStatusMessage("audio driver may block playback - see output section", 6000);

    // Create a delayed popup with clickable link after a brief moment
    juce::Timer::callAfterDelay(1500, [this, driverName]() {
        // SAFETY: Check component is still valid
        if (!isEditorValid.load()) return;

        auto* alertWindow = new juce::AlertWindow(
            "audio driver compatibility",
            driverName + " may block audio playback in VSTs.\n"
            "FlexASIO (free, open-source) works much better.",
            juce::MessageBoxIconType::InfoIcon,
            this);

        // Add download button that opens the GitHub releases page
        alertWindow->addButton("download FlexASIO", 1);
        alertWindow->addButton("continue anyway", 0);

        // Add note below the buttons
        alertWindow->addTextBlock("Note: After installing/switching to a new audio driver,\n"
            "you'll need to reload this VST in your DAW.");

        alertWindow->enterModalState(true,
            juce::ModalCallbackFunction::create([this, alertWindow](int result) {
                if (result == 1) // Download button clicked
                {
                    juce::URL("https://github.com/dechamps/FlexASIO/releases").launchInDefaultBrowser();
                    showStatusMessage("opened FlexASIO download page", 4000);
                }
                // Both buttons will close the modal - clean up the AlertWindow
                delete alertWindow;
                }));
        });
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

    // CHECK FOR PROBLEMATIC ASIO DRIVERS
    auto* currentDevice = audioDeviceManager->getCurrentAudioDevice();
    if (currentDevice != nullptr)
    {
        auto driverName = currentDevice->getName().toLowerCase();
        auto typeName = currentDevice->getTypeName().toLowerCase();

        bool isProblematicDriver = driverName.contains("asio4all") ||
            driverName.contains("asio 4 all") ||
            driverName.contains("realtek asio") ||
            typeName.contains("asio4all") ||
            typeName.contains("realtek asio");

        if (isProblematicDriver)
        {
            DBG("Detected problematic ASIO driver: " + driverName + " (" + typeName + ")");
            showAsioDriverWarning(driverName);
        }
        else
        {
            DBG("Audio driver OK: " + driverName + " (" + typeName + ")");
        }
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
            showStatusMessage("playback finished", 1500);
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

    showStatusMessage("output cleared", 2000);
    repaint();
}

void Gary4juceAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    // SAFETY: Validate component state early
    if (!isEditorValid.load())
    {
        DBG("MouseDown ignored - editor not valid");
        return;
    }

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
    // SAFETY: Early validation
    if (!isEditorValid.load())
    {
        DBG("MouseDrag ignored - editor not valid");
        return;
    }

    // Check if we're dragging from the output waveform area
    if (isMouseOverOutputWaveform(dragStartPosition) && hasOutputAudio && outputAudioFile.existsAsFile())
    {
        // SAFETY: Additional file validation before starting drag
        if (outputAudioFile.getSize() < 1000)
        {
            DBG("Output file too small for drag");
            return;
        }

        // Calculate drag distance to determine if this is a drag operation
        auto dragDistance = dragStartPosition.getDistanceFrom(event.getPosition());

        if (dragDistance > 10 && !dragStarted) // 10 pixel threshold for drag
        {
            // SAFETY: Check if we're already dragging
            if (isDragInProgress.load())
            {
                DBG("Already dragging, ignoring new drag attempt");
                return;
            }

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

            showStatusMessage("seek to " + juce::String(timeInSeconds, 1) + "s", 1500);
            repaint(); // Update cursor position immediately
        }
    }
    else if (isPausedOutput)
    {
        // Currently paused - move cursor and stay paused
        pausedPosition = timeInSeconds;
        currentPlaybackPosition = timeInSeconds;

        showStatusMessage("seek to " + juce::String(timeInSeconds, 1) + "s (paused)", 1500);
        repaint(); // Update cursor position immediately
    }
    else if (hasOutputAudio)
    {
        // Not playing but we have audio - move cursor and prepare for playback
        currentPlaybackPosition = timeInSeconds;
        pausedPosition = timeInSeconds;
        isPausedOutput = true; // Set paused state so next play resumes from here

        showStatusMessage("seek to " + juce::String(timeInSeconds, 1) + "s", 1500);
        repaint(); // Update cursor position immediately
    }
}

void Gary4juceAudioProcessorEditor::startAudioDrag()
{
    // Prevent multiple simultaneous drags
    if (isDragInProgress.load())
    {
        DBG("Drag already in progress, ignoring request");
        return;
    }

    // SAFETY: Validate component state before starting
    if (!isEditorValid.load())
    {
        DBG("Editor not valid, aborting drag");
        return;
    }

    if (!outputAudioFile.existsAsFile())
    {
        DBG("No output audio file to drag");
        return;
    }

    // SAFETY: Additional file validation
    if (outputAudioFile.getSize() < 1000) // Should be at least 1KB for real audio
    {
        DBG("Output file too small, aborting drag");
        return;
    }

    // Set drag in progress flag
    isDragInProgress.store(true);

    juce::File uniqueDragFile;

    try
    {
        // Protect file operations with critical section
        juce::ScopedLock lock(fileLock);

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
                showStatusMessage("drag failed - folder creation error", 2000);
                isDragInProgress.store(false);
                return;
            }
        }

        // Create a unique filename with timestamp for the drag
        auto timestamp = juce::String(juce::Time::getCurrentTime().toMilliseconds());
        auto uniqueFileName = "gary4juce_" + timestamp + ".wav";
        uniqueDragFile = draggedAudioDir.getChildFile(uniqueFileName);

        // SAFETY: More robust file copy with validation
        if (!outputAudioFile.existsAsFile())
        {
            DBG("Source file no longer exists during drag preparation");
            isDragInProgress.store(false);
            return;
        }

        // Copy the current output file to the unique drag file
        if (!outputAudioFile.copyFileTo(uniqueDragFile))
        {
            DBG("Failed to create unique copy for dragging");
            showStatusMessage("drag failed - file copy error", 2000);
            isDragInProgress.store(false);
            return;
        }

        // SAFETY: Validate the copy was successful
        if (!uniqueDragFile.existsAsFile() || uniqueDragFile.getSize() < 1000)
        {
            DBG("Copy validation failed");
            uniqueDragFile.deleteFile();
            isDragInProgress.store(false);
            return;
        }

    } // Release lock after file operations are complete
    catch (const std::exception& e)
    {
        DBG("Exception during drag file preparation: " + juce::String(e.what()));
        showStatusMessage("drag failed - exception occurred", 2000);
        isDragInProgress.store(false);
        return;
    }

    // Small delay to ensure file is fully written
    juce::Thread::sleep(50); // Increased from 10ms

    // Create array of files to drag
    juce::StringArray filesToDrag;
    filesToDrag.add(uniqueDragFile.getFullPathName());
    DBG("Starting drag for persistent file: " + uniqueDragFile.getFullPathName());

    // SAFETY: Enhanced callback with multiple safety checks
    auto success = performExternalDragDropOfFiles(filesToDrag, true, this, [this, uniqueDragFile]()
        {
            // SAFETY CHECK 1: Component validity
            if (!isEditorValid.load())
            {
                DBG("Drag callback ignored - editor no longer valid");
                isDragInProgress.store(false);
                // Still try to clean up the file
                if (uniqueDragFile.existsAsFile())
                {
                    uniqueDragFile.deleteFile();
                }
                return;
            }

            // SAFETY CHECK 2: Use MessageManager to ensure main thread execution
            juce::MessageManager::callAsync([this, uniqueDragFile]()
                {
                    // SAFETY CHECK 3: Double-check component validity on main thread
                    if (!isEditorValid.load())
                    {
                        DBG("Drag callback ignored on main thread - editor no longer valid");
                        isDragInProgress.store(false);
                        if (uniqueDragFile.existsAsFile())
                        {
                            uniqueDragFile.deleteFile();
                        }
                        return;
                    }

                    DBG("Drag operation completed successfully");
                    showStatusMessage("audio dragged successfully!", 2000);
                    isDragInProgress.store(false);

                    // Clean up with delay and additional safety
                    juce::Timer::callAfterDelay(3000, [uniqueDragFile]()
                        {
                            try
                            {
                                if (uniqueDragFile.existsAsFile())
                                {
                                    uniqueDragFile.deleteFile();
                                    DBG("Cleaned up temporary drag file");
                                }
                            }
                            catch (...)
                            {
                                DBG("Exception during drag file cleanup - ignoring");
                            }
                        });
                });
        });

    if (!success)
    {
        DBG("Failed to start drag operation");
        showStatusMessage("drag failed - try again", 2000);

        // Clean up with lock protection
        try
        {
            juce::ScopedLock lock(fileLock);
            uniqueDragFile.deleteFile();
        }
        catch (...)
        {
            DBG("Exception during drag cleanup - ignoring");
        }
        isDragInProgress.store(false);
    }
}

std::pair<bool, juce::File> Gary4juceAudioProcessorEditor::prepareFileForDrag()
{
    try
    {
        // Create dragged_audio folder
        auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        auto garyDir = documentsDir.getChildFile("gary4juce");
        auto draggedAudioDir = garyDir.getChildFile("dragged_audio");

        if (!draggedAudioDir.exists())
        {
            auto result = draggedAudioDir.createDirectory();
            if (!result.wasOk())
            {
                DBG("Failed to create dragged_audio directory: " + result.getErrorMessage());
                juce::MessageManager::callAsync([this]() {
                    showStatusMessage("drag failed - folder creation error", 2000);
                    });
                return { false, juce::File{} };
            }
        }

        // Create unique filename with timestamp
        auto timestamp = juce::String(juce::Time::getCurrentTime().toMilliseconds());
        auto uniqueFileName = "gary4juce_" + timestamp + ".wav";
        auto uniqueDragFile = draggedAudioDir.getChildFile(uniqueFileName);

        // THREAD SAFETY: Use a more robust file copy approach
        juce::FileInputStream sourceStream(outputAudioFile);
        if (!sourceStream.openedOk())
        {
            DBG("Failed to open source file for reading");
            juce::MessageManager::callAsync([this]() {
                showStatusMessage("drag failed - source file locked", 2000);
                });
            return { false, juce::File{} };
        }

        juce::FileOutputStream destStream(uniqueDragFile);
        if (!destStream.openedOk())
        {
            DBG("Failed to open destination file for writing");
            juce::MessageManager::callAsync([this]() {
                showStatusMessage("drag failed - destination error", 2000);
                });
            return { false, juce::File{} };
        }

        // Copy file data in chunks to avoid locking issues
        const int bufferSize = 8192;
        char buffer[bufferSize];

        while (!sourceStream.isExhausted())
        {
            auto bytesRead = sourceStream.read(buffer, bufferSize);
            if (bytesRead > 0)
            {
                destStream.write(buffer, bytesRead);
            }
        }

        destStream.flush();

        // Verify the copy worked
        if (!uniqueDragFile.existsAsFile() || uniqueDragFile.getSize() < 1000)
        {
            DBG("File copy verification failed");
            uniqueDragFile.deleteFile();
            juce::MessageManager::callAsync([this]() {
                showStatusMessage("drag failed - copy verification failed", 2000);
                });
            return { false, juce::File{} };
        }

        DBG("File prepared for drag: " + uniqueDragFile.getFullPathName());
        return { true, uniqueDragFile };
    }
    catch (const std::exception& e)
    {
        DBG("Exception in prepareFileForDrag: " + juce::String(e.what()));
        juce::MessageManager::callAsync([this]() {
            showStatusMessage("drag failed - exception occurred", 2000);
            });
        return { false, juce::File{} };
    }
}

bool Gary4juceAudioProcessorEditor::performDragOperation(const juce::File& dragFile)
{
    juce::StringArray filesToDrag;
    filesToDrag.add(dragFile.getFullPathName());

    DBG("Starting thread-safe drag for: " + dragFile.getFullPathName());

    // THREAD SAFETY: Use WeakReference to avoid accessing deleted component
    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis = this;

    auto success = performExternalDragDropOfFiles(filesToDrag, true, this,
        [safeThis, dragFile]() {
            // THREAD SAFETY: Always check if component still exists
            if (safeThis.getComponent() == nullptr)
            {
                DBG("Component deleted during drag - cleaning up file");
                dragFile.deleteFile();
                return;
            }

            // THREAD SAFETY: Call UI updates on message thread
            juce::MessageManager::callAsync([safeThis, dragFile]() {
                if (auto* editor = safeThis.getComponent())
                {
                    editor->showStatusMessage("audio dragged successfully!", 2000);
                    DBG("Drag operation completed successfully");
                }

                // Clean up the temporary file after a delay
                juce::Timer::callAfterDelay(5000, [dragFile]() {
                    if (dragFile.existsAsFile())
                    {
                        dragFile.deleteFile();
                        DBG("Cleaned up temporary drag file");
                    }
                    });
                });
        });

    if (!success)
    {
        DBG("Failed to start drag operation");
        showStatusMessage("drag failed - try again", 2000);
        dragFile.deleteFile();
        return false;
    }

    return true;
}

bool Gary4juceAudioProcessorEditor::isMouseOverOutputWaveform(const juce::Point<int>& position) const
{
    return outputWaveformArea.contains(position);
}





void Gary4juceAudioProcessorEditor::updatePlayButtonIcon()
{
    if (!playIcon || !pauseIcon)
        return;

    if (isPlayingOutput)
    {
        // Currently playing -> show pause icon
        playOutputButton.setIcon(pauseIcon->createCopy());
        playOutputButton.setTooltip("pause");
    }
    else
    {
        // Not playing (stopped or paused) -> show play icon
        playOutputButton.setIcon(playIcon->createCopy());
        if (isPausedOutput)
            playOutputButton.setTooltip("resume");
        else
            playOutputButton.setTooltip("play output...duh");
    }
}






void Gary4juceAudioProcessorEditor::cropAudioAtCurrentPosition()
{
    if (!hasOutputAudio || totalAudioDuration <= 0.0)
    {
        showStatusMessage("no audio to crop", 2000);
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
        showStatusMessage("cannot crop at beginning - play or seek to position first", 4000);
        DBG("Cannot crop: audio is stopped at beginning position");
        return;
    }

    // Validate crop position
    if (cropPosition <= 0.1)
    {
        showStatusMessage("cannot crop at very beginning - seek forward first", 3000);
        return;
    }

    if (cropPosition >= (totalAudioDuration - 0.1))
    {
        showStatusMessage("cannot crop at end - seek backward first", 3000);
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
        showStatusMessage("failed to read audio file for cropping", 3000);
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
        showStatusMessage("invalid crop position", 3000);
        DBG("ERROR: Invalid samples to keep: " + juce::String(samplesToKeep));
        return;
    }

    // Read the cropped portion into buffer
    juce::AudioBuffer<float> croppedBuffer((int)reader->numChannels, samplesToKeep);

    DBG("Created buffer: " + juce::String(croppedBuffer.getNumChannels()) + " channels, " +
        juce::String(croppedBuffer.getNumSamples()) + " samples");

    if (!reader->read(&croppedBuffer, 0, samplesToKeep, 0, true, true))
    {
        showStatusMessage("failed to read audio data", 3000);
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
        showStatusMessage("failed to create temp file", 3000);
        DBG("ERROR: Could not create output stream for temp file");
        return;
    }

    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        wavFormat->createWriterFor(fileStream.get(), reader->sampleRate,
            (unsigned int)reader->numChannels, 24, {}, 0));

    if (!writer)
    {
        showStatusMessage("failed to create audio writer", 3000);
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
        showStatusMessage("failed to write cropped audio", 3000);
        DBG("ERROR: writeFromAudioSampleBuffer failed");
        tempFile.deleteFile(); // Clean up temp file
        return;
    }

    DBG("Successfully wrote audio to temp file");
    DBG("Temp file size: " + juce::String(tempFile.getSize()) + " bytes");

    // Verify temp file was written correctly
    if (!tempFile.exists() || tempFile.getSize() < 1000) // Should be at least 1KB for any real audio
    {
        showStatusMessage("temp file write failed", 3000);
        DBG("ERROR: Temp file doesn't exist or is too small");
        tempFile.deleteFile();
        return;
    }

    // Replace original file with temp file
    if (!sourceFile.deleteFile())
    {
        showStatusMessage("failed to delete original file", 3000);
        DBG("ERROR: Could not delete original file");
        tempFile.deleteFile();
        return;
    }

    DBG("Deleted original file");

    if (!tempFile.moveFileTo(sourceFile))
    {
        showStatusMessage("failed to move temp file", 3000);
        DBG("ERROR: Could not move temp file to original location");
        return;
    }

    DBG("Moved temp file to original location");
    DBG("Final file size: " + juce::String(sourceFile.getSize()) + " bytes");

    // Reload the cropped audio (this will update currentAudioSampleRate)
    loadOutputAudioFile();

    // Reset playback position
    currentPlaybackPosition = 0.0;
    pausedPosition = 0.0;
    isPausedOutput = false;

    // FIXED: Use the output file's native sample rate for accurate duration display
    double newDuration = (double)outputAudioBuffer.getNumSamples() / currentAudioSampleRate;
    DBG("New audio duration after reload: " + juce::String(newDuration, 2) + "s");

    showStatusMessage("audio cropped at " + juce::String(cropPosition, 1) + "s", 3000);
    DBG("Crop operation completed successfully");

    // Store previous state for logging
    juce::String previousSessionId = audioProcessor.getCurrentSessionId();
    bool hadUndo = audioProcessor.getUndoTransformAvailable();
    bool hadRetry = audioProcessor.getRetryAvailable();

    DBG("Previous session ID: '" + previousSessionId + "'");
    DBG("Had undo available: " + juce::String(hadUndo ? "true" : "false"));
    DBG("Had retry available: " + juce::String(hadRetry ? "true" : "false"));

    // Clear session ID - it's no longer valid for the cropped audio
    audioProcessor.clearCurrentSessionId();

    // Clear operation availability - can't undo/retry operations on different audio
    audioProcessor.setUndoTransformAvailable(false);
    audioProcessor.setRetryAvailable(false);
    updateRetryButtonState();
    updateTerrySourceButtons();

    // Force a repaint to update the waveform display
    repaint();
}


// ========== UPDATED PAINT METHOD ==========
void Gary4juceAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colours::black);

    // Title - draw logo image if available, otherwise fallback to text
    if (logoImage.isValid())
    {
        // Calculate aspect ratio and fit logo in title area
        float logoAspect = (float)logoImage.getWidth() / (float)logoImage.getHeight();
        float areaAspect = (float)titleArea.getWidth() / (float)titleArea.getHeight();
        
        juce::Rectangle<int> logoRect = titleArea;
        
        // Scale to fit while maintaining aspect ratio
        if (logoAspect > areaAspect)
        {
            // Logo is wider - fit to width
            int newHeight = (int)(titleArea.getWidth() / logoAspect);
            logoRect = logoRect.withSizeKeepingCentre(titleArea.getWidth(), newHeight);
        }
        else
        {
            // Logo is taller - fit to height  
            int newWidth = (int)(titleArea.getHeight() * logoAspect);
            logoRect = logoRect.withSizeKeepingCentre(newWidth, titleArea.getHeight());
        }
        
        g.drawImage(logoImage, logoRect.toFloat());
    }
    else
    {
        // Fallback to text if logo didn't load
        g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
        g.setColour(juce::Colours::white);
        g.drawFittedText("gary4juce", titleArea, juce::Justification::centred, 1);
    }

    // Connection status display (left side of connection area)
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    
    // Calculate the text area (left side, leaving space for button stack on right)
    auto connectionTextArea = connectionStatusArea.withTrimmedRight(120); // Reduced space for buttons, more for text
    
    if (isConnected)
    {
        // White flashing for connected state
        g.setColour(connectionFlashState ? juce::Colours::white : juce::Colour(0xffcccccc));
        juce::String statusText = "connected (" + audioProcessor.getCurrentBackendType() + ")";
        g.drawFittedText(statusText, connectionTextArea, juce::Justification::centredLeft, 1);
    }
    else
    {
        // Static grey for disconnected state
        g.setColour(juce::Colour(0xff666666));
        juce::String statusText = "disconnected (" + audioProcessor.getCurrentBackendType() + ")";
        g.drawFittedText(statusText, connectionTextArea, juce::Justification::centredLeft, 1);
    }

    // Recording status section header (using reserved area)
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawFittedText("recording buffer", recordingLabelArea, juce::Justification::centred, 1);

    // Draw the INPUT waveform
    drawWaveform(g, waveformArea);

    // Status text below input waveform (using reserved area)
    g.setFont(juce::FontOptions(12.0f));

    if (hasStatusMessage && !statusMessage.isEmpty())
    {
        g.setColour(juce::Colours::white);
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
            g.setColour(juce::Colours::white);
        }
        else
        {
            statusText = "press PLAY in DAW to start recording";
            g.setColour(juce::Colours::grey);
        }
        g.drawText(statusText, inputStatusArea, juce::Justification::centred);
    }

    if (recordedSamples > 0)
    {
        // FIXED: Use actual sample rate instead of hardcoded 44100
        const double currentSampleRate = audioProcessor.getCurrentSampleRate();
        double recordedSeconds = (double)recordedSamples / currentSampleRate;
        double savedSeconds = (double)savedSamples / currentSampleRate;

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
        // FIXED: Use stored output file sample rate instead of hardcoded 44100
        double outputSeconds = (double)outputAudioBuffer.getNumSamples() / currentAudioSampleRate;
        juce::String outputInfo = juce::String::formatted("output: %.1fs - %d samples",
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

    // 2. Connection status area (fixed height) - now includes check button
    juce::Component connectionComponent;
    juce::FlexItem connectionItem(connectionComponent);
    connectionItem.height = 35;  // Taller to accommodate button
    connectionItem.margin = juce::FlexItem::Margin(5, 20, 5, 20);

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

    // 7. Buffer control buttons area (fixed height) - now just 1 row
    juce::Component bufferControlsComponent;
    juce::FlexItem bufferControlsItem(bufferControlsComponent);
    bufferControlsItem.height = 40;  // Height for 1 row of buttons
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

    // Manual layout for connection status area: stacked buttons on the right
    auto buttonStackArea = connectionStatusArea.removeFromRight(120); // Reserve space for buttons
    
    // Stack the buttons vertically in the button area
    auto toggleButtonBounds = buttonStackArea.removeFromTop(25).withSizeKeepingCentre(80, 25);
    buttonStackArea.removeFromTop(5); // Small gap
    auto checkButtonBounds = buttonStackArea.removeFromTop(28).withSizeKeepingCentre(120, 28);
    
    // Position the buttons
    backendToggleButton.setBounds(toggleButtonBounds);
    checkConnectionButton.setBounds(checkButtonBounds);

    // Layout buffer control buttons within their allocated space
    auto bufferControlsBounds = topSectionFlexBox.items[6].currentBounds.toNearestInt();

    // Layout buffer control buttons (only Save and Clear now)
    juce::FlexBox bufferButtonsFlexBox;
    bufferButtonsFlexBox.flexDirection = juce::FlexBox::Direction::row;
    bufferButtonsFlexBox.justifyContent = juce::FlexBox::JustifyContent::center;
    bufferButtonsFlexBox.alignItems = juce::FlexBox::AlignItems::center;

    // Save buffer button (left side)
    juce::FlexItem saveItem(saveBufferButton);
    saveItem.width = 150;  // Fixed width
    saveItem.height = 35;
    saveItem.margin = juce::FlexItem::Margin(0, 10, 0, 0);

    // Clear buffer button (right side) - more square proportions
    juce::FlexItem clearBufferItem(clearBufferButton);
    clearBufferItem.width = 50;   // Much narrower
    clearBufferItem.height = 35;  // Same height
    clearBufferItem.margin = juce::FlexItem::Margin(0, 0, 0, 10);

    bufferButtonsFlexBox.items.add(saveItem);
    bufferButtonsFlexBox.items.add(clearBufferItem);
    bufferButtonsFlexBox.performLayout(bufferControlsBounds);

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

    // 5. Button row container (continue + retry buttons)
    juce::Component garyButtonRowComponent;
    juce::FlexItem garyButtonRowItem(garyButtonRowComponent);
    garyButtonRowItem.height = 40;
    garyButtonRowItem.margin = juce::FlexItem::Margin(5, 50, 10, 50);

    // Add all items to Gary FlexBox
    garyFlexBox.items.add(garyTitleItem);
    garyFlexBox.items.add(promptRowItem);
    garyFlexBox.items.add(modelRowItem);
    garyFlexBox.items.add(sendToGaryItem);
    garyFlexBox.items.add(garyButtonRowItem);

    // Perform layout for Gary controls
    garyFlexBox.performLayout(garyBounds);

    // Layout continue and retry buttons side by side
    auto garyButtonRowBounds = garyFlexBox.items[4].currentBounds.toNearestInt();

    juce::FlexBox garyButtonFlexBox;
    garyButtonFlexBox.flexDirection = juce::FlexBox::Direction::row;
    garyButtonFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;

    juce::FlexItem continueItem(continueButton);
    continueItem.flexGrow = 1;
    continueItem.margin = juce::FlexItem::Margin(0, 3, 0, 0);  // Small gap on right

    juce::FlexItem retryItem(retryButton);
    retryItem.flexGrow = 1;
    retryItem.margin = juce::FlexItem::Margin(0, 0, 0, 3);     // Small gap on left

    garyButtonFlexBox.items.add(continueItem);
    garyButtonFlexBox.items.add(retryItem);
    garyButtonFlexBox.performLayout(garyButtonRowBounds);

    // Position Gary help icon next to title text (only if Gary tab is active)
    if (helpIcon && currentTab == ModelTab::Gary)
    {
        auto garyLabelBounds = garyFlexBox.items[0].currentBounds.toNearestInt(); // Title item
        auto textWidth = garyLabel.getFont().getStringWidth(garyLabel.getText());
        auto textStartX = garyLabelBounds.getX() + (garyLabelBounds.getWidth() - textWidth) / 2; // Center-aligned text start
        auto garyHelpArea = juce::Rectangle<int>(
            textStartX + textWidth + 5,  // 5px to the right of actual text
            garyLabelBounds.getY() + (garyLabelBounds.getHeight() - 20) / 2,  // Vertically centered
            20, 20  // Bigger 20x20 icon
        );
        garyHelpButton.setBounds(garyHelpArea);
    }

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

    // Position Jerry help icon next to title text (only if Jerry tab is active)
    if (helpIcon && currentTab == ModelTab::Jerry)
    {
        auto jerryLabelBounds = jerryFlexBox.items[0].currentBounds.toNearestInt(); // Title item  
        auto textWidth = jerryLabel.getFont().getStringWidth(jerryLabel.getText());
        auto textStartX = jerryLabelBounds.getX() + (jerryLabelBounds.getWidth() - textWidth) / 2; // Center-aligned text start
        auto jerryHelpArea = juce::Rectangle<int>(
            textStartX + textWidth + 5,  // 5px to the right of actual text
            jerryLabelBounds.getY() + (jerryLabelBounds.getHeight() - 20) / 2,
            20, 20
        );
        jerryHelpButton.setBounds(jerryHelpArea);
    }

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

void Gary4juceAudioProcessorEditor::updateRetryButtonState()
{
    juce::String sessionId = audioProcessor.getCurrentSessionId();  // Validates automatically
    bool hasValidSession = !sessionId.isEmpty();  // Empty if stale
    bool retryAvailable = audioProcessor.getRetryAvailable();  // Get the explicit flag

    // FIXED: Include retryAvailable in the canRetry calculation
    bool canRetry = hasValidSession && retryAvailable && !isGenerating && isConnected;

    retryButton.setEnabled(canRetry);
    retryButton.setButtonText("retry");

    DBG("=== RETRY BUTTON STATE UPDATE ===");
    DBG("Session ID (validated): '" + sessionId + "'");
    DBG("Has valid session: " + juce::String(hasValidSession ? "true" : "false"));
    DBG("Retry available flag: " + juce::String(retryAvailable ? "true" : "false"));
    DBG("Is generating: " + juce::String(isGenerating ? "true" : "false"));
    DBG("Is connected: " + juce::String(isConnected ? "true" : "false"));
    DBG("Can retry: " + juce::String(canRetry ? "true" : "false"));
}

void Gary4juceAudioProcessorEditor::updateContinueButtonState()
{
    // Continue button should be enabled if we have output audio and are connected
    bool canContinue = hasOutputAudio && isConnected && !isGenerating;
    continueButton.setEnabled(canContinue);
    continueButton.setButtonText("continue");
}

//==============================================================================
// Backend Disconnection and Stall Handling Methods
//==============================================================================

bool Gary4juceAudioProcessorEditor::checkForGenerationStall()
{
    if (!isGenerating) return false;

    auto currentTime = juce::Time::getCurrentTime().toMilliseconds();

    // If this is our first progress update, initialize the timer
    if (lastProgressUpdateTime == 0)
    {
        lastProgressUpdateTime = currentTime;
        lastKnownServerProgress = 0;
        return false;
    }

    // Check if we've been stuck without progress for too long
    auto timeSinceLastUpdate = currentTime - lastProgressUpdateTime;

    // Smart timeout: longer for startup (0% progress), shorter once generation begins
    int timeoutSeconds;
    juce::String timeoutReason;

    if (lastKnownServerProgress == 0)
    {
        // Still at 0% - model might still be loading
        timeoutSeconds = STARTUP_TIMEOUT_SECONDS;
        timeoutReason = "startup/model loading";
    }
    else
    {
        // Generation has started - should be making regular progress
        timeoutSeconds = GENERATION_TIMEOUT_SECONDS;
        timeoutReason = "generation progress";
    }

    bool isStalled = (timeSinceLastUpdate > (timeoutSeconds * 1000));

    if (isStalled && !hasDetectedStall)
    {
        DBG("Generation stall detected (" + timeoutReason + ") - no progress for " +
            juce::String(timeSinceLastUpdate / 1000) + " seconds (timeout: " +
            juce::String(timeoutSeconds) + "s)");
        hasDetectedStall = true;
        return true;
    }

    return false;
}

void Gary4juceAudioProcessorEditor::handleGenerationStall()
{
    DBG("Handling generation stall - checking backend health");
    
    // Stop polling immediately
    stopPolling();
    
    // Show immediate feedback
    showStatusMessage("checking backend connection...", 3000);
    
    // Check if backend is actually down
    audioProcessor.checkBackendHealth();
    
    // Give health check time to complete, then handle result
    juce::Timer::callAfterDelay(6000, [this]() {
        if (!audioProcessor.isBackendConnected())
        {
            // Backend is confirmed down
            handleBackendDisconnection();
        }
        else
        {
            // Backend is up but generation failed for other reasons
            handleGenerationFailure("generation timed out - try again or check backend logs");
        }
    });
}

void Gary4juceAudioProcessorEditor::handleBackendDisconnection()
{
    DBG("=== BACKEND DISCONNECTION CONFIRMED - CLEANING UP STATE ===");
    
    // Clean up generation state
    isGenerating = false;
    isPolling = false;
    generationProgress = 0;
    lastProgressUpdateTime = 0;
    lastKnownServerProgress = 0;
    hasDetectedStall = false;
    
    // Update connection status
    updateConnectionStatus(false);
    
    // Update all button states centrally
    updateAllGenerationButtonStates();
    
    // Show user-friendly error with communication options
    showBackendDisconnectionDialog();
    
    repaint();
}

void Gary4juceAudioProcessorEditor::handleGenerationFailure(const juce::String& reason)
{
    DBG("Generation failed: " + reason);
    
    // Clean up generation state (same as disconnection)
    isGenerating = false;
    isPolling = false;
    generationProgress = 0;
    lastProgressUpdateTime = 0;
    lastKnownServerProgress = 0;
    hasDetectedStall = false;
    
    // Update all button states centrally
    updateAllGenerationButtonStates();
    
    // Show error message
    showStatusMessage(reason, 5000);
    
    repaint();
}

void Gary4juceAudioProcessorEditor::resetStallDetection()
{
    auto currentTime = juce::Time::getCurrentTime().toMilliseconds();

    DBG("=== AGGRESSIVE STALL RESET ===");
    DBG("Old lastProgressUpdateTime: " + juce::String(lastProgressUpdateTime));

    lastProgressUpdateTime = currentTime; // Set to NOW instead of 0
    lastKnownServerProgress = 0;
    hasDetectedStall = false;
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;

    DBG("New lastProgressUpdateTime: " + juce::String(lastProgressUpdateTime));
    DBG("Stall detection AND animation state aggressively reset");
}

void Gary4juceAudioProcessorEditor::performSmartHealthCheck()
{
    auto currentTime = juce::Time::getCurrentTime().toMilliseconds();
    
    // Don't spam health checks
    if (currentTime - lastHealthCheckTime < MIN_HEALTH_CHECK_INTERVAL_MS)
    {
        DBG("Skipping health check - too soon since last check");
        return;
    }
    
    lastHealthCheckTime = currentTime;
    audioProcessor.checkBackendHealth();
    DBG("Performing smart health check");
}

void Gary4juceAudioProcessorEditor::showBackendDisconnectionDialog()
{
    DBG("=== SHOWING BACKEND DISCONNECTION DIALOG ===");

    // Create custom button panel component with safer memory management
    class CustomButtonPanel : public juce::Component
    {
    public:
        CustomButtonPanel(Gary4juceAudioProcessorEditor* editor)
            : parentEditor(editor)
        {
            // Create Discord button with icon
            discordButton = std::make_unique<CustomButton>();
            discordButton->setButtonStyle(CustomButton::ButtonStyle::Standard);
            if (editor->discordIcon)
                discordButton->setIcon(editor->discordIcon->createCopy());
            discordButton->setTooltip("Join Discord server");
            discordButton->onClick = [this]() {
                juce::URL("https://discord.gg/VECkyXEnAd").launchInDefaultBrowser();
                // Note: Don't close modal here - let user choose to close or try Twitter too
                };
            addAndMakeVisible(discordButton.get());

            // Create X button with icon
            xButton = std::make_unique<CustomButton>();
            xButton->setButtonStyle(CustomButton::ButtonStyle::Standard);
            if (editor->xIcon)
                xButton->setIcon(editor->xIcon->createCopy());
            xButton->setTooltip("Follow on X/Twitter");
            xButton->onClick = [this]() {
                juce::URL("https://twitter.com/@thepatch_kev").launchInDefaultBrowser();
                // Note: Don't close modal here - let user choose to close or try Discord too
                };
            addAndMakeVisible(xButton.get());

            // Visual reference area with bar chart icon
            visualRefLabel = std::make_unique<juce::Label>();
            visualRefLabel->setText("in a few minutes, click this button in the main window:", juce::dontSendNotification);
            visualRefLabel->setJustificationType(juce::Justification::centred);
            visualRefLabel->setColour(juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible(visualRefLabel.get());

            // Create visual reference button (non-clickable)
            if (editor->checkConnectionIcon)
            {
                visualRefButton = std::make_unique<CustomButton>();
                visualRefButton->setButtonStyle(CustomButton::ButtonStyle::Standard);
                visualRefButton->setIcon(editor->checkConnectionIcon->createCopy());
                visualRefButton->setEnabled(false); // Make it non-clickable
                visualRefButton->setTooltip("this is the check connection button in the main UI");
                addAndMakeVisible(visualRefButton.get());
            }

            setSize(300, 120);
        }

        void resized() override
        {
            auto area = getLocalBounds();

            // Visual reference section (top part)
            auto visualRefArea = area.removeFromTop(60);
            visualRefLabel->setBounds(visualRefArea.removeFromTop(30));
            if (visualRefButton)
                visualRefButton->setBounds(visualRefArea.reduced(10).withSizeKeepingCentre(30, 30));

            // Button section (bottom part)  
            auto buttonArea = area.reduced(10);
            int buttonWidth = buttonArea.getWidth() / 2 - 5;
            discordButton->setBounds(buttonArea.removeFromLeft(buttonWidth));
            buttonArea.removeFromLeft(10); // spacing
            xButton->setBounds(buttonArea);
        }

    private:
        Gary4juceAudioProcessorEditor* parentEditor;
        std::unique_ptr<CustomButton> discordButton;
        std::unique_ptr<CustomButton> xButton;
        std::unique_ptr<CustomButton> visualRefButton;
        std::unique_ptr<juce::Label> visualRefLabel;
    };

    // Create alert window with updated message
    auto* alertWindow = new juce::AlertWindow("backend down",
        "our backend runs on a spot vm\n"
        "and azure prolly deallocated it.\n"
        "hit up kev in discord/twitter or",
        juce::MessageBoxIconType::WarningIcon,
        this);

    // Create and add custom button panel - AlertWindow will own it
    auto* customButtons = new CustomButtonPanel(this);
    alertWindow->addCustomComponent(customButtons);

    // Add only close button as standard button - this will auto-close the modal
    alertWindow->addButton("close", 999);

    // Enter modal state - REMOVE the exitModalState call!
    alertWindow->enterModalState(true,
        juce::ModalCallbackFunction::create([alertWindow](int result) {
            // The modal is already closed when this callback runs
            // Just clean up and handle the result
            DBG("Modal closed with result: " + juce::String(result));
            delete alertWindow; // This is all you need!
            }));
}

void Gary4juceAudioProcessorEditor::updateModelAvailability()
{
    // hoenn_lofi (ID 4) is only available on localhost
    bool hoennLofiAvailable = isUsingLocalhost;

    modelComboBox.setItemEnabled(4, hoennLofiAvailable);

    // If hoenn_lofi is currently selected but no longer available, switch to default model
    if (!hoennLofiAvailable && modelComboBox.getSelectedId() == 4)
    {
        // Switch to vanya_ai_dnb_0.1 (ID 1) as default
        modelComboBox.setSelectedId(1);
        currentModelIndex = 0; // Update the index to match

        DBG("Switched from hoenn_lofi to vanya_ai_dnb_0.1 due to backend change");
        showStatusMessage("switched to vanya_ai_dnb_0.1 (hoenn_lofi not available on remote)", 4000);
    }

    DBG("Model availability updated - hoenn_lofi " + juce::String(hoennLofiAvailable ? "enabled" : "disabled"));
}


