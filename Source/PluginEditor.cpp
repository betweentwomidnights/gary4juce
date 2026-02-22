/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "./Utils/BarTrim.h"
#include "./Utils/MacDockIcon.h"
#include "./Components/Base/CustomComboBox.h"

namespace
{
    int loopTypeStringToIndex(const juce::String& type)
    {
        if (type.equalsIgnoreCase("drums"))
            return 1;
        if (type.equalsIgnoreCase("instruments"))
            return 2;
        return 0;
    }

    juce::String loopTypeIndexToString(int index)
    {
        switch (index)
        {
        case 1: return "drums";
        case 2: return "instruments";
        default: return "auto";
        }
    }

    const juce::StringArray& getTerryVariationNames()
    {
        static const juce::StringArray names = []()
        {
            juce::StringArray arr;
            arr.add("accordion_folk");
            arr.add("banjo_bluegrass");
            arr.add("piano_classical");
            arr.add("celtic");
            arr.add("strings_quartet");
            arr.add("synth_retro");
            arr.add("synth_modern");
            arr.add("synth_edm");
            arr.add("lofi_chill");
            arr.add("synth_bass");
            arr.add("rock_band");
            arr.add("cinematic_epic");
            arr.add("retro_rpg");
            arr.add("chiptune");
            arr.add("steel_drums");
            arr.add("gamelan_fusion");
            arr.add("music_box");
            arr.add("trap_808");
            arr.add("lo_fi_drums");
            arr.add("boom_bap");
            arr.add("percussion_ensemble");
            arr.add("future_bass");
            arr.add("synthwave_retro");
            arr.add("melodic_techno");
            arr.add("dubstep_wobble");
            arr.add("glitch_hop");
            arr.add("digital_disruption");
            arr.add("circuit_bent");
            arr.add("orchestral_glitch");
            arr.add("vapor_drums");
            arr.add("industrial_textures");
            arr.add("jungle_breaks");
            return arr;
        }();

        return names;
    }

    bool localhostHealthResponseLooksOnline(const juce::String& responseText)
    {
        if (responseText.trim().isEmpty())
            return false;

        auto parsed = juce::JSON::parse(responseText);
        if (auto* obj = parsed.getDynamicObject())
        {
            auto status = obj->getProperty("status").toString().trim().toLowerCase();
            if (status.isNotEmpty())
            {
                if (status == "unhealthy" || status == "failed" || status == "down" || status == "error")
                    return false;
                return true;
            }
        }

        // If health payload is non-empty but non-standard, treat as online.
        return true;
    }
}

//==============================================================================
Gary4juceAudioProcessorEditor::Gary4juceAudioProcessorEditor(Gary4juceAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
    cropButton("Crop", juce::DrawableButton::ImageFitted),
    garyHelpButton("gary help", juce::DrawableButton::ImageFitted),
    jerryHelpButton("jerry help", juce::DrawableButton::ImageFitted),
    terryHelpButton("terry help", juce::DrawableButton::ImageFitted),
    dariusHelpButton("darius help", juce::DrawableButton::ImageFitted)

{
    setSize(400, 850);  // Made taller to accommodate controls

#if JUCE_MAC
    applyStandaloneDockIconIfAvailable();
#endif

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
    jerryTabButton.onClick = [this]()
        {
            const bool wasJerry = (currentTab == ModelTab::Jerry);
            switchToTab(ModelTab::Jerry);

            // If we were already on Jerry, still allow a fetch (remote-only, TTL’d)
            if (wasJerry && !audioProcessor.getIsUsingLocalhost())
                maybeFetchRemoteJerryPrompts();
        };
    addAndMakeVisible(jerryTabButton);

    terryTabButton.setButtonText("terry");
    terryTabButton.setButtonStyle(CustomButton::ButtonStyle::Terry);
    terryTabButton.onClick = [this]() { switchToTab(ModelTab::Terry); };
    // terryTabButton.setEnabled(false);  // Disabled until implemented
    addAndMakeVisible(terryTabButton);

    dariusTabButton.setButtonText("darius");
    dariusTabButton.setButtonStyle(CustomButton::ButtonStyle::Darius); // We'll need to add this style
    dariusTabButton.onClick = [this]() { switchToTab(ModelTab::Darius); };
    addAndMakeVisible(dariusTabButton);

    garyUI = std::make_unique<GaryUI>();
    addAndMakeVisible(*garyUI);

    garyUI->onPromptDurationChanged = [this](float seconds)
    {
        currentPromptDuration = seconds;
        updateAllGenerationButtonStates();
    };

    garyUI->onModelChanged = [this](int index)
    {
        currentModelIndex = index;

        if (audioProcessor.getIsUsingLocalhost())
            applyGaryQuantizationDefaultForCurrentModel();
    };

    garyUI->onQuantizationModeChanged = [this](const juce::String& mode)
    {
        currentGaryQuantizationMode = mode;
    };

    garyUI->onSendToGary = [this]()
    {
        sendToGary();
    };

    garyUI->onContinue = [this]()
    {
        continueMusic();
    };

    garyUI->onRetry = [this]()
    {
        retryLastContinuation();
    };

    garyUI->setPromptDuration(currentPromptDuration);
    garyUI->setUsingLocalhost(audioProcessor.getIsUsingLocalhost());
    garyUI->setQuantizationMode(currentGaryQuantizationMode, juce::dontSendNotification);

    // Fetch available models from backend (will populate dropdown when response arrives)
    if (isConnected)
        fetchGaryAvailableModels();

    // ========== JERRY CONTROLS SETUP ==========
    jerryUI = std::make_unique<JerryUI>();
    addAndMakeVisible(*jerryUI);

    // Configure for standalone or plugin mode
    jerryUI->setIsStandalone(juce::JUCEApplicationBase::isStandaloneApp());

    // Handle manual BPM changes in standalone mode
    jerryUI->onManualBpmChanged = [this](int newBpm)
    {
        DBG("Manual BPM changed to: " + juce::String(newBpm));
        // The BPM will be retrieved via jerryUI->getManualBpm() when generating
    };

    jerryUI->onPromptChanged = [this](const juce::String& text)
    {
        currentJerryPrompt = text;
        updateAllGenerationButtonStates();
    };

    jerryUI->onCfgChanged = [this](float value)
    {
        currentJerryCfg = value;
    };

    jerryUI->onStepsChanged = [this](int value)
    {
        currentJerrySteps = value;
    };

    jerryUI->onSmartLoopToggled = [this](bool enabled)
    {
        generateAsLoop = enabled;
        updateAllGenerationButtonStates();
    };

    jerryUI->onLoopTypeChanged = [this](int index)
    {
        currentLoopType = loopTypeIndexToString(index);
    };

    jerryUI->onGenerate = [this]()
    {
        sendToJerry();
    };

    jerryUI->onModelChanged = [this](int index, bool isFinetune)
        {
            DBG("Jerry model changed to index " + juce::String(index) +
                " (isFinetune: " + juce::String(isFinetune ? "true" : "false") + ")");

            currentJerryModelIndex = index;
            currentJerryIsFinetune = isFinetune;

            if (jerryUI)
            {
                currentJerryModelKey = jerryUI->getSelectedModelKey();
                currentJerryModelType = jerryUI->getSelectedModelType();
                currentJerryFinetuneRepo = jerryUI->getSelectedFinetuneRepo();
                currentJerryFinetuneCheckpoint = jerryUI->getSelectedFinetuneCheckpoint();
                currentJerrySamplerType = jerryUI->getSelectedSamplerType();

                DBG("Selected model components:");
                DBG("  Type: " + currentJerryModelType);
                DBG("  Repo: " + currentJerryFinetuneRepo);
                DBG("  Checkpoint: " + currentJerryFinetuneCheckpoint);
                DBG("  Sampler: " + currentJerrySamplerType);
            }
            if (!audioProcessor.getIsUsingLocalhost())
            {
                // Remote mode: avoid hammering, but ensure we fetch when selection changes.
                if (isFinetune
                    && currentJerryFinetuneRepo.isNotEmpty()
                    && currentJerryFinetuneCheckpoint.isNotEmpty())
                {
                    DBG("[prompts] onModelChanged -> remote -> fetching by repo+ckpt: "
                        + currentJerryFinetuneRepo + " | " + currentJerryFinetuneCheckpoint);
                    fetchJerryPrompts(currentJerryFinetuneRepo, currentJerryFinetuneCheckpoint);
                }
                else
                {
                    // Either standard model or we don't have repo/ckpt yet—ask backend for the active finetune.
                    DBG("[prompts] onModelChanged -> remote -> using prefer=finetune (TTL guarded)");
                    maybeFetchRemoteJerryPrompts(); // will no-op if TTL/in-flight/cache says so
                }
            }
            else
            {
                // Localhost mode: resolve prompts for the selected finetune directly.
                if (isFinetune
                    && currentJerryFinetuneRepo.isNotEmpty()
                    && currentJerryFinetuneCheckpoint.isNotEmpty())
                {
                    DBG("[prompts] onModelChanged -> localhost -> fetching by repo+ckpt: "
                        + currentJerryFinetuneRepo + " | " + currentJerryFinetuneCheckpoint);
                    fetchJerryPrompts(currentJerryFinetuneRepo, currentJerryFinetuneCheckpoint);
                }
            }
        };

    jerryUI->onSamplerTypeChanged = [this](const juce::String& samplerType)
        {
            currentJerrySamplerType = samplerType;
            DBG("Jerry sampler type changed to: " + samplerType);
        };

    jerryUI->onFetchCheckpoints = [this](const juce::String& repo) {
        fetchJerryCheckpoints(repo);
    };

    jerryUI->onAddCustomModel = [this](const juce::String& repo, const juce::String& checkpoint) {
        addCustomJerryModel(repo, checkpoint);
    };

    jerryUI->setPromptText(currentJerryPrompt);
    jerryUI->setCfg(currentJerryCfg);
    jerryUI->setSteps(currentJerrySteps);
    jerryUI->setSmartLoop(generateAsLoop);
    jerryUI->setLoopType(loopTypeStringToIndex(currentLoopType));
    jerryUI->setBpm((int)audioProcessor.getCurrentBPM());
    jerryUI->setButtonsEnabled(false, isConnected, isGenerating);

    // Update localhost status
    jerryUI->setUsingLocalhost(audioProcessor.getIsUsingLocalhost());

    // ========== TERRY UI SETUP ==========
    terryUI = std::make_unique<TerryUI>();
    addAndMakeVisible(*terryUI);

    terryVariationNames = getTerryVariationNames();

    terryUI->setVariations(terryVariationNames, currentTerryVariation);
    terryUI->setCustomPrompt(currentTerryCustomPrompt);
    terryUI->setFlowstep(currentTerryFlowstep);
    terryUI->setUseMidpointSolver(useMidpointSolver);
    terryUI->setAudioSourceRecording(transformRecording);
    terryUI->setVisibleForTab(false);
    terryUI->setBpm(audioProcessor.getCurrentBPM());

    terryUI->onVariationChanged = [this](int index)
    {
        currentTerryVariation = index;
        if (currentTerryVariation >= 0)
            currentTerryCustomPrompt = "";
        updateTerryEnablementSnapshot();
    };

    terryUI->onCustomPromptChanged = [this](const juce::String& text)
    {
        currentTerryCustomPrompt = text;
        if (!currentTerryCustomPrompt.trim().isEmpty())
            currentTerryVariation = -1;
        updateTerryEnablementSnapshot();
    };

    terryUI->onFlowstepChanged = [this](float value)
    {
        currentTerryFlowstep = value;
    };

    terryUI->onSolverChanged = [this](bool useMidpointValue)
    {
        useMidpointSolver = useMidpointValue;
    };

    terryUI->onAudioSourceChanged = [this](bool useRecording)
    {
        setTerryAudioSource(useRecording);
    };

    terryUI->onTransform = [this]()
    {
        sendToTerry();
    };

    terryUI->onUndo = [this]()
    {
        undoTerryTransform();
    };

    updateTerryEnablementSnapshot();

    // ========== DARIUS UI ==========
    dariusUI = std::make_unique<DariusUI>();
    addAndMakeVisible(*dariusUI);

    dariusUI->onUrlChanged = [this](const juce::String& newUrl)
    {
        dariusBackendUrl = newUrl;
    };

    dariusUI->onHealthCheckRequested = [this]()
    {
        checkDariusHealth();
    };

    dariusUI->onRefreshConfigRequested = [this]()
    {
        clearDariusSteeringAssets();
        fetchDariusConfig();
    };

    dariusUI->onFetchCheckpointsRequested = [this]()
    {
        if (dariusIsFetchingCheckpoints)
            return;
        if (!dariusConnected)
            return;
        if (!dariusUI)
            return;
        if (dariusUI->getUsingBaseModel())
            return;

        syncDariusRepoFromField();
        dariusUI->requestOpenCheckpointMenuAfterFetch();
        dariusIsFetchingCheckpoints = true;
        dariusUI->setIsFetchingCheckpoints(true);
        fetchDariusCheckpoints(dariusFinetuneRepo, dariusFinetuneRevision);
    };

    dariusUI->onApplyWarmRequested = [this]()
    {
        beginDariusApplyAndWarm();
    };

    dariusUI->onGenerateRequested = [this]()
    {
        onClickGenerate();
    };

    dariusUI->onUseBaseModelToggled = [this](bool useBase)
    {
        dariusUseBaseModel = useBase;
        dariusUI->setUsingBaseModel(useBase);
        if (useBase)
        {
            dariusSelectedStepStr = "latest";
            clearDariusSteeringAssets();
        }
        updateDariusModelConfigUI();
    };

    dariusUI->onFinetuneRepoChanged = [this](const juce::String& repoText)
    {
        auto trimmed = repoText.trim();
        if (trimmed.isEmpty())
            trimmed = "thepatch/magenta-ft";
        dariusFinetuneRepo = trimmed;
        dariusUI->setFinetuneRepo(dariusFinetuneRepo);
    };

    dariusUI->onCheckpointSelected = [this](const juce::String& step)
    {
        dariusSelectedStepStr = step;
        updateDariusModelControlsEnabled();
    };

    dariusUI->onAudioSourceChanged = [this](bool useRecording)
    {
        DBG("Darius generation source set to " + juce::String(useRecording ? "Recording" : "Output"));
        audioProcessor.setTransformRecording(useRecording);
    };

    dariusUI->setBackendUrl(dariusBackendUrl);
    dariusUI->setConnectionStatusText("not checked");
    dariusUI->setUsingBaseModel(dariusUseBaseModel);
    dariusUI->setFinetuneRepo(dariusFinetuneRepo);
    dariusUI->setCheckpointSteps(dariusCheckpointSteps);
    dariusUI->setSelectedCheckpointStep(dariusSelectedStepStr);
    dariusUI->setConnected(dariusConnected);
    dariusUI->setSavedRecordingAvailable(savedSamples > 0);
    dariusUI->setOutputAudioAvailable(hasOutputAudio);
    dariusUI->setAudioSourceRecording(audioProcessor.getTransformRecording());
    dariusUI->setBpm(audioProcessor.getCurrentBPM());
    dariusUI->setSteeringAssets(dariusAssetsMeanAvailable, dariusAssetsCentroidCount, dariusCentroidWeights);

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
        if (audioProcessor.getIsUsingLocalhost())
            triggerLocalServiceHealthPoll(true);
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

    // Only show save buffer button in plugin mode (not needed in standalone with drag & drop)
    bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();
    if (!isStandalone)
    {
        addAndMakeVisible(saveBufferButton);
    }

    addAndMakeVisible(clearBufferButton);

    // Start timer to update recording status (refresh every 50ms for smooth waveform)
    startTimer(50);

    // Initial status update
    updateRecordingStatus();
    if (audioProcessor.getIsUsingLocalhost())
        triggerLocalServiceHealthPoll(true);

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

        // darius help button
        dariusHelpButton.setImages(helpIcon.get());
        dariusHelpButton.setTooltip("learn more about magenta-realtime");
        dariusHelpButton.onClick = [this]() {
            juce::URL("https://huggingface.co/spaces/thecollabagepatch/magenta-retry").launchInDefaultBrowser();
        };
        addAndMakeVisible(dariusHelpButton);
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


    
    // 3. CRITICAL: Restore Terry audio source selection
    const bool restoredTransformRecording = audioProcessor.getTransformRecording();
    setTerryAudioSource(restoredTransformRecording);

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

    // 5. Update all button states after restoration
    updateAllGenerationButtonStates();
    updateRetryButtonState();        
    updateContinueButtonState();     
    updateTerryEnablementSnapshot(); 

    // 6. Double-check button states after connection stabilizes
    juce::Timer::callAfterDelay(2000, [this]() {
        updateRetryButtonState();
        updateTerryEnablementSnapshot();
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

    setActiveOp(ActiveOp::None);
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

    // Stop playback through processor (host audio)
    audioProcessor.stopOutputPlayback();

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

    if (garyUI)
        garyUI->setVisibleForTab(showGary);

    if (jerryUI)
    {
        jerryUI->setVisibleForTab(showJerry);

        // NEW: Fetch available models when switching to Jerry tab
        if (showJerry)
        {
            if (audioProcessor.getIsUsingLocalhost())
                triggerLocalServiceHealthPoll(true);
            fetchJerryAvailableModels();

            // NEW: Only for remote backend, fetch prompts once (TTL’d) for the active finetune
            if (!audioProcessor.getIsUsingLocalhost())
                maybeFetchRemoteJerryPrompts();
        }
    }

    // Terry controls visibility
    bool showTerry = (tab == ModelTab::Terry);
    if (terryUI)
        terryUI->setVisibleForTab(showTerry);

    // Darius controls visibility
    const bool showDarius = (tab == ModelTab::Darius);
    if (dariusUI)
    {
        dariusUI->setVisible(showDarius);

        // Only fetch when showing Darius (avoid needless calls when leaving the tab)
        if (showDarius)
            fetchDariusAssetsStatus();  // async; callback should re-check currentTab == ModelTab::Darius
    }

    // Help button visibility
    if (helpIcon)
    {
        garyHelpButton.setVisible(showGary);
        jerryHelpButton.setVisible(showJerry);
        terryHelpButton.setVisible(showTerry);
        dariusHelpButton.setVisible(showDarius);
    }

    DBG("Switched to tab: " + juce::String(showGary ? "Gary" : (showJerry ? "Jerry" : (showTerry ? "Terry" : "Darius"))));

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

    dariusTabButton.setButtonStyle(currentTab == ModelTab::Darius ?
        CustomButton::ButtonStyle::Darius : CustomButton::ButtonStyle::Inactive);
}

void Gary4juceAudioProcessorEditor::updateAllGenerationButtonStates()
{
    updateGaryButtonStates(!isGenerating);
    updateRetryButtonState();

    if (jerryUI)
    {
        const bool jerryConnected = audioProcessor.getIsUsingLocalhost()
            ? isLocalServiceOnline(ServiceType::Jerry)
            : isConnected;
        const bool canGenerate = jerryConnected && !currentJerryPrompt.trim().isEmpty();
        const bool canSmartLoop = jerryConnected;
        jerryUI->setButtonsEnabled(canGenerate, canSmartLoop, isGenerating);
    }

    updateTerryEnablementSnapshot();
}

void Gary4juceAudioProcessorEditor::updateGaryButtonStates(bool resetTexts)
{
    if (!garyUI)
        return;

    const bool hasAudio = savedSamples > 0;
    const bool continueAvailable = hasOutputAudio;
    const juce::String sessionId = audioProcessor.getCurrentSessionId();
    const bool hasValidSession = !sessionId.isEmpty();
    const bool retryAvailableFlag = audioProcessor.getRetryAvailable();
    const bool retryAvailable = hasValidSession && retryAvailableFlag;

    garyUI->setButtonsEnabled(hasAudio, isConnected, isGenerating, retryAvailable, continueAvailable);

    if (resetTexts || !isGenerating)
    {
        garyUI->setSendButtonText("send to gary");
        garyUI->setContinueButtonText("continue");
        garyUI->setRetryButtonText("retry");
    }
}


//==============================================================================
// Updated timerCallback() with smooth progress animation:
void Gary4juceAudioProcessorEditor::timerCallback()
{
    updateRecordingStatus();

    // Update Jerry BPM display with current DAW BPM (only in plugin mode, not standalone)
    double currentBPM = audioProcessor.getCurrentBPM();
    if (jerryUI && !juce::JUCEApplicationBase::isStandaloneApp())
        jerryUI->setBpm(juce::roundToInt(currentBPM));

    if (terryUI)
        terryUI->setBpm(currentBPM);

    if (dariusUI)
    {
        dariusUI->setBpm(currentBPM);
        dariusUI->setSavedRecordingAvailable(savedSamples > 0);
        dariusUI->setOutputAudioAvailable(hasOutputAudio);
        dariusUI->setAudioSourceRecording(audioProcessor.getTransformRecording());
    }

    // Darius progress polling cadence (every ~250ms)
    if (dariusIsPollingProgress) {
        if (++dariusProgressPollTick >= 5) { // 5 * 50ms = 250ms
            dariusProgressPollTick = 0;
            pollDariusProgress();
        }
    }

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
    const bool flashConnected = audioProcessor.getIsUsingLocalhost()
        ? (localOnlineCount > 0)
        : isConnected;
    if (flashConnected)
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

    if (audioProcessor.getIsUsingLocalhost())
    {
        localHealthPollCounter++;
        if (localHealthPollCounter >= 60) // 60 * 50ms = 3 seconds
        {
            localHealthPollCounter = 0;
            triggerLocalServiceHealthPoll(false);
        }
    }
    else
    {
        localHealthPollCounter = 0;
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

    // Update save button state (only in plugin mode)
    bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();
    if (!isStandalone)
    {
        saveBufferButton.setEnabled(recordedSamples > 0);
    }

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
        // Show "waiting" state with different message based on plugin mode
        g.setFont(juce::FontOptions(14.0f));
        g.setColour(juce::Colours::darkgrey);

        bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();
        juce::String emptyMessage = isStandalone
            ? "drag an audio file here to use with gary, terry, or darius"
            : "press PLAY in DAW to start recording";

        g.drawText(emptyMessage, area, juce::Justification::centred);
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

    // Show reselection hint for standalone mode when file is available
    bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();
    if (isStandalone && savedSamples > 0 && lastDraggedAudioFile.existsAsFile())
    {
        // Draw hint text at bottom-right of waveform
        g.setFont(juce::FontOptions(13.0f));
        g.setColour(juce::Colours::lightgrey.withAlpha(0.8f));
        // Create hint area from bottom-right of waveform without modifying original area
        auto hintArea = juce::Rectangle<int>(area.getX(), area.getBottom() - 15, area.getWidth() - 4, 15);
        g.drawText("double-click to reselect", hintArea, juce::Justification::centredRight);
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

            const auto activeOp = getActiveOp();
            const bool pollJerry = (activeOp == ActiveOp::JerryGenerate);
            const bool pollTerry = (activeOp == ActiveOp::TerryTransform);
            const ServiceType pollService = pollTerry
                ? ServiceType::Terry
                : (pollJerry ? ServiceType::Jerry : ServiceType::Gary);
            juce::URL pollUrl(getServiceUrl(pollService, "/api/juce/poll_status/" + sessionId));

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
                            showStatusMessage("warming up... (network jitter)", 2500);
                            // also reset stall timer so we don�t trip
                            lastProgressUpdateTime = juce::Time::getCurrentTime().toMilliseconds();
                        });
                    return; // simply wait for next timer tick to poll again
                }

                const bool terryLocalPoll = audioProcessor.getIsUsingLocalhost()
                    && getActiveOp() == ActiveOp::TerryTransform;
                if (terryLocalPoll)
                {
                    juce::MessageManager::callAsync([this]()
                        {
                            handleGenerationFailure("cannot connect to terry on localhost - ensure terry is running in gary4local");
                        });
                    return;
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
                            showStatusMessage("warming up... (transient)", 2500);
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
                            showStatusMessage("warming up... (transient)", 2500);
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
    auto resetJerryButtonIfNeeded = [this]()
    {
        if (getActiveOp() == ActiveOp::JerryGenerate && jerryUI)
            jerryUI->setGenerateButtonText("generate with jerry");
    };
    const bool isLocalTerryOp = audioProcessor.getIsUsingLocalhost()
        && getActiveOp() == ActiveOp::TerryTransform;

    if (responseText.isEmpty())
    {
        DBG("Empty polling response - backend likely down");
        stopPolling();

        if (isLocalTerryOp)
        {
            handleGenerationFailure("cannot connect to terry on localhost - ensure terry is running in gary4local");
            return;
        }

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
        resetJerryButtonIfNeeded();
        setActiveOp(ActiveOp::None);
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
                resetJerryButtonIfNeeded();
                setActiveOp(ActiveOp::None);
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
                    juce::String verb = currentOperationVerb();
                    if (verb == "processing")
                        verb = transformInProgress ? "transforming" : "cooking";

                    showStatusMessage(verb + ": " + juce::String(serverProgress) + "%", 5000);
                    DBG("Progress (" + verb + "): " + juce::String(serverProgress) + "%, animating from " +
                        juce::String(lastKnownProgress));
                }
                else
                {
                    // Fallback message when we have no specific queue info but task is in progress
                    if (getActiveOp() == ActiveOp::TerryTransform || transformInProgress)
                        showStatusMessage("processing transform...", 5000);
                    else
                        showStatusMessage("processing audio...", 5000);
                }

                return;
            }

            // COMPLETED - Check what TYPE of completion this is FIRST
            auto audioData = responseObj->getProperty("audio_data").toString();
            auto status = responseObj->getProperty("status").toString();
            const auto activeOp = getActiveOp();
            const bool isTransformOp = transformInProgress || activeOp == ActiveOp::TerryTransform;

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
                if (isTransformOp)
                {
                    // TRANSFORM COMPLETION - Keep session ID for undo
                    if (terryUI)
                        terryUI->setTransformButtonText("transform with terry");
                    showStatusMessage("transform complete!", 3000);
                    saveGeneratedAudio(audioData);
                    DBG("Successfully received transformed audio: " + juce::String(audioData.length()) + " chars");

                    // Enable undo button now that transform is complete
                    audioProcessor.setUndoTransformAvailable(true); // ADD THIS
                    audioProcessor.setRetryAvailable(false);
                    // DON'T clear currentSessionId - we need it for undo!

                    updateTerryEnablementSnapshot();

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
                        if (garyUI)
                            garyUI->setRetryButtonText("retry");  // Reset retry button text
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

                resetJerryButtonIfNeeded();
                setActiveOp(ActiveOp::None);
            }
            else
            {
                // COMPLETED but no audio data
                if (status == "failed")
                {
                    auto error = responseObj->getProperty("error").toString();
                    stopPolling();

                    if (isTransformOp)
                    {
                        showStatusMessage("transform failed: " + error, 5000);
                        audioProcessor.setUndoTransformAvailable(false);
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

                    resetJerryButtonIfNeeded();
                    setActiveOp(ActiveOp::None);
                }
                else if (status == "completed")
                {
                    stopPolling();

                    if (isTransformOp)
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

                    resetJerryButtonIfNeeded();
                    setActiveOp(ActiveOp::None);
                }
            }
        }
        else
        {
            DBG("Failed to parse polling response as JSON - backend likely down");
            stopPolling();

            if (isLocalTerryOp)
            {
                handleGenerationFailure("terry returned an invalid response");
                return;
            }

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
            resetJerryButtonIfNeeded();
            setActiveOp(ActiveOp::None);
        }
    }
    catch (...)
    {
        DBG("Exception parsing polling response - backend likely down");
        stopPolling();

        if (isLocalTerryOp)
        {
            handleGenerationFailure("failed to parse terry response");
            return;
        }

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
        resetJerryButtonIfNeeded();
        setActiveOp(ActiveOp::None);
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

        if (!garyDir.exists())
        {
            auto result = garyDir.createDirectory();
            if (!result.wasOk())
            {
                showStatusMessage("failed to create Documents/gary4juce folder", 6000);
                DBG("Failed to create gary4juce directory: " + result.getErrorMessage());
                return;
            }
        }

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
            if (garyUI)
            {
                garyUI->setSendButtonText("send to gary");
                garyUI->setContinueButtonText("continue");
            }

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
    setActiveOp(ActiveOp::GaryGenerate);

    auto cancelGaryOperation = [this]() {
        isGenerating = false;
        continueInProgress = false;
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
    };

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
        cancelGaryOperation();
        showStatusMessage("please save your recording first!");
        return;
    }

    if (!isConnected)
    {
        cancelGaryOperation();
        showStatusMessage("backend not connected - check connection first");
        return;
    }

    // Starting initial Gary generation - clear any previous session
    DBG("Starting initial Gary generation - clearing previous session");
    audioProcessor.clearCurrentSessionId();
    updateRetryButtonState();

    // Get the selected model from dynamic list
    auto selectedModel = getSelectedGaryModelPath();
    const bool isLocalhostRequest = audioProcessor.getIsUsingLocalhost();
    const juce::String capturedQuantizationMode = currentGaryQuantizationMode;

    // Debug current values
    DBG("Current prompt duration value: " + juce::String(currentPromptDuration) + " (will be cast to: " + juce::String((int)currentPromptDuration) + ")");

    // Read the saved audio file
    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");
    auto audioFile = garyDir.getChildFile("myBuffer.wav");

    if (!audioFile.exists())
    {
        cancelGaryOperation();
        showStatusMessage("audio file not found - save recording first");
        return;
    }

    // Read and encode audio file
    juce::MemoryBlock audioData;
    if (!audioFile.loadFileAsData(audioData))
    {
        cancelGaryOperation();
        showStatusMessage("failed to read audio file");
        return;
    }

    // Verify we have audio data
    if (audioData.getSize() == 0)
    {
        cancelGaryOperation();
        showStatusMessage("audio file is empty");
        return;
    }

    auto base64Audio = juce::Base64::toBase64(audioData.getData(), audioData.getSize());

    // Debug: Log audio data size
    DBG("Audio file size: " + juce::String(audioData.getSize()) + " bytes");
    DBG("Base64 length: " + juce::String(base64Audio.length()) + " chars");

    // Button text feedback and status during processing (AFTER button state update)
    if (garyUI)
        garyUI->setSendButtonText("sending...");
    showStatusMessage("sending audio to gary...");

    updateAllGenerationButtonStates();
    repaint(); // Force immediate UI update

    // Create HTTP request in background thread
    juce::Thread::launch([this, selectedModel, base64Audio, isLocalhostRequest, capturedQuantizationMode, cancelGaryOperation]() {
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
        if (isLocalhostRequest)
            jsonRequest->setProperty("quantization_mode", capturedQuantizationMode);

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
        juce::MessageManager::callAsync([this, responseText, statusCode, startTime, cancelGaryOperation]() {
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

                        cancelGaryOperation();
                        if (garyUI)
                            garyUI->setSendButtonText("send to gary");
                    }
                }
                else
                {
                    showStatusMessage("Invalid JSON response from server", 4000);
                    DBG("Failed to parse JSON response: " + responseText.substring(0, 100));

                    cancelGaryOperation();
                    if (garyUI)
                        garyUI->setSendButtonText("send to gary");
                }
            }
            else
            {
                juce::String errorMsg;
                bool shouldCheckHealth = false;
                
                if (statusCode == 0 && audioProcessor.getIsUsingLocalhost())
                {
                    errorMsg = "Cannot connect to localhost - ensure Docker Compose is running";
                    markBackendDisconnectedFromRequestFailure("gary request");
                }
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
                if (garyUI)
                    garyUI->setSendButtonText("send to gary");

                cancelGaryOperation();
            }


            });
        });
}

void Gary4juceAudioProcessorEditor::continueMusic()
{
    setActiveOp(ActiveOp::GaryContinue);

    auto cancelContinueOperation = [this]() {
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
    };

    if (!hasOutputAudio)
    {
        showStatusMessage("no audio to continue", 2000);
        cancelContinueOperation();
        return;
    }

    // Encode current output audio as base64
    if (!outputAudioFile.exists())
    {
        showStatusMessage("output file not found", 2000);
        cancelContinueOperation();
        return;
    }

    // Read the audio file as binary data
    juce::MemoryBlock audioData;
    if (!outputAudioFile.loadFileAsData(audioData))
    {
        showStatusMessage("failed to read audio file", 3000);
        cancelContinueOperation();
        return;
    }

    // Convert to base64
    auto base64Audio = juce::Base64::toBase64(audioData.getData(), audioData.getSize());

    sendContinueRequest(base64Audio);
}

// UPDATED: sendContinueRequest with proper debugging
void Gary4juceAudioProcessorEditor::sendContinueRequest(const juce::String& audioData)
{
    DBG("Sending continue request with " + juce::String(audioData.length()) + " chars of audio data");
    showStatusMessage("requesting continuation...", 3000);

    auto cancelContinueOperation = [this]() {
        isGenerating = false;
        continueInProgress = false;
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
    };

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
    if (garyUI)
        garyUI->setContinueButtonText("continuing...");

    // Capture current model path NOW (before thread launch)
    juce::String capturedModelPath = getSelectedGaryModelPath();
    const bool isLocalhostRequest = audioProcessor.getIsUsingLocalhost();
    const juce::String capturedQuantizationMode = currentGaryQuantizationMode;
    DBG("Captured model path for continue: " + capturedModelPath);

    // Create HTTP request in background thread
    juce::Thread::launch([this, audioData, capturedModelPath, isLocalhostRequest, capturedQuantizationMode, cancelContinueOperation]() {
        // SAFETY: Exit if generation stopped
        if (!isGenerating) {
            DBG("Continue request aborted - generation stopped");
            return;
        }

        auto startTime = juce::Time::getCurrentTime();

        DBG("Continue using model: " + capturedModelPath);

        // Create JSON payload - same structure as sendToGary
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("audio_data", audioData);
        jsonRequest->setProperty("prompt_duration", (int)currentPromptDuration);
        jsonRequest->setProperty("model_name", capturedModelPath); // Use captured model path
        jsonRequest->setProperty("top_k", 250);
        jsonRequest->setProperty("temperature", 1.0);
        jsonRequest->setProperty("cfg_coef", 3.0);
        jsonRequest->setProperty("description", "");
        if (isLocalhostRequest)
            jsonRequest->setProperty("quantization_mode", capturedQuantizationMode);

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
        juce::MessageManager::callAsync([this, responseText, statusCode, cancelContinueOperation]() {
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
                        if (garyUI)
                            garyUI->setContinueButtonText("continue");

                        cancelContinueOperation();
                    }
                }
                else
                {
                    showStatusMessage("invalid response format", 3000);
                    if (garyUI)
                        garyUI->setContinueButtonText("continue");

                    cancelContinueOperation();
                }
            }
            else
            {
                juce::String errorMsg;
                bool shouldCheckHealth = false;
                
                if (statusCode == 0 && audioProcessor.getIsUsingLocalhost())
                {
                    errorMsg = "Cannot connect to localhost - ensure Docker Compose is running";
                    markBackendDisconnectedFromRequestFailure("continue request");
                }
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
                
                if (garyUI)
                    garyUI->setContinueButtonText("continue");

                cancelContinueOperation();
            }
            });
        });
}

void Gary4juceAudioProcessorEditor::retryLastContinuation()
{
    setActiveOp(ActiveOp::GaryRetry);

    auto cancelRetryOperation = [this]() {
        isGenerating = false;
        continueInProgress = false;
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
    };

    // Check if we have a session ID from the last continue operation
    juce::String sessionId = audioProcessor.getCurrentSessionId();
    if (sessionId.isEmpty())
    {
        showStatusMessage("no previous continuation to retry", 3000);
        cancelRetryOperation();
        return;
    }

    if (!isConnected)
    {
        showStatusMessage("backend not connected - check connection first");
        cancelRetryOperation();
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
    if (garyUI)
        garyUI->setRetryButtonText("retrying...");
    
    showStatusMessage("retrying last continuation...", 2000);

    const bool isLocalhostRequest = audioProcessor.getIsUsingLocalhost();
    const juce::String capturedQuantizationMode = currentGaryQuantizationMode;

    // Create HTTP request in background thread (same pattern as other requests)
    juce::Thread::launch([this, sessionId, isLocalhostRequest, capturedQuantizationMode, cancelRetryOperation]() {
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

        // Get current model selection from dynamic list
        jsonRequest->setProperty("model_name", getSelectedGaryModelPath());
        jsonRequest->setProperty("top_k", 250);
        jsonRequest->setProperty("temperature", 1.0);
        jsonRequest->setProperty("cfg_coef", 3.0);
        jsonRequest->setProperty("description", "");
        if (isLocalhostRequest)
            jsonRequest->setProperty("quantization_mode", capturedQuantizationMode);

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
        juce::MessageManager::callAsync([this, responseText, statusCode, cancelRetryOperation]() {
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
                        if (garyUI)
                            garyUI->setRetryButtonText("retry");

                        cancelRetryOperation();
                    }
                }
                else
                {
                    showStatusMessage("invalid retry response format", 3000);
                    updateRetryButtonState();
                    if (garyUI)
                        garyUI->setRetryButtonText("retry");

                    cancelRetryOperation();
                }
            }
            else
            {
                juce::String errorMsg;
                bool shouldCheckHealth = false;
                
                if (statusCode == 0 && audioProcessor.getIsUsingLocalhost())
                {
                    errorMsg = "Cannot connect to localhost - ensure Docker Compose is running";
                    markBackendDisconnectedFromRequestFailure("retry request");
                }
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
                if (garyUI)
                    garyUI->setRetryButtonText("retry");

                cancelRetryOperation();
            }
        });
    });
}











// GARY MODEL API CALLS

void Gary4juceAudioProcessorEditor::fetchGaryAvailableModels()
{
    if (!isConnected)
    {
        DBG("Not connected - skipping Gary model fetch");
        return;
    }

    juce::Thread::launch([this]()
        {
            // Both localhost and remote use the same endpoint for Gary models
            juce::String endpoint = "/api/models";

            juce::URL url(getServiceUrl(ServiceType::Gary, endpoint));

            std::unique_ptr<juce::InputStream> stream(url.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(10000)
            ));

            juce::String responseText;
            if (stream != nullptr)
                responseText = stream->readEntireStreamAsString();

            juce::MessageManager::callAsync([this, responseText]()
                {
                    handleGaryModelsResponse(responseText);
                });
        });
}

void Gary4juceAudioProcessorEditor::handleGaryModelsResponse(const juce::String& responseText)
{
    if (responseText.isEmpty())
    {
        DBG("Empty Gary models response - using fallback");
        showStatusMessage("failed to load gary models", 3000);

        // Fallback to safe defaults
        garyModelList.clear();
        garyModelList.push_back({"vanya ai dnb 0.1", "thepatch/vanya_ai_dnb_0.1", "small", 1});

        garyModelItems.clear();
        garyModelItems.add("vanya ai dnb 0.1");

        if (garyUI)
            garyUI->setModelItems(garyModelItems, 0);

        currentModelIndex = 0;
        if (audioProcessor.getIsUsingLocalhost())
            applyGaryQuantizationDefaultForCurrentModel();

        return;
    }

    try
    {
        auto parsed = juce::JSON::parse(responseText);

        if (!parsed.isObject())
        {
            DBG("Invalid Gary models response format - not an object");
            return;
        }

        auto* obj = parsed.getDynamicObject();
        if (!obj || !obj->hasProperty("models"))
        {
            DBG("Response missing 'models' property");
            return;
        }

        auto modelsVar = obj->getProperty("models");
        if (!modelsVar.isObject())
        {
            DBG("models property is not an object");
            return;
        }

        auto* modelsObj = modelsVar.getDynamicObject();
        if (!modelsObj)
            return;

        // Clear existing model data
        garyModelList.clear();
        garyModelItems.clear();

        int nextId = 1;  // ComboBox IDs start at 1
        int firstSelectableId = 0;  // Track first selectable item ID

        // Build hierarchical menu structure
        std::vector<CustomComboBox::MenuItem> menuItems;

        // Process each size category (small, medium, large)
        const juce::StringArray sizeCategories = {"small", "medium", "large"};

        for (const auto& size : sizeCategories)
        {
            if (!modelsObj->hasProperty(size))
                continue;

            auto sizeArray = modelsObj->getProperty(size);
            if (!sizeArray.isArray())
                continue;

            auto* array = sizeArray.getArray();
            if (!array || array->size() == 0)
                continue;

            // Add section header
            juce::String headerText = size.substring(0, 1).toUpperCase() + size.substring(1) + " Models";
            CustomComboBox::MenuItem sectionHeader;
            sectionHeader.name = headerText.toUpperCase();
            sectionHeader.itemId = 0;
            sectionHeader.isSectionHeader = true;
            sectionHeader.isSubMenu = false;
            menuItems.push_back(sectionHeader);

            // Process models in this size category
            for (int i = 0; i < array->size(); ++i)
            {
                auto modelVar = array->getUnchecked(i);
                if (!modelVar.isObject())
                    continue;

                auto* modelObj = modelVar.getDynamicObject();
                if (!modelObj)
                    continue;

                auto typeStr = modelObj->getProperty("type").toString();

                if (typeStr == "single")
                {
                    // Single model - add as regular menu item
                    auto name = modelObj->getProperty("name").toString();
                    auto path = modelObj->getProperty("path").toString();

                    CustomComboBox::MenuItem item;
                    item.name = name;
                    item.itemId = nextId;
                    item.isSectionHeader = false;
                    item.isSubMenu = false;
                    menuItems.push_back(item);

                    garyModelList.push_back({name, path, size, nextId});
                    if (firstSelectableId == 0)
                        firstSelectableId = nextId;

                    nextId++;
                }
                else if (typeStr == "group")
                {
                    // Group of checkpoints
                    auto groupName = modelObj->getProperty("name").toString();

                    if (!modelObj->hasProperty("checkpoints"))
                        continue;

                    auto checkpointsVar = modelObj->getProperty("checkpoints");
                    if (!checkpointsVar.isArray())
                        continue;

                    auto* checkpointsArray = checkpointsVar.getArray();
                    if (!checkpointsArray || checkpointsArray->size() == 0)
                        continue;

                    // If only ONE checkpoint, add it as a regular item (no submenu)
                    if (checkpointsArray->size() == 1)
                    {
                        auto checkpointVar = checkpointsArray->getUnchecked(0);
                        if (checkpointVar.isObject())
                        {
                            auto* checkpointObj = checkpointVar.getDynamicObject();
                            if (checkpointObj)
                            {
                                auto checkpointName = checkpointObj->getProperty("name").toString();
                                auto checkpointPath = checkpointObj->getProperty("path").toString();
                                auto epoch = checkpointObj->getProperty("epoch");

                                juce::String displayName = checkpointName;
                                if (!epoch.isVoid())
                                    displayName += " (epoch " + epoch.toString() + ")";

                                CustomComboBox::MenuItem item;
                                item.name = displayName;
                                item.itemId = nextId;
                                item.isSectionHeader = false;
                                item.isSubMenu = false;
                                menuItems.push_back(item);

                                garyModelList.push_back({checkpointName, checkpointPath, size, nextId});
                                if (firstSelectableId == 0)
                                    firstSelectableId = nextId;

                                nextId++;
                            }
                        }
                    }
                    else
                    {
                        // Multiple checkpoints - create submenu
                        CustomComboBox::MenuItem groupItem;
                        groupItem.name = groupName;
                        groupItem.itemId = 0;
                        groupItem.isSectionHeader = false;
                        groupItem.isSubMenu = true;

                        // Add checkpoints as sub-items
                        for (int j = 0; j < checkpointsArray->size(); ++j)
                        {
                            auto checkpointVar = checkpointsArray->getUnchecked(j);
                            if (!checkpointVar.isObject())
                                continue;

                            auto* checkpointObj = checkpointVar.getDynamicObject();
                            if (!checkpointObj)
                                continue;

                            auto checkpointName = checkpointObj->getProperty("name").toString();
                            auto checkpointPath = checkpointObj->getProperty("path").toString();
                            auto epoch = checkpointObj->getProperty("epoch");

                            juce::String displayName = checkpointName;
                            if (!epoch.isVoid())
                                displayName += " (epoch " + epoch.toString() + ")";

                            CustomComboBox::MenuItem subItem;
                            subItem.name = displayName;
                            subItem.itemId = nextId;
                            subItem.isSectionHeader = false;
                            subItem.isSubMenu = false;
                            groupItem.subItems.push_back(subItem);

                            garyModelList.push_back({checkpointName, checkpointPath, size, nextId});
                            if (firstSelectableId == 0)
                                firstSelectableId = nextId;

                            nextId++;
                        }

                        menuItems.push_back(groupItem);
                    }
                }
            }
        }

        // Update UI with hierarchical menu
        if (garyUI)
        {
            auto* modelComboBox = dynamic_cast<CustomComboBox*>(&garyUI->getModelComboBox());
            if (modelComboBox)
            {
                modelComboBox->setHierarchicalItems(menuItems);

                // Set default selection to first selectable item
                if (firstSelectableId > 0)
                {
                    modelComboBox->setSelectedId(firstSelectableId, juce::dontSendNotification);

                    // Find the index in garyModelList that corresponds to this ID
                    for (size_t i = 0; i < garyModelList.size(); ++i)
                    {
                        if (garyModelList[i].dropdownId == firstSelectableId)
                        {
                            currentModelIndex = static_cast<int>(i);
                            break;
                        }
                    }
                }
            }
        }

        if (audioProcessor.getIsUsingLocalhost())
            applyGaryQuantizationDefaultForCurrentModel();

        DBG("Loaded " + juce::String(garyModelList.size()) + " Gary models with hierarchical menu");
    }
    catch (...)
    {
        DBG("Exception parsing Gary models response");
        showStatusMessage("failed to parse gary models", 3000);
    }
}

juce::String Gary4juceAudioProcessorEditor::getSelectedGaryModelPath() const
{
    // Get the selected ID from the ComboBox
    if (!garyUI)
    {
        DBG("GaryUI not available, using fallback");
        return "thepatch/vanya_ai_dnb_0.1";
    }

    auto* modelComboBox = dynamic_cast<const CustomComboBox*>(&garyUI->getModelComboBox());
    if (!modelComboBox)
    {
        DBG("Model ComboBox not available, using fallback");
        return "thepatch/vanya_ai_dnb_0.1";
    }

    int selectedId = modelComboBox->getSelectedId();
    if (selectedId == 0)
    {
        DBG("No model selected, using fallback");
        return "thepatch/vanya_ai_dnb_0.1";
    }

    // Find the model with this ID in our list
    for (const auto& model : garyModelList)
    {
        if (model.dropdownId == selectedId)
        {
            DBG("Selected Gary model: " + model.fullPath + " (ID: " + juce::String(selectedId) + ")");
            return model.fullPath;
        }
    }

    // Fallback to safe default
    DBG("Invalid Gary model ID: " + juce::String(selectedId) + ", using fallback");
    return "thepatch/vanya_ai_dnb_0.1";
}

juce::String Gary4juceAudioProcessorEditor::getSelectedGaryModelSizeCategory() const
{
    if (!garyUI)
        return {};

    auto* modelComboBox = dynamic_cast<const CustomComboBox*>(&garyUI->getModelComboBox());
    if (!modelComboBox)
        return {};

    const int selectedId = modelComboBox->getSelectedId();
    for (const auto& model : garyModelList)
    {
        if (model.dropdownId == selectedId)
            return model.sizeCategory;
    }

    if (currentModelIndex >= 0 && currentModelIndex < static_cast<int>(garyModelList.size()))
        return garyModelList[static_cast<size_t>(currentModelIndex)].sizeCategory;

    return {};
}

juce::String Gary4juceAudioProcessorEditor::getDefaultGaryQuantizationForSize(const juce::String& sizeCategory) const
{
    const juce::String normalized = sizeCategory.trim().toLowerCase();

    if (normalized == "small")
        return "q8_decoder_linears";
    if (normalized == "medium" || normalized == "large")
        return "q4_decoder_linears";

    return "q4_decoder_linears";
}

void Gary4juceAudioProcessorEditor::applyGaryQuantizationDefaultForCurrentModel()
{
    if (!garyUI)
        return;

    const juce::String sizeCategory = getSelectedGaryModelSizeCategory();
    const juce::String defaultMode = getDefaultGaryQuantizationForSize(sizeCategory);
    currentGaryQuantizationMode = defaultMode;
    garyUI->setQuantizationMode(defaultMode, juce::dontSendNotification);

    DBG("Gary quantization default -> size: "
        + (sizeCategory.isNotEmpty() ? sizeCategory : "unknown")
        + ", mode: " + defaultMode);
}

// JERRY API CALLS

void Gary4juceAudioProcessorEditor::fetchJerryAvailableModels()
{
    if (jerryModelsFetchInFlight.exchange(true))
    {
        DBG("Jerry models fetch already in flight - skipping");
        return;
    }

    const bool isLocalhost = audioProcessor.getIsUsingLocalhost();
    if (!isLocalhost && !isConnected)
    {
        DBG("Not connected - skipping model fetch");
        jerryModelsFetchInFlight.store(false);
        return;
    }
    if (isLocalhost && !isLocalServiceOnline(ServiceType::Jerry))
    {
        DBG("Jerry service offline on localhost - skipping model fetch");
        if (jerryUI)
            jerryUI->setLoadingModel(false);
        jerryModelsFetchInFlight.store(false);
        return;
    }

    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis(this);
    juce::Thread::launch([safeThis]()
        {
            if (safeThis == nullptr)
                return;

            try
            {
                // Determine endpoint based on connection type (same pattern as sendToJerry)
                juce::String endpoint = safeThis->audioProcessor.getIsUsingLocalhost()
                    ? "/models/status"           // Localhost: no /audio prefix
                    : "/audio/models/status";    // Remote: requires /audio prefix

                juce::URL url(safeThis->getServiceUrl(ServiceType::Jerry, endpoint));

                std::unique_ptr<juce::InputStream> stream(url.createInputStream(
                    juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(10000)
                ));

                juce::String responseText;
                if (stream != nullptr)
                    responseText = stream->readEntireStreamAsString();

                juce::MessageManager::callAsync([safeThis, responseText]()
                    {
                        if (safeThis == nullptr)
                            return;
                        safeThis->jerryModelsFetchInFlight.store(false);
                        safeThis->handleJerryModelsResponse(responseText);
                    });
            }
            catch (...)
            {
                juce::MessageManager::callAsync([safeThis]()
                    {
                        if (safeThis == nullptr)
                            return;
                        safeThis->jerryModelsFetchInFlight.store(false);
                        safeThis->handleJerryModelsResponse({});
                    });
            }
        });
}

void Gary4juceAudioProcessorEditor::handleJerryModelsResponse(const juce::String& responseText)
{
    if (responseText.isEmpty())
    {
        DBG("Empty models response");
        if (jerryUI)
            jerryUI->setLoadingModel(false);
        return;
    }

    DBG("=== RAW MODELS RESPONSE ===");
    DBG("Response length: " + juce::String(responseText.length()) + " characters");

    try
    {
        auto parsed = juce::JSON::parse(responseText);

        if (!parsed.isObject())
        {
            DBG("Invalid models response format - not an object");
            if (jerryUI)
                jerryUI->setLoadingModel(false);
            return;
        }

        auto* obj = parsed.getDynamicObject();
        if (!obj || !obj->hasProperty("model_details"))
        {
            DBG("Response missing 'model_details' property");
            if (jerryUI)
                jerryUI->setLoadingModel(false);
            return;
        }

        auto modelDetails = obj->getProperty("model_details");
        if (!modelDetails.isObject())
        {
            DBG("model_details is not an object");
            if (jerryUI)
                jerryUI->setLoadingModel(false);
            return;
        }

        // Parse models into component arrays
        juce::StringArray modelNames;
        juce::StringArray modelKeys;
        juce::StringArray modelTypes;
        juce::StringArray modelRepos;
        juce::StringArray modelCheckpoints;
        juce::StringArray modelSamplerProfiles;
        juce::Array<bool> isFinetune;

        const auto determineSamplerProfile = [](const juce::String& modelType,
                                                const juce::String& modelSource,
                                                juce::int64 sampleSize)
        {
            const auto source = modelSource.toLowerCase();

            if (sampleSize > 524288)
                return juce::String("sao10");

            if (source.contains("stable-audio-open-1.0")
                || source.contains("stable_audio_open_1_0")
                || source.contains("stableaudioopen1.0")
                || source.contains("sao1"))
            {
                return juce::String("sao10");
            }

            if (modelType == "finetune")
                return juce::String("saos_finetune");

            return juce::String("standard");
        };

        auto* detailsObj = modelDetails.getDynamicObject();
        if (!detailsObj)
        {
            DBG("Failed to get model_details as dynamic object");
            return;
        }

        DBG("Found " + juce::String(detailsObj->getProperties().size()) + " models in cache");

        for (auto& entry : detailsObj->getProperties())
        {
            juce::String modelKey = entry.name.toString();
            auto* modelData = entry.value.getDynamicObject();

            if (modelData)
            {
                juce::String source = modelData->getProperty("source").toString();
                juce::String type = modelData->getProperty("type").toString();
                const juce::int64 sampleSize = modelData->getProperty("sample_size").toString().getLargeIntValue();
                const juce::String samplerProfile = determineSamplerProfile(type, source, sampleSize);

                DBG("Processing model - Key: " + modelKey + ", Source: " + source + ", Type: " + type);

                juce::String displayName;
                juce::String repo;
                juce::String checkpoint;

                if (type == "standard")
                {
                    displayName = "Standard SAOS";
                    repo = "";
                    checkpoint = "";
                }
                else if (type == "finetune")
                {
                    // Parse source: "thepatch/jerry_grunge/jerry_encoded_epoch=33-step=100.ckpt"
                    juce::StringArray parts;
                    parts.addTokens(source, "/", "");

                    if (parts.size() >= 3)
                    {
                        repo = parts[0] + "/" + parts[1];  // "thepatch/jerry_grunge"
                        checkpoint = parts[2];              // "jerry_encoded_epoch=33-step=100.ckpt"

                        // Create base display name from repo
                        juce::String baseName = parts[1].replace("_", " ");

                        // Capitalize each word
                        juce::StringArray words;
                        words.addTokens(baseName, " ", "");
                        baseName.clear();
                        for (auto& word : words)
                        {
                            if (word.isNotEmpty())
                            {
                                baseName += word.substring(0, 1).toUpperCase() +
                                    word.substring(1).toLowerCase() + " ";
                            }
                        }
                        baseName = baseName.trim();

                        // NEW: Extract step/epoch info from checkpoint filename
                        juce::String checkpointInfo = extractCheckpointInfo(checkpoint);

                        if (checkpointInfo.isNotEmpty())
                        {
                            displayName = baseName + " (" + checkpointInfo + ")";
                        }
                        else
                        {
                            displayName = baseName;
                        }
                    }
                    else
                    {
                        displayName = "Unknown Finetune";
                        repo = source;
                        checkpoint = "";
                    }
                }
                else
                {
                    displayName = "Unknown Model";
                    repo = "";
                    checkpoint = "";
                }

                modelNames.add(displayName);
                modelKeys.add(modelKey);
                modelTypes.add(type);
                modelRepos.add(repo);
                modelCheckpoints.add(checkpoint);
                modelSamplerProfiles.add(samplerProfile);
                isFinetune.add(type == "finetune");

                DBG("Added model: " + displayName);
                DBG("  Type: " + type + ", Repo: " + repo + ", Checkpoint: " + checkpoint);
                DBG("  Sample size: " + juce::String(sampleSize) + ", Sampler profile: " + samplerProfile);
            }
        }

        if (jerryUI && modelNames.size() > 0)
        {
            jerryUI->setAvailableModels(modelNames, isFinetune, modelKeys,
                modelTypes, modelRepos, modelCheckpoints, modelSamplerProfiles);
            jerryUI->setLoadingModel(false);

            // When running localhost, ensure the dice bank is hydrated for
            // the currently selected finetune even if selection did not change.
            if (audioProcessor.getIsUsingLocalhost() && jerryUI->getSelectedModelIsFinetune())
            {
                const auto selectedRepo = jerryUI->getSelectedFinetuneRepo();
                const auto selectedCheckpoint = jerryUI->getSelectedFinetuneCheckpoint();
                if (selectedRepo.isNotEmpty() && selectedCheckpoint.isNotEmpty())
                    fetchJerryPrompts(selectedRepo, selectedCheckpoint);
            }

            DBG("=== SUCCESS: Updated Jerry UI with " + juce::String(modelNames.size()) + " models ===");
        }
        else if (jerryUI)
        {
            jerryUI->setLoadingModel(false);
        }
    }
    catch (...)
    {
        DBG("Exception parsing models response");
        if (jerryUI)
            jerryUI->setLoadingModel(false);
    }
}

void Gary4juceAudioProcessorEditor::fetchJerryCheckpoints(const juce::String& repo)
{
    if (jerryUI)
        jerryUI->setFetchingCheckpoints(true);

    juce::Thread::launch([this, repo]() {
        juce::String endpoint = "/models/checkpoints";  // No /audio prefix on localhost
        juce::URL url(getServiceUrl(ServiceType::Jerry, endpoint));

        // Build JSON body
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("finetune_repo", repo);
        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        juce::URL postUrl = url.withPOSTData(jsonString);

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(10000)
            .withExtraHeaders("Content-Type: application/json");

        std::unique_ptr<juce::InputStream> stream(postUrl.createInputStream(options));

        juce::String responseText;
        if (stream != nullptr)
            responseText = stream->readEntireStreamAsString();

        juce::MessageManager::callAsync([this, responseText]() {
            handleJerryCheckpointsResponse(responseText);
        });
    });
}

void Gary4juceAudioProcessorEditor::handleJerryCheckpointsResponse(const juce::String& responseText)
{
    if (jerryUI)
        jerryUI->setFetchingCheckpoints(false);

    if (responseText.isEmpty()) {
        showStatusMessage("failed to fetch checkpoints", 3000);
        return;
    }

    try {
        auto parsed = juce::JSON::parse(responseText);
        if (auto* obj = parsed.getDynamicObject()) {
            bool success = obj->getProperty("success");

            if (success) {
                juce::StringArray checkpoints;
                auto checkpointsArray = obj->getProperty("checkpoints");

                if (auto* arr = checkpointsArray.getArray()) {
                    for (auto& item : *arr) {
                        checkpoints.add(item.toString());
                    }
                }

                if (jerryUI)
                    jerryUI->setAvailableCheckpoints(checkpoints);

                showStatusMessage(juce::String(checkpoints.size()) + " checkpoints found", 2500);
            } else {
                auto error = obj->getProperty("error").toString();
                showStatusMessage("fetch failed: " + error, 4000);
            }
        }
    } catch (...) {
        showStatusMessage("invalid checkpoint response", 3000);
    }
}

juce::URL Gary4juceAudioProcessorEditor::buildPromptsUrl(const juce::String& repo,
    const juce::String& checkpoint) const
{
    const bool isLocal = audioProcessor.getIsUsingLocalhost();
    const juce::String endpoint = isLocal ? "/models/prompts" : "/audio/models/prompts";

    // getServiceUrl(...) -> juce::String  ⇒  wrap it into a juce::URL
    juce::URL url(getServiceUrl(ServiceType::Jerry, endpoint));

    url = url.withParameter("repo", repo)
        .withParameter("checkpoint", checkpoint);

    return url;
}

// --- In PluginEditor.cpp ---
void Gary4juceAudioProcessorEditor::fetchJerryPrompts(const juce::String& repo,
    const juce::String& checkpoint)
{
    const auto cacheKey = repo + "|" + checkpoint;
    if (promptsCache.contains(cacheKey))
    {
        DBG("[prompts] cache hit for " + cacheKey + " - applying");
        applyJerryPromptsToUI(repo, checkpoint, promptsCache[cacheKey]);
        return;
    }

    juce::Thread::launch([this, repo, checkpoint]()
        {
            juce::URL url = buildPromptsUrl(repo, checkpoint);

            int statusCode = 0;
            juce::StringPairArray responseHeaders;

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(10000)
                .withStatusCode(&statusCode)
                .withResponseHeaders(&responseHeaders);

            std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));
            juce::String responseText;
            if (stream) responseText = stream->readEntireStreamAsString();

            DBG("[prompts] GET " + url.toString(true) +
                " status=" + juce::String(statusCode) +
                " bytes=" + juce::String((int)responseText.getNumBytesAsUTF8()));

            if (statusCode < 200 || statusCode >= 300)
            {
                DBG("[prompts] non-2xx - first512: " + responseText.substring(0, 512));
            }

            juce::MessageManager::callAsync([this, repo, checkpoint, responseText, statusCode]()
                {
                    applyJerryPromptsToUI(repo, checkpoint, responseText, statusCode);
                });
        });
}


void Gary4juceAudioProcessorEditor::maybeFetchRemoteJerryPrompts()
{
    DBG("[prompts] maybeFetchRemoteJerryPrompts called");
    // Don’t re-enter
    if (promptsFetchInFlight) { DBG("[prompts] in flight – skipping"); return; }

    // TTL: only refresh every 5 minutes (tweak if you want)
    const auto now = static_cast<std::int64_t>(juce::Time::getCurrentTime().toMilliseconds());
    if (now - lastPromptsFetchMs < kPromptsTTLms) {
        DBG("[prompts] TTL not expired – skipping");
        return;
    }

    // If we already have a bank for the currently selected finetune, bail early
    if (jerryUI)
    {
        const auto repo = jerryUI->getSelectedFinetuneRepo();
        const auto ckpt = jerryUI->getSelectedFinetuneCheckpoint();
        if (repo.isNotEmpty() && ckpt.isNotEmpty())
        {
            const auto key = repo + "|" + ckpt;
            if (promptsCache.contains(key))
            {
                DBG("[prompts] cache hit for " + key + " – applying");
                // We already have it cached – apply it to the UI and skip fetch
                applyJerryPromptsToUI(repo, ckpt, promptsCache[key]);
                lastPromptsFetchMs = now;
                return;
            }
        }
    }

    // If we don't know exact repo/ckpt (or not cached), ask backend for the active finetune
    promptsFetchInFlight = true;

    juce::Thread::launch([this]()
        {
            // Prefer whatever finetune is currently active on the remote cache
            juce::URL url(getServiceUrl(ServiceType::Jerry, "/audio/models/prompts"));
            url = url.withParameter("prefer", "finetune");

            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(8000);

            std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));
            juce::String responseText;
            if (stream) responseText = stream->readEntireStreamAsString();

            juce::MessageManager::callAsync([this, responseText]()
                {
                    promptsFetchInFlight = false;
                    lastPromptsFetchMs = static_cast<std::int64_t>(juce::Time::getCurrentTime().toMilliseconds());
                    if (responseText.isEmpty()) return;

                    // Parse and store under the resolved repo/ckpt that the backend returns
                    auto parsed = juce::JSON::parse(responseText);
                    if (auto* obj = parsed.getDynamicObject())
                    {
                        if (!obj->getProperty("success")) return;

                        const auto promptsVar = obj->getProperty("prompts");
                        const auto repo = obj->getProperty("source").toString();
                        const auto ckpt = obj->getProperty("checkpoint").toString();

                        if (promptsVar.isObject() && repo.isNotEmpty() && ckpt.isNotEmpty())
                        {
                            const auto cacheKey = repo + "|" + ckpt;
                            promptsCache.set(cacheKey, responseText);

                            if (jerryUI)
                                jerryUI->setFinetunePromptBank(repo, ckpt, promptsVar);
                        }
                    }
                });
        });
}


void Gary4juceAudioProcessorEditor::applyJerryPromptsToUI(const juce::String& repo,
    const juce::String& checkpoint,
    const juce::String& jsonText,
    int statusCode /* = 200 */)
{
    if (jsonText.isEmpty())
    {
        DBG("[prompts] empty response - skipping");
        return;
    }

    juce::var parsed;
    try
    {
        parsed = juce::JSON::parse(jsonText);
    }
    catch (...)
    {
        DBG("[prompts] JSON parse error - first512: " + jsonText.substring(0, 512));
        return;
    }

    if (auto* obj = parsed.getDynamicObject())
    {
        if (!obj->getProperty("success"))
        {
            DBG("[prompts] success=false - payload first512: " + jsonText.substring(0, 512));
            return;
        }

        auto promptsVar = obj->getProperty("prompts");
        if (!promptsVar.isObject())
        {
            DBG("[prompts] prompts missing/invalid - payload first512: " + jsonText.substring(0, 512));
            return;
        }

        // Trust backend's resolved source/checkpoint if provided; otherwise use ours
        auto resolvedRepo = obj->getProperty("source").toString();
        auto resolvedCkpt = obj->getProperty("checkpoint").toString();
        if (resolvedRepo.isEmpty()) resolvedRepo = repo;
        if (resolvedCkpt.isEmpty()) resolvedCkpt = checkpoint;

        const auto cacheKey = resolvedRepo + "|" + resolvedCkpt;
        promptsCache.set(cacheKey, jsonText);

        if (jerryUI)
            jerryUI->setFinetunePromptBank(resolvedRepo, resolvedCkpt, promptsVar);

        DBG("[prompts] stored bank for " + cacheKey);
    }
}



void Gary4juceAudioProcessorEditor::addCustomJerryModel(const juce::String& repo, const juce::String& checkpoint)
{
    // Extract display info for loading message
    juce::String checkpointInfo = extractCheckpointInfo(checkpoint);
    juce::String loadingMessage = checkpointInfo.isNotEmpty()
        ? "loading " + checkpointInfo
        : "loading " + checkpoint;

    showStatusMessage(loadingMessage + "...", 15000);  // Much longer duration for model downloads

    // Set loading state in UI
    if (jerryUI)
    {
        jerryUI->setLoadingModel(true, checkpointInfo);
    }

    juce::Thread::launch([this, repo, checkpoint]() {
        juce::String endpoint = "/models/switch";  // No /audio prefix on localhost
        juce::URL url(getServiceUrl(ServiceType::Jerry, endpoint));

        // Build JSON body
        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("model_type", "finetune");
        jsonRequest->setProperty("finetune_repo", repo);
        jsonRequest->setProperty("finetune_checkpoint", checkpoint);
        auto jsonString = juce::JSON::toString(juce::var(jsonRequest.get()));

        juce::URL postUrl = url.withPOSTData(jsonString);

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(300000)  // Increased to 5 minutes - model downloads can take several minutes!
            .withExtraHeaders("Content-Type: application/json");

        std::unique_ptr<juce::InputStream> stream(postUrl.createInputStream(options));

        juce::String responseText;
        if (stream != nullptr)
            responseText = stream->readEntireStreamAsString();

        juce::MessageManager::callAsync([this, responseText, repo, checkpoint]() {
            handleAddCustomModelResponse(responseText, repo, checkpoint);
        });
    });
}

void Gary4juceAudioProcessorEditor::handleAddCustomModelResponse(const juce::String& responseText,
                                                                  const juce::String& repo,
                                                                  const juce::String& checkpoint)
{
    if (responseText.isEmpty()) {
        showStatusMessage("failed to switch model - connection timeout", 5000);

        // Clear loading state on error
        if (jerryUI)
        {
            jerryUI->setLoadingModel(false);
        }
        return;
    }

    try {
        auto parsed = juce::JSON::parse(responseText);
        if (auto* obj = parsed.getDynamicObject()) {
            bool success = obj->getProperty("success");

            if (success) {
                // Show success message with checkpoint info
                juce::String checkpointInfo = extractCheckpointInfo(checkpoint);
                juce::String successMsg = checkpointInfo.isNotEmpty()
                    ? "model loaded: " + checkpointInfo
                    : "model loaded successfully";

                showStatusMessage(successMsg + "!", 4000);

                DBG("Model switch successful - refreshing model list");

                // CRITICAL: Refresh model list FIRST, then clear loading state
                // The refresh will update the dropdown with the new model
                fetchJerryAvailableModels();

                // Clear loading state after a short delay to ensure model list refresh completes
                // Also try to select the newly loaded model if we can identify it
                juce::Timer::callAfterDelay(500, [this, checkpointInfo, repo, checkpoint]() {
                    if (jerryUI)
                    {
                        jerryUI->setLoadingModel(false);

                        // Best-effort auto-select by repo (you already do this)
                        jerryUI->selectModelByRepo(repo);

                        // Now fetch prompts for THIS finetune explicitly
                        fetchJerryPrompts(repo, checkpoint);
                    }
                    });
            } else {
                auto error = obj->getProperty("error").toString();
                showStatusMessage("load failed: " + error, 5000);

                // Clear loading state on error
                if (jerryUI)
                {
                    jerryUI->setLoadingModel(false);
                }

                DBG("Model switch failed: " + error);
            }
        }
    } catch (...) {
        showStatusMessage("invalid response from server", 4000);

        // Clear loading state on error
        if (jerryUI)
        {
            jerryUI->setLoadingModel(false);
        }
    }
}

juce::String Gary4juceAudioProcessorEditor::extractCheckpointInfo(const juce::String& checkpoint)
{
    // Try to extract step or epoch from checkpoint filename
    // Examples:
    // "jerry_encoded_epoch=33-step=100.ckpt" → "step 100"
    // "jerry_un-encoded_epoch=32-step=2000.ckpt" → "step 2000"
    // "model_epoch=5.ckpt" → "epoch 5"

    // Try step first (preferred)
    if (checkpoint.contains("step="))
    {
        auto stepStart = checkpoint.indexOf("step=") + 5;
        auto stepEnd = checkpoint.indexOfChar(stepStart, '.');
        if (stepEnd < 0) stepEnd = checkpoint.indexOfChar(stepStart, '-');
        if (stepEnd < 0) stepEnd = checkpoint.length();

        auto stepValue = checkpoint.substring(stepStart, stepEnd);
        if (stepValue.containsOnly("0123456789"))
        {
            return "step " + stepValue;
        }
    }

    // Fall back to epoch
    if (checkpoint.contains("epoch="))
    {
        auto epochStart = checkpoint.indexOf("epoch=") + 6;
        auto epochEnd = checkpoint.indexOfChar(epochStart, '.');
        if (epochEnd < 0) epochEnd = checkpoint.indexOfChar(epochStart, '-');
        if (epochEnd < 0) epochEnd = checkpoint.length();

        auto epochValue = checkpoint.substring(epochStart, epochEnd);
        if (epochValue.containsOnly("0123456789"))
        {
            return "epoch " + epochValue;
        }
    }

    return "";  // No step/epoch info found
}


void Gary4juceAudioProcessorEditor::sendToJerry()
{
    const bool useLocalAsyncPolling = audioProcessor.getIsUsingLocalhost();
    const bool jerryConnected = useLocalAsyncPolling
        ? isLocalServiceOnline(ServiceType::Jerry)
        : isConnected;

    if (!jerryConnected)
    {
        if (useLocalAsyncPolling)
            showStatusMessage("jerry service not connected on localhost - start jerry in gary4local");
        else
            showStatusMessage("backend not connected - check connection first");
        return;
    }

    if (currentJerryPrompt.trim().isEmpty())
    {
        showStatusMessage("please enter a text prompt for jerry");
        return;
    }

    // 1. Get BPM (from DAW in plugin mode, from UI in standalone mode)
    double bpm = audioProcessor.getCurrentBPM();  // Default from processor

    bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();
    if (isStandalone && jerryUI)
    {
        bpm = jerryUI->getManualBpm();
        DBG("Using manual BPM in standalone: " + juce::String(bpm));
    }
    else
    {
        DBG("Using DAW BPM in plugin: " + juce::String(bpm));
    }

    // 2. Append BPM to prompt (same pattern as Max for Live)
    juce::String fullPrompt = currentJerryPrompt + " " + juce::String((int)bpm) + "bpm";

    // 3. Determine endpoint based on smart loop toggle
    juce::String endpoint;

    if (audioProcessor.getIsUsingLocalhost())
    {
        // Localhost endpoints do NOT have the /audio prefix
        if (useLocalAsyncPolling)
            endpoint = generateAsLoop ? "/generate/loop/async" : "/generate/async";
        else
            endpoint = generateAsLoop ? "/generate/loop" : "/generate";
    }
    else
    {
        // Remote backend requires /audio prefix
        endpoint = generateAsLoop ? "/audio/generate/loop" : "/audio/generate";
    }

    juce::String statusText = generateAsLoop ?
        "cooking a smart loop with jerry..." : "baking with jerry...";

    if (useLocalAsyncPolling)
    {
        setActiveOp(ActiveOp::JerryGenerate);
        isGenerating = true;
        generationProgress = 0;
        lastKnownProgress = 0;
        targetProgress = 0;
        smoothProgressAnimation = false;
        isCurrentlyQueued = false;
        resetStallDetection();

        audioProcessor.clearCurrentSessionId();
        audioProcessor.setRetryAvailable(false);
        updateRetryButtonState();
        updateContinueButtonState();
        updateAllGenerationButtonStates();
        repaint();
    }

    DBG("=== JERRY GENERATION REQUEST ===");
    DBG("Jerry generating with prompt: " + fullPrompt);
    DBG("Endpoint: " + endpoint);
    DBG("Model key: " + currentJerryModelKey);
    DBG("Model is finetune: " + juce::String(currentJerryIsFinetune ? "true" : "false"));
    DBG("Sampler type: " + currentJerrySamplerType);
    DBG("CFG: " + juce::String(currentJerryCfg, 1) + ", Steps: " + juce::String(currentJerrySteps));

    if (generateAsLoop) {
        DBG("Loop Type: " + currentLoopType);
    }

    auto cancelJerryOperation = [this]()
    {
        stopPolling();
        isGenerating = false;
        isCurrentlyQueued = false;
        generationProgress = 0;
        smoothProgressAnimation = false;
        if (jerryUI)
            jerryUI->setGenerateButtonText("generate with jerry");
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
        repaint();
    };

    // Button text feedback and status during processing
    if (jerryUI)
        jerryUI->setGenerateButtonText("generating");
    showStatusMessage(statusText, 2000);

    juce::String requestId;
    if (useLocalAsyncPolling)
        requestId = juce::Uuid().toString();

    // Create HTTP request in background thread
    juce::Thread::launch([this, fullPrompt, endpoint, useLocalAsyncPolling, requestId, cancelJerryOperation]() {
        auto startTime = juce::Time::getCurrentTime();

        juce::DynamicObject::Ptr jsonRequest = new juce::DynamicObject();
        jsonRequest->setProperty("prompt", fullPrompt);
        jsonRequest->setProperty("steps", currentJerrySteps);
        jsonRequest->setProperty("cfg_scale", currentJerryCfg);
        jsonRequest->setProperty("return_format", "base64");
        jsonRequest->setProperty("seed", -1);

        // NEW: Send model components instead of model_key
        jsonRequest->setProperty("model_type", currentJerryModelType);
        if (currentJerryIsFinetune)
        {
            jsonRequest->setProperty("finetune_repo", currentJerryFinetuneRepo);
            jsonRequest->setProperty("finetune_checkpoint", currentJerryFinetuneCheckpoint);
        }
        jsonRequest->setProperty("sampler_type", currentJerrySamplerType);

        // Add loop-specific parameters if using smart loop
        if (generateAsLoop) {
            jsonRequest->setProperty("loop_type", currentLoopType);
        }

        if (useLocalAsyncPolling)
            jsonRequest->setProperty("request_id", requestId);

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
                .withConnectionTimeoutMs(useLocalAsyncPolling ? 15000 : 30000)
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
        juce::MessageManager::callAsync([this, responseText, statusCode, startTime, useLocalAsyncPolling, requestId, cancelJerryOperation]() {
           

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
                        if (useLocalAsyncPolling)
                        {
                            juce::String sessionId = responseObj->getProperty("session_id").toString();
                            if (sessionId.isEmpty())
                                sessionId = requestId;

                            if (sessionId.isEmpty())
                            {
                                showStatusMessage("jerry async request missing session id", 5000);
                                cancelJerryOperation();
                                return;
                            }

                            showStatusMessage("sent to jerry. processing...", 2000);
                            startPollingForResults(sessionId);
                            return;
                        }

                        // Get the base64 audio data
                        auto audioBase64 = responseObj->getProperty("audio_base64").toString();

                        if (audioBase64.isNotEmpty())
                        {
                            if (jerryUI)
                                jerryUI->setGenerateButtonText("generate with jerry");
                            
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
                        if (useLocalAsyncPolling)
                            cancelJerryOperation();
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
                    if (useLocalAsyncPolling)
                        cancelJerryOperation();
                }
            }
            else
            {
                juce::String errorMsg;
                bool shouldCheckHealth = false;
                
                if (statusCode == 0 && audioProcessor.getIsUsingLocalhost())
                {
                    errorMsg = "cannot connect to jerry on localhost - ensure jerry is running in gary4local";
                }
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
                if (useLocalAsyncPolling)
                    cancelJerryOperation();
                
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

            if (!useLocalAsyncPolling && jerryUI)
                jerryUI->setGenerateButtonText("generate with jerry");
            });
        });
}

void Gary4juceAudioProcessorEditor::setTerryAudioSource(bool useRecording)
{
    transformRecording = useRecording;

    // Persist this state in processor so rest of app is aware
    audioProcessor.setTransformRecording(useRecording);

    DBG("Terry audio source set to: " + juce::String(useRecording ? "Recording" : "Output"));
    if (terryUI)
        terryUI->setAudioSourceRecording(useRecording);

    updateTerryEnablementSnapshot();
}

void Gary4juceAudioProcessorEditor::updateTerryEnablementSnapshot()
{
    if (!terryUI)
        return;

    const bool recordingAvailable = savedSamples > 0;
    const bool outputAvailable = hasOutputAudio;
    terryUI->setAudioSourceAvailability(recordingAvailable, outputAvailable);

    const bool hasVariation = currentTerryVariation >= 0;
    const bool hasCustomPrompt = !currentTerryCustomPrompt.trim().isEmpty();

    bool canTransform = false;
    if (transformRecording)
        canTransform = recordingAvailable;
    else
        canTransform = outputAvailable;

    canTransform = canTransform && (hasVariation || hasCustomPrompt);
    if (!audioProcessor.getIsUsingLocalhost())
        canTransform = canTransform && isConnected;

    const bool undoAvailable = audioProcessor.getUndoTransformAvailable() &&
        !audioProcessor.getCurrentSessionId().isEmpty();

    terryUI->setButtonsEnabled(canTransform, isGenerating, undoAvailable);

    if (!isGenerating)
    {
        terryUI->setTransformButtonText("transform with terry");
        terryUI->setUndoButtonText("undo transform");
    }
}

void Gary4juceAudioProcessorEditor::sendToTerry()
{
    setActiveOp(ActiveOp::TerryTransform);

    auto cancelTerryOperation = [this]() {
        isGenerating = false;
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
    };

    // Reset generation state immediately (like Gary)
    isGenerating = true;
    generationProgress = 0;
    lastKnownProgress = 0;
    resetStallDetection();
    targetProgress = 0;
    smoothProgressAnimation = false;


    // Clear any previous session ID since we're starting fresh transform
    audioProcessor.clearCurrentSessionId();
    audioProcessor.setRetryAvailable(false);  // ADD THIS - clear retry for new transform
    updateRetryButtonState();

    updateAllGenerationButtonStates();
    repaint(); // Force immediate UI update

    // Basic validation checks (like Gary and Jerry)
    if (!audioProcessor.getIsUsingLocalhost() && !isConnected)
    {
        showStatusMessage("backend not connected - check connection first");
        cancelTerryOperation();
        return;
    }

    // Check that we have either a variation selected OR a custom prompt
    bool hasVariation = (currentTerryVariation >= 0);
    bool hasCustomPrompt = !currentTerryCustomPrompt.trim().isEmpty();

    if (!hasVariation && !hasCustomPrompt)
    {
        showStatusMessage("please select a variation OR enter a custom prompt");
        cancelTerryOperation();
        updateAllGenerationButtonStates();
        return;
    }

    // Check that we have the selected audio source
    if (transformRecording)
    {
        if (savedSamples <= 0)
        {
            showStatusMessage("no recording available - save your recording first");
            cancelTerryOperation();
            updateAllGenerationButtonStates();
            return;
        }
    }
    else // transforming output
    {
        if (!hasOutputAudio)
        {
            showStatusMessage("no output audio available - generate with gary or jerry first");
            cancelTerryOperation();
            updateAllGenerationButtonStates();
            return;
        }
    }

    // Get the variation names array (matches your backend list)
    const auto& variationNames = terryVariationNames.isEmpty() ? getTerryVariationNames() : terryVariationNames;

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
        showStatusMessage("audio file not found - " + audioFile.getFileName());
        cancelTerryOperation();
        updateAllGenerationButtonStates();
        return;
    }

    // Read and encode audio file (same as Gary)
    juce::MemoryBlock audioData;
    if (!audioFile.loadFileAsData(audioData))
    {
        showStatusMessage("failed to read audio file");
        cancelTerryOperation();
        updateAllGenerationButtonStates();
        return;
    }

    if (audioData.getSize() == 0)
    {
        showStatusMessage("audio file is empty");
        cancelTerryOperation();
        updateAllGenerationButtonStates();
        return;
    }

    auto base64Audio = juce::Base64::toBase64(audioData.getData(), audioData.getSize());

    // Debug: Log audio data size
    DBG("Terry audio file size: " + juce::String(audioData.getSize()) + " bytes");
    DBG("Terry base64 length: " + juce::String(base64Audio.length()) + " chars");
    DBG("Terry flowstep: " + juce::String(currentTerryFlowstep, 3));
    DBG("Terry solver: " + juce::String(useMidpointSolver ? "midpoint" : "euler"));

    // Button text feedback and status during processing
    if (terryUI)
        terryUI->setTransformButtonText("transforming...");
    showStatusMessage("sending audio to terry for transformation...");

    // Create HTTP request in background thread (same pattern as Gary and Jerry)
    juce::Thread::launch([this, base64Audio, variationNames, hasVariation, hasCustomPrompt, cancelTerryOperation]() {
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
        juce::MessageManager::callAsync([this, responseText, statusCode, startTime, cancelTerryOperation]() {
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
                        updateTerryEnablementSnapshot();
                    }
                    else
                    {
                        juce::String error = responseObj->getProperty("error").toString();
                        showStatusMessage("terry error: " + error, 5000);
                        DBG("Terry server error: " + error);

                        if (terryUI)
                            terryUI->setTransformButtonText("transform with terry");
                        cancelTerryOperation();
                    }
                }
                else
                {
                    showStatusMessage("invalid JSON response from terry", 4000);
                    DBG("Failed to parse Terry JSON response: " + responseText.substring(0, 100));

                    if (terryUI)
                        terryUI->setTransformButtonText("transform with terry");
                    cancelTerryOperation();
                }
            }
            else
            {
                juce::String errorMsg;
                bool shouldCheckHealth = false;
                
                if (statusCode == 0 && audioProcessor.getIsUsingLocalhost())
                {
                    errorMsg = "cannot connect to terry on localhost - ensure terry is running in gary4local";
                }
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

                if (terryUI)
                    terryUI->setTransformButtonText("transform with terry");

                cancelTerryOperation();
            }

            // Re-enable transform button (same pattern as Gary/Jerry)
            updateTerryEnablementSnapshot();

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
    updateTerryEnablementSnapshot();
    if (terryUI)
        terryUI->setUndoButtonText("undoing...");

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
                            audioProcessor.setRetryAvailable(false);          // ADD THIS
                            updateTerryEnablementSnapshot();
                            if (terryUI)
                                terryUI->setUndoButtonText("undo transform");
                            updateRetryButtonState();
                        }
                        else
                        {
                            showStatusMessage("undo completed but no audio data received", 3000);
                            DBG("Terry undo success but missing audio data");
                            // Re-enable undo button since this is an error case
                            audioProcessor.setUndoTransformAvailable(true);
                            updateTerryEnablementSnapshot();
                            if (terryUI)
                                terryUI->setUndoButtonText("undo transform");
                        }
                    }
                    else
                    {
                        auto error = responseObj->getProperty("error").toString();
                        showStatusMessage("undo failed: " + error, 4000);
                        DBG("Terry undo server error: " + error);
                        audioProcessor.setUndoTransformAvailable(true);
                        updateTerryEnablementSnapshot();
                        if (terryUI)
                            terryUI->setUndoButtonText("undo transform");
                    }
                }
                else
                {
                    showStatusMessage("invalid undo response format", 3000);
                    DBG("Failed to parse Terry undo JSON response");
                    audioProcessor.setUndoTransformAvailable(true);
                    updateTerryEnablementSnapshot();
                    if (terryUI)
                        terryUI->setUndoButtonText("undo transform");
                }
            }
            else
            {
                juce::String errorMsg;
                if (statusCode == 0 && audioProcessor.getIsUsingLocalhost())
                {
                    errorMsg = "cannot connect for undo on localhost - ensure terry is running in gary4local";
                }
                else if (statusCode == 0)
                    errorMsg = "failed to connect for undo on remote backend";
                else if (statusCode >= 400)
                    errorMsg = "undo server error (HTTP " + juce::String(statusCode) + ")";
                else
                    errorMsg = "empty undo response";

                showStatusMessage(errorMsg, 4000);
                DBG("Terry undo request failed: " + errorMsg);
                audioProcessor.setUndoTransformAvailable(true);
                updateTerryEnablementSnapshot();
                if (terryUI)
                    terryUI->setUndoButtonText("undo transform");
            }
            });
        });
}




void Gary4juceAudioProcessorEditor::checkDariusHealth()
{
    if (dariusBackendUrl.trim().isEmpty())
    {
        showStatusMessage("enter backend url first");
        if (dariusUI)
            dariusUI->setHealthCheckInProgress(false);
        return;
    }

    if (dariusUI)
    {
        dariusUI->setHealthCheckInProgress(true);
        dariusUI->setConnectionStatus("checking connection...", juce::Colours::yellow);
    }

    // Create health check request in background thread
    juce::Thread::launch([this]() {
        juce::String healthUrl = dariusBackendUrl.trim();
        if (!healthUrl.endsWith("/"))
            healthUrl += "/";
        healthUrl += "health";

        juce::URL url(healthUrl);

        std::unique_ptr<juce::InputStream> stream(url.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(10000)  // 10 second timeout
        ));

        juce::String responseText;
        bool connectionSucceeded = false;

        if (stream != nullptr)
        {
            responseText = stream->readEntireStreamAsString();
            connectionSucceeded = !responseText.isEmpty();
        }

        // Handle response on main thread
        juce::MessageManager::callAsync([this, responseText, connectionSucceeded]() {
            handleDariusHealthResponse(responseText, connectionSucceeded);
            });
        });
}

void Gary4juceAudioProcessorEditor::handleDariusHealthResponse(const juce::String& response, bool connectionSucceeded)
{
    if (dariusUI)
        dariusUI->setHealthCheckInProgress(false);

    auto clearModelStatus = [this]()
    {
        if (dariusUI)
            dariusUI->setModelStatus({}, {}, {}, false, false);
    };

    if (!connectionSucceeded)
    {
        dariusConnected = false;
        updateDariusModelControlsEnabled();
        clearModelStatus();
        if (dariusUI)
            dariusUI->setConnectionStatus("connection failed", juce::Colours::red);
        showStatusMessage("failed to connect to darius backend");
        return;
    }

    juce::var parsed;
    try { parsed = juce::JSON::parse(response); }
    catch (...) {
        dariusConnected = false;
        updateDariusModelControlsEnabled();
        clearModelStatus();
        if (dariusUI)
            dariusUI->setConnectionStatus("invalid health response", juce::Colours::red);
        showStatusMessage("invalid health response");
        return;
    }

    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
    {
        dariusConnected = false;
        updateDariusModelControlsEnabled();
        clearModelStatus();
        if (dariusUI)
            dariusUI->setConnectionStatus("unexpected health format", juce::Colours::red);
        showStatusMessage("unexpected health format");
        return;
    }

    juce::String status = obj->getProperty("status").toString();
    const bool ok = (bool)obj->getProperty("ok");

    if (status == "template_mode")
    {
        dariusConnected = false;
        updateDariusModelControlsEnabled();
        clearModelStatus();
        if (dariusUI)
            dariusUI->setConnectionStatus("not ready: template space", juce::Colours::orange);
        showStatusMessage("not ready: this space is a GPU template. duplicate it and select an L40s/A100-class runtime to use the API.", 8000);
    }
    else if (status == "gpu_unavailable")
    {
        dariusConnected = false;
        updateDariusModelControlsEnabled();
        clearModelStatus();
        if (dariusUI)
            dariusUI->setConnectionStatus("gpu not available", juce::Colours::red);
        showStatusMessage("GPU not visible - select a GPU runtime");
    }
    else if (ok && (status == "ready" || status == "initializing"))
    {
        dariusConnected = true;
        updateDariusModelControlsEnabled();
        const bool warmed = (bool)obj->getProperty("warmed");
        if (dariusUI)
            dariusUI->setConnectionStatus(warmed ? "ready" : "initializing", juce::Colours::green);
        showStatusMessage(warmed ? "darius backend ready" : "darius backend initializing");
        clearDariusSteeringAssets();
        fetchDariusConfig();
    }
    else
    {
        dariusConnected = false;
        updateDariusModelControlsEnabled();
        clearModelStatus();
        if (dariusUI)
            dariusUI->setConnectionStatus("unknown status: " + status, juce::Colours::red);
        showStatusMessage("darius backend reported: " + status);
    }
}




void Gary4juceAudioProcessorEditor::fetchDariusConfig()
{
    if (dariusBackendUrl.trim().isEmpty())
    {
        showStatusMessage("enter backend url first");
        return;
    }

    // Launch background fetch (same pattern as health)
    juce::Thread::launch([this]()
        {
            // Build URL exactly like checkDariusHealth(), but for /model/config
            juce::String base = dariusBackendUrl.trim();
            if (!base.endsWith("/"))
                base += "/";
            const juce::String full = base + "model/config";

            juce::URL url(full);

            std::unique_ptr<juce::InputStream> stream(url.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(10000) // 10s like health
            ));

            juce::String responseText;
            int statusCode = 0;

            if (auto* p = dynamic_cast<juce::WebInputStream*>(stream.get()))
            {
                // If createInputStream returns WebInputStream, we can read status
                statusCode = p->getStatusCode();
            }

            if (stream != nullptr)
                responseText = stream->readEntireStreamAsString();

            // Bounce back to main thread
            juce::MessageManager::callAsync([this, responseText, statusCode]()
                {
                    handleDariusConfigResponse(responseText, statusCode);
                });
        });
}

void Gary4juceAudioProcessorEditor::handleDariusConfigResponse(const juce::String& responseText, int statusCode)
{
    if (responseText.isEmpty())
    {
        juce::String msg;
        if (statusCode == 0) msg = "failed to connect to /model/config";
        else if (statusCode >= 400) msg = "server error (HTTP " + juce::String(statusCode) + ") for /model/config";
        else msg = "empty response from /model/config";
        DBG(msg);
        showStatusMessage(msg, 4000);
        return;
    }

    // Parse JSON safely
    juce::var parsed;
    try
    {
        parsed = juce::JSON::parse(responseText);
    }
    catch (...)
    {
        showStatusMessage("invalid /model/config payload", 4000);
        DBG("Failed to parse /model/config JSON");
        return;
    }

    if (!parsed.isObject())
    {
        showStatusMessage("unexpected /model/config format", 4000);
        DBG("Config JSON is not an object");
        return;
    }

    // Keep it simple for now: stash & log some headline fields
    lastDariusConfig = parsed;
    updateDariusModelConfigUI();

    auto* obj = parsed.getDynamicObject();
    const auto size = obj->getProperty("size").toString();
    const auto repo = obj->getProperty("repo").toString();
    const auto selectedStep = obj->getProperty("selected_step").toString();
    const bool loaded = (bool)obj->getProperty("loaded");
    const bool warmed = (bool)obj->getProperty("warmup_done");

    DBG("[/model/config] size=" + size +
        " repo=" + (repo.isEmpty() ? "-" : repo) +
        " step=" + (selectedStep.isEmpty() ? "-" : selectedStep) +
        " loaded=" + juce::String(loaded ? "true" : "false") +
        " warmup=" + juce::String(warmed ? "true" : "false"));

    // Friendly, short status ping so we can see something in the UI for now
    showStatusMessage("config: " + size + (warmed ? " (warm)" : ""), 2500);

    fetchDariusAssetsStatus();
}

void Gary4juceAudioProcessorEditor::clearDariusSteeringAssets()
{
    dariusAssetsMeanAvailable = false;
    dariusAssetsCentroidCount = 0;
    dariusCentroidWeights.clear();

    if (dariusUI)
        dariusUI->setSteeringAssets(false, 0, {});
}

void Gary4juceAudioProcessorEditor::fetchDariusCheckpoints(const juce::String& repo, const juce::String& revision)
{
    if (dariusBackendUrl.trim().isEmpty())
    {
        showStatusMessage("enter backend url first");
        return;
    }

    juce::Thread::launch([this, repo, revision]()
        {
            juce::String base = dariusBackendUrl.trim();
            if (!base.endsWith("/"))
                base += "/";
            const juce::String endpoint = base + "model/checkpoints";

            juce::URL url(endpoint);
            // Add query parameters like the Swift service (repo_id, revision)
            url = url.withParameter("repo_id", repo)
                .withParameter("revision", revision);

            std::unique_ptr<juce::InputStream> stream(url.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(10000) // same 10s as health
            ));

            juce::String responseText;
            int statusCode = 0;
            if (auto* web = dynamic_cast<juce::WebInputStream*>(stream.get()))
                statusCode = web->getStatusCode();

            if (stream != nullptr)
                responseText = stream->readEntireStreamAsString();

            juce::MessageManager::callAsync([this, responseText, statusCode]()
                {
                    handleDariusCheckpointsResponse(responseText, statusCode);
                });
        });
}

void Gary4juceAudioProcessorEditor::handleDariusCheckpointsResponse(const juce::String& responseText, int statusCode)
{
    if (responseText.isEmpty())
    {
        const juce::String msg = (statusCode == 0) ? "failed to fetch checkpoints" :
            (statusCode >= 400) ? "checkpoints error (HTTP " + juce::String(statusCode) + ")" :
            "empty checkpoints response";
        DBG(msg);
        showStatusMessage(msg, 3500);
        dariusIsFetchingCheckpoints = false;
        updateDariusModelControlsEnabled();
        return;
    }

    juce::var parsed;
    try { parsed = juce::JSON::parse(responseText); }
    catch (...)
    {
        showStatusMessage("invalid checkpoints payload", 3000);
        dariusIsFetchingCheckpoints = false;
        updateDariusModelControlsEnabled();
        return;
    }

    if (auto* obj = parsed.getDynamicObject())
    {
        dariusCheckpointSteps.clearQuick();
        if (auto stepsVar = obj->getProperty("steps"); stepsVar.isArray())
        {
            auto* arr = stepsVar.getArray();
            for (auto& v : *arr)
                dariusCheckpointSteps.add((int)v);
        }
        int latest = -1;
        if (obj->hasProperty("latest"))
            latest = (int)obj->getProperty("latest");

        dariusLatestCheckpoint = latest;

        dariusIsFetchingCheckpoints = false;
        updateDariusModelControlsEnabled();

        if (dariusUI)
        {
            dariusUI->setCheckpointSteps(dariusCheckpointSteps);
            dariusUI->setSelectedCheckpointStep(dariusSelectedStepStr);
        }

        DBG("[/model/checkpoints] steps=" + juce::String(dariusCheckpointSteps.size())
            + " latest=" + juce::String(dariusLatestCheckpoint));
        showStatusMessage("checkpoints: " + juce::String(dariusCheckpointSteps.size()), 2200);
    }
    else
    {
        showStatusMessage("unexpected checkpoints format", 3000);
        dariusIsFetchingCheckpoints = false;
        updateDariusModelControlsEnabled();
    }
}


void Gary4juceAudioProcessorEditor::updateDariusModelConfigUI()
{
    if (!dariusUI || !lastDariusConfig.isObject())
        return;

    auto* obj = lastDariusConfig.getDynamicObject();
    if (obj == nullptr)
        return;

    const auto size = obj->getProperty("size").toString();
    const auto repo = obj->getProperty("repo").toString();
    const auto step = obj->getProperty("selected_step").toString();
    const bool loaded = (bool)obj->getProperty("loaded");
    const bool warm = (bool)obj->getProperty("warmup_done");

    if (warm && (dariusIsWarming || dariusIsApplying))
    {
        dariusIsApplying = false;
        dariusIsWarming = false;
        dariusUI->stopWarmDots();
        updateDariusModelControlsEnabled();
        showStatusMessage("Model ready (prewarmed)", 2200);
    }

    dariusUI->setModelStatus(size, repo, step, loaded, warm);
}


// MODEL SELECT

void Gary4juceAudioProcessorEditor::startWarmDots()
{
    if (dariusUI)
        dariusUI->startWarmDots();

}


void Gary4juceAudioProcessorEditor::stopWarmDots()
{
    if (dariusUI)
        dariusUI->stopWarmDots();

}



void Gary4juceAudioProcessorEditor::syncDariusRepoFromField()
{
    if (!dariusUI)
        return;

    auto repo = dariusUI->getFinetuneRepo().trim();
    if (repo.isEmpty())
        repo = "thepatch/magenta-ft";

    dariusFinetuneRepo = repo;
    dariusUI->setFinetuneRepo(dariusFinetuneRepo);
}

void Gary4juceAudioProcessorEditor::beginDariusApplyAndWarm()
{
    if (!dariusConnected || !dariusUI)
        return;

    clearDariusSteeringAssets();

    dariusIsApplying = true;
    dariusUI->startWarmDots();
    updateDariusModelControlsEnabled();

    postDariusSelect(makeDariusSelectApplyRequest());
}


void Gary4juceAudioProcessorEditor::postDariusSelect(const juce::var& requestObj)
{
    if (!requestObj.isObject())
    {
        showStatusMessage("invalid select request", 2500);
        return;
    }
    if (dariusBackendUrl.trim().isEmpty())
    {
        showStatusMessage("enter backend url first");
        return;
    }

    juce::Thread::launch([this, requestObj]()
        {
            juce::String base = dariusBackendUrl.trim();
            if (!base.endsWith("/"))
                base += "/";
            const juce::String endpoint = base + "model/select";

            // Build JSON body (follow your existing POST pattern)
            const juce::String jsonString = juce::JSON::toString(requestObj);

            juce::URL url(endpoint);
            juce::URL postUrl = url.withPOSTData(jsonString);

            juce::String responseText;
            int statusCode = 0;

            try
            {
                auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(180000)
                    .withExtraHeaders("Content-Type: application/json");

                auto stream = postUrl.createInputStream(options);

                if (auto* web = dynamic_cast<juce::WebInputStream*>(stream.get()))
                    statusCode = web->getStatusCode();

                if (stream != nullptr)
                    responseText = stream->readEntireStreamAsString();
            }
            catch (const std::exception& e)
            {
                DBG("select exception: " + juce::String(e.what()));
            }

            juce::MessageManager::callAsync([this, responseText, statusCode]()
                {
                    handleDariusSelectResponse(responseText, statusCode);
                });
        });
}

juce::var Gary4juceAudioProcessorEditor::makeDariusSelectApplyRequest()
{
    // Determine size target. If you already added a size picker, read it; otherwise default.
    juce::String size = "large";
    if (lastDariusConfig.isObject())
        if (auto* o = lastDariusConfig.getDynamicObject())
            if (!o->getProperty("size").toString().isEmpty())
                size = o->getProperty("size").toString();

    // Step logic: base model => "none"; finetune => dariusSelectedStepStr or "latest"
    juce::String stepStr;
    if (dariusUseBaseModel) stepStr = "none";
    else stepStr = (dariusSelectedStepStr.isNotEmpty() ? dariusSelectedStepStr : "latest");

    // Ensure the repo string is synced from the field
    syncDariusRepoFromField();
    const juce::String repo = dariusUseBaseModel ? juce::String() : dariusFinetuneRepo;
    const juce::String revision = dariusUseBaseModel ? juce::String() : dariusFinetuneRevision;

    auto o = new juce::DynamicObject();
    o->setProperty("size", size);
    if (!dariusUseBaseModel) {
        o->setProperty("repo_id", repo);
        o->setProperty("revision", revision);
        o->setProperty("assets_repo_id", repo);
    }
    o->setProperty("step", stepStr);
    o->setProperty("sync_assets", !dariusUseBaseModel); // match Swift default; adjust if you add a toggle
    o->setProperty("prewarm", true);                    // ← key UX: run synchronous warmup server-side
    o->setProperty("stop_active", true);
    o->setProperty("dry_run", false);

    return juce::var(o);
}


void Gary4juceAudioProcessorEditor::handleDariusSelectResponse(const juce::String& responseText, int statusCode)
{
    if (responseText.isEmpty())
    {
        // Likely a client timeout while server is still warming. Keep UX optimistic and poll.
        DBG("select: empty/timeout response; continuing to poll /model/config");
        // Mark POST phase done, enter warm polling
        dariusIsApplying = false;
        dariusIsWarming = true;
        updateDariusModelControlsEnabled();
        startDariusWarmPolling(0);
        return;
    }

    juce::var parsed;
    try { parsed = juce::JSON::parse(responseText); }
    catch (...) { showStatusMessage("invalid select response", 3500); return; }

    lastDariusSelectResp = parsed;

    bool ok = false, dryRun = false, warmDone = false;
    if (auto* obj = parsed.getDynamicObject())
    {
        ok = (bool)obj->getProperty("ok");
        dryRun = (bool)obj->getProperty("dry_run");
        warmDone = (bool)obj->getProperty("warmup_done"); // server returns this when prewarm=true
    }

    // Old UI messages (keep if you like)
    if (!ok) { showStatusMessage(dryRun ? "validation failed" : "select failed", 3500); }

    // Refresh snapshot
    fetchDariusConfig();
    fetchDariusAssetsStatus();

    // NEW: finish-or-poll flow
    onDariusApplyFinished(ok, warmDone);
}

void Gary4juceAudioProcessorEditor::onDariusApplyFinished(bool ok, bool warmAlready)
{
    // We are done with the POST
    dariusIsApplying = false;

    if (!ok)
    {
        // Fail → spinner off, controls back
        dariusIsWarming = false;
        stopWarmDots();
        updateDariusModelControlsEnabled();
        return;
    }

    // If server reports warmed (prewarm finished), we're done
    if (warmAlready)
    {
        dariusIsWarming = false;
        stopWarmDots();
        updateDariusModelControlsEnabled();
        showStatusMessage("Model ready (prewarmed)", 2200);
        return;
    }

    // Otherwise, poll /model/config until warmup_done==true
    dariusIsWarming = true;
    updateDariusModelControlsEnabled();
    startDariusWarmPolling(0);
}

void Gary4juceAudioProcessorEditor::startDariusWarmPolling(int attempt)
{
    // Re-fetch config; the select handler already did one fetch, but we keep polling.
    fetchDariusConfig();

    // Inspect the latest snapshot after a short delay (let network return)
    juce::Timer::callAfterDelay(300, [this, attempt]()
        {
            bool warmed = false;
            if (lastDariusConfig.isObject())
            {
                if (auto* o = lastDariusConfig.getDynamicObject())
                    warmed = (bool)o->getProperty("warmup_done");
            }

            if (warmed)
            {
                dariusIsWarming = false;
                stopWarmDots();
                updateDariusModelControlsEnabled();
                showStatusMessage("Model ready (prewarmed)", 2200);
                updateDariusModelConfigUI();
                return;
            }

            // Keep polling up to ~20 seconds (e.g., 40 * 500ms)
            if (attempt >= 40)
            {
                // Give up gracefully but leave UI enabled
                dariusIsWarming = false;
                stopWarmDots();
                updateDariusModelControlsEnabled();
                showStatusMessage("Still warming... try again or check backend logs", 3000);
                return;
            }

            // Schedule another tick
            juce::Timer::callAfterDelay(500, [this, attempt]() { startDariusWarmPolling(attempt + 1); });
        });
}



void Gary4juceAudioProcessorEditor::updateDariusModelControlsEnabled()
{
    if (!dariusUI)
        return;

    dariusUI->setConnected(dariusConnected);
    dariusUI->setUsingBaseModel(dariusUseBaseModel);
    dariusUI->setIsFetchingCheckpoints(dariusIsFetchingCheckpoints);
    dariusUI->setApplyInProgress(dariusIsApplying);
    dariusUI->setWarmInProgress(dariusIsWarming);
    dariusUI->setSelectedCheckpointStep(dariusSelectedStepStr);
}













// GENERATION STUFF


































// Helper: base directory used by Terry
static juce::File getGaryDir()
{
    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    return documentsDir.getChildFile("gary4juce");
}

// Generation: which file to upload to /generate
juce::String Gary4juceAudioProcessorEditor::getGenAudioFilePath() const
{
    const bool useRec = audioProcessor.getTransformRecording(); // shared Terry flag
    auto garyDir = getGaryDir();
    const auto audioFile = useRec ? garyDir.getChildFile("myBuffer.wav")
                                  : garyDir.getChildFile("myOutput.wav");
    DBG("Darius generate path: " + audioFile.getFullPathName());
    return audioFile.getFullPathName();
}

juce::String Gary4juceAudioProcessorEditor::centroidWeightsCSV() const
{
    juce::StringArray arr;
    if (dariusUI)
    {
        auto weights = dariusUI->getCentroidWeights();
        arr.ensureStorageAllocated((int)weights.size());
        for (double v : weights)
            arr.add(juce::String(v, 4));
        const_cast<Gary4juceAudioProcessorEditor*>(this)->dariusCentroidWeights = weights;
    }
    else
    {
        for (double v : dariusCentroidWeights)
            arr.add(juce::String(v, 4));
    }
    return arr.joinIntoString(",");
}



void Gary4juceAudioProcessorEditor::onClickGenerate()
{
    if (genIsGenerating)
        return;

    if (dariusBackendUrl.trim().isEmpty())
    {
        showStatusMessage("enter backend url first");
        return;
    }

    const juce::File loopFile(getGenAudioFilePath());
    if (!loopFile.existsAsFile())
    {
        showStatusMessage("no loop audio found (record or render first)");
        return;
    }

    genIsGenerating = true;
    if (dariusUI)
        dariusUI->setGenerating(true);

    isGenerating = true;
    generationProgress = 0;
    updateAllGenerationButtonStates();

    postDariusGenerate();
}


juce::URL Gary4juceAudioProcessorEditor::makeGenerateURL(const juce::String& requestId) const
{
    juce::String base = dariusBackendUrl.trim();
    if (!base.endsWith("/")) base += "/";
    juce::URL url(base + "generate");

    const juce::File originalFile(getGenAudioFilePath());
    const double bpm = dariusUI ? dariusUI->getBpm() : audioProcessor.getCurrentBPM();
    const int    beatsPerBar = 4;

    // NEW: trim to whole bars AND ≤ maxSeconds (only writes a copy when needed)
    const double maxSeconds = 9.9; // MRT wants <10s context
    juce::File uploadFile = makeBarAlignedMaxSecondsCopy(originalFile, bpm, beatsPerBar, maxSeconds);

    // Defensive fallback
    if (!uploadFile.existsAsFile() || uploadFile.getSize() <= 0)
        uploadFile = originalFile;

    DBG("Generate upload: " + uploadFile.getFullPathName()
        + " (" + juce::String((int)uploadFile.getSize()) + " bytes)");

    url = url.withFileToUpload("loop_audio", uploadFile, "audio/wav");

    const int bars = dariusUI ? dariusUI->getBars() : 4;
    const juce::String styles = dariusUI ? dariusUI->getStylesCSV() : juce::String();
    const juce::String styleWeights = dariusUI ? dariusUI->getStyleWeightsCSV() : juce::String();
    const double loopInfluence = dariusUI ? dariusUI->getLoopInfluence() : 0.5;
    const double guidance = dariusUI ? dariusUI->getGuidance() : 5.0;
    const double temperature = dariusUI ? dariusUI->getTemperature() : 1.2;
    const int topK = dariusUI ? dariusUI->getTopK() : 40;

    url = url
        .withParameter("bpm", juce::String(bpm, 3))
        .withParameter("bars", juce::String(bars))
        .withParameter("beats_per_bar", "4")
        .withParameter("styles", styles)
        .withParameter("style_weights", styleWeights)
        .withParameter("loop_weight", juce::String(loopInfluence, 3))
        .withParameter("guidance_weight", juce::String(guidance, 3))
        .withParameter("temperature", juce::String(temperature, 3))
        .withParameter("topk", juce::String(topK))
        .withParameter("loudness_mode", "none")
        .withParameter("loudness_headroom_db", "1.0")
        .withParameter("intro_bars_to_drop", "0")
        .withParameter("request_id", requestId);

    if (genAssetsAvailable())
    {
        if (dariusAssetsMeanAvailable && dariusUI)
            url = url.withParameter("mean", juce::String(dariusUI->getMean(), 4));

        if (dariusAssetsCentroidCount > 0 && dariusUI)
            url = url.withParameter("centroid_weights", centroidWeightsCSV());
    }

    return url;
}


void Gary4juceAudioProcessorEditor::postDariusGenerate()
{
    // Create request_id and start polling immediately so UI shows 0%
    const juce::String reqId = juce::Uuid().toString();
    startDariusProgressPoll(reqId);

    // mark UI busy state (reuse your existing flags)
    isGenerating = true;
    genIsGenerating = true;
    setActiveOp(ActiveOp::DariusGenerate);
    generationProgress = 0;
    targetProgress = 0;
    lastKnownProgress = 0;
    smoothProgressAnimation = false;
    if (dariusUI) dariusUI->setGenerating(true);

    // Build the URL with request_id included
    juce::URL url = makeGenerateURL(reqId);

    // Fire the POST on a worker thread
    juce::Thread::launch([this, url]()
        {
            juce::String responseText;
            int statusCode = 0;

            try
            {
                auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                    .withHttpRequestCmd("POST")
                    .withConnectionTimeoutMs(180000);

                auto stream = url.createInputStream(options);

                if (auto* web = dynamic_cast<juce::WebInputStream*>(stream.get()))
                    statusCode = web->getStatusCode();

                if (stream != nullptr)
                    responseText = stream->readEntireStreamAsString();
            }
            catch (const std::exception& e)
            {
                DBG("generate exception: " + juce::String(e.what()));
            }

            juce::MessageManager::callAsync([this, responseText, statusCode]()
                {
                    handleDariusGenerateResponse(responseText, statusCode);
                });
        });
}


void Gary4juceAudioProcessorEditor::handleDariusGenerateResponse(const juce::String& responseText, int statusCode)
{
    auto finish = [this]()
        {
            stopDariusProgressPoll();            // <-- ensure we stop polling

            genIsGenerating = false;
            if (dariusUI) dariusUI->setGenerating(false);

            isGenerating = false;

            // snap to 100 on success; on error we’ll reset below
            // (if you prefer, leave whatever the poller last reported)
            updateAllGenerationButtonStates();
            resized();
            repaint();
        };

    if (responseText.isEmpty())
    {
        showStatusMessage(statusCode == 0 ? "generate: no connection"
            : "generate error (HTTP " + juce::String(statusCode) + ")", 3500);
        generationProgress = 0;             // reset on error
        finish();
        return;
    }

    juce::var parsed;
    try { parsed = juce::JSON::parse(responseText); }
    catch (...) {
        showStatusMessage("invalid generate response", 3000);
        generationProgress = 0;
        finish();
        return;
    }

    juce::String audio64;
    if (auto* o = parsed.getDynamicObject())
        audio64 = o->getProperty("audio_base64").toString();

    if (audio64.isEmpty())
    {
        showStatusMessage("generate failed (no audio)", 3000);
        generationProgress = 0;
        finish();
        return;
    }

    // success
    saveGeneratedAudio(audio64);
    generationProgress = 100;               // snap to full
    showStatusMessage("Generated!", 1500);
    finish();
}


juce::URL Gary4juceAudioProcessorEditor::makeDariusProgressURL(const juce::String& reqId) const
{
    juce::URL u(dariusBackendUrl);                    // from DariusUI
    return u.withNewSubPath("progress")
        .withParameter("request_id", reqId);
}

void Gary4juceAudioProcessorEditor::startDariusProgressPoll(const juce::String& requestId)
{
    dariusProgressRequestId = requestId;
    dariusIsPollingProgress = true;
    dariusProgressPollTick = 0;
    // ensure the main 50ms timer is running (you already startTimer(50) in ctor)
}

void Gary4juceAudioProcessorEditor::stopDariusProgressPoll()
{
    dariusIsPollingProgress = false;
    dariusProgressRequestId.clear();
}

void Gary4juceAudioProcessorEditor::pollDariusProgress()
{
    if (!dariusIsPollingProgress || dariusProgressRequestId.isEmpty())
        return;

    auto url = makeDariusProgressURL(dariusProgressRequestId);

    std::async(std::launch::async, [this, url]()
        {
            std::unique_ptr<juce::InputStream> s(url.createInputStream(false, nullptr, nullptr, {}, 10000));
            if (!s) return;

            auto jsonText = s->readEntireStreamAsString();
            juce::var resp = juce::JSON::parse(jsonText);
            if (!resp.isObject()) return;

            if (auto* obj = resp.getDynamicObject())
            {
                // Use Identifiers and guard missing values
                const juce::var pctVar = obj->getProperty(juce::Identifier("percent"));
                const juce::var stageVar = obj->getProperty(juce::Identifier("stage"));

                const int pct = pctVar.isVoid() ? 0 : (int)pctVar;
                const juce::String stage = stageVar.toString();

                juce::MessageManager::callAsync([this, pct, stage]()
                    {
                        if (!dariusIsPollingProgress) return;

                        // feed your smoothing fields
                        lastKnownProgress = generationProgress;
                        targetProgress = juce::jlimit(0, 100, pct);
                        lastProgressUpdateTime = juce::Time::getCurrentTime().toMilliseconds();
                        smoothProgressAnimation = true;

                        if (stage == "done" || stage == "error" || pct >= 100)
                        {
                            stopDariusProgressPoll();
                            // leave isGenerating changes to the POST response handler
                        }
                    });
            }
        });
}





void Gary4juceAudioProcessorEditor::fetchDariusAssetsStatus()
{
    clearDariusSteeringAssets();

    if (dariusBackendUrl.trim().isEmpty())
        return;

    juce::Thread::launch([this]()
        {
            juce::String base = dariusBackendUrl.trim();
            if (!base.endsWith("/")) base += "/";
            juce::URL url(base + "model/assets/status");

            juce::String responseText;
            int statusCode = 0;

            std::unique_ptr<juce::InputStream> stream(url.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(10000)
            ));
            if (auto* web = dynamic_cast<juce::WebInputStream*>(stream.get()))
                statusCode = web->getStatusCode();
            if (stream) responseText = stream->readEntireStreamAsString();

            juce::MessageManager::callAsync([this, responseText, statusCode]() {
                handleDariusAssetsStatusResponse(responseText, statusCode);
                });
        });
}

void Gary4juceAudioProcessorEditor::handleDariusAssetsStatusResponse(const juce::String& responseText, int statusCode)
{
    if (responseText.isEmpty())
        return;

    juce::var parsed;
    try { parsed = juce::JSON::parse(responseText); }
    catch (...) { return; }

    if (auto* o = parsed.getDynamicObject())
    {
        const bool meanLoaded = (bool)o->getProperty("mean_loaded");
        int centroidCount = 0;
        if (o->hasProperty("centroid_count") && !o->getProperty("centroid_count").isVoid())
            centroidCount = (int)o->getProperty("centroid_count");

        std::vector<double> weights;
        if (auto weightsVar = o->getProperty("centroid_weights"); weightsVar.isArray())
        {
            if (auto* arr = weightsVar.getArray())
            {
                weights.reserve(arr->size());
                for (const auto& v : *arr)
                    weights.push_back((double)v);
            }
        }

        dariusAssetsMeanAvailable = meanLoaded;
        dariusAssetsCentroidCount = juce::jmax(0, centroidCount);

        if (!weights.empty())
            dariusCentroidWeights = weights;
        else
            dariusCentroidWeights.assign((size_t)dariusAssetsCentroidCount, 0.0);

        if ((int)dariusCentroidWeights.size() > dariusAssetsCentroidCount)
            dariusCentroidWeights.resize((size_t)dariusAssetsCentroidCount);
        if ((int)dariusCentroidWeights.size() < dariusAssetsCentroidCount)
            dariusCentroidWeights.resize((size_t)dariusAssetsCentroidCount, 0.0);

        if (dariusUI)
            dariusUI->setSteeringAssets(dariusAssetsMeanAvailable, dariusAssetsCentroidCount, dariusCentroidWeights);
    }
}

































void Gary4juceAudioProcessorEditor::clearRecordingBuffer()
{
    audioProcessor.clearRecordingBuffer();
    savedSamples = audioProcessor.getSavedSamples();  // Will be 0 after clear
    updateRecordingStatus();
}

void Gary4juceAudioProcessorEditor::resetLocalServiceHealthSnapshot()
{
    localGaryOnline = false;
    localTerryOnline = false;
    localJerryOnline = false;
    localOnlineCount = 0;
    localHealthLastPollMs = 0;
    localHealthPollCounter = 0;
    localHealthPollInFlight.store(false);
}

bool Gary4juceAudioProcessorEditor::isLocalServiceOnline(ServiceType service) const
{
    switch (service)
    {
    case ServiceType::Gary: return localGaryOnline;
    case ServiceType::Terry: return localTerryOnline;
    case ServiceType::Jerry: return localJerryOnline;
    }
    return false;
}

Gary4juceAudioProcessorEditor::ServiceType Gary4juceAudioProcessorEditor::getActiveLocalService() const
{
    switch (currentTab)
    {
    case ModelTab::Jerry: return ServiceType::Jerry;
    case ModelTab::Terry: return ServiceType::Terry;
    case ModelTab::Gary:
    case ModelTab::Darius:
    default:
        return ServiceType::Gary;
    }
}

bool Gary4juceAudioProcessorEditor::isActiveLocalServiceOnline() const
{
    if (currentTab == ModelTab::Darius)
        return localOnlineCount > 0;
    return isLocalServiceOnline(getActiveLocalService());
}

juce::String Gary4juceAudioProcessorEditor::getLocalConnectionLineOne() const
{
    if (localOnlineCount <= 0)
        return "disconnected (local)";
    if (isActiveLocalServiceOnline())
        return "connected (local)";
    return "partial (local)";
}

void Gary4juceAudioProcessorEditor::triggerLocalServiceHealthPoll(bool force)
{
    if (!audioProcessor.getIsUsingLocalhost())
        return;

    const auto nowMs = juce::Time::getCurrentTime().toMilliseconds();
    if (!force && nowMs - localHealthLastPollMs < 2000)
        return;
    if (localHealthPollInFlight.exchange(true))
        return;
    localHealthLastPollMs = nowMs;

    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis(this);
    juce::Thread::launch([safeThis]() {
        auto probeHealth = [](const juce::String& urlText) -> bool
        {
            try
            {
                juce::URL healthUrl(urlText);
                int statusCode = 0;
                auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(1500)
                    .withStatusCode(&statusCode)
                    .withExtraHeaders("Accept: application/json");
                auto stream = healthUrl.createInputStream(options);
                if (stream == nullptr || statusCode >= 400)
                    return false;
                auto responseText = stream->readEntireStreamAsString();
                return localhostHealthResponseLooksOnline(responseText);
            }
            catch (...)
            {
                return false;
            }
        };

        const bool garyOnline = probeHealth("http://127.0.0.1:8000/health");
        const bool terryOnline = probeHealth("http://127.0.0.1:8002/health");
        const bool jerryOnline = probeHealth("http://127.0.0.1:8005/health");

        juce::MessageManager::callAsync([safeThis, garyOnline, terryOnline, jerryOnline]() {
            if (safeThis == nullptr)
                return;

            safeThis->localHealthPollInFlight.store(false);

            const bool changed =
                safeThis->localGaryOnline != garyOnline ||
                safeThis->localTerryOnline != terryOnline ||
                safeThis->localJerryOnline != jerryOnline;

            safeThis->localGaryOnline = garyOnline;
            safeThis->localTerryOnline = terryOnline;
            safeThis->localJerryOnline = jerryOnline;
            safeThis->localOnlineCount = (garyOnline ? 1 : 0) + (terryOnline ? 1 : 0) + (jerryOnline ? 1 : 0);

            safeThis->updateAllGenerationButtonStates();

            if (!jerryOnline && safeThis->jerryUI)
                safeThis->jerryUI->setLoadingModel(false);

            // Keep Jerry model dropdown fresh while Jerry tab is active on localhost,
            // so external model changes (e.g. gary4local "use model") appear immediately.
            if (jerryOnline && safeThis->currentTab == ModelTab::Jerry)
                safeThis->fetchJerryAvailableModels();

            if (changed)
                safeThis->repaint();
        });
    });
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

        // Fetch Gary models when connection is established
        if (connected)
        {
            fetchGaryAvailableModels();
            if (currentTab == ModelTab::Jerry)
                fetchJerryAvailableModels();
        }

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

    // Update connection status display
    updateConnectionStatus(false);
    resetLocalServiceHealthSnapshot();
    if (isUsingLocalhost)
        triggerLocalServiceHealthPoll(true);

    // Clear any loading state when switching backends
    if (jerryUI)
    {
        jerryUI->setLoadingModel(false);
        jerryUI->setUsingLocalhost(isUsingLocalhost);
    }

    if (garyUI)
    {
        garyUI->setUsingLocalhost(isUsingLocalhost);
        if (isUsingLocalhost)
            applyGaryQuantizationDefaultForCurrentModel();
    }

    DBG("Switched to " + audioProcessor.getCurrentBackendType() + " backend");

    // Re-fetch Gary models for the new backend
    // (Will be fetched when connection is re-established)
}

void Gary4juceAudioProcessorEditor::updateBackendToggleButton()
{
    if (isUsingLocalhost)
    {
        backendToggleButton.setButtonText("local");
        backendToggleButton.setButtonStyle(CustomButton::ButtonStyle::Gary); // Red style for local
        backendToggleButton.setTooltip("using localhost backend (ports 8000/8002/8005) - click to switch to remote");
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
        totalAudioDuration = 0.0;
        currentAudioSampleRate = 44100.0;
        updateGaryButtonStates(!isGenerating);
        return;
    }

    // Read the audio file for waveform display
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(outputAudioFile));

    if (reader != nullptr)
    {
        // Load into editor buffer for waveform visualization
        outputAudioBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&outputAudioBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

        // Store properties
        totalAudioDuration = (double)reader->lengthInSamples / reader->sampleRate;
        currentAudioSampleRate = reader->sampleRate;

        // Load into processor for host audio playback
        audioProcessor.loadOutputAudioForPlayback(outputAudioFile);

        hasOutputAudio = true;
        playOutputButton.setEnabled(true);
        stopOutputButton.setEnabled(true);
        clearOutputButton.setEnabled(true);
        cropButton.setEnabled(true);

        DBG("Loaded output audio: " + juce::String(reader->lengthInSamples) + " samples, " +
            juce::String(reader->numChannels) + " channels, " +
            juce::String(totalAudioDuration, 2) + " seconds at " +
            juce::String(reader->sampleRate) + " Hz");

        updateGaryButtonStates(!isGenerating);
    }
    else
    {
        DBG("Failed to read output audio file");
        hasOutputAudio = false;
        playOutputButton.setEnabled(false);
        clearOutputButton.setEnabled(false);
        cropButton.setEnabled(false);
        totalAudioDuration = 0.0;
        currentAudioSampleRate = 44100.0;
        updateGaryButtonStates(!isGenerating);
    }
}

juce::String Gary4juceAudioProcessorEditor::currentOperationVerb() const
{
    switch (getActiveOp())
    {
        case ActiveOp::TerryTransform:
            return "transforming";
        case ActiveOp::GaryGenerate:
        case ActiveOp::GaryContinue:
        case ActiveOp::GaryRetry:
        case ActiveOp::JerryGenerate:
            return "cooking";
        default:
            return "processing";
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
            switch (getActiveOp())
            {
                case ActiveOp::TerryTransform:
                    displayText = "queued for transform";
                    break;
                case ActiveOp::GaryGenerate:
                case ActiveOp::GaryContinue:
                case ActiveOp::GaryRetry:
                case ActiveOp::JerryGenerate:
                    displayText = "queued for generation";
                    break;
                default:
                    displayText = "queued for processing";
                    break;
            }
        }
        else
        {
            // Show traditional progress when actually processing
            switch (getActiveOp())
            {
                case ActiveOp::TerryTransform:
                    if (generationProgress <= 0)
                        displayText = "transforming...";
                    else
                        displayText = "transforming: " + juce::String(generationProgress) + "%";
                    break;
                case ActiveOp::GaryGenerate:
                case ActiveOp::GaryContinue:
                case ActiveOp::GaryRetry:
                case ActiveOp::JerryGenerate:
                    if (generationProgress <= 0)
                        displayText = "cooking...";
                    else
                        displayText = "cooking: " + juce::String(generationProgress) + "%";
                    break;
                default:
                    if (generationProgress <= 0)
                        displayText = "processing...";
                    else
                        displayText = "processing: " + juce::String(generationProgress) + "%";
                    break;
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

    // Get current state from processor
    bool processorIsPlaying = audioProcessor.getIsPlayingOutput();
    bool processorIsPaused = audioProcessor.getIsPausedOutput();

    if (processorIsPlaying)
    {
        // Currently playing -> pause it
        audioProcessor.pauseOutputPlayback();
        isPausedOutput = true;
        isPlayingOutput = false;
        pausedPosition = audioProcessor.getOutputPlaybackPosition();
        currentPlaybackPosition = pausedPosition;
        updatePlayButtonIcon();
        showStatusMessage("paused", 1500);
        repaint();
    }
    else if (processorIsPaused || isPausedOutput || currentPlaybackPosition > 0.0)
    {
        // Paused or has seek position -> resume from that position
        audioProcessor.startOutputPlayback(currentPlaybackPosition);
        isPlayingOutput = true;
        isPausedOutput = false;
        updatePlayButtonIcon();
        showStatusMessage("resumed from " + juce::String(currentPlaybackPosition, 1) + "s", 1500);
    }
    else
    {
        // Fresh start from beginning
        audioProcessor.startOutputPlayback(0.0);
        isPlayingOutput = true;
        isPausedOutput = false;
        currentPlaybackPosition = 0.0;
        updatePlayButtonIcon();
        showStatusMessage("playing output...", 2000);
    }
}

// Updated stopOutputPlayback() - distinguish between pause and full stop
void Gary4juceAudioProcessorEditor::stopOutputPlayback()
{
    audioProcessor.stopOutputPlayback();

    isPlayingOutput = false;
    isPausedOutput = false;
    currentPlaybackPosition = 0.0;
    pausedPosition = 0.0;

    updatePlayButtonIcon();
    repaint();

    DBG("Stopped output playback");
}

// New function: Full stop (different from pause)
void Gary4juceAudioProcessorEditor::fullStopOutputPlayback()
{
    stopOutputPlayback(); // This does everything we need for a full stop
}

// Updated checkPlaybackStatus() - full stop when audio finishes naturally
void Gary4juceAudioProcessorEditor::checkPlaybackStatus()
{
    // Update position from processor
    if (isPlayingOutput)
    {
        currentPlaybackPosition = audioProcessor.getOutputPlaybackPosition();

        // Check if processor has stopped (reached end)
        if (!audioProcessor.getIsPlayingOutput())
        {
            isPlayingOutput = false;
            isPausedOutput = false;
            currentPlaybackPosition = 0.0;
            updatePlayButtonIcon();
            showStatusMessage("playback finished", 1500);
        }

        repaint();
    }
}

void Gary4juceAudioProcessorEditor::clearOutputAudio()
{
    hasOutputAudio = false;
    outputAudioBuffer.clear();
    playOutputButton.setEnabled(false);
   clearOutputButton.setEnabled(false);
    cropButton.setEnabled(false);
    updateGaryButtonStates(!isGenerating);

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

void Gary4juceAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    // Check if double-click is on the recorded audio waveform area
    if (waveformArea.contains(event.getPosition()))
    {
        // Only proceed if we have a previously dragged file that still exists
        if (lastDraggedAudioFile.existsAsFile())
        {
            DBG("Double-click on waveform - reopening selection dialog for: " +
                lastDraggedAudioFile.getFileName());
            loadAudioFileIntoBuffer(lastDraggedAudioFile);
        }
        else
        {
            showStatusMessage("drag an audio file here first", 2000);
        }
        return;
    }

    // Call parent implementation for other areas
    juce::Component::mouseDoubleClick(event);
}

// ============================================================================
// FileDragAndDropTarget Interface (for dragging audio INTO recording buffer)
// ============================================================================

bool Gary4juceAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    // Only accept single audio files
    if (files.size() != 1)
        return false;

    juce::File file(files[0]);

    // Accept common audio formats
    juce::String extension = file.getFileExtension().toLowerCase();
    return extension == ".wav" || extension == ".mp3" ||
           extension == ".aiff" || extension == ".flac" ||
           extension == ".ogg" || extension == ".m4a";
}

void Gary4juceAudioProcessorEditor::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    // Only show hover if dragging over the recording waveform area
    if (waveformArea.contains(x, y))
    {
        isDragHoveringInput = true;
        repaint();
    }
}

void Gary4juceAudioProcessorEditor::fileDragExit(const juce::StringArray& files)
{
    isDragHoveringInput = false;
    repaint();
}

void Gary4juceAudioProcessorEditor::filesDropped(const juce::StringArray& files, int x, int y)
{
    isDragHoveringInput = false;
    repaint();

    // Only accept drops on the recording waveform area
    if (!waveformArea.contains(x, y))
    {
        showStatusMessage("drop audio files on the recording buffer", 3000);
        return;
    }

    if (files.isEmpty())
        return;

    // Take the first file only
    loadAudioFileIntoBuffer(juce::File(files[0]));
}

void Gary4juceAudioProcessorEditor::loadAudioFileIntoBuffer(const juce::File& audioFile)
{
    // If this is a different file than the last one, reset the selection position
    if (audioFile != lastDraggedAudioFile)
    {
        lastSelectionStartTime = 0.0;
        DBG("New file detected - resetting selection position to start");
    }

    // Store for double-click reselection (do this after checking, before any early returns)
    lastDraggedAudioFile = audioFile;

    if (!audioFile.existsAsFile())
    {
        showStatusMessage("file not found", 2000);
        return;
    }

    // Use existing format manager pattern from loadOutputAudioFile()
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));

    if (!reader)
    {
        showStatusMessage("couldn't read audio file", 3000);
        return;
    }

    // Calculate duration
    double fileDuration = (double)reader->lengthInSamples / reader->sampleRate;

    DBG("Dropped file: " + audioFile.getFileName() +
        " - Duration: " + juce::String(fileDuration, 2) + "s");

    if (fileDuration <= 30.0)
    {
        // File is short enough - load directly
        DBG("File ≤30s - loading directly into buffer");

        // Read entire file into temp buffer
        juce::AudioBuffer<float> tempBuffer((int)reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&tempBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

        // Check if resampling is needed (match existing pattern from loadOutputAudioForPlayback)
        double fileSampleRate = reader->sampleRate;
        double hostSampleRate = audioProcessor.getCurrentSampleRate();

        if (std::abs(fileSampleRate - hostSampleRate) > 1.0)  // Different sample rates
        {
            DBG("Resampling from " + juce::String(fileSampleRate) +
                "Hz to " + juce::String(hostSampleRate) + "Hz");

            // Calculate resampled size
            double sizeRatio = hostSampleRate / fileSampleRate;
            int resampledNumSamples = (int)((double)tempBuffer.getNumSamples() * sizeRatio);
            double speedRatio = fileSampleRate / hostSampleRate;

            // Create resampled buffer
            juce::AudioBuffer<float> resampledBuffer((int)reader->numChannels, resampledNumSamples);

            // Resample each channel
            for (int channel = 0; channel < (int)reader->numChannels; ++channel)
            {
                juce::LagrangeInterpolator interpolator;
                interpolator.reset();

                const float* readPtr = tempBuffer.getReadPointer(channel);
                float* writePtr = resampledBuffer.getWritePointer(channel);

                interpolator.process(
                    speedRatio,
                    readPtr,
                    writePtr,
                    resampledNumSamples,
                    tempBuffer.getNumSamples(),
                    0
                );
            }

            // Use resampled buffer
            tempBuffer = std::move(resampledBuffer);
        }

        // Load into processor's recording buffer
        audioProcessor.loadAudioIntoRecordingBuffer(tempBuffer);

        // Save to myBuffer.wav (reuse existing save pattern)
        auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        auto garyDir = documentsDir.getChildFile("gary4juce");

        // CRITICAL FIX: Ensure directory exists before saving
        if (!garyDir.exists())
        {
            auto result = garyDir.createDirectory();
            if (!result.wasOk())
            {
                showStatusMessage("failed to create gary4juce folder: " + result.getErrorMessage(), 5000);
                DBG("ERROR: Could not create gary4juce directory: " + result.getErrorMessage());
                return;
            }
            DBG("Created gary4juce directory for first-time drag-and-drop");
        }

        auto bufferFile = garyDir.getChildFile("myBuffer.wav");

        // Use the processor's existing saveRecordingToFile method to maintain consistency
        audioProcessor.saveRecordingToFile(bufferFile);

        // Update savedSamples and UI state
        savedSamples = audioProcessor.getSavedSamples();

        showStatusMessage("loaded " + juce::String(fileDuration, 1) + "s from " +
                         audioFile.getFileNameWithoutExtension(), 3000);

        updateAllGenerationButtonStates();
        repaint();
    }
    else
    {
        // File is >30s - show selection dialog
        DBG("File >30s (" + juce::String(fileDuration, 1) + "s) - showing selection dialog");

        // Create the AudioSelectionDialog
        auto* dialog = new AudioSelectionDialog();

        // Load the audio file into the dialog
        if (!dialog->loadAudioFile(audioFile))
        {
            delete dialog;
            showStatusMessage("failed to load audio file", 3000);
            return;
        }

        // If we're reopening the same file, restore the previous selection position
        if (lastSelectionStartTime > 0.0)
        {
            dialog->setInitialSelectionStartTime(lastSelectionStartTime);
            DBG("Restoring previous selection position: " + juce::String(lastSelectionStartTime, 2) + "s");
        }

        // Store filename for the confirm callback
        juce::String filename = audioFile.getFileNameWithoutExtension();

        // Create a DialogWindow to hold the dialog
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(dialog);
        options.dialogTitle = "Select Audio Segment - " + filename;
        options.dialogBackgroundColour = juce::Colour(0x1e, 0x1e, 0x1e);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.useBottomRightCornerResizer = false;

        // Launch the dialog and capture the window reference
        auto dialogWindow = options.launchAsync();

        // Set up the confirm callback to process the selected segment and close the dialog
        dialog->onConfirm = [this, filename, dialogWindow](const juce::AudioBuffer<float>& selectedBuffer,
                                                              double sourceSampleRate,
                                                              double selectionStartTime) mutable
        {
            DBG("Processing selected segment from " + filename + " starting at " + juce::String(selectionStartTime, 2) + "s");
            DBG("Source sample rate: " + juce::String(sourceSampleRate) + " Hz");

            // Store the selection start time for future reopening
            lastSelectionStartTime = selectionStartTime;

            // Make a copy of the selected buffer
            juce::AudioBuffer<float> tempBuffer(selectedBuffer.getNumChannels(), selectedBuffer.getNumSamples());
            for (int ch = 0; ch < selectedBuffer.getNumChannels(); ++ch)
            {
                tempBuffer.copyFrom(ch, 0, selectedBuffer, ch, 0, selectedBuffer.getNumSamples());
            }

            // Check if resampling is needed (match existing pattern from loadOutputAudioForPlayback)
            double hostSampleRate = audioProcessor.getCurrentSampleRate();
            DBG("Host sample rate: " + juce::String(hostSampleRate) + " Hz");

            if (std::abs(sourceSampleRate - hostSampleRate) > 1.0)  // Different sample rates
            {
                DBG("Resampling from " + juce::String(sourceSampleRate) +
                    " Hz to " + juce::String(hostSampleRate) + " Hz");

                // Calculate resampled size
                double sizeRatio = hostSampleRate / sourceSampleRate;
                int resampledNumSamples = (int)((double)tempBuffer.getNumSamples() * sizeRatio);
                double speedRatio = sourceSampleRate / hostSampleRate;

                DBG("Size ratio: " + juce::String(sizeRatio) +
                    ", Speed ratio: " + juce::String(speedRatio));
                DBG("Original samples: " + juce::String(tempBuffer.getNumSamples()) +
                    ", Resampled samples: " + juce::String(resampledNumSamples));

                // Create resampled buffer
                juce::AudioBuffer<float> resampledBuffer(tempBuffer.getNumChannels(), resampledNumSamples);

                // Resample each channel using JUCE's Lagrange interpolator
                for (int channel = 0; channel < tempBuffer.getNumChannels(); ++channel)
                {
                    juce::LagrangeInterpolator interpolator;
                    interpolator.reset();

                    const float* readPtr = tempBuffer.getReadPointer(channel);
                    float* writePtr = resampledBuffer.getWritePointer(channel);

                    interpolator.process(
                        speedRatio,             // Speed ratio for reading input
                        readPtr,                // Source data
                        writePtr,               // Destination data
                        resampledNumSamples,    // Number of output samples
                        tempBuffer.getNumSamples(), // Number of input samples available
                        0                       // Wrap (0 = no wrap)
                    );
                }

                DBG("Resampling complete");
                tempBuffer = std::move(resampledBuffer);
            }
            else
            {
                DBG("Sample rates match - no resampling needed");
            }

            // Load into processor's recording buffer (now at correct host rate)
            audioProcessor.loadAudioIntoRecordingBuffer(tempBuffer);

            // Save to myBuffer.wav (reuse existing save pattern)
            auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
            auto garyDir = documentsDir.getChildFile("gary4juce");

            // CRITICAL FIX: Ensure directory exists before saving
            if (!garyDir.exists())
            {
                auto result = garyDir.createDirectory();
                if (!result.wasOk())
                {
                    showStatusMessage("failed to create gary4juce folder: " + result.getErrorMessage(), 5000);
                    DBG("ERROR: Could not create gary4juce directory: " + result.getErrorMessage());
                    return;
                }
                DBG("Created gary4juce directory for first-time drag-and-drop");
            }

            auto bufferFile = garyDir.getChildFile("myBuffer.wav");

            // Use the processor's existing saveRecordingToFile method
            audioProcessor.saveRecordingToFile(bufferFile);

            // Update savedSamples and UI state
            savedSamples = audioProcessor.getSavedSamples();

            // Calculate actual duration from the final buffer
            double finalDuration = (double)tempBuffer.getNumSamples() / hostSampleRate;
            showStatusMessage("loaded " + juce::String(finalDuration, 1) + "s from " + filename, 3000);

            updateAllGenerationButtonStates();
            repaint();

            // Close the dialog
            if (dialogWindow != nullptr)
            {
                dialogWindow->exitModalState(0);
                dialogWindow->setVisible(false);
            }
        };

        // Set up the cancel callback to close the dialog window
        dialog->onCancel = [dialogWindow]() mutable
        {
            if (dialogWindow != nullptr)
            {
                dialogWindow->exitModalState(0);
                dialogWindow->setVisible(false);
            }
        };
    }
}

void Gary4juceAudioProcessorEditor::seekToPosition(double timeInSeconds)
{
    // Clamp time to valid range
    timeInSeconds = juce::jlimit(0.0, totalAudioDuration, timeInSeconds);

    // Update processor's seek position
    audioProcessor.seekOutputPlayback(timeInSeconds);

    // Update local tracking
    currentPlaybackPosition = timeInSeconds;
    pausedPosition = timeInSeconds;

    if (isPlayingOutput)
    {
        showStatusMessage("seek to " + juce::String(timeInSeconds, 1) + "s", 1500);
    }
    else if (isPausedOutput)
    {
        showStatusMessage("seek to " + juce::String(timeInSeconds, 1) + "s (paused)", 1500);
    }
    else if (hasOutputAudio)
    {
        // Set paused state so next play resumes from here
        isPausedOutput = true;
        showStatusMessage("seek to " + juce::String(timeInSeconds, 1) + "s", 1500);
    }

    repaint();
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

    // Stop playback through processor
    fullStopOutputPlayback();

    // Give the system a moment to ensure processor has stopped
    juce::Thread::sleep(50);

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
    updateTerryEnablementSnapshot();

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
    const auto connectionTextArea = connectionStatusArea;
    if (audioProcessor.getIsUsingLocalhost())
    {
        g.setFont(juce::FontOptions(13.5f, juce::Font::bold));
        const auto lineOne = getLocalConnectionLineOne();
        const auto lineTwo = juce::String(localOnlineCount) + "/3 online";
        const bool anyOnline = localOnlineCount > 0;
        const bool activeOnline = isActiveLocalServiceOnline();

        if (!anyOnline)
            g.setColour(juce::Colour(0xff666666));
        else if (activeOnline)
            g.setColour(connectionFlashState ? juce::Colours::white : juce::Colour(0xffb7ffd1));
        else
            g.setColour(juce::Colour(0xffe1c46d));

        g.drawFittedText(lineOne + "\n" + lineTwo, connectionTextArea, juce::Justification::centredLeft, 2);
    }
    else
    {
        g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
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
    }

    // Recording status section header (using reserved area)
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawFittedText("recording buffer", recordingLabelArea, juce::Justification::centred, 1);

    // Draw the INPUT waveform
    drawWaveform(g, waveformArea);

    // Add drag hover feedback for input waveform
    if (isDragHoveringInput)
    {
        g.setColour(juce::Colours::yellow.withAlpha(0.3f));
        g.fillRoundedRectangle(waveformArea.toFloat(), 4.0f);

        g.setColour(juce::Colours::yellow);
        g.drawRoundedRectangle(waveformArea.toFloat(), 4.0f, 2.0f);

        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.setColour(juce::Colours::white);
        g.drawText("drop audio file here", waveformArea, juce::Justification::centred);
    }

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
            bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();
            statusText = isStandalone
                ? "drag an audio file here to use with gary, terry, or darius"
                : "press PLAY in DAW to start recording";
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
    connectionItem.height = 42;  // Supports two-line localhost status + button stack
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
    auto connectionRowArea = topSectionFlexBox.items[1].currentBounds.toNearestInt();
    recordingLabelArea = topSectionFlexBox.items[2].currentBounds.toNearestInt();
    waveformArea = topSectionFlexBox.items[3].currentBounds.toNearestInt();
    inputStatusArea = topSectionFlexBox.items[4].currentBounds.toNearestInt();
    inputInfoArea = topSectionFlexBox.items[5].currentBounds.toNearestInt();

    // Manual layout for connection status area: stacked buttons on the right
    auto buttonStackArea = connectionRowArea.removeFromRight(120); // Reserve space for buttons
    connectionStatusArea = connectionRowArea;
    
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

    bool isStandalone = juce::JUCEApplicationBase::isStandaloneApp();

    // Save buffer button (only in plugin mode - not needed in standalone with drag & drop)
    if (!isStandalone)
    {
        juce::FlexItem saveItem(saveBufferButton);
        saveItem.width = 150;  // Fixed width
        saveItem.height = 35;
        saveItem.margin = juce::FlexItem::Margin(0, 10, 0, 0);
        bufferButtonsFlexBox.items.add(saveItem);
    }

    // Clear buffer button (right side) - more square proportions
    juce::FlexItem clearBufferItem(clearBufferButton);
    clearBufferItem.width = 50;   // Much narrower
    clearBufferItem.height = 35;  // Same height
    clearBufferItem.margin = juce::FlexItem::Margin(0, 0, 0, 10);

    bufferButtonsFlexBox.items.add(clearBufferItem);
    bufferButtonsFlexBox.performLayout(bufferControlsBounds);

    // ========== TAB AREA ==========
    auto tabSectionBounds = bounds.removeFromTop(320).reduced(20, 10);
    fullTabAreaRect = tabSectionBounds.expanded(20, 10); // Expand back to account for the reduced margins

    // Tab buttons area
    tabArea = tabSectionBounds.removeFromTop(35);
    auto tabButtonWidth = tabArea.getWidth() / 4;

    garyTabButton.setBounds(tabArea.removeFromLeft(tabButtonWidth).reduced(2, 2));
    jerryTabButton.setBounds(tabArea.removeFromLeft(tabButtonWidth).reduced(2, 2));
    terryTabButton.setBounds(tabArea.removeFromLeft(tabButtonWidth).reduced(2, 2));
    dariusTabButton.setBounds(tabArea.reduced(2, 2));

    // Model controls area (below tabs)
    modelControlsArea = tabSectionBounds.reduced(0, 5);

    // ========== GARY CONTROLS LAYOUT ==========
    if (garyUI)
    {
        garyUI->setBounds(modelControlsArea);
    }

    if (helpIcon && currentTab == ModelTab::Gary && garyUI)
    {
        auto titleBounds = garyUI->getTitleBounds().translated(garyUI->getX(), garyUI->getY());
        juce::Font titleFont(juce::FontOptions(16.0f, juce::Font::bold));
        const int textWidth = juce::roundToInt(titleFont.getStringWidthFloat("gary (musicgen)"));
        auto textStartX = titleBounds.getX() + (titleBounds.getWidth() - textWidth) / 2;
        auto helpBounds = juce::Rectangle<int>(
            textStartX + textWidth + 5,
            titleBounds.getY() + (titleBounds.getHeight() - 20) / 2,
            20, 20);
        garyHelpButton.setBounds(helpBounds);
    }

    if (jerryUI)
        jerryUI->setBounds(modelControlsArea);

    if (helpIcon && currentTab == ModelTab::Jerry && jerryUI)
    {
        auto titleBounds = jerryUI->getTitleBounds().translated(jerryUI->getX(), jerryUI->getY());
        juce::Font titleFont(juce::FontOptions(16.0f, juce::Font::bold));
        const int textWidth = juce::roundToInt(titleFont.getStringWidthFloat("jerry (stable audio open small)"));
        auto textStartX = titleBounds.getX() + (titleBounds.getWidth() - textWidth) / 2;
        auto helpBounds = juce::Rectangle<int>(
            textStartX + textWidth + 5,
            titleBounds.getY() + (titleBounds.getHeight() - 20) / 2,
            20, 20);
        jerryHelpButton.setBounds(helpBounds);
    }

    if (terryUI)
        terryUI->setBounds(modelControlsArea);

    if (helpIcon && currentTab == ModelTab::Terry && terryUI)
    {
        auto titleBounds = terryUI->getTitleBounds().translated(terryUI->getX(), terryUI->getY());
        juce::Font titleFont(juce::FontOptions(16.0f, juce::Font::bold));
        const int textWidth = juce::roundToInt(titleFont.getStringWidthFloat("terry (melodyflow)"));
        auto textStartX = titleBounds.getX() + (titleBounds.getWidth() - textWidth) / 2;
        auto helpBounds = juce::Rectangle<int>(
            textStartX + textWidth + 5,
            titleBounds.getY() + (titleBounds.getHeight() - 20) / 2,
            20, 20);
        terryHelpButton.setBounds(helpBounds);
    }

    if (dariusUI)
        dariusUI->setBounds(modelControlsArea);

    if (helpIcon && currentTab == ModelTab::Darius && dariusUI)
    {
        auto titleBounds = dariusUI->getTitleBounds().translated(dariusUI->getX(), dariusUI->getY());
        juce::Font titleFont(juce::FontOptions(16.0f, juce::Font::bold));
        const int textWidth = juce::roundToInt(titleFont.getStringWidthFloat("darius (magentaRT)"));
        auto textStartX = titleBounds.getX() + (titleBounds.getWidth() - textWidth) / 2;
        auto helpBounds = juce::Rectangle<int>(
            textStartX + textWidth + 5,
            titleBounds.getY() + (titleBounds.getHeight() - 20) / 2,
            20, 20);
        dariusHelpButton.setBounds(helpBounds);
    }


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

    updateGaryButtonStates(false);
    if (garyUI)
        garyUI->setRetryButtonText("retry");

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
    updateGaryButtonStates(false);
    if (garyUI)
        garyUI->setContinueButtonText("continue");
}

//==============================================================================
// Backend Disconnection and Stall Handling Methods
//==============================================================================

void Gary4juceAudioProcessorEditor::markBackendDisconnectedFromRequestFailure(const juce::String& context)
{
    DBG("Marking backend disconnected after request failure: " + context);

    audioProcessor.setBackendConnectionStatus(false);

    // If processor was already false but editor state is stale, force UI sync.
    if (isConnected)
        updateConnectionStatus(false);
}

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

    if (getActiveOp() == ActiveOp::JerryGenerate && jerryUI)
        jerryUI->setGenerateButtonText("generate with jerry");

    setActiveOp(ActiveOp::None);
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

    if (getActiveOp() == ActiveOp::JerryGenerate && jerryUI)
        jerryUI->setGenerateButtonText("generate with jerry");

    setActiveOp(ActiveOp::None);
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
