/*
  ==============================================================================
    Backend and connection-related PluginEditor method definitions.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

// ========== BACKEND TOGGLE METHODS ==========
juce::String Gary4juceAudioProcessorEditor::getServiceUrl(ServiceType service, const juce::String& endpoint) const
{
    // Map editor ServiceType to processor ServiceType
    Gary4juceAudioProcessor::ServiceType processorService;
    switch (service)
    {
        case ServiceType::Gary:       processorService = Gary4juceAudioProcessor::ServiceType::Gary; break;
        case ServiceType::Jerry:      processorService = Gary4juceAudioProcessor::ServiceType::Jerry; break;
        case ServiceType::Terry:      processorService = Gary4juceAudioProcessor::ServiceType::Terry; break;
        case ServiceType::Carey:      processorService = Gary4juceAudioProcessor::ServiceType::Carey; break;
        case ServiceType::Foundation: processorService = Gary4juceAudioProcessor::ServiceType::Foundation; break;
        default: processorService = Gary4juceAudioProcessor::ServiceType::Gary; break;
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

    // Clear any loading state when switching backends
    if (jerryUI)
    {
        jerryUI->setLoadingModel(false);
        jerryUI->setUsingLocalhost(isUsingLocalhost);
    }

    // Update carey tab availability (now available on both backends)
    updateCareyTabAvailability();

    // Update Foundation sub-tab states (now available on both backends)
    updateJerrySubTabStates();

    // Reset local health snapshot and trigger fresh poll if switching to localhost
    resetLocalServiceHealthSnapshot();
    if (isUsingLocalhost)
        triggerLocalServiceHealthPoll(true);

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
        backendToggleButton.setTooltip("using localhost backend (gary:8000 terry:8002 jerry:8005 carey:8003 foundation:8015) - click to switch to remote");
    }
    else
    {
        backendToggleButton.setButtonText("remote");
        backendToggleButton.setButtonStyle(CustomButton::ButtonStyle::Standard); // Standard style for remote
        backendToggleButton.setTooltip("using remote backend (g4l.thecollabagepatch.com) - click to switch to localhost");
    }
}

// ========== LOCAL SERVICE HEALTH POLLING ==========

static bool localhostHealthResponseLooksOnline(const juce::String& responseText)
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

bool Gary4juceAudioProcessorEditor::isLocalServiceOnline(ServiceType service) const
{
    switch (service)
    {
    case ServiceType::Gary:       return localGaryOnline;
    case ServiceType::Terry:      return localTerryOnline;
    case ServiceType::Jerry:      return localJerryOnline;
    case ServiceType::Carey:      return localCareyOnline;
    case ServiceType::Foundation: return localFoundationOnline;
    }
    return false;
}

Gary4juceAudioProcessorEditor::ServiceType Gary4juceAudioProcessorEditor::getActiveLocalService() const
{
    switch (currentTab)
    {
    case ModelTab::Jerry:
        return (jerrySubTab == JerrySubTab::Foundation) ? ServiceType::Foundation : ServiceType::Jerry;
    case ModelTab::Terry:  return ServiceType::Terry;
    case ModelTab::Carey:  return ServiceType::Carey;
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

bool Gary4juceAudioProcessorEditor::isServiceReachable(ServiceType service) const
{
    if (audioProcessor.getIsUsingLocalhost())
        return isLocalServiceOnline(service);
    return isConnected; // remote backend — single endpoint serves all
}

void Gary4juceAudioProcessorEditor::resetLocalServiceHealthSnapshot()
{
    localGaryOnline = false;
    localTerryOnline = false;
    localJerryOnline = false;
    localCareyOnline = false;
    localFoundationOnline = false;
    localOnlineCount = 0;
    localHealthLastPollMs = 0;
    localHealthPollCounter = 0;
    localHealthPollInFlight.store(false);
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

        const bool garyOnline       = probeHealth("http://127.0.0.1:8000/health");
        const bool terryOnline      = probeHealth("http://127.0.0.1:8002/health");
        const bool jerryOnline      = probeHealth("http://127.0.0.1:8005/health");
        const bool careyOnline      = probeHealth("http://127.0.0.1:8003/health");
        const bool foundationOnline = probeHealth("http://127.0.0.1:8015/health");

        juce::MessageManager::callAsync([safeThis, garyOnline, terryOnline, jerryOnline, careyOnline, foundationOnline]() {
            if (safeThis == nullptr)
                return;

            safeThis->localHealthPollInFlight.store(false);

            const bool careyStatusChanged = safeThis->localCareyOnline != careyOnline;

            const bool changed =
                safeThis->localGaryOnline != garyOnline ||
                safeThis->localTerryOnline != terryOnline ||
                safeThis->localJerryOnline != jerryOnline ||
                safeThis->localCareyOnline != careyOnline ||
                safeThis->localFoundationOnline != foundationOnline;

            safeThis->localGaryOnline = garyOnline;
            safeThis->localTerryOnline = terryOnline;
            safeThis->localJerryOnline = jerryOnline;
            safeThis->localCareyOnline = careyOnline;
            safeThis->localFoundationOnline = foundationOnline;
            safeThis->localOnlineCount = (garyOnline ? 1 : 0) + (terryOnline ? 1 : 0)
                + (jerryOnline ? 1 : 0) + (careyOnline ? 1 : 0) + (foundationOnline ? 1 : 0);

            safeThis->updateAllGenerationButtonStates();

            if (!jerryOnline && safeThis->jerryUI)
                safeThis->jerryUI->setLoadingModel(false);

            // Keep Jerry model dropdown fresh while Jerry tab is active on localhost
            if (jerryOnline && safeThis->currentTab == ModelTab::Jerry)
                safeThis->fetchJerryAvailableModels();

            if (safeThis->currentTab == ModelTab::Carey && (careyStatusChanged || careyOnline))
            {
                safeThis->refreshCareyAvailableLoras(careyStatusChanged);
                safeThis->updateCareyEnablementSnapshot();
            }

            if (changed)
                safeThis->repaint();
        });
    });
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
    const auto stalledToken = currentGenerationAsyncToken();
    
    // Show immediate feedback
    showStatusMessage("checking backend connection...", 3000);
    
    // Check if backend is actually down
    audioProcessor.checkBackendHealth();
    
    // Give health check time to complete, then handle result
    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis(this);
    juce::Timer::callAfterDelay(6000, [safeThis, stalledToken]() {
        if (safeThis == nullptr || !safeThis->isGenerationAsyncWorkCurrent(stalledToken))
            return;

        if (!safeThis->audioProcessor.isBackendConnected())
        {
            // Backend is confirmed down
            safeThis->handleBackendDisconnection();
        }
        else
        {
            // Backend is up but generation failed for other reasons
            safeThis->handleGenerationFailure("generation timed out - try again or check backend logs");
        }
    });
}

void Gary4juceAudioProcessorEditor::handleBackendDisconnection()
{
    DBG("=== BACKEND DISCONNECTION CONFIRMED - CLEANING UP STATE ===");
    
    // Clean up generation state
    isGenerating = false;
    isPolling = false;
    invalidateGenerationAsyncWork();
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

    setActiveOp(ActiveOp::None);
}

void Gary4juceAudioProcessorEditor::handleGenerationFailure(const juce::String& reason)
{
    DBG("Generation failed: " + reason);
    
    // Clean up generation state (same as disconnection)
    isGenerating = false;
    isPolling = false;
    invalidateGenerationAsyncWork();
    generationProgress = 0;
    lastProgressUpdateTime = 0;
    lastKnownServerProgress = 0;
    hasDetectedStall = false;
    
    // Update all button states centrally
    updateAllGenerationButtonStates();

    // Show error message
    showStatusMessage(reason, 5000);
    if (shouldShowGenerationFailureDialog(reason))
        showGenerationFailureDialog(reason);

    repaint();

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

bool Gary4juceAudioProcessorEditor::shouldShowGenerationFailureDialog(const juce::String& reason) const
{
    if (audioProcessor.getIsUsingLocalhost())
        return false;

    const juce::String normalized = reason.trim().toLowerCase();
    if (normalized.isEmpty())
        return false;

    const auto containsAny = [&normalized](std::initializer_list<const char*> needles) {
        for (const auto* needle : needles)
        {
            if (normalized.contains(needle))
                return true;
        }
        return false;
    };

    if (containsAny({
            "cannot connect",
            "failed to connect",
            "backend not responding",
            "docker compose",
            "localhost",
            "network error",
            "polling failed",
            "failed to fetch checkpoints",
            "failed to fetch model config",
            "check connection first",
        }))
    {
        return false;
    }

    if (containsAny({
            "missing mybuffer.wav",
            "save your recording first",
            "failed to read mybuffer.wav",
            "failed to decode mybuffer.wav",
            "failed to load mybuffer.wav",
            "failed to base64 encode",
            "failed to create ",
            "failed to write ",
            "conditioning audio",
            "audio_duration must be",
            "cover_noise_strength must be",
            "audio_cover_strength must be",
        }))
    {
        return false;
    }

    if (containsAny({
            "traceback",
            "cublas",
            "cuda",
            "runtimeerror",
            "exception",
            "error:",
            "generation failed",
            "request failed",
            "submit failed",
            "status response",
            "timed out",
            "unknown task_id",
            "http ",
            "server error",
        }))
    {
        return true;
    }

    return normalized.length() > 80;
}

void Gary4juceAudioProcessorEditor::showSupportDialog(const juce::String& title,
                                                      const juce::String& message,
                                                      const juce::String& detailText,
                                                      bool showCheckConnectionHint)
{
    DBG("=== SHOWING SUPPORT DIALOG: " + title + " ===");

    class SupportButtonPanel : public juce::Component
    {
    public:
        SupportButtonPanel(Gary4juceAudioProcessorEditor* editor,
                           const juce::String& detailsToCopy,
                           const bool shouldShowConnectionHint)
            : parentEditor(editor),
              detailText(detailsToCopy),
              showConnectionHint(shouldShowConnectionHint)
        {
            if (detailText.isNotEmpty())
            {
                detailEditor = std::make_unique<juce::TextEditor>();
                detailEditor->setMultiLine(true);
                detailEditor->setReadOnly(true);
                detailEditor->setScrollbarsShown(true);
                detailEditor->setCaretVisible(false);
                detailEditor->setPopupMenuEnabled(true);
                detailEditor->setText(detailText, juce::dontSendNotification);
                addAndMakeVisible(detailEditor.get());

                copyButton = std::make_unique<CustomButton>();
                copyButton->setButtonText("copy error");
                copyButton->setButtonStyle(CustomButton::ButtonStyle::Standard);
                copyButton->setTooltip("copy the error details to your clipboard");
                copyButton->onClick = [this]() {
                    juce::SystemClipboard::copyTextToClipboard(detailText);
                    copyButton->setButtonText("copied");
                    if (parentEditor != nullptr)
                        parentEditor->showStatusMessage("error copied to clipboard", 2000);
                };
                addAndMakeVisible(copyButton.get());
            }

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

            if (showConnectionHint)
            {
                visualRefLabel = std::make_unique<juce::Label>();
                visualRefLabel->setText("in a few minutes, click this button in the main window:", juce::dontSendNotification);
                visualRefLabel->setJustificationType(juce::Justification::centred);
                visualRefLabel->setColour(juce::Label::textColourId, juce::Colours::white);
                addAndMakeVisible(visualRefLabel.get());

                if (editor->checkConnectionIcon)
                {
                    visualRefButton = std::make_unique<CustomButton>();
                    visualRefButton->setButtonStyle(CustomButton::ButtonStyle::Standard);
                    visualRefButton->setIcon(editor->checkConnectionIcon->createCopy());
                    visualRefButton->setEnabled(false);
                    visualRefButton->setTooltip("this is the check connection button in the main UI");
                    addAndMakeVisible(visualRefButton.get());
                }
            }

            int height = 70;
            if (detailEditor) height += 140;
            if (showConnectionHint) height += 70;
            setSize(340, height);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(8);

            if (detailEditor)
            {
                detailEditor->setBounds(area.removeFromTop(140));
                area.removeFromTop(8);
            }

            if (showConnectionHint && visualRefLabel)
            {
                auto visualRefArea = area.removeFromTop(62);
                visualRefLabel->setBounds(visualRefArea.removeFromTop(28));
                if (visualRefButton)
                    visualRefButton->setBounds(visualRefArea.reduced(10).withSizeKeepingCentre(30, 30));
                area.removeFromTop(6);
            }

            auto buttonArea = area.removeFromTop(32);
            const int buttonCount = copyButton ? 3 : 2;
            const int gap = 8;
            const int buttonWidth = (buttonArea.getWidth() - (gap * (buttonCount - 1))) / buttonCount;

            if (copyButton)
            {
                copyButton->setBounds(buttonArea.removeFromLeft(buttonWidth));
                buttonArea.removeFromLeft(gap);
            }
            discordButton->setBounds(buttonArea.removeFromLeft(buttonWidth));
            buttonArea.removeFromLeft(gap);
            xButton->setBounds(buttonArea.removeFromLeft(buttonWidth));
        }

    private:
        Gary4juceAudioProcessorEditor* parentEditor;
        juce::String detailText;
        bool showConnectionHint = false;
        std::unique_ptr<CustomButton> copyButton;
        std::unique_ptr<CustomButton> discordButton;
        std::unique_ptr<CustomButton> xButton;
        std::unique_ptr<CustomButton> visualRefButton;
        std::unique_ptr<juce::Label> visualRefLabel;
        std::unique_ptr<juce::TextEditor> detailEditor;
    };

    auto* alertWindow = new juce::AlertWindow(title,
        message,
        juce::MessageBoxIconType::WarningIcon,
        this);

    auto* customButtons = new SupportButtonPanel(this, detailText, showCheckConnectionHint);
    alertWindow->addCustomComponent(customButtons);
    alertWindow->addButton("close", 999);
    alertWindow->enterModalState(true,
        juce::ModalCallbackFunction::create([alertWindow](int result) {
            DBG("Modal closed with result: " + juce::String(result));
            delete alertWindow;
        }));
}

void Gary4juceAudioProcessorEditor::showBackendDisconnectionDialog()
{
    showSupportDialog("backend down",
        "our backend runs on a spot vm\n"
        "and azure prolly deallocated it.\n"
        "hit up kev in discord/twitter or",
        {},
        true);
}

void Gary4juceAudioProcessorEditor::showGenerationFailureDialog(const juce::String& reason)
{
    showSupportDialog("generation failed",
        "copy the details below and send them to kev\n"
        "on discord or twitter.\n"
        "if it looks generic, please still send it anyway.",
        reason,
        false);
}
