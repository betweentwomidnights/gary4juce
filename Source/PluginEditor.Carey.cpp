/*
  ==============================================================================
    Carey-specific PluginEditor method definitions.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginEditorCareyHelpers.h"

using plugin_editor_detail::parseCareyFailureResponse;
using plugin_editor_detail::resolveCareyProgressPercent;
using plugin_editor_detail::stripAnsiAndControlChars;

namespace
{
juce::String cleanCareyQueueMessageText(const juce::String& raw)
{
    const juce::String trimmed = stripAnsiAndControlChars(raw);
    if (trimmed.isEmpty())
        return {};

    // Already a clean status string from our backend wrapper — pass through
    if (trimmed == "generating..." || trimmed == "downloading result..."
        || trimmed == "preparing audio..." || trimmed == "submitting..."
        || trimmed == "loading model..." || trimmed == "queued"
        || trimmed == "submitting to ace-step..." || trimmed == "downloading audio..."
        || trimmed == "complete")
        return trimmed;

    // tqdm progress bar: "72%|#######2  | 36/50 [00:02<00:00, 14.11steps/s]"
    // Extract "M/T steps" from the "M/T" portion
    if (trimmed.contains("|") && trimmed.contains("/"))
    {
        // Find the step fraction like "36/50"
        int bestCurrent = -1, bestTotal = -1;
        int searchFrom = 0;
        while (searchFrom < trimmed.length())
        {
            const int slash = trimmed.indexOfChar(searchFrom, '/');
            if (slash < 0) break;
            int leftStart = slash - 1;
            while (leftStart >= 0 && juce::CharacterFunctions::isDigit(trimmed[leftStart])) --leftStart;
            int rightEnd = slash + 1;
            while (rightEnd < trimmed.length() && juce::CharacterFunctions::isDigit(trimmed[rightEnd])) ++rightEnd;
            if ((leftStart + 1) < slash && (slash + 1) < rightEnd)
            {
                bestCurrent = trimmed.substring(leftStart + 1, slash).getIntValue();
                bestTotal = trimmed.substring(slash + 1, rightEnd).getIntValue();
            }
            searchFrom = slash + 1;
        }
        if (bestTotal > 0 && bestCurrent >= 0)
            return juce::String(bestCurrent) + "/" + juce::String(bestTotal) + " steps";
    }

    // Timestamped log line: "09:43:16 | INFO | CUDA GPU detected: ..."
    if (trimmed.length() > 12 && trimmed[2] == ':' && trimmed[5] == ':' && trimmed.contains("| INFO |"))
    {
        const int infoIdx = trimmed.indexOf("| INFO |");
        if (infoIdx >= 0)
        {
            juce::String payload = trimmed.substring(infoIdx + 8).trim();
            if (payload.startsWithIgnoreCase("CUDA") || payload.startsWithIgnoreCase("Loading"))
                return "loading model...";
            if (payload.startsWithIgnoreCase("[tiled_decode]") || payload.startsWithIgnoreCase("decode"))
                return "decoding audio...";
            // Generic log — show a shortened version
            if (payload.length() > 50)
                payload = payload.substring(0, 50) + "...";
            return payload;
        }
    }

    // Fallback: return trimmed raw text (sanitization in showStatusMessage handles the rest)
    if (trimmed.length() > 60)
        return trimmed.substring(0, 60) + "...";
    return trimmed;
}
}

juce::String Gary4juceAudioProcessorEditor::cleanCareyQueueMessage(const juce::String& raw)
{
    return cleanCareyQueueMessageText(raw);
}

void Gary4juceAudioProcessorEditor::updateCareyEnablementSnapshot()
{
    if (!careyUI)
        return;

    const auto garyDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("gary4juce");
    const bool hasInputAudio = savedSamples > 0 && garyDir.getChildFile("myBuffer.wav").existsAsFile();
    const bool backendLooksOnline = isServiceReachable(ServiceType::Carey);

    const bool canGenerate = backendLooksOnline && hasInputAudio;
    careyUI->setGenerateButtonEnabled(canGenerate, isGenerating);
}

double Gary4juceAudioProcessorEditor::getCareyBpmForRequest() const
{
    double bpm = currentCareyBpm;
    if (!(bpm > 0.0))
        bpm = audioProcessor.getCurrentBPM();
    if (!(bpm > 0.0))
        bpm = 120.0;
    return bpm;
}

bool Gary4juceAudioProcessorEditor::isCareyTabAvailable() const
{
    return true;
}

void Gary4juceAudioProcessorEditor::updateCareyTabAvailability()
{
    const bool available = isCareyTabAvailable();
    careyTabButton.setEnabled(available);

    if (careyUI)
    {
        careyUI->setCompleteRemoteModelSelectionEnabled(!audioProcessor.getIsUsingLocalhost());
        careyUI->setCoverModelSelectionEnabled(kCareyCoverModelExperimentEnabled);
        careyUI->setCoverRemoteModelSelectionEnabled(!audioProcessor.getIsUsingLocalhost());
        careyUI->setExtractRemoteGenerationEnabled(!audioProcessor.getIsUsingLocalhost());
    }

    if (currentTab == ModelTab::Carey)
        refreshCareyAvailableLoras(false);

    if (audioProcessor.getIsUsingLocalhost())
        careyTabButton.setTooltip("carey (ace-step 1.5) - localhost:8003");
    else
        careyTabButton.setTooltip("carey (ace-step lego) - remote backend");

    if (!available && currentTab == ModelTab::Carey)
        switchToTab(ModelTab::Gary);

    updateTabButtonStates();
    updateCareyEnablementSnapshot();
}

juce::String Gary4juceAudioProcessorEditor::getSelectedCareyCompleteLora() const
{
    if (!currentCareyCompleteUseLora)
        return {};

    const juce::String requestedLora = currentCareyCompleteLora.trim();
    if (requestedLora.isEmpty())
        return {};

    for (const auto& availableLora : availableCareyLoras)
    {
        if (availableLora.equalsIgnoreCase(requestedLora))
            return availableLora;
    }

    return {};
}

juce::String Gary4juceAudioProcessorEditor::getSelectedCareyCoverLora() const
{
    if (!currentCoverUseLora)
        return {};

    const juce::String requestedLora = currentCoverLora.trim();
    if (requestedLora.isEmpty())
        return {};

    for (const auto& availableLora : availableCareyLoras)
    {
        if (availableLora.equalsIgnoreCase(requestedLora))
            return availableLora;
    }

    return {};
}

void Gary4juceAudioProcessorEditor::syncCareyLoraUi()
{
    if (!careyUI)
        return;

    careyUI->setAvailableCompleteLoras(availableCareyLoras);
    careyUI->setCompleteSelectedLora(currentCareyCompleteLora);
    careyUI->setCompleteLoraScale(currentCareyCompleteLoraScale);
    careyUI->setCompleteUseLora(currentCareyCompleteUseLora);
    careyUI->setAvailableCoverLoras(availableCareyLoras);
    careyUI->setCoverSelectedLora(currentCoverLora);
    careyUI->setCoverLoraScale(currentCoverLoraScale);
    careyUI->setCoverUseLora(currentCoverUseLora);
}

void Gary4juceAudioProcessorEditor::refreshCareyAvailableLoras(bool force)
{
    const juce::String loraUrl = getServiceUrl(ServiceType::Carey, "/loras");
    const auto nowMs = juce::Time::getCurrentTime().toMilliseconds();

    if (!force
        && loraUrl == careyLoraFetchBackendUrl
        && nowMs - careyLoraLastFetchMs < 2000)
        return;

    if (careyLoraFetchInFlight.exchange(true))
        return;

    careyLoraFetchBackendUrl = loraUrl;
    careyLoraLastFetchMs = nowMs;
    const int fetchNonce = careyLoraFetchNonce.fetch_add(1) + 1;
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, fetchNonce, loraUrl]()
    {
        juce::StringArray fetchedLoras;
        bool success = false;

        try
        {
            juce::URL url(loraUrl);
            int statusCode = 0;
            auto requestOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withHttpRequestCmd("GET")
                .withConnectionTimeoutMs(15000)
                .withStatusCode(&statusCode)
                .withExtraHeaders("Accept: application/json");

            auto stream = url.createInputStream(requestOptions);
            if (stream != nullptr && statusCode < 400)
            {
                const juce::String responseText = stream->readEntireStreamAsString();
                const juce::var responseVar = juce::JSON::parse(responseText);
                if (auto* responseObj = responseVar.getDynamicObject())
                {
                    for (const auto& entry : responseObj->getProperties())
                    {
                        const juce::String loraName = entry.name.toString().trim();
                        if (loraName.isNotEmpty() && !fetchedLoras.contains(loraName))
                            fetchedLoras.add(loraName);
                    }

                    fetchedLoras.sortNatural();
                    success = true;
                }
            }
        }
        catch (const std::exception& e)
        {
            DBG("[carey-lora] failed to fetch /loras: " + juce::String(e.what()));
        }
        catch (...)
        {
            DBG("[carey-lora] failed to fetch /loras");
        }

        juce::MessageManager::callAsync([asyncAlive, editor, fetchNonce, loraUrl, fetchedLoras, success]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            editor->careyLoraFetchInFlight.store(false);

            if (editor->careyLoraFetchNonce.load() != fetchNonce)
                return;

            if (editor->getServiceUrl(ServiceType::Carey, "/loras") != loraUrl)
                return;

            editor->availableCareyLoras = success ? fetchedLoras : juce::StringArray();

            juce::String resolvedLora;
            for (const auto& availableLora : editor->availableCareyLoras)
            {
                if (availableLora.equalsIgnoreCase(editor->currentCareyCompleteLora))
                {
                    resolvedLora = availableLora;
                    break;
                }
            }

            if (resolvedLora.isNotEmpty())
                editor->currentCareyCompleteLora = resolvedLora;
            else if (!editor->availableCareyLoras.isEmpty())
                editor->currentCareyCompleteLora = editor->availableCareyLoras[0];
            else
                editor->currentCareyCompleteLora = {};

            juce::String resolvedCoverLora;
            for (const auto& availableLora : editor->availableCareyLoras)
            {
                if (availableLora.equalsIgnoreCase(editor->currentCoverLora))
                {
                    resolvedCoverLora = availableLora;
                    break;
                }
            }

            if (resolvedCoverLora.isNotEmpty())
                editor->currentCoverLora = resolvedCoverLora;
            else if (!editor->availableCareyLoras.isEmpty())
                editor->currentCoverLora = editor->availableCareyLoras[0];
            else
                editor->currentCoverLora = {};

            if (editor->availableCareyLoras.isEmpty())
            {
                editor->currentCareyCompleteUseLora = false;
                editor->currentCoverUseLora = false;
            }

            editor->syncCareyLoraUi();
        });
    });
}

void Gary4juceAudioProcessorEditor::requestCareyCompleteCaption()
{
    if (!isServiceReachable(ServiceType::Carey))
    {
        showStatusMessage("carey not reachable - check connection first");
        return;
    }

    const juce::String requestedLora = getSelectedCareyCompleteLora();
    const juce::String captionsUrl = getServiceUrl(ServiceType::Carey, "/captions");
    const int requestNonce = careyCaptionRequestNonce.fetch_add(1) + 1;
    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis(this);

    showStatusMessage(requestedLora.isNotEmpty()
        ? ("loading " + requestedLora + " caption...")
        : "loading carey caption...",
        1500);

    juce::Thread::launch([safeThis, requestNonce, requestedLora, captionsUrl]()
    {
        if (safeThis == nullptr)
            return;

        juce::String caption;
        juce::String failureReason = requestedLora.isNotEmpty()
            ? ("failed to load " + requestedLora + " caption")
            : "failed to load carey caption";

        auto extractErrorMessage = [](const juce::var& responseVar) -> juce::String
        {
            if (auto* responseObj = responseVar.getDynamicObject())
            {
                const juce::String message = responseObj->getProperty("detail").toString().trim();
                if (message.isNotEmpty())
                    return message;

                const juce::String error = responseObj->getProperty("error").toString().trim();
                if (error.isNotEmpty())
                    return error;

                const juce::String statusMessage = responseObj->getProperty("message").toString().trim();
                if (statusMessage.isNotEmpty())
                    return statusMessage;
            }

            return {};
        };

        try
        {
            juce::URL url(captionsUrl);
            if (requestedLora.isNotEmpty())
                url = url.withParameter("lora", requestedLora);

            int statusCode = 0;
            auto requestOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withHttpRequestCmd("GET")
                .withConnectionTimeoutMs(15000)
                .withStatusCode(&statusCode)
                .withExtraHeaders("Accept: application/json");

            auto stream = url.createInputStream(requestOptions);
            if (stream != nullptr)
            {
                const juce::String responseText = stream->readEntireStreamAsString();
                const juce::var responseVar = juce::JSON::parse(responseText);

                if (statusCode < 400)
                {
                    if (auto* responseObj = responseVar.getDynamicObject())
                        caption = responseObj->getProperty("caption").toString().trim();

                    if (caption.isEmpty())
                        failureReason = "carey caption response was missing a caption";
                }
                else
                {
                    const juce::String serverMessage = extractErrorMessage(responseVar);
                    if (serverMessage.isNotEmpty())
                        failureReason = serverMessage;
                }
            }
        }
        catch (const std::exception& e)
        {
            failureReason = "failed to load carey caption: " + juce::String(e.what());
        }
        catch (...)
        {
            failureReason = "failed to load carey caption";
        }

        juce::MessageManager::callAsync([safeThis, requestNonce, captionsUrl, caption, failureReason]()
        {
            if (safeThis == nullptr)
                return;

            if (safeThis->careyCaptionRequestNonce.load() != requestNonce)
                return;

            if (safeThis->getServiceUrl(ServiceType::Carey, "/captions") != captionsUrl)
                return;

            if (caption.isNotEmpty())
            {
                safeThis->currentCareyCompleteCaption = caption;
                if (safeThis->careyUI)
                    safeThis->careyUI->setCompleteCaptionText(caption);
                return;
            }

            safeThis->showStatusMessage(failureReason, 3500);
        });
    });
}

void Gary4juceAudioProcessorEditor::requestCareyCoverCaption()
{
    if (!isServiceReachable(ServiceType::Carey))
    {
        showStatusMessage("carey not reachable - check connection first");
        return;
    }

    const juce::String requestedLora = getSelectedCareyCoverLora();
    const juce::String captionsUrl = getServiceUrl(ServiceType::Carey, "/captions");
    const int requestNonce = careyCoverCaptionRequestNonce.fetch_add(1) + 1;
    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis(this);

    showStatusMessage(requestedLora.isNotEmpty()
        ? ("loading " + requestedLora + " cover caption...")
        : "loading carey cover caption...",
        1500);

    juce::Thread::launch([safeThis, requestNonce, requestedLora, captionsUrl]()
    {
        if (safeThis == nullptr)
            return;

        juce::String caption;
        juce::String failureReason = requestedLora.isNotEmpty()
            ? ("failed to load " + requestedLora + " cover caption")
            : "failed to load carey cover caption";

        auto extractErrorMessage = [](const juce::var& responseVar) -> juce::String
        {
            if (auto* responseObj = responseVar.getDynamicObject())
            {
                const juce::String message = responseObj->getProperty("detail").toString().trim();
                if (message.isNotEmpty())
                    return message;

                const juce::String error = responseObj->getProperty("error").toString().trim();
                if (error.isNotEmpty())
                    return error;

                const juce::String statusMessage = responseObj->getProperty("message").toString().trim();
                if (statusMessage.isNotEmpty())
                    return statusMessage;
            }

            return {};
        };

        try
        {
            juce::URL url(captionsUrl);
            if (requestedLora.isNotEmpty())
                url = url.withParameter("lora", requestedLora);

            int statusCode = 0;
            auto requestOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withHttpRequestCmd("GET")
                .withConnectionTimeoutMs(15000)
                .withStatusCode(&statusCode)
                .withExtraHeaders("Accept: application/json");

            auto stream = url.createInputStream(requestOptions);
            if (stream != nullptr)
            {
                const juce::String responseText = stream->readEntireStreamAsString();
                const juce::var responseVar = juce::JSON::parse(responseText);

                if (statusCode < 400)
                {
                    if (auto* responseObj = responseVar.getDynamicObject())
                        caption = responseObj->getProperty("caption").toString().trim();

                    if (caption.isEmpty())
                        failureReason = "carey cover caption response was missing a caption";
                }
                else
                {
                    const juce::String serverMessage = extractErrorMessage(responseVar);
                    if (serverMessage.isNotEmpty())
                        failureReason = serverMessage;
                }
            }
        }
        catch (const std::exception& e)
        {
            failureReason = "failed to load carey cover caption: " + juce::String(e.what());
        }
        catch (...)
        {
            failureReason = "failed to load carey cover caption";
        }

        juce::MessageManager::callAsync([safeThis, requestNonce, captionsUrl, caption, failureReason]()
        {
            if (safeThis == nullptr)
                return;

            if (safeThis->careyCoverCaptionRequestNonce.load() != requestNonce)
                return;

            if (safeThis->getServiceUrl(ServiceType::Carey, "/captions") != captionsUrl)
                return;

            if (caption.isNotEmpty())
            {
                safeThis->currentCoverCaption = caption;
                if (safeThis->careyUI)
                    safeThis->careyUI->setCoverCaptionText(caption);
                return;
            }

            safeThis->showStatusMessage(failureReason, 3500);
        });
    });
}

void Gary4juceAudioProcessorEditor::sendToCarey()
{
    if (!isServiceReachable(ServiceType::Carey))
    {
        showStatusMessage("carey not reachable - check connection first");
        return;
    }

    const int requestNonce = careyRequestNonce.fetch_add(1) + 1;

    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");
    const juce::File bufferFile = garyDir.getChildFile("myBuffer.wav");

    if (!bufferFile.existsAsFile())
    {
        showStatusMessage("missing myBuffer.wav - save your recording first");
        return;
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(bufferFile));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
    {
        showStatusMessage("failed to read myBuffer.wav for carey");
        return;
    }

    juce::String trackName = currentCareyTrackName.trim().toLowerCase();
    static const juce::StringArray kAllowedCareyTracks = {
        "vocals", "backing_vocals", "drums", "bass", "guitar", "piano",
        "strings", "synth", "keyboard", "percussion", "brass", "woodwinds"
    };
    if (!kAllowedCareyTracks.contains(trackName))
        trackName = "vocals";

    const int bpm = juce::jmax(1, juce::roundToInt(getCareyBpmForRequest()));
    const int inferenceSteps = juce::jmax(1, currentCareySteps);
    const double guidanceScale = juce::jlimit(3.0, 10.0, currentLegoCfg);
    const juce::String caption = currentCareyCaption.trim();
    const juce::String lyrics = currentCareyLyrics;
    const juce::String keyScale = currentCareyKeyScale;
    const juce::String timeSig = currentCareyTimeSig;
    const juce::String language = currentCareyLanguage;
    const bool loopAssistEnabled = currentCareyLoopAssistEnabled;
    const bool trimToInputEnabled = currentCareyTrimToInputEnabled;

    DBG("[carey] remote request metas - bpm=" + juce::String(bpm)
        + ", steps=" + juce::String(inferenceSteps)
        + ", track=" + trackName
        + ", key_scale=" + (keyScale.isEmpty() ? "none" : keyScale)
        + ", time_sig=" + (timeSig.isEmpty() ? "auto" : timeSig)
        + ", caption_empty=" + juce::String(caption.isEmpty() ? "true" : "false")
        + ", lyrics_empty=" + juce::String(lyrics.trim().isEmpty() ? "true" : "false")
        + ", loop_assist=" + juce::String(loopAssistEnabled ? "on" : "off")
        + ", trim_to_input=" + juce::String(trimToInputEnabled ? "on" : "off"));

    const auto cancelCareyOperation = [this, requestNonce]()
    {
        if (careyRequestNonce.load() != requestNonce)
            return;

        isGenerating = false;
        isCurrentlyQueued = false;
        generationProgress = 0;
        smoothProgressAnimation = false;
        setCareyWaveformState(0, false);
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
        repaint();
    };

    setActiveOp(ActiveOp::CareyGenerate);
    isGenerating = true;
    isCurrentlyQueued = true;
    generationProgress = 0;
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;
    setCareyWaveformState(0, true);
    audioProcessor.setUndoTransformAvailable(false);
    audioProcessor.setRetryAvailable(false);
    updateRetryButtonState();
    updateContinueButtonState();
    updateAllGenerationButtonStates();
    repaint();
    showStatusMessage("submitting carey request...", 2500);
    const juce::String submitUrlText = getServiceUrl(ServiceType::Carey, "/lego");
    const juce::String statusUrlPrefix = getServiceUrl(ServiceType::Carey, "/lego/status/");
    const bool allowTextProgressFallback = !audioProcessor.getIsUsingLocalhost();
    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis(this);
    const auto generationToken = beginGenerationAsyncWork();

    juce::Thread::launch([safeThis, generationToken, requestNonce, bufferFile, caption, lyrics, keyScale, timeSig, language, trackName, bpm, inferenceSteps, guidanceScale, loopAssistEnabled, trimToInputEnabled, submitUrlText, statusUrlPrefix, allowTextProgressFallback]()
    {
        auto isRequestCurrent = [safeThis, generationToken, requestNonce]() {
            return safeThis != nullptr
                && safeThis->isGenerationAsyncWorkCurrent(generationToken)
                && safeThis->careyRequestNonce.load() == requestNonce;
        };

        if (!isRequestCurrent())
        {
            DBG("[carey] request aborted - generation stopped");
            return;
        }

        juce::String failureReason;
        juce::String failureDetail;
        juce::String downloadedBase64;
        bool success = false;
        double originalInputDurationSeconds = 0.0;

        do
        {
            try
            {
                juce::AudioFormatManager threadFormatManager;
                threadFormatManager.registerBasicFormats();

                std::unique_ptr<juce::AudioFormatReader> threadReader(threadFormatManager.createReaderFor(bufferFile));
                if (threadReader == nullptr || threadReader->sampleRate <= 0.0 || threadReader->lengthInSamples <= 0)
                {
                    failureReason = "failed to read myBuffer.wav for carey request";
                    break;
                }

                const int sourceChannels = juce::jmax(1, (int)threadReader->numChannels);
                const int sourceSamples = juce::jmax(1, (int)threadReader->lengthInSamples);
                const double sourceSampleRate = threadReader->sampleRate;
                originalInputDurationSeconds = (double)sourceSamples / sourceSampleRate;

                juce::AudioBuffer<float> sourceBuffer(sourceChannels, sourceSamples);
                const bool readOk = threadReader->read(&sourceBuffer, 0, sourceSamples, 0, true, true);
                if (!readOk)
                {
                    failureReason = "failed to decode myBuffer.wav for carey request";
                    break;
                }

                juce::AudioBuffer<float>* conditioningBuffer = &sourceBuffer;
                juce::AudioBuffer<float> loopAssistBuffer;

                if (loopAssistEnabled)
                {
                    constexpr double kMinContextSeconds = 120.0;  // guarantee at least 2 minutes of context
                    const double bpmSafe = juce::jmax(1.0, (double)bpm);
                    const double secondsPerBar = 240.0 / bpmSafe;
                    const int barsForMinContext = (int)std::ceil(kMinContextSeconds / secondsPerBar);
                    const int kLoopAssistBars = juce::jmax(32, barsForMinContext);
                    const double contextWindowSeconds = secondsPerBar * (double)kLoopAssistBars;
                    const int targetSamples = juce::jmax(1, juce::roundToInt(contextWindowSeconds * sourceSampleRate));

                    // Skip loop assist if input already fills the context window
                    if (originalInputDurationSeconds >= contextWindowSeconds)
                    {
                        DBG("[carey] loop assist skipped: input (" + juce::String(originalInputDurationSeconds, 1)
                            + "s) already fills " + juce::String(kLoopAssistBars) + "-bar context ("
                            + juce::String(contextWindowSeconds, 1) + "s)");
                    }
                    else if (targetSamples != sourceSamples)
                    {
                        // Snap source to nearest whole bar count for seamless looping
                        const double barsInInput = originalInputDurationSeconds / secondsPerBar;
                        int snappedBars = juce::jmax(1, juce::roundToInt(barsInInput));
                        int snappedSamples = juce::jmax(1, juce::roundToInt((double)snappedBars * secondsPerBar * sourceSampleRate));

                        // If rounding up exceeds actual audio, floor instead
                        if (snappedSamples > sourceSamples)
                        {
                            snappedBars = juce::jmax(1, (int)std::floor(barsInInput));
                            snappedSamples = juce::jmin(sourceSamples,
                                juce::jmax(1, juce::roundToInt((double)snappedBars * secondsPerBar * sourceSampleRate)));
                        }

                        DBG("[carey] loop assist bar-snap: " + juce::String(barsInInput, 2) + " bars -> "
                            + juce::String(snappedBars) + " bars (" + juce::String(snappedSamples)
                            + " of " + juce::String(sourceSamples) + " samples)");

                        loopAssistBuffer.setSize(sourceChannels, targetSamples, false, true, true);
                        for (int channel = 0; channel < sourceChannels; ++channel)
                        {
                            const float* src = sourceBuffer.getReadPointer(channel);
                            float* dst = loopAssistBuffer.getWritePointer(channel);
                            for (int i = 0; i < targetSamples; ++i)
                                dst[i] = src[i % snappedSamples];
                        }

                        conditioningBuffer = &loopAssistBuffer;
                        const double targetDuration = (double)targetSamples / sourceSampleRate;
                        DBG("[carey] loop assist normalized context to " + juce::String(kLoopAssistBars)
                            + " bars (" + juce::String(targetDuration, 2) + "s)");
                    }
                }

                juce::MemoryBlock sourceAudioData;
                if (conditioningBuffer == &sourceBuffer)
                {
                    if (!bufferFile.loadFileAsData(sourceAudioData) || sourceAudioData.getSize() <= 0)
                    {
                        failureReason = "failed to load myBuffer.wav bytes for carey request";
                        break;
                    }
                }
                else
                {
                    const juce::File tempWav = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("carey-loop-assist-", ".wav", false);

                    {
                        std::unique_ptr<juce::FileOutputStream> stream(tempWav.createOutputStream());
                        if (stream == nullptr)
                        {
                            failureReason = "failed to create temporary carey conditioning file";
                            break;
                        }

                        juce::WavAudioFormat wavFormat;
                        std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
                            stream.release(),
                            sourceSampleRate,
                            (unsigned int)conditioningBuffer->getNumChannels(),
                            16, {}, 0));

                        if (writer == nullptr)
                        {
                            tempWav.deleteFile();
                            failureReason = "failed to encode loop-assist conditioning wav";
                            break;
                        }

                        const bool writeOk = writer->writeFromAudioSampleBuffer(
                            *conditioningBuffer, 0, conditioningBuffer->getNumSamples());
                        writer.reset();

                        if (!writeOk)
                        {
                            tempWav.deleteFile();
                            failureReason = "failed to write loop-assist conditioning wav";
                            break;
                        }
                    }

                    if (!tempWav.loadFileAsData(sourceAudioData) || sourceAudioData.getSize() <= 0)
                    {
                        tempWav.deleteFile();
                        failureReason = "failed to read loop-assist conditioning bytes";
                        break;
                    }

                    tempWav.deleteFile();
                }

                const juce::String sourceAudioBase64 = juce::Base64::toBase64(sourceAudioData.getData(), sourceAudioData.getSize());
                if (sourceAudioBase64.isEmpty())
                {
                    failureReason = "failed to base64 encode carey conditioning audio";
                    break;
                }

                if (!isRequestCurrent())
                    return;

                juce::URL submitUrl(submitUrlText);

                juce::DynamicObject::Ptr submitPayload = new juce::DynamicObject();
                submitPayload->setProperty("audio_data", sourceAudioBase64);
                submitPayload->setProperty("track_name", trackName);
                submitPayload->setProperty("bpm", bpm);
                submitPayload->setProperty("inference_steps", inferenceSteps);
                submitPayload->setProperty("caption", caption);
                submitPayload->setProperty("lyrics", lyrics);
                if (keyScale.isNotEmpty())
                    submitPayload->setProperty("key_scale", keyScale);
                if (language.isNotEmpty() && language != "en")
                    submitPayload->setProperty("language", language);
                submitPayload->setProperty("guidance_scale", guidanceScale);
                if (timeSig.isNotEmpty())
                    submitPayload->setProperty("time_signature", timeSig);
                submitPayload->setProperty("batch_size", 1);
                submitPayload->setProperty("audio_format", "wav");

                const juce::String submitJson = juce::JSON::toString(juce::var(submitPayload.get()));
                juce::URL submitPost = submitUrl.withPOSTData(submitJson);

                int submitStatusCode = 0;
                auto submitOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withHttpRequestCmd("POST")
                    .withConnectionTimeoutMs(120000)
                    .withStatusCode(&submitStatusCode)
                    .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json");

                auto submitStream = submitPost.createInputStream(submitOptions);
                if (submitStream == nullptr || submitStatusCode >= 400)
                {
                    failureReason = "carey /lego submit failed";
                    break;
                }

                const juce::String submitResponse = submitStream->readEntireStreamAsString();
                const juce::var submitVar = juce::JSON::parse(submitResponse);
                auto* submitObj = submitVar.getDynamicObject();
                if (submitObj == nullptr)
                {
                    failureReason = "invalid carey submit response";
                    break;
                }

                juce::String taskId = submitObj->getProperty("task_id").toString();
                if (taskId.isEmpty())
                {
                    if (auto* dataObj = submitObj->getProperty("data").getDynamicObject())
                        taskId = dataObj->getProperty("task_id").toString();
                }

                const bool submitSuccess = (bool)submitObj->getProperty("success");
                if (taskId.isEmpty() || !submitSuccess)
                {
                    const juce::String errorText = submitObj->getProperty("error").toString().trim();
                    failureReason = errorText.isNotEmpty() ? errorText : "missing task_id from carey submit response";
                    break;
                }

                if (!isRequestCurrent())
                    return;

                juce::URL queryUrl(statusUrlPrefix + taskId);
                constexpr int kPollIntervalMs = 1500;
                constexpr int kMaxPollCount = 600;
                int lastResolvedProgress = 0;

                for (int pollCount = 0; pollCount < kMaxPollCount; ++pollCount)
                {
                    if (!isRequestCurrent())
                        return;

                    int queryStatusCode = 0;
                    auto queryOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                        .withHttpRequestCmd("GET")
                        .withConnectionTimeoutMs(15000)
                        .withStatusCode(&queryStatusCode)
                        .withExtraHeaders("Accept: application/json");

                    auto queryStream = queryUrl.createInputStream(queryOptions);
                    if (queryStream == nullptr || queryStatusCode >= 400)
                    {
                        failureReason = "carey status polling failed";
                        break;
                    }

                    const juce::String queryResponse = queryStream->readEntireStreamAsString();
                    const juce::var queryVar = juce::JSON::parse(queryResponse);
                    auto* queryObj = queryVar.getDynamicObject();
                    if (queryObj == nullptr)
                    {
                        failureReason = "invalid carey status response";
                        break;
                    }

                    const juce::String status = queryObj->getProperty("status").toString().trim().toLowerCase();
                    const bool querySuccess = (bool)queryObj->getProperty("success");
                    const bool generationInProgress = (bool)queryObj->getProperty("generation_in_progress");
                    const bool transformInProgress = (bool)queryObj->getProperty("transform_in_progress");

                    if (status == "failed")
                    {
                        const auto failureInfo = parseCareyFailureResponse(queryVar, "carey generation failed");
                        failureReason = failureInfo.userMessage;
                        failureDetail = failureInfo.popupDetail;
                        break;
                    }

                    if (!querySuccess && !(generationInProgress || transformInProgress))
                    {
                        const auto failureInfo = parseCareyFailureResponse(queryVar, "carey request failed");
                        failureReason = failureInfo.userMessage;
                        failureDetail = failureInfo.popupDetail;
                        break;
                    }

                    if (status == "completed")
                    {
                        downloadedBase64 = queryObj->getProperty("audio_data").toString();
                        if (downloadedBase64.isEmpty())
                        {
                            failureReason = "carey completed without audio_data";
                            break;
                        }
                        success = true;
                        break;
                    }

                    juce::String queueStatus, queueMessage;
                    if (auto* queueObj = queryObj->getProperty("queue_status").getDynamicObject())
                    {
                        queueStatus = queueObj->getProperty("status").toString().trim().toLowerCase();
                        queueMessage = queueObj->getProperty("message").toString().trim();
                    }

                    queueMessage = stripAnsiAndControlChars(queueMessage);
                    const int progressPercent = resolveCareyProgressPercent(queryObj->getProperty("progress"),
                                                                           queueMessage,
                                                                           allowTextProgressFallback,
                                                                           lastResolvedProgress);
                    lastResolvedProgress = juce::jmax(lastResolvedProgress, progressPercent);

                    bool queued = (queueStatus == "queued");
                    if (progressPercent > 0 || queueStatus == "ready") queued = false;
                    juce::String statusText = cleanCareyQueueMessageText(queueMessage);
                    if (statusText.isEmpty())
                    {
                        if (queued) statusText = "queued - starting soon...";
                        else if (progressPercent > 0) statusText = "processing";
                        else statusText = status.isNotEmpty() ? status : "processing";
                    }

                    juce::MessageManager::callAsync([safeThis, generationToken, requestNonce, progressPercent, queued, statusText]()
                    {
                        if (safeThis == nullptr
                            || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                            || safeThis->careyRequestNonce.load() != requestNonce)
                            return;

                        safeThis->smoothProgressAnimation = false;
                        safeThis->generationProgress = progressPercent;
                        safeThis->isCurrentlyQueued = queued;
                        safeThis->setCareyWaveformState(progressPercent, queued);
                        if (statusText.isNotEmpty())
                            safeThis->showStatusMessage("carey: " + statusText, 2500);
                        safeThis->repaint();
                    });

                    juce::Thread::sleep(kPollIntervalMs);
                }

                if (!success && failureReason.isEmpty())
                    failureReason = "carey generation timed out";
            }
            catch (const std::exception& e)
            {
                failureReason = "carey request failed: " + juce::String(e.what());
            }
            catch (...)
            {
                failureReason = "carey request failed with unknown error";
            }
        } while (false);

        if (success && downloadedBase64.isNotEmpty())
        {
            juce::String finalBase64 = downloadedBase64;

            if (trimToInputEnabled && originalInputDurationSeconds > 0.0)
            {
                juce::MemoryOutputStream decodedOutputBytes;
                if (!juce::Base64::convertFromBase64(decodedOutputBytes, downloadedBase64))
                {
                    DBG("[carey] trim-to-input skipped: failed to decode generated audio");
                }
                else
                {
                    const juce::MemoryBlock& decodedData = decodedOutputBytes.getMemoryBlock();
                    const juce::File tempInput = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("carey-trim-input-", ".wav", false);
                    const juce::File tempOutput = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("carey-trim-output-", ".wav", false);

                    const auto cleanupTemps = [&tempInput, &tempOutput]()
                    {
                        tempInput.deleteFile();
                        tempOutput.deleteFile();
                    };

                    do
                    {
                        if (!tempInput.replaceWithData(decodedData.getData(), decodedData.getSize()))
                        {
                            DBG("[carey] trim-to-input skipped: failed to stage generated wav");
                            break;
                        }

                        juce::AudioFormatManager trimFormatManager;
                        trimFormatManager.registerBasicFormats();
                        std::unique_ptr<juce::AudioFormatReader> trimReader(trimFormatManager.createReaderFor(tempInput));
                        if (trimReader == nullptr || trimReader->sampleRate <= 0.0 || trimReader->lengthInSamples <= 0)
                        {
                            DBG("[carey] trim-to-input skipped: failed to read generated wav");
                            break;
                        }

                        const int generatedSamples = juce::jmax(1, (int)trimReader->lengthInSamples);
                        const int targetSamples = juce::jlimit(1, generatedSamples,
                            juce::roundToInt(originalInputDurationSeconds * trimReader->sampleRate));

                        if (targetSamples >= generatedSamples) break;

                        const int trimChannels = juce::jmax(1, (int)trimReader->numChannels);
                        juce::AudioBuffer<float> trimmedBuffer(trimChannels, targetSamples);
                        if (!trimReader->read(&trimmedBuffer, 0, targetSamples, 0, true, true))
                        {
                            DBG("[carey] trim-to-input skipped: failed to decode generated samples for trimming");
                            break;
                        }

                        std::unique_ptr<juce::FileOutputStream> trimStream(tempOutput.createOutputStream());
                        if (trimStream == nullptr)
                        {
                            DBG("[carey] trim-to-input skipped: failed to create output wav");
                            break;
                        }

                        juce::WavAudioFormat trimWavFormat;
                        std::unique_ptr<juce::AudioFormatWriter> trimWriter(trimWavFormat.createWriterFor(
                            trimStream.release(), trimReader->sampleRate,
                            (unsigned int)trimChannels, 16, {}, 0));

                        if (trimWriter == nullptr)
                        {
                            DBG("[carey] trim-to-input skipped: failed to initialize output wav writer");
                            break;
                        }

                        const bool trimWriteOk = trimWriter->writeFromAudioSampleBuffer(trimmedBuffer, 0, targetSamples);
                        trimWriter.reset();
                        if (!trimWriteOk)
                        {
                            DBG("[carey] trim-to-input skipped: failed to write trimmed wav");
                            break;
                        }

                        juce::MemoryBlock trimmedData;
                        if (!tempOutput.loadFileAsData(trimmedData) || trimmedData.getSize() == 0)
                        {
                            DBG("[carey] trim-to-input skipped: failed to read trimmed wav");
                            break;
                        }

                        finalBase64 = juce::Base64::toBase64(trimmedData.getData(), trimmedData.getSize());
                        DBG("[carey] trim-to-input applied: " + juce::String((double)generatedSamples / trimReader->sampleRate, 2)
                            + "s -> " + juce::String((double)targetSamples / trimReader->sampleRate, 2) + "s");
                    } while (false);

                    cleanupTemps();
                }
            }

            juce::MessageManager::callAsync([safeThis, generationToken, requestNonce, finalBase64]()
            {
                if (safeThis == nullptr
                    || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                    || safeThis->careyRequestNonce.load() != requestNonce)
                    return;

                safeThis->isCurrentlyQueued = false;
                safeThis->generationProgress = 100;
                safeThis->smoothProgressAnimation = false;
                safeThis->setCareyWaveformState(100, false);
                safeThis->setActiveOp(ActiveOp::None);
                safeThis->saveGeneratedAudio(finalBase64);
                safeThis->showStatusMessage("carey generation complete", 2500);
                safeThis->updateAllGenerationButtonStates();
            });
            return;
        }

        const juce::String finalError = failureReason.isNotEmpty() ? failureReason : "carey request failed";
        const juce::String popupDetail = failureDetail.isNotEmpty() ? failureDetail : finalError;
        juce::MessageManager::callAsync([safeThis, generationToken, requestNonce, finalError, popupDetail]()
        {
            if (safeThis == nullptr
                || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                || safeThis->careyRequestNonce.load() != requestNonce)
                return;

            auto cancelCareyOperation = [safeThis, generationToken, requestNonce]()
            {
                if (safeThis == nullptr
                    || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                    || safeThis->careyRequestNonce.load() != requestNonce)
                    return;

                safeThis->isGenerating = false;
                safeThis->isCurrentlyQueued = false;
                safeThis->generationProgress = 0;
                safeThis->smoothProgressAnimation = false;
                safeThis->setCareyWaveformState(0, false);
                safeThis->setActiveOp(ActiveOp::None);
                safeThis->updateAllGenerationButtonStates();
                safeThis->repaint();
            };

            safeThis->showStatusMessage(finalError, 4500);
            if (safeThis->shouldShowGenerationFailureDialog(popupDetail))
                safeThis->showGenerationFailureDialog(popupDetail);
            cancelCareyOperation();
        });
    });
}

void Gary4juceAudioProcessorEditor::sendToCareyExtract()
{
    if (audioProcessor.getIsUsingLocalhost())
    {
        showStatusMessage("carey extract is remote-only for now");
        return;
    }

    if (!isServiceReachable(ServiceType::Carey))
    {
        showStatusMessage("carey not reachable - check connection first");
        return;
    }

    const int requestNonce = careyRequestNonce.fetch_add(1) + 1;

    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");
    const juce::File bufferFile = garyDir.getChildFile("myBuffer.wav");

    if (!bufferFile.existsAsFile())
    {
        showStatusMessage("missing myBuffer.wav - save your recording first");
        return;
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(bufferFile));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
    {
        showStatusMessage("failed to read myBuffer.wav for carey extract");
        return;
    }

    juce::String trackName = currentCareyExtractTrackName.trim().toLowerCase();
    static const juce::StringArray kAllowedExtractTracks = {
        "drums", "vocals", "backing_vocals", "bass", "guitar", "piano",
        "synth", "keyboard", "strings", "percussion", "brass", "woodwinds"
    };
    if (!kAllowedExtractTracks.contains(trackName))
        trackName = "drums";

    int bpm = juce::jlimit(20, 300, juce::roundToInt(getCareyBpmForRequest()));
    if (juce::JUCEApplicationBase::isStandaloneApp())
        bpm = juce::jlimit(20, 300, currentCareyExtractBpm);
    const int inferenceSteps = juce::jlimit(32, 100, currentCareyExtractSteps);
    const double guidanceScale = juce::jlimit(3.0, 10.0, currentCareyExtractCfg);

    DBG("[carey-extract] remote request metas - bpm=" + juce::String(bpm)
        + ", steps=" + juce::String(inferenceSteps)
        + ", track=" + trackName
        + ", cfg=" + juce::String(guidanceScale, 1));

    const auto cancelCareyOperation = [this, requestNonce]()
    {
        if (careyRequestNonce.load() != requestNonce)
            return;

        isGenerating = false;
        isCurrentlyQueued = false;
        generationProgress = 0;
        smoothProgressAnimation = false;
        setCareyWaveformState(0, false);
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
        repaint();
    };

    setActiveOp(ActiveOp::CareyGenerate);
    isGenerating = true;
    isCurrentlyQueued = true;
    generationProgress = 0;
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;
    setCareyWaveformState(0, true);
    audioProcessor.setUndoTransformAvailable(false);
    audioProcessor.setRetryAvailable(false);
    updateRetryButtonState();
    updateContinueButtonState();
    updateAllGenerationButtonStates();
    repaint();
    showStatusMessage("submitting carey extract request...", 2500);
    const juce::String submitUrlText = getServiceUrl(ServiceType::Carey, "/extract");
    const juce::String statusUrlPrefix = getServiceUrl(ServiceType::Carey, "/extract/status/");
    const bool allowTextProgressFallback = !audioProcessor.getIsUsingLocalhost();
    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis(this);
    const auto generationToken = beginGenerationAsyncWork();

    juce::Thread::launch([safeThis, generationToken, requestNonce, bufferFile, trackName, bpm, inferenceSteps, guidanceScale, submitUrlText, statusUrlPrefix, allowTextProgressFallback]()
    {
        auto isRequestCurrent = [safeThis, generationToken, requestNonce]() {
            return safeThis != nullptr
                && safeThis->isGenerationAsyncWorkCurrent(generationToken)
                && safeThis->careyRequestNonce.load() == requestNonce;
        };

        if (!isRequestCurrent())
        {
            DBG("[carey-extract] request aborted - generation stopped");
            return;
        }

        juce::String failureReason;
        juce::String failureDetail;
        juce::String downloadedBase64;
        bool success = false;

        do
        {
            try
            {
                juce::MemoryBlock sourceAudioData;
                if (!bufferFile.loadFileAsData(sourceAudioData) || sourceAudioData.getSize() <= 0)
                {
                    failureReason = "failed to load myBuffer.wav bytes for carey extract request";
                    break;
                }

                const juce::String sourceAudioBase64 = juce::Base64::toBase64(sourceAudioData.getData(), sourceAudioData.getSize());
                if (sourceAudioBase64.isEmpty())
                {
                    failureReason = "failed to base64 encode carey extract audio";
                    break;
                }

                if (!isRequestCurrent())
                    return;

                juce::URL submitUrl(submitUrlText);

                juce::DynamicObject::Ptr submitPayload = new juce::DynamicObject();
                submitPayload->setProperty("audio_data", sourceAudioBase64);
                submitPayload->setProperty("track_name", trackName);
                submitPayload->setProperty("bpm", bpm);
                submitPayload->setProperty("guidance_scale", guidanceScale);
                submitPayload->setProperty("inference_steps", inferenceSteps);
                submitPayload->setProperty("batch_size", 1);
                submitPayload->setProperty("audio_format", "wav");

                const juce::String submitJson = juce::JSON::toString(juce::var(submitPayload.get()));
                juce::URL submitPost = submitUrl.withPOSTData(submitJson);

                int submitStatusCode = 0;
                auto submitOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withHttpRequestCmd("POST")
                    .withConnectionTimeoutMs(120000)
                    .withStatusCode(&submitStatusCode)
                    .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json");

                auto submitStream = submitPost.createInputStream(submitOptions);
                if (submitStream == nullptr || submitStatusCode >= 400)
                {
                    failureReason = "carey /extract submit failed";
                    break;
                }

                const juce::String submitResponse = submitStream->readEntireStreamAsString();
                const juce::var submitVar = juce::JSON::parse(submitResponse);
                auto* submitObj = submitVar.getDynamicObject();
                if (submitObj == nullptr)
                {
                    failureReason = "invalid carey extract submit response";
                    break;
                }

                juce::String taskId = submitObj->getProperty("task_id").toString();
                if (taskId.isEmpty())
                {
                    if (auto* dataObj = submitObj->getProperty("data").getDynamicObject())
                        taskId = dataObj->getProperty("task_id").toString();
                }

                const bool submitSuccess = (bool)submitObj->getProperty("success");
                if (taskId.isEmpty() || !submitSuccess)
                {
                    const juce::String errorText = submitObj->getProperty("error").toString().trim();
                    failureReason = errorText.isNotEmpty() ? errorText : "missing task_id from carey extract submit response";
                    break;
                }

                if (!isRequestCurrent())
                    return;

                juce::URL queryUrl(statusUrlPrefix + taskId);
                constexpr int kPollIntervalMs = 1500;
                constexpr int kMaxPollCount = 600;
                int lastResolvedProgress = 0;

                for (int pollCount = 0; pollCount < kMaxPollCount; ++pollCount)
                {
                    if (!isRequestCurrent())
                        return;

                    int queryStatusCode = 0;
                    auto queryOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                        .withHttpRequestCmd("GET")
                        .withConnectionTimeoutMs(15000)
                        .withStatusCode(&queryStatusCode)
                        .withExtraHeaders("Accept: application/json");

                    auto queryStream = queryUrl.createInputStream(queryOptions);
                    if (queryStream == nullptr || queryStatusCode >= 400)
                    {
                        failureReason = "carey extract status polling failed";
                        break;
                    }

                    const juce::String queryResponse = queryStream->readEntireStreamAsString();
                    const juce::var queryVar = juce::JSON::parse(queryResponse);
                    auto* queryObj = queryVar.getDynamicObject();
                    if (queryObj == nullptr)
                    {
                        failureReason = "invalid carey extract status response";
                        break;
                    }

                    const juce::String status = queryObj->getProperty("status").toString().trim().toLowerCase();
                    const bool querySuccess = (bool)queryObj->getProperty("success");
                    const bool generationInProgress = (bool)queryObj->getProperty("generation_in_progress");
                    const bool transformInProgress = (bool)queryObj->getProperty("transform_in_progress");

                    if (status == "failed")
                    {
                        const auto failureInfo = parseCareyFailureResponse(queryVar, "carey extract generation failed");
                        failureReason = failureInfo.userMessage;
                        failureDetail = failureInfo.popupDetail;
                        break;
                    }

                    if (!querySuccess && !(generationInProgress || transformInProgress))
                    {
                        const auto failureInfo = parseCareyFailureResponse(queryVar, "carey extract request failed");
                        failureReason = failureInfo.userMessage;
                        failureDetail = failureInfo.popupDetail;
                        break;
                    }

                    if (status == "completed")
                    {
                        downloadedBase64 = queryObj->getProperty("audio_data").toString();
                        if (downloadedBase64.isEmpty())
                        {
                            failureReason = "carey extract completed without audio_data";
                            break;
                        }
                        success = true;
                        break;
                    }

                    juce::String queueStatus, queueMessage;
                    if (auto* queueObj = queryObj->getProperty("queue_status").getDynamicObject())
                    {
                        queueStatus = queueObj->getProperty("status").toString().trim().toLowerCase();
                        queueMessage = queueObj->getProperty("message").toString().trim();
                    }

                    if (queueMessage.isEmpty())
                    {
                        queueMessage = queryObj->getProperty("message").toString().trim();
                        if (queueMessage.isEmpty())
                            queueMessage = queryObj->getProperty("status_message").toString().trim();
                    }

                    queueMessage = stripAnsiAndControlChars(queueMessage);
                    const int progressPercent = resolveCareyProgressPercent(queryObj->getProperty("progress"),
                                                                           queueMessage,
                                                                           allowTextProgressFallback,
                                                                           lastResolvedProgress);
                    lastResolvedProgress = juce::jmax(lastResolvedProgress, progressPercent);

                    bool queued = (queueStatus == "queued");
                    if (progressPercent > 0 || queueStatus == "ready") queued = false;
                    juce::String statusText = cleanCareyQueueMessageText(queueMessage);
                    if (statusText.isEmpty())
                    {
                        if (queued) statusText = "queued - starting soon...";
                        else if (progressPercent > 0) statusText = "processing";
                        else statusText = status.isNotEmpty() ? status : "processing";
                    }

                    juce::MessageManager::callAsync([safeThis, generationToken, requestNonce, progressPercent, queued, statusText]()
                    {
                        if (safeThis == nullptr
                            || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                            || safeThis->careyRequestNonce.load() != requestNonce)
                            return;

                        safeThis->smoothProgressAnimation = false;
                        safeThis->generationProgress = progressPercent;
                        safeThis->isCurrentlyQueued = queued;
                        safeThis->setCareyWaveformState(progressPercent, queued);
                        if (statusText.isNotEmpty())
                            safeThis->showStatusMessage("carey extract: " + statusText, 2500);
                        safeThis->repaint();
                    });

                    juce::Thread::sleep(kPollIntervalMs);
                }

                if (!success && failureReason.isEmpty())
                    failureReason = "carey extract request timed out";
            }
            catch (const std::exception& e)
            {
                failureReason = "carey extract request failed: " + juce::String(e.what());
            }
            catch (...)
            {
                failureReason = "carey extract request failed with unknown error";
            }
        } while (false);

        if (success && downloadedBase64.isNotEmpty())
        {
            juce::MessageManager::callAsync([safeThis, generationToken, requestNonce, downloadedBase64]()
            {
                if (safeThis == nullptr
                    || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                    || safeThis->careyRequestNonce.load() != requestNonce)
                    return;

                safeThis->isCurrentlyQueued = false;
                safeThis->generationProgress = 100;
                safeThis->smoothProgressAnimation = false;
                safeThis->setCareyWaveformState(100, false);
                safeThis->setActiveOp(ActiveOp::None);
                safeThis->saveGeneratedAudio(downloadedBase64);
                safeThis->showStatusMessage("carey extract complete", 2500);
                safeThis->updateAllGenerationButtonStates();
            });
            return;
        }

        const juce::String finalError = failureReason.isNotEmpty() ? failureReason : "carey extract request failed";
        const juce::String popupDetail = failureDetail.isNotEmpty() ? failureDetail : finalError;
        juce::MessageManager::callAsync([safeThis, generationToken, requestNonce, finalError, popupDetail]()
        {
            if (safeThis == nullptr
                || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                || safeThis->careyRequestNonce.load() != requestNonce)
                return;

            auto cancelCareyOperation = [safeThis, generationToken, requestNonce]()
            {
                if (safeThis == nullptr
                    || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                    || safeThis->careyRequestNonce.load() != requestNonce)
                    return;

                safeThis->isGenerating = false;
                safeThis->isCurrentlyQueued = false;
                safeThis->generationProgress = 0;
                safeThis->smoothProgressAnimation = false;
                safeThis->setCareyWaveformState(0, false);
                safeThis->setActiveOp(ActiveOp::None);
                safeThis->updateAllGenerationButtonStates();
                safeThis->repaint();
            };

            safeThis->showStatusMessage(finalError, 4500);
            if (safeThis->shouldShowGenerationFailureDialog(popupDetail))
                safeThis->showGenerationFailureDialog(popupDetail);
            cancelCareyOperation();
        });
    });
}

void Gary4juceAudioProcessorEditor::sendToCareyComplete()
{
    if (!isServiceReachable(ServiceType::Carey))
    {
        showStatusMessage("carey not reachable - check connection first");
        return;
    }

    const int requestNonce = careyRequestNonce.fetch_add(1) + 1;

    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");
    const juce::File bufferFile = garyDir.getChildFile("myBuffer.wav");

    if (!bufferFile.existsAsFile())
    {
        showStatusMessage("missing myBuffer.wav - save your recording first");
        return;
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(bufferFile));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
    {
        showStatusMessage("failed to read myBuffer.wav for carey complete");
        return;
    }

    int bpm = juce::jmax(1, juce::roundToInt(getCareyBpmForRequest()));
    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        const int standaloneBpm = juce::jlimit(40, 240, currentCareyCompleteBpm);
        if (standaloneBpm > 0)
            bpm = standaloneBpm;
    }
    const bool useRemoteCompleteModel = !isUsingLocalhost;
    const juce::String requestedCompleteModel = currentCareyCompleteModel.trim().toLowerCase();
    const bool useCompleteTurboModel = requestedCompleteModel.isEmpty() || requestedCompleteModel.contains("turbo");
    const bool useCompleteSftModel = !useCompleteTurboModel && requestedCompleteModel.contains("sft");
    const juce::String submitCompleteModel = useRemoteCompleteModel
        ? (useCompleteTurboModel ? "xl-turbo" : "xl-base")
        : (useCompleteTurboModel ? "turbo" : (useCompleteSftModel ? "sft" : "base"));
    const int inferenceSteps = useCompleteTurboModel
        ? CareyUI::kFixedCompleteTurboSteps
        : juce::jlimit(32, 100, currentCareyCompleteSteps);
    const double guidanceScale = useCompleteTurboModel
        ? CareyUI::kFixedCompleteTurboCfg
        : juce::jlimit(3.0, 10.0, currentCompleteCfg);
    const juce::String caption = currentCareyCompleteCaption.trim();
    const juce::String lyrics = currentCareyLyrics;  // Shared lyrics across all tabs
    const juce::String keyScale = currentCareyKeyScale;
    const juce::String timeSig = currentCareyTimeSig;
    const juce::String language = currentCareyLanguage;
    const int targetDurationSeconds = juce::jlimit(30, 180, currentCareyCompleteDurationSeconds);
    const juce::String selectedLora = getSelectedCareyCompleteLora();
    const double loraScale = selectedLora.isNotEmpty()
        ? juce::jlimit(0.0, 1.0, currentCareyCompleteLoraScale)
        : -1.0;
    const bool useSrcAsRef = currentCompleteUseSrcAsRef;

    DBG("[carey-complete] request metas - bpm=" + juce::String(bpm)
        + ", steps=" + juce::String(inferenceSteps)
        + ", model=" + submitCompleteModel
        + ", lora=" + (selectedLora.isEmpty() ? juce::String("none") : selectedLora)
        + ", lora_scale=" + (selectedLora.isEmpty() ? juce::String("default") : juce::String(loraScale, 2))
        + ", key_scale=" + (keyScale.isEmpty() ? "none" : keyScale)
        + ", time_sig=" + (timeSig.isEmpty() ? "auto" : timeSig)
        + ", target_duration=" + juce::String(targetDurationSeconds)
        + ", use_src_ref=" + juce::String(useSrcAsRef ? "on" : "off")
        + ", caption_empty=" + juce::String(caption.isEmpty() ? "true" : "false")
        + ", lyrics_empty=" + juce::String(lyrics.trim().isEmpty() ? "true" : "false"));

    const auto cancelCareyOperation = [this, requestNonce]()
    {
        if (careyRequestNonce.load() != requestNonce)
            return;

        isGenerating = false;
        isCurrentlyQueued = false;
        generationProgress = 0;
        smoothProgressAnimation = false;
        setCareyWaveformState(0, false);
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
        repaint();
    };

    setActiveOp(ActiveOp::CareyGenerate);
    isGenerating = true;
    isCurrentlyQueued = true;
    generationProgress = 0;
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;
    setCareyWaveformState(0, true);
    audioProcessor.setUndoTransformAvailable(false);
    audioProcessor.setRetryAvailable(false);
    updateRetryButtonState();
    updateContinueButtonState();
    updateAllGenerationButtonStates();
    repaint();

    showStatusMessage("submitting carey complete request...", 2500);
    const juce::String submitUrlText = getServiceUrl(ServiceType::Carey, "/complete");
    const juce::String statusUrlPrefix = getServiceUrl(ServiceType::Carey, "/complete/status/");
    const bool allowTextProgressFallback = !audioProcessor.getIsUsingLocalhost();
    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis(this);
    const auto generationToken = beginGenerationAsyncWork();

    juce::Thread::launch([safeThis, generationToken, requestNonce, bufferFile, caption, lyrics, keyScale, timeSig, language, bpm, inferenceSteps, guidanceScale, targetDurationSeconds, selectedLora, loraScale, useSrcAsRef, submitCompleteModel, submitUrlText, statusUrlPrefix, allowTextProgressFallback]()
    {
        auto isRequestCurrent = [safeThis, generationToken, requestNonce]() {
            return safeThis != nullptr
                && safeThis->isGenerationAsyncWorkCurrent(generationToken)
                && safeThis->careyRequestNonce.load() == requestNonce;
        };

        if (!isRequestCurrent())
        {
            DBG("[carey-complete] request aborted - generation stopped");
            return;
        }

        juce::String failureReason;
        juce::String failureDetail;
        juce::String downloadedBase64;
        bool success = false;

        do
        {
            try
            {
                juce::MemoryBlock sourceAudioData;
                if (!bufferFile.loadFileAsData(sourceAudioData) || sourceAudioData.getSize() <= 0)
                {
                    failureReason = "failed to load myBuffer.wav bytes for carey complete request";
                    break;
                }

                const juce::String sourceAudioBase64 = juce::Base64::toBase64(sourceAudioData.getData(), sourceAudioData.getSize());
                if (sourceAudioBase64.isEmpty())
                {
                    failureReason = "failed to base64 encode carey complete conditioning audio";
                    break;
                }

                if (!isRequestCurrent())
                    return;

                juce::URL submitUrl(submitUrlText);

                juce::DynamicObject::Ptr submitPayload = new juce::DynamicObject();
                submitPayload->setProperty("audio_data", sourceAudioBase64);
                submitPayload->setProperty("bpm", bpm);
                submitPayload->setProperty("inference_steps", inferenceSteps);
                submitPayload->setProperty("model", submitCompleteModel);
                submitPayload->setProperty("audio_duration", (double)targetDurationSeconds);
                submitPayload->setProperty("caption", caption);
                submitPayload->setProperty("lyrics", lyrics);
                if (selectedLora.isNotEmpty())
                {
                    submitPayload->setProperty("lora", selectedLora);
                    submitPayload->setProperty("lora_scale", loraScale);
                }
                if (keyScale.isNotEmpty())
                    submitPayload->setProperty("key_scale", keyScale);
                if (language.isNotEmpty() && language != "en")
                    submitPayload->setProperty("language", language);
                submitPayload->setProperty("guidance_scale", guidanceScale);
                submitPayload->setProperty("use_src_as_ref", useSrcAsRef);
                if (timeSig.isNotEmpty())
                    submitPayload->setProperty("time_signature", timeSig);
                submitPayload->setProperty("batch_size", 1);
                submitPayload->setProperty("audio_format", "wav");

                const juce::String submitJson = juce::JSON::toString(juce::var(submitPayload.get()));
                juce::URL submitPost = submitUrl.withPOSTData(submitJson);

                int submitStatusCode = 0;
                auto submitOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withHttpRequestCmd("POST")
                    .withConnectionTimeoutMs(120000)
                    .withStatusCode(&submitStatusCode)
                    .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json");

                auto submitStream = submitPost.createInputStream(submitOptions);
                if (submitStream == nullptr || submitStatusCode >= 400)
                {
                    failureReason = "carey /complete submit failed";
                    break;
                }

                const juce::String submitResponse = submitStream->readEntireStreamAsString();
                const juce::var submitVar = juce::JSON::parse(submitResponse);
                auto* submitObj = submitVar.getDynamicObject();
                if (submitObj == nullptr)
                {
                    failureReason = "invalid carey complete submit response";
                    break;
                }

                juce::String taskId = submitObj->getProperty("task_id").toString();
                if (taskId.isEmpty())
                {
                    if (auto* dataObj = submitObj->getProperty("data").getDynamicObject())
                        taskId = dataObj->getProperty("task_id").toString();
                }

                const bool submitSuccess = (bool)submitObj->getProperty("success");
                if (taskId.isEmpty() || !submitSuccess)
                {
                    const juce::String errorText = submitObj->getProperty("error").toString().trim();
                    failureReason = errorText.isNotEmpty() ? errorText : "missing task_id from carey complete submit response";
                    break;
                }

                if (!isRequestCurrent())
                    return;

                juce::URL queryUrl(statusUrlPrefix + taskId);
                constexpr int kPollIntervalMs = 1500;
                constexpr int kMaxPollCount = 600;
                int lastResolvedProgress = 0;

                for (int pollCount = 0; pollCount < kMaxPollCount; ++pollCount)
                {
                    if (!isRequestCurrent())
                        return;

                    int queryStatusCode = 0;
                    auto queryOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                        .withHttpRequestCmd("GET")
                        .withConnectionTimeoutMs(15000)
                        .withStatusCode(&queryStatusCode)
                        .withExtraHeaders("Accept: application/json");

                    auto queryStream = queryUrl.createInputStream(queryOptions);
                    if (queryStream == nullptr || queryStatusCode >= 400)
                    {
                        failureReason = "carey complete status polling failed";
                        break;
                    }

                    const juce::String queryResponse = queryStream->readEntireStreamAsString();
                    const juce::var queryVar = juce::JSON::parse(queryResponse);
                    auto* queryObj = queryVar.getDynamicObject();
                    if (queryObj == nullptr)
                    {
                        failureReason = "invalid carey complete status response";
                        break;
                    }

                    const juce::String status = queryObj->getProperty("status").toString().trim().toLowerCase();
                    const bool querySuccess = (bool)queryObj->getProperty("success");
                    const bool generationInProgress = (bool)queryObj->getProperty("generation_in_progress");
                    const bool transformInProgress = (bool)queryObj->getProperty("transform_in_progress");

                    if (status == "failed")
                    {
                        const auto failureInfo = parseCareyFailureResponse(queryVar, "carey complete generation failed");
                        failureReason = failureInfo.userMessage;
                        failureDetail = failureInfo.popupDetail;
                        break;
                    }

                    if (!querySuccess && !(generationInProgress || transformInProgress))
                    {
                        const auto failureInfo = parseCareyFailureResponse(queryVar, "carey complete request failed");
                        failureReason = failureInfo.userMessage;
                        failureDetail = failureInfo.popupDetail;
                        break;
                    }

                    if (status == "completed")
                    {
                        downloadedBase64 = queryObj->getProperty("audio_data").toString();
                        if (downloadedBase64.isEmpty())
                        {
                            failureReason = "carey complete finished without audio_data";
                            break;
                        }
                        success = true;
                        break;
                    }

                    juce::String queueStatus, queueMessage;
                    if (auto* queueObj = queryObj->getProperty("queue_status").getDynamicObject())
                    {
                        queueStatus = queueObj->getProperty("status").toString().trim().toLowerCase();
                        queueMessage = queueObj->getProperty("message").toString().trim();
                    }

                    queueMessage = stripAnsiAndControlChars(queueMessage);
                    const int progressPercent = resolveCareyProgressPercent(queryObj->getProperty("progress"),
                                                                           queueMessage,
                                                                           allowTextProgressFallback,
                                                                           lastResolvedProgress);
                    lastResolvedProgress = juce::jmax(lastResolvedProgress, progressPercent);

                    bool queued = (queueStatus == "queued");
                    if (progressPercent > 0 || queueStatus == "ready") queued = false;

                    juce::String statusText = cleanCareyQueueMessageText(queueMessage);
                    if (statusText.isEmpty())
                    {
                        if (queued) statusText = "queued - starting soon...";
                        else if (progressPercent > 0) statusText = "processing";
                        else statusText = status.isNotEmpty() ? status : "processing";
                    }

                    juce::MessageManager::callAsync([safeThis, generationToken, requestNonce, progressPercent, queued, statusText]()
                    {
                        if (safeThis == nullptr
                            || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                            || safeThis->careyRequestNonce.load() != requestNonce)
                            return;

                        safeThis->smoothProgressAnimation = false;
                        safeThis->generationProgress = progressPercent;
                        safeThis->isCurrentlyQueued = queued;
                        safeThis->setCareyWaveformState(progressPercent, queued);
                        if (statusText.isNotEmpty())
                            safeThis->showStatusMessage("carey complete: " + statusText, 2500);
                        safeThis->repaint();
                    });

                    juce::Thread::sleep(kPollIntervalMs);
                }

                if (!success && failureReason.isEmpty())
                    failureReason = "carey complete request timed out";
            }
            catch (const std::exception& e)
            {
                failureReason = "carey complete request failed: " + juce::String(e.what());
            }
            catch (...)
            {
                failureReason = "carey complete request failed with unknown error";
            }
        } while (false);

        if (success && downloadedBase64.isNotEmpty())
        {
            juce::MessageManager::callAsync([safeThis, generationToken, requestNonce, downloadedBase64]()
            {
                if (safeThis == nullptr
                    || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                    || safeThis->careyRequestNonce.load() != requestNonce)
                    return;

                safeThis->isCurrentlyQueued = false;
                safeThis->generationProgress = 100;
                safeThis->smoothProgressAnimation = false;
                safeThis->setCareyWaveformState(100, false);
                safeThis->setActiveOp(ActiveOp::None);
                safeThis->saveGeneratedAudio(downloadedBase64);
                safeThis->showStatusMessage("carey continuation complete", 2500);
                safeThis->updateAllGenerationButtonStates();
            });
            return;
        }

        const juce::String finalError = failureReason.isNotEmpty() ? failureReason : "carey complete request failed";
        const juce::String popupDetail = failureDetail.isNotEmpty() ? failureDetail : finalError;
        juce::MessageManager::callAsync([safeThis, generationToken, requestNonce, finalError, popupDetail]()
        {
            if (safeThis == nullptr
                || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                || safeThis->careyRequestNonce.load() != requestNonce)
                return;

            auto cancelCareyOperation = [safeThis, generationToken, requestNonce]()
            {
                if (safeThis == nullptr
                    || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                    || safeThis->careyRequestNonce.load() != requestNonce)
                    return;

                safeThis->isGenerating = false;
                safeThis->isCurrentlyQueued = false;
                safeThis->generationProgress = 0;
                safeThis->smoothProgressAnimation = false;
                safeThis->setCareyWaveformState(0, false);
                safeThis->setActiveOp(ActiveOp::None);
                safeThis->updateAllGenerationButtonStates();
                safeThis->repaint();
            };

            safeThis->showStatusMessage(finalError, 4500);
            if (safeThis->shouldShowGenerationFailureDialog(popupDetail))
                safeThis->showGenerationFailureDialog(popupDetail);
            cancelCareyOperation();
        });
    });
}

void Gary4juceAudioProcessorEditor::sendToCareyCover()
{
    if (!isServiceReachable(ServiceType::Carey))
    {
        showStatusMessage("carey not reachable - check connection first");
        return;
    }

    const int requestNonce = careyRequestNonce.fetch_add(1) + 1;

    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");
    const juce::File bufferFile = garyDir.getChildFile("myBuffer.wav");

    if (!bufferFile.existsAsFile())
    {
        showStatusMessage("missing myBuffer.wav - save your recording first");
        return;
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(bufferFile));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
    {
        showStatusMessage("failed to read myBuffer.wav for carey cover");
        return;
    }

    const int bpm = juce::jmax(1, juce::roundToInt(getCareyBpmForRequest()));
    const juce::String caption = currentCoverCaption.trim();
    const juce::String lyrics = currentCareyLyrics;  // Shared lyrics across all tabs
    const juce::String keyScale = currentCareyKeyScale;
    const juce::String timeSig = currentCareyTimeSig;
    const juce::String language = currentCareyLanguage;
    const double coverNoiseStrength = juce::jlimit(0.0, 1.0, currentCoverNoiseStrength);
    const double audioCoverStrength = juce::jlimit(0.0, 1.0, currentCoverAudioStrength);
    const bool useCoverModelExperiment = kCareyCoverModelExperimentEnabled;
    const juce::String requestedCoverModel = currentCoverModel.trim().toLowerCase();
    const bool useCoverTurboModel = !useCoverModelExperiment
        || requestedCoverModel.isEmpty()
        || requestedCoverModel.contains("turbo");
    const juce::String submitCoverModel = useCoverModelExperiment
        ? (isUsingLocalhost ? (useCoverTurboModel ? "turbo" : "base")
                            : (useCoverTurboModel ? "xl-turbo" : "xl-base"))
        : juce::String();
    const double guidanceScale = useCoverTurboModel
        ? CareyUI::kFixedCoverCfg
        : juce::jlimit(3.0, 10.0, currentCoverCfg);
    const int inferenceSteps = useCoverTurboModel
        ? CareyUI::kFixedCoverSteps
        : juce::jlimit(8, 100, currentCoverSteps);
    const juce::String selectedLora = getSelectedCareyCoverLora();
    const double loraScale = selectedLora.isNotEmpty()
        ? juce::jlimit(0.0, 1.0, currentCoverLoraScale)
        : -1.0;
    const bool useSrcAsRef = currentCoverUseSrcAsRef;
    const bool loopAssistEnabled = currentCoverLoopAssistEnabled;
    const bool trimToInputEnabled = currentCoverTrimToInputEnabled;

    DBG("[carey-cover] request metas - bpm=" + juce::String(bpm)
        + ", steps=" + juce::String(inferenceSteps)
        + ", model=" + (submitCoverModel.isEmpty() ? juce::String("default") : submitCoverModel)
        + ", lora=" + (selectedLora.isEmpty() ? juce::String("none") : selectedLora)
        + ", lora_scale=" + (selectedLora.isEmpty() ? juce::String("default") : juce::String(loraScale, 2))
        + ", key_scale=" + (keyScale.isEmpty() ? "none" : keyScale)
        + ", time_sig=" + (timeSig.isEmpty() ? "auto" : timeSig)
        + ", noise_str=" + juce::String(coverNoiseStrength, 3)
        + ", audio_str=" + juce::String(audioCoverStrength, 2)
        + ", cfg=" + juce::String(guidanceScale, 1)
        + ", use_src_ref=" + juce::String(useSrcAsRef ? "on" : "off")
        + ", loop_assist=" + juce::String(loopAssistEnabled ? "on" : "off")
        + ", trim_to_input=" + juce::String(trimToInputEnabled ? "on" : "off")
        + ", caption_empty=" + juce::String(caption.isEmpty() ? "true" : "false")
        + ", lyrics_empty=" + juce::String(lyrics.trim().isEmpty() ? "true" : "false"));

    const auto cancelCareyOperation = [this, requestNonce]()
    {
        if (careyRequestNonce.load() != requestNonce)
            return;

        isGenerating = false;
        isCurrentlyQueued = false;
        generationProgress = 0;
        smoothProgressAnimation = false;
        setCareyWaveformState(0, false);
        setActiveOp(ActiveOp::None);
        updateAllGenerationButtonStates();
        repaint();
    };

    setActiveOp(ActiveOp::CareyGenerate);
    isGenerating = true;
    isCurrentlyQueued = true;
    generationProgress = 0;
    lastKnownProgress = 0;
    targetProgress = 0;
    smoothProgressAnimation = false;
    setCareyWaveformState(0, true);
    audioProcessor.setUndoTransformAvailable(false);
    audioProcessor.setRetryAvailable(false);
    updateRetryButtonState();
    updateContinueButtonState();
    updateAllGenerationButtonStates();
    repaint();

    showStatusMessage("submitting carey cover request...", 2500);
    const juce::String submitUrlText = getServiceUrl(ServiceType::Carey, "/cover");
    const juce::String statusUrlPrefix = getServiceUrl(ServiceType::Carey, "/cover/status/");
    const bool allowTextProgressFallback = !audioProcessor.getIsUsingLocalhost();
    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis(this);
    const auto generationToken = beginGenerationAsyncWork();

    juce::Thread::launch([safeThis, generationToken, requestNonce, bufferFile, caption, lyrics, keyScale, timeSig, language, bpm, coverNoiseStrength,
                          audioCoverStrength, guidanceScale, inferenceSteps, selectedLora, loraScale, submitCoverModel, useSrcAsRef,
                          loopAssistEnabled, trimToInputEnabled, submitUrlText, statusUrlPrefix, allowTextProgressFallback]()
    {
        auto isRequestCurrent = [safeThis, generationToken, requestNonce]() {
            return safeThis != nullptr
                && safeThis->isGenerationAsyncWorkCurrent(generationToken)
                && safeThis->careyRequestNonce.load() == requestNonce;
        };

        if (!isRequestCurrent())
        {
            DBG("[carey-cover] request aborted - generation stopped");
            return;
        }

        juce::String failureReason;
        juce::String failureDetail;
        juce::String downloadedBase64;
        bool success = false;
        double originalInputDurationSeconds = 0.0;

        do
        {
            try
            {
                // Read source buffer for loop assist processing
                juce::AudioFormatManager threadFormatManager;
                threadFormatManager.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> threadReader(threadFormatManager.createReaderFor(bufferFile));
                if (threadReader == nullptr || threadReader->sampleRate <= 0.0 || threadReader->lengthInSamples <= 0)
                {
                    failureReason = "failed to read myBuffer.wav for carey cover request";
                    break;
                }

                const int sourceChannels = juce::jmax(1, (int)threadReader->numChannels);
                const int sourceSamples = juce::jmax(1, (int)threadReader->lengthInSamples);
                const double sourceSampleRate = threadReader->sampleRate;
                originalInputDurationSeconds = (double)sourceSamples / sourceSampleRate;

                juce::AudioBuffer<float> sourceBuffer(sourceChannels, sourceSamples);
                const bool readOk = threadReader->read(&sourceBuffer, 0, sourceSamples, 0, true, true);
                if (!readOk)
                {
                    failureReason = "failed to decode myBuffer.wav for carey cover request";
                    break;
                }

                juce::AudioBuffer<float>* conditioningBuffer = &sourceBuffer;
                juce::AudioBuffer<float> loopAssistBuffer;

                if (loopAssistEnabled)
                {
                    constexpr double kMinContextSeconds = 120.0;  // guarantee at least 2 minutes of context
                    const double bpmSafe = juce::jmax(1.0, (double)bpm);
                    const double secondsPerBar = 240.0 / bpmSafe;
                    const int barsForMinContext = (int)std::ceil(kMinContextSeconds / secondsPerBar);
                    const int kLoopAssistBars = juce::jmax(32, barsForMinContext);
                    const double contextWindowSeconds = secondsPerBar * (double)kLoopAssistBars;
                    const int targetSamples = juce::jmax(1, juce::roundToInt(contextWindowSeconds * sourceSampleRate));

                    // Skip loop assist if input already fills the context window
                    if (originalInputDurationSeconds >= contextWindowSeconds)
                    {
                        DBG("[carey-cover] loop assist skipped: input (" + juce::String(originalInputDurationSeconds, 1)
                            + "s) already fills " + juce::String(kLoopAssistBars) + "-bar context ("
                            + juce::String(contextWindowSeconds, 1) + "s)");
                    }
                    else if (targetSamples != sourceSamples)
                    {
                        // Snap source to nearest whole bar count for seamless looping
                        const double barsInInput = originalInputDurationSeconds / secondsPerBar;
                        int snappedBars = juce::jmax(1, juce::roundToInt(barsInInput));
                        int snappedSamples = juce::jmax(1, juce::roundToInt((double)snappedBars * secondsPerBar * sourceSampleRate));

                        // If rounding up exceeds actual audio, floor instead
                        if (snappedSamples > sourceSamples)
                        {
                            snappedBars = juce::jmax(1, (int)std::floor(barsInInput));
                            snappedSamples = juce::jmin(sourceSamples,
                                juce::jmax(1, juce::roundToInt((double)snappedBars * secondsPerBar * sourceSampleRate)));
                        }

                        DBG("[carey-cover] loop assist bar-snap: " + juce::String(barsInInput, 2) + " bars -> "
                            + juce::String(snappedBars) + " bars (" + juce::String(snappedSamples)
                            + " of " + juce::String(sourceSamples) + " samples)");

                        loopAssistBuffer.setSize(sourceChannels, targetSamples, false, true, true);
                        for (int channel = 0; channel < sourceChannels; ++channel)
                        {
                            const float* src = sourceBuffer.getReadPointer(channel);
                            float* dst = loopAssistBuffer.getWritePointer(channel);
                            for (int i = 0; i < targetSamples; ++i)
                                dst[i] = src[i % snappedSamples];
                        }

                        conditioningBuffer = &loopAssistBuffer;
                        const double targetDuration = (double)targetSamples / sourceSampleRate;
                        DBG("[carey-cover] loop assist normalized context to " + juce::String(kLoopAssistBars)
                            + " bars (" + juce::String(targetDuration, 2) + "s)");
                    }
                }

                juce::MemoryBlock sourceAudioData;
                if (conditioningBuffer == &sourceBuffer)
                {
                    if (!bufferFile.loadFileAsData(sourceAudioData) || sourceAudioData.getSize() <= 0)
                    {
                        failureReason = "failed to load myBuffer.wav bytes for carey cover request";
                        break;
                    }
                }
                else
                {
                    const juce::File tempWav = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("carey-cover-loop-assist-", ".wav", false);

                    {
                        std::unique_ptr<juce::FileOutputStream> stream(tempWav.createOutputStream());
                        if (stream == nullptr)
                        {
                            failureReason = "failed to create cover loop-assist temp file";
                            break;
                        }

                        juce::WavAudioFormat wavFormat;
                        std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
                            stream.release(),
                            sourceSampleRate,
                            (unsigned int)conditioningBuffer->getNumChannels(),
                            16, {}, 0));

                        if (writer == nullptr)
                        {
                            tempWav.deleteFile();
                            failureReason = "failed to encode cover loop-assist conditioning wav";
                            break;
                        }

                        const bool writeOk = writer->writeFromAudioSampleBuffer(
                            *conditioningBuffer, 0, conditioningBuffer->getNumSamples());
                        writer.reset();

                        if (!writeOk)
                        {
                            tempWav.deleteFile();
                            failureReason = "failed to write cover loop-assist conditioning wav";
                            break;
                        }
                    }

                    if (!tempWav.loadFileAsData(sourceAudioData) || sourceAudioData.getSize() <= 0)
                    {
                        tempWav.deleteFile();
                        failureReason = "failed to read cover loop-assist conditioning bytes";
                        break;
                    }

                    tempWav.deleteFile();
                }

                const juce::String sourceAudioBase64 = juce::Base64::toBase64(sourceAudioData.getData(), sourceAudioData.getSize());
                if (sourceAudioBase64.isEmpty())
                {
                    failureReason = "failed to base64 encode carey cover conditioning audio";
                    break;
                }

                if (!isRequestCurrent())
                    return;

                juce::URL submitUrl(submitUrlText);

                juce::DynamicObject::Ptr submitPayload = new juce::DynamicObject();
                submitPayload->setProperty("audio_data", sourceAudioBase64);
                submitPayload->setProperty("bpm", bpm);
                submitPayload->setProperty("caption", caption);
                submitPayload->setProperty("lyrics", lyrics);
                if (selectedLora.isNotEmpty())
                {
                    submitPayload->setProperty("lora", selectedLora);
                    submitPayload->setProperty("lora_scale", loraScale);
                }
                if (submitCoverModel.isNotEmpty())
                    submitPayload->setProperty("model", submitCoverModel);
                if (keyScale.isNotEmpty())
                    submitPayload->setProperty("key_scale", keyScale);
                if (language.isNotEmpty() && language != "en")
                    submitPayload->setProperty("language", language);
                submitPayload->setProperty("cover_noise_strength", coverNoiseStrength);
                submitPayload->setProperty("audio_cover_strength", audioCoverStrength);
                submitPayload->setProperty("guidance_scale", guidanceScale);
                submitPayload->setProperty("inference_steps", inferenceSteps);
                submitPayload->setProperty("use_src_as_ref", useSrcAsRef);
                if (timeSig.isNotEmpty())
                    submitPayload->setProperty("time_signature", timeSig);
                submitPayload->setProperty("batch_size", 1);
                submitPayload->setProperty("audio_format", "wav");

                const juce::String submitJson = juce::JSON::toString(juce::var(submitPayload.get()));
                juce::URL submitPost = submitUrl.withPOSTData(submitJson);

                int submitStatusCode = 0;
                auto submitOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withHttpRequestCmd("POST")
                    .withConnectionTimeoutMs(120000)
                    .withStatusCode(&submitStatusCode)
                    .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json");

                auto submitStream = submitPost.createInputStream(submitOptions);
                if (submitStream == nullptr || submitStatusCode >= 400)
                {
                    failureReason = "carey /cover submit failed";
                    break;
                }

                const juce::String submitResponse = submitStream->readEntireStreamAsString();
                const juce::var submitVar = juce::JSON::parse(submitResponse);
                auto* submitObj = submitVar.getDynamicObject();
                if (submitObj == nullptr)
                {
                    failureReason = "invalid carey cover submit response";
                    break;
                }

                juce::String taskId = submitObj->getProperty("task_id").toString();
                if (taskId.isEmpty())
                {
                    if (auto* dataObj = submitObj->getProperty("data").getDynamicObject())
                        taskId = dataObj->getProperty("task_id").toString();
                }

                const bool submitSuccess = (bool)submitObj->getProperty("success");
                if (taskId.isEmpty() || !submitSuccess)
                {
                    const juce::String errorText = submitObj->getProperty("error").toString().trim();
                    failureReason = errorText.isNotEmpty() ? errorText : "missing task_id from carey cover submit response";
                    break;
                }

                DBG("[carey-cover] task_id: " + taskId);
                if (!isRequestCurrent())
                    return;

                juce::URL queryUrl(statusUrlPrefix + taskId);
                constexpr int kPollIntervalMs = 1500;
                constexpr int kMaxPollCount = 600;
                int lastResolvedProgress = 0;

                for (int pollCount = 0; pollCount < kMaxPollCount; ++pollCount)
                {
                    if (!isRequestCurrent())
                        return;

                    int queryStatusCode = 0;
                    auto queryOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                        .withHttpRequestCmd("GET")
                        .withConnectionTimeoutMs(15000)
                        .withStatusCode(&queryStatusCode)
                        .withExtraHeaders("Accept: application/json");

                    auto queryStream = queryUrl.createInputStream(queryOptions);
                    if (queryStream == nullptr || queryStatusCode >= 400)
                    {
                        failureReason = "carey cover status polling failed";
                        break;
                    }

                    const juce::String queryResponse = queryStream->readEntireStreamAsString();
                    DBG("[carey-cover] RAW RESPONSE: " + queryResponse.substring(0, 500));
                    const juce::var queryVar = juce::JSON::parse(queryResponse);
                    auto* queryObj = queryVar.getDynamicObject();
                    if (queryObj == nullptr)
                    {
                        failureReason = "invalid carey cover status response";
                        break;
                    }

                    const juce::String status = queryObj->getProperty("status").toString().trim().toLowerCase();
                    const bool querySuccess = (bool)queryObj->getProperty("success");
                    const bool generationInProgress = (bool)queryObj->getProperty("generation_in_progress");
                    const bool transformInProgress = (bool)queryObj->getProperty("transform_in_progress");

                    if (status == "failed")
                    {
                        const auto failureInfo = parseCareyFailureResponse(queryVar, "carey cover generation failed");
                        failureReason = failureInfo.userMessage;
                        failureDetail = failureInfo.popupDetail;
                        break;
                    }

                    if (!querySuccess && !(generationInProgress || transformInProgress))
                    {
                        const auto failureInfo = parseCareyFailureResponse(queryVar, "carey cover request failed");
                        failureReason = failureInfo.userMessage;
                        failureDetail = failureInfo.popupDetail;
                        break;
                    }

                    if (status == "completed")
                    {
                        downloadedBase64 = queryObj->getProperty("audio_data").toString();
                        if (downloadedBase64.isEmpty())
                        {
                            failureReason = "carey cover finished without audio_data";
                            break;
                        }
                        success = true;
                        break;
                    }

                    juce::String queueStatus, queueMessage;
                    if (auto* queueObj = queryObj->getProperty("queue_status").getDynamicObject())
                    {
                        queueStatus = queueObj->getProperty("status").toString().trim().toLowerCase();
                        queueMessage = queueObj->getProperty("message").toString().trim();
                    }

                    // Cover backend may put message at top level or in status_message
                    if (queueMessage.isEmpty())
                    {
                        queueMessage = queryObj->getProperty("message").toString().trim();
                        if (queueMessage.isEmpty())
                            queueMessage = queryObj->getProperty("status_message").toString().trim();
                    }

                    queueMessage = stripAnsiAndControlChars(queueMessage);

                    DBG("[carey-cover] poll: status=" + status
                        + " progress=" + queryObj->getProperty("progress").toString()
                        + " queueStatus=" + queueStatus
                        + " queueMessage=[" + queueMessage + "]"
                        + " parsedProgress=" + juce::String(plugin_editor_detail::parseCareyProgressValue(queryObj->getProperty("progress"))));

                    const int progressPercent = resolveCareyProgressPercent(queryObj->getProperty("progress"),
                                                                           queueMessage,
                                                                           allowTextProgressFallback,
                                                                           lastResolvedProgress);
                    lastResolvedProgress = juce::jmax(lastResolvedProgress, progressPercent);

                    bool queued = (queueStatus == "queued");
                    if (progressPercent > 0 || queueStatus == "ready") queued = false;

                    juce::String statusText = cleanCareyQueueMessageText(queueMessage);
                    if (statusText.isEmpty())
                    {
                        if (queued) statusText = "queued - starting soon...";
                        else if (progressPercent > 0) statusText = "processing";
                        else statusText = status.isNotEmpty() ? status : "processing";
                    }

                    juce::MessageManager::callAsync([safeThis, generationToken, requestNonce, progressPercent, queued, statusText]()
                    {
                        if (safeThis == nullptr
                            || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                            || safeThis->careyRequestNonce.load() != requestNonce)
                            return;

                        safeThis->smoothProgressAnimation = false;
                        safeThis->generationProgress = progressPercent;
                        safeThis->isCurrentlyQueued = queued;
                        safeThis->setCareyWaveformState(progressPercent, queued);
                        if (statusText.isNotEmpty())
                            safeThis->showStatusMessage("carey cover: " + statusText, 2500);
                        safeThis->repaint();
                    });

                    juce::Thread::sleep(kPollIntervalMs);
                }

                if (!success && failureReason.isEmpty())
                    failureReason = "carey cover request timed out";
            }
            catch (const std::exception& e)
            {
                failureReason = "carey cover request failed: " + juce::String(e.what());
            }
            catch (...)
            {
                failureReason = "carey cover request failed with unknown error";
            }
        } while (false);

        if (success && downloadedBase64.isNotEmpty())
        {
            juce::String finalBase64 = downloadedBase64;

            if (trimToInputEnabled && originalInputDurationSeconds > 0.0)
            {
                juce::MemoryOutputStream decodedOutputBytes;
                if (!juce::Base64::convertFromBase64(decodedOutputBytes, downloadedBase64))
                {
                    DBG("[carey-cover] trim-to-input skipped: failed to decode generated audio");
                }
                else
                {
                    const juce::MemoryBlock& decodedData = decodedOutputBytes.getMemoryBlock();
                    const juce::File tempInput = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("carey-cover-trim-input-", ".wav", false);
                    const juce::File tempOutput = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("carey-cover-trim-output-", ".wav", false);

                    const auto cleanupTemps = [&tempInput, &tempOutput]()
                    {
                        tempInput.deleteFile();
                        tempOutput.deleteFile();
                    };

                    do
                    {
                        if (!tempInput.replaceWithData(decodedData.getData(), decodedData.getSize()))
                        {
                            DBG("[carey-cover] trim-to-input skipped: failed to stage generated wav");
                            break;
                        }

                        juce::AudioFormatManager trimFormatManager;
                        trimFormatManager.registerBasicFormats();
                        std::unique_ptr<juce::AudioFormatReader> trimReader(trimFormatManager.createReaderFor(tempInput));
                        if (trimReader == nullptr || trimReader->sampleRate <= 0.0 || trimReader->lengthInSamples <= 0)
                        {
                            DBG("[carey-cover] trim-to-input skipped: failed to read generated wav");
                            break;
                        }

                        const int generatedSamples = juce::jmax(1, (int)trimReader->lengthInSamples);
                        const int targetSamples = juce::jlimit(1, generatedSamples,
                            juce::roundToInt(originalInputDurationSeconds * trimReader->sampleRate));

                        if (targetSamples >= generatedSamples) break;

                        const int trimChannels = juce::jmax(1, (int)trimReader->numChannels);
                        juce::AudioBuffer<float> trimmedBuffer(trimChannels, targetSamples);
                        if (!trimReader->read(&trimmedBuffer, 0, targetSamples, 0, true, true))
                        {
                            DBG("[carey-cover] trim-to-input skipped: failed to decode generated samples for trimming");
                            break;
                        }

                        std::unique_ptr<juce::FileOutputStream> trimStream(tempOutput.createOutputStream());
                        if (trimStream == nullptr)
                        {
                            DBG("[carey-cover] trim-to-input skipped: failed to create output wav");
                            break;
                        }

                        juce::WavAudioFormat trimWavFormat;
                        std::unique_ptr<juce::AudioFormatWriter> trimWriter(trimWavFormat.createWriterFor(
                            trimStream.release(), trimReader->sampleRate,
                            (unsigned int)trimChannels, 16, {}, 0));

                        if (trimWriter == nullptr)
                        {
                            DBG("[carey-cover] trim-to-input skipped: failed to initialize output wav writer");
                            break;
                        }

                        const bool trimWriteOk = trimWriter->writeFromAudioSampleBuffer(trimmedBuffer, 0, targetSamples);
                        trimWriter.reset();
                        if (!trimWriteOk)
                        {
                            DBG("[carey-cover] trim-to-input skipped: failed to write trimmed wav");
                            break;
                        }

                        juce::MemoryBlock trimmedData;
                        if (!tempOutput.loadFileAsData(trimmedData) || trimmedData.getSize() == 0)
                        {
                            DBG("[carey-cover] trim-to-input skipped: failed to read trimmed wav");
                            break;
                        }

                        finalBase64 = juce::Base64::toBase64(trimmedData.getData(), trimmedData.getSize());
                        DBG("[carey-cover] trim-to-input applied: " + juce::String((double)generatedSamples / trimReader->sampleRate, 2)
                            + "s -> " + juce::String((double)targetSamples / trimReader->sampleRate, 2) + "s");
                    } while (false);

                    cleanupTemps();
                }
            }

            juce::MessageManager::callAsync([safeThis, generationToken, requestNonce, finalBase64]()
            {
                if (safeThis == nullptr
                    || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                    || safeThis->careyRequestNonce.load() != requestNonce)
                    return;

                safeThis->isCurrentlyQueued = false;
                safeThis->generationProgress = 100;
                safeThis->smoothProgressAnimation = false;
                safeThis->setCareyWaveformState(100, false);
                safeThis->setActiveOp(ActiveOp::None);
                safeThis->saveGeneratedAudio(finalBase64);
                safeThis->showStatusMessage("carey cover remix complete", 2500);
                safeThis->updateAllGenerationButtonStates();
            });
            return;
        }

        const juce::String finalError = failureReason.isNotEmpty() ? failureReason : "carey cover request failed";
        const juce::String popupDetail = failureDetail.isNotEmpty() ? failureDetail : finalError;
        juce::MessageManager::callAsync([safeThis, generationToken, requestNonce, finalError, popupDetail]()
        {
            if (safeThis == nullptr
                || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                || safeThis->careyRequestNonce.load() != requestNonce)
                return;

            auto cancelCareyOperation = [safeThis, generationToken, requestNonce]()
            {
                if (safeThis == nullptr
                    || !safeThis->isGenerationAsyncWorkCurrent(generationToken)
                    || safeThis->careyRequestNonce.load() != requestNonce)
                    return;

                safeThis->isGenerating = false;
                safeThis->isCurrentlyQueued = false;
                safeThis->generationProgress = 0;
                safeThis->smoothProgressAnimation = false;
                safeThis->setCareyWaveformState(0, false);
                safeThis->setActiveOp(ActiveOp::None);
                safeThis->updateAllGenerationButtonStates();
                safeThis->repaint();
            };

            safeThis->showStatusMessage(finalError, 4500);
            if (safeThis->shouldShowGenerationFailureDialog(popupDetail))
                safeThis->showGenerationFailureDialog(popupDetail);
            cancelCareyOperation();
        });
    });
}
