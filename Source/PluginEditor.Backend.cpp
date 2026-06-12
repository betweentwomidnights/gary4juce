// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

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
        case ServiceType::SA3:        processorService = Gary4juceAudioProcessor::ServiceType::SA3; break;
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

    if (sa3UI)
        sa3UI->setRemoteAvailable(isServiceReachable(ServiceType::SA3));

    // Local and remote SA3 can expose different LoRA registries.
    availableSA3Loras.clear();
    sa3LoraFetchBackendUrl.clear();
    sa3LoraLastFetchMs = 0;
    syncSA3LoraUi();

    // Update carey tab availability (now available on both backends)
    updateCareyTabAvailability();

    // Update Jerry sub-tab states after backend mode changes.
    updateJerrySubTabStates();

    if (currentTab == ModelTab::Jerry)
    {
        const bool showSA3 = (jerrySubTab == JerrySubTab::SA3);
        const bool showSAOS = (jerrySubTab == JerrySubTab::SAOS);
        if (sa3UI) sa3UI->setVisibleForTab(showSA3);
        if (showSA3) refreshSA3AvailableLoras(true);
        if (jerryUI) jerryUI->setVisibleForTab(showSAOS);
        if (foundationUI) foundationUI->setVisibleForTab(jerrySubTab == JerrySubTab::Foundation);
        if (helpIcon)
        {
            sa3HelpButton.setVisible(showSA3);
            jerryHelpButton.setVisible(showSAOS);
            foundationHelpButton.setVisible(jerrySubTab == JerrySubTab::Foundation);
        }
    }

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
        backendToggleButton.setTooltip("using localhost backend (gary:8000 terry:8002 jerry:8005 sa3:8006 carey:8003 foundation:8015) - click to switch to remote");
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

#if JUCE_WINDOWS
static bool rawHealthResponseLooksOnline(
    const juce::String& responseText, bool allowUnframedBody)
{
    const auto headerEnd = responseText.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return false;

    const auto statusLine = responseText.upToFirstOccurrenceOf("\r\n", false, false);
    juce::StringArray statusParts;
    statusParts.addTokens(statusLine, " ", "");
    if (statusParts.size() < 2)
        return false;

    const int statusCode = statusParts[1].getIntValue();
    if (statusCode < 200 || statusCode >= 300)
        return false;

    const auto responseHeaders = responseText.substring(0, headerEnd);
    auto responseBody = responseText.substring(headerEnd + 4);
    if (responseHeaders.containsIgnoreCase("Transfer-Encoding: chunked"))
    {
        const auto chunkSizeEnd = responseBody.indexOf("\r\n");
        if (chunkSizeEnd < 0)
            return false;

        const int chunkSize = responseBody.substring(0, chunkSizeEnd).getHexValue32();
        if (responseBody.substring(chunkSizeEnd + 2).getNumBytesAsUTF8() < chunkSize)
            return false;

        responseBody = responseBody.substring(chunkSizeEnd + 2, chunkSizeEnd + 2 + chunkSize);
    }
    else
    {
        int contentLength = -1;
        juce::StringArray headerLines;
        headerLines.addLines(responseHeaders);
        for (const auto& headerLine : headerLines)
        {
            if (headerLine.startsWithIgnoreCase("Content-Length:"))
            {
                contentLength = headerLine.fromFirstOccurrenceOf(":", false, false)
                    .trim().getIntValue();
                break;
            }
        }

        if (contentLength >= 0
            && responseBody.getNumBytesAsUTF8() < contentLength)
            return false;
        if (contentLength < 0 && !allowUnframedBody)
            return false;
    }

    return localhostHealthResponseLooksOnline(responseBody);
}
#endif

static bool probeLocalhostHealth(int port)
{
    const auto startedAtMs = juce::Time::getMillisecondCounterHiRes();
    juce::ignoreUnused(startedAtMs);

    try
    {
       #if JUCE_WINDOWS
        constexpr int connectTimeoutMs = 400;
        constexpr int responseTimeoutMs = 1000;
        juce::StreamingSocket socket;
        if (!socket.connect("127.0.0.1", port, connectTimeoutMs))
        {
            DBG("[local health] port " + juce::String(port) + " offline after "
                + juce::String(juce::Time::getMillisecondCounterHiRes() - startedAtMs, 1) + " ms");
            return false;
        }

        const juce::String request =
            "GET /health HTTP/1.1\r\n"
            "Host: 127.0.0.1:" + juce::String(port) + "\r\n"
            "Accept: application/json\r\n"
            "Connection: close\r\n\r\n";
        const auto requestUtf8 = request.toRawUTF8();
        const int requestBytes = static_cast<int>(request.getNumBytesAsUTF8());

        if (socket.waitUntilReady(false, connectTimeoutMs) != 1
            || socket.write(requestUtf8, requestBytes) != requestBytes)
            return false;

        juce::MemoryOutputStream response;
        const auto readDeadlineMs = juce::Time::getMillisecondCounterHiRes() + responseTimeoutMs;
        char buffer[4096];

        while (juce::Time::getMillisecondCounterHiRes() < readDeadlineMs
            && response.getDataSize() < 1024 * 1024)
        {
            const auto remainingMs = juce::jmax(
                1, static_cast<int>(readDeadlineMs - juce::Time::getMillisecondCounterHiRes()));
            const int ready = socket.waitUntilReady(true, remainingMs);
            if (ready <= 0)
                break;

            const int bytesRead = socket.read(buffer, static_cast<int>(sizeof(buffer)), false);
            if (bytesRead <= 0)
                break;

            response.write(buffer, static_cast<size_t>(bytesRead));

            if (rawHealthResponseLooksOnline(response.toString(), false))
            {
                DBG("[local health] port " + juce::String(port) + " online after "
                    + juce::String(juce::Time::getMillisecondCounterHiRes() - startedAtMs, 1) + " ms");
                return true;
            }
        }

        const bool online = rawHealthResponseLooksOnline(response.toString(), true);
        DBG("[local health] port " + juce::String(port) + " "
            + (online ? "online" : "offline") + " after "
            + juce::String(juce::Time::getMillisecondCounterHiRes() - startedAtMs, 1) + " ms");
        return online;
       #else
        juce::URL healthUrl("http://127.0.0.1:" + juce::String(port) + "/health");
        int statusCode = 0;
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(1500)
            .withStatusCode(&statusCode)
            .withExtraHeaders("Accept: application/json");
        auto stream = healthUrl.createInputStream(options);
        if (stream == nullptr || statusCode >= 400)
            return false;

        return localhostHealthResponseLooksOnline(stream->readEntireStreamAsString());
       #endif
    }
    catch (...)
    {
        return false;
    }
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
    case ServiceType::SA3:        return localSA3Online;
    }
    return false;
}

Gary4juceAudioProcessorEditor::ServiceType Gary4juceAudioProcessorEditor::getActiveLocalService() const
{
    switch (currentTab)
    {
    case ModelTab::Jerry:
        if (jerrySubTab == JerrySubTab::Foundation)
            return ServiceType::Foundation;
        if (jerrySubTab == JerrySubTab::SA3)
            return ServiceType::SA3;
        return ServiceType::Jerry;
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
    localSA3Online = false;
    localOnlineCount = 0;
    localHealthLastPollMs = 0;
    localHealthPollCounter = 0;
    localHealthPollNonce.fetch_add(1);
    localHealthPollInFlight.store(false);
    audioProcessor.clearLocalServiceHealthSnapshot();
}

void Gary4juceAudioProcessorEditor::restoreLocalServiceHealthSnapshot()
{
    const auto snapshot = audioProcessor.getLocalServiceHealthSnapshot();
    if (!snapshot.valid)
        return;

    localGaryOnline = snapshot.garyOnline;
    localTerryOnline = snapshot.terryOnline;
    localJerryOnline = snapshot.jerryOnline;
    localCareyOnline = snapshot.careyOnline;
    localFoundationOnline = snapshot.foundationOnline;
    localSA3Online = snapshot.sa3Online;
    localOnlineCount = snapshot.getOnlineCount();
    localHealthLastPollMs = snapshot.updatedAtMs;
}

void Gary4juceAudioProcessorEditor::applyLocalServiceHealthResult(
    ServiceType service, bool online, bool pollComplete)
{
    const bool statusChanged = isLocalServiceOnline(service) != online;

    switch (service)
    {
    case ServiceType::Gary:       localGaryOnline = online; break;
    case ServiceType::Terry:      localTerryOnline = online; break;
    case ServiceType::Jerry:      localJerryOnline = online; break;
    case ServiceType::Carey:      localCareyOnline = online; break;
    case ServiceType::Foundation: localFoundationOnline = online; break;
    case ServiceType::SA3:        localSA3Online = online; break;
    }

    localOnlineCount = (localGaryOnline ? 1 : 0) + (localTerryOnline ? 1 : 0)
        + (localJerryOnline ? 1 : 0) + (localCareyOnline ? 1 : 0)
        + (localFoundationOnline ? 1 : 0) + (localSA3Online ? 1 : 0);

    Gary4juceAudioProcessor::LocalServiceHealthSnapshot snapshot;
    snapshot.valid = true;
    snapshot.garyOnline = localGaryOnline;
    snapshot.terryOnline = localTerryOnline;
    snapshot.jerryOnline = localJerryOnline;
    snapshot.careyOnline = localCareyOnline;
    snapshot.foundationOnline = localFoundationOnline;
    snapshot.sa3Online = localSA3Online;
    snapshot.updatedAtMs = juce::Time::getCurrentTime().toMilliseconds();
    audioProcessor.setLocalServiceHealthSnapshot(snapshot);

    updateAllGenerationButtonStates();
    updateJerrySubTabStates();

    if (service == ServiceType::Jerry)
    {
        if (!online && jerryUI)
            jerryUI->setLoadingModel(false);
        else if (online && currentTab == ModelTab::Jerry)
            fetchJerryAvailableModels();
    }

    if (service == ServiceType::Carey && currentTab == ModelTab::Carey
        && (statusChanged || online))
    {
        refreshCareyAvailableLoras(statusChanged);
        updateCareyEnablementSnapshot();
    }

    if (service == ServiceType::SA3
        && currentTab == ModelTab::Jerry
        && jerrySubTab == JerrySubTab::SA3)
    {
        if (online)
            refreshSA3AvailableLoras(statusChanged);
        else
        {
            availableSA3Loras.clear();
            syncSA3LoraUi();
        }
        updateSA3EnablementSnapshot();
    }

    if (pollComplete)
    {
        localHealthPollInFlight.store(false);
        checkConnectionButton.setEnabled(true);
    }

    if (statusChanged)
        repaint();
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
    const int pollNonce = localHealthPollNonce.fetch_add(1) + 1;

    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;
    const auto completedProbes = std::make_shared<std::atomic<int>>(0);

    auto launchProbe = [asyncAlive, editor, pollNonce, completedProbes](
        ServiceType service, int port)
    {
        juce::Thread::launch([asyncAlive, editor, pollNonce, completedProbes, service, port]()
        {
            const bool online = probeLocalhostHealth(port);

            juce::MessageManager::callAsync([asyncAlive, editor, pollNonce, completedProbes, service, online]()
            {
                const auto alive = asyncAlive.lock();
                if (alive == nullptr || !alive->load(std::memory_order_acquire))
                    return;
                if (pollNonce != editor->localHealthPollNonce.load()
                    || !editor->audioProcessor.getIsUsingLocalhost())
                    return;

                const bool pollComplete = completedProbes->fetch_add(1) + 1 == 6;
                editor->applyLocalServiceHealthResult(service, online, pollComplete);
            });
        });
    };

    auto portForService = [](ServiceType service)
    {
        switch (service)
        {
        case ServiceType::Gary:       return 8000;
        case ServiceType::Terry:      return 8002;
        case ServiceType::Jerry:      return 8005;
        case ServiceType::Carey:      return 8003;
        case ServiceType::Foundation: return 8015;
        case ServiceType::SA3:        return 8006;
        }
        return 0;
    };

    const auto activeService = getActiveLocalService();
    launchProbe(activeService, portForService(activeService));

    constexpr ServiceType services[] = {
        ServiceType::Gary,
        ServiceType::Terry,
        ServiceType::Jerry,
        ServiceType::Carey,
        ServiceType::Foundation,
        ServiceType::SA3
    };

    for (const auto service : services)
        if (service != activeService)
            launchProbe(service, portForService(service));
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
