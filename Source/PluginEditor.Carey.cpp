// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

/*
  ==============================================================================
    Carey-specific PluginEditor method definitions.

    Every mode is one ace-server job (see PluginEditorCareyHelpers.h): the
    mode prepares an AceRequest, startCareyJob submits it and follows it.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginEditorCareyHelpers.h"

namespace
{
// The fields every mode sends.
juce::DynamicObject::Ptr careyRequest(const juce::String& taskType,
                                      const juce::String& caption,
                                      const juce::String& lyrics,
                                      int bpm,
                                      const juce::String& keyScale,
                                      const juce::String& timeSig,
                                      const juce::String& language,
                                      int steps,
                                      double cfg,
                                      juce::int64 seed)
{
    juce::DynamicObject::Ptr json = new juce::DynamicObject();
    json->setProperty("task_type", taskType);
    json->setProperty("caption", caption);
    json->setProperty("lyrics", lyrics);
    json->setProperty("bpm", bpm);
    if (keyScale.isNotEmpty())
        json->setProperty("keyscale", keyScale);
    if (timeSig.isNotEmpty())
        json->setProperty("timesignature", timeSig);
    json->setProperty("vocal_language", language.isNotEmpty() ? language : juce::String("en"));
    json->setProperty("inference_steps", steps);
    json->setProperty("guidance_scale", cfg);
    json->setProperty("seed", seed);
    json->setProperty("output_format", "wav16");
    return json;
}

// Name the DiT for `variant` at the chosen size, and the LoRA if one is
// chosen, checking both against what the backend offers.
bool careySelectModels(const carey_native::Props& props,
                       const juce::String& variant,
                       bool xl,
                       const juce::String& lora,
                       double loraScale,
                       juce::DynamicObject& json,
                       juce::String& error)
{
    const juce::String size = xl ? "XL" : "2B";
    const juce::String dit = carey_native::resolveDit(props.dits, variant, xl);
    if (dit.isEmpty())
    {
        error = "this backend has no " + size + " " + variant + " model - download it in gary4local or switch size";
        return false;
    }
    json.setProperty("synth_model", dit);

    if (lora.isEmpty())
        return true;
    const auto adapter = std::find_if(props.adapters.begin(), props.adapters.end(),
                                      [&lora](const carey_native::Adapter& a) { return a.name.equalsIgnoreCase(lora); });
    if (adapter == props.adapters.end())
    {
        error = "lora " + lora + " is not on this backend";
        return false;
    }
    if (adapter->family.isNotEmpty() && adapter->family != (xl ? "xl" : "standard"))
    {
        error = "lora " + lora + " was trained on " + juce::String(xl ? "2B" : "XL") + " - switch the model size";
        return false;
    }
    json.setProperty("adapter", adapter->name);
    json.setProperty("adapter_scale", loraScale);
    return true;
}

// The recorded buffer as WAV bytes, for conditioning. With loop assist on, a
// short buffer is snapped to whole bars and looped to at least 32 bars and two
// minutes, which gives lego and cover enough context.
bool careyConditioningAudio(const juce::File& bufferFile,
                            int bpm,
                            bool loopAssist,
                            juce::MemoryBlock& wav,
                            double& inputSeconds,
                            double& conditioningSeconds,
                            juce::String& error)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(bufferFile));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
    {
        error = "failed to read myBuffer.wav for carey";
        return false;
    }

    const int channels = juce::jmax(1, (int) reader->numChannels);
    const int samples = juce::jmax(1, (int) reader->lengthInSamples);
    const double sampleRate = reader->sampleRate;
    inputSeconds = (double) samples / sampleRate;
    conditioningSeconds = inputSeconds;

    const double secondsPerBar = 240.0 / juce::jmax(1.0, (double) bpm);
    const int bars = juce::jmax(32, (int) std::ceil(120.0 / secondsPerBar));
    const double contextSeconds = secondsPerBar * (double) bars;
    if (!loopAssist || inputSeconds >= contextSeconds)
    {
        if (!bufferFile.loadFileAsData(wav) || wav.getSize() == 0)
        {
            error = "failed to load myBuffer.wav for carey";
            return false;
        }
        return true;
    }

    juce::AudioBuffer<float> source(channels, samples);
    if (!reader->read(&source, 0, samples, 0, true, true))
    {
        error = "failed to decode myBuffer.wav for carey";
        return false;
    }

    // Loop whole bars so the seam lands on a downbeat; round down when
    // rounding up would run past the recording.
    const double barsInInput = inputSeconds / secondsPerBar;
    int loopBars = juce::jmax(1, juce::roundToInt(barsInInput));
    int loopSamples = juce::roundToInt((double) loopBars * secondsPerBar * sampleRate);
    if (loopSamples > samples)
    {
        loopBars = juce::jmax(1, (int) std::floor(barsInInput));
        loopSamples = juce::jmin(samples, juce::roundToInt((double) loopBars * secondsPerBar * sampleRate));
    }
    loopSamples = juce::jmax(1, loopSamples);

    const int targetSamples = juce::jmax(1, juce::roundToInt(contextSeconds * sampleRate));
    juce::AudioBuffer<float> looped(channels, targetSamples);
    for (int channel = 0; channel < channels; ++channel)
    {
        const float* src = source.getReadPointer(channel);
        float* dst = looped.getWritePointer(channel);
        for (int i = 0; i < targetSamples; ++i)
            dst[i] = src[i % loopSamples];
    }

    wav.reset();
    {
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
            new juce::MemoryOutputStream(wav, false), sampleRate, (unsigned int) channels, 16, {}, 0));
        if (writer == nullptr || !writer->writeFromAudioSampleBuffer(looped, 0, targetSamples))
        {
            error = "failed to encode loop-assist conditioning audio";
            return false;
        }
    }
    conditioningSeconds = (double) targetSamples / sampleRate;
    DBG("[carey] loop assist: " + juce::String(barsInInput, 2) + " bars looped as " + juce::String(loopBars)
        + " to " + juce::String(bars) + " bars (" + juce::String(conditioningSeconds, 2) + "s)");
    return true;
}

// Cut `wav` to its first `seconds`. Leaves it whole when it is shorter or
// cannot be read.
void careyTrimWav(juce::MemoryBlock& wav, double seconds)
{
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatReader> reader(wavFormat.createReaderFor(
        new juce::MemoryInputStream(wav, false), true));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return;

    const int generated = (int) reader->lengthInSamples;
    const int target = juce::jlimit(1, generated, juce::roundToInt(seconds * reader->sampleRate));
    if (target >= generated)
        return;

    const int channels = juce::jmax(1, (int) reader->numChannels);
    juce::AudioBuffer<float> trimmed(channels, target);
    if (!reader->read(&trimmed, 0, target, 0, true, true))
        return;

    juce::MemoryBlock out;
    {
        std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
            new juce::MemoryOutputStream(out, false), reader->sampleRate, (unsigned int) channels, 16, {}, 0));
        if (writer == nullptr || !writer->writeFromAudioSampleBuffer(trimmed, 0, target))
            return;
    }
    DBG("[carey] trim to input: " + juce::String((double) generated / reader->sampleRate, 2) + "s -> "
        + juce::String(seconds, 2) + "s");
    wav = std::move(out);
}

// The variant a saved model setting names: base, sft or turbo. Settings saved
// before v5.1 could carry an xl- prefix, which the size buttons now carry.
juce::String careyVariant(const juce::String& model, bool allowSft)
{
    const juce::String normalized = model.trim().toLowerCase();
    if (normalized.isEmpty() || normalized.contains("turbo"))
        return "turbo";
    if (allowSft && normalized.contains("sft"))
        return "sft";
    return "base";
}
}

void Gary4juceAudioProcessorEditor::setCareyLastSeed(const juce::String& seed)
{
    const auto trimmed = seed.trim();
    if (trimmed.isEmpty())
        return;

    currentCareyLastSeed = trimmed;
    currentCareySeedText = trimmed;
    if (careyUI)
        careyUI->setLastSeed(trimmed);
}

void Gary4juceAudioProcessorEditor::updateCareyEnablementSnapshot()
{
    if (!careyUI)
        return;

    const bool hasInputAudio = savedSamples > 0 && getGaryBufferFile().existsAsFile();
    const bool backendLooksOnline = isServiceReachable(ServiceType::Carey);

    careyUI->setGenerateButtonEnabled(backendLooksOnline, hasInputAudio, isGenerating);
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
        careyUI->setCompleteMaximumDurationSeconds(audioProcessor.getIsUsingLocalhost()
            ? CareyUI::kLocalMaximumDurationSeconds
            : CareyUI::kRemoteMaximumDurationSeconds);
        careyUI->setCoverModelSelectionEnabled(kCareyCoverModelExperimentEnabled);
        careyUI->setExtractRemoteGenerationEnabled(true);
    }

    if (currentTab == ModelTab::Carey)
        refreshCareyAvailableLoras(false);

    if (audioProcessor.getIsUsingLocalhost())
        careyTabButton.setTooltip("carey (ace-step 1.5) - localhost:8003");
    else
        careyTabButton.setTooltip("carey (ace-step 1.5) - remote backend");

    if (!available && currentTab == ModelTab::Carey)
        switchToTab(ModelTab::Gary);

    updateTabButtonStates();
    updateCareyEnablementSnapshot();
}

juce::String Gary4juceAudioProcessorEditor::getSelectedCareyLegoLora() const
{
    if (!currentCareyLegoUseLora)
        return {};

    const juce::String requestedLora = currentCareyLegoLora.trim();
    if (requestedLora.isEmpty())
        return {};

    for (const auto& availableLora : availableCareyLoras)
    {
        if (availableLora.equalsIgnoreCase(requestedLora))
            return availableLora;
    }

    return {};
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

    careyUI->setAvailableDits(careyDits);
    careyUI->setModelFamilyXl(currentCareyXl);
    careyUI->setAvailableLegoLoras(availableCareyLoras);
    careyUI->setLegoSelectedLora(currentCareyLegoLora);
    careyUI->setLegoLoraScale(currentCareyLegoLoraScale);
    careyUI->setLegoUseLora(currentCareyLegoUseLora);
    careyUI->setAvailableCompleteLoras(availableCareyLoras);
    careyUI->setCompleteSelectedLora(currentCareyCompleteLora);
    careyUI->setCompleteLoraScale(currentCareyCompleteLoraScale);
    careyUI->setCompleteUseLora(currentCareyCompleteUseLora);
    careyUI->setAvailableCoverLoras(availableCareyLoras);
    careyUI->setCoverSelectedLora(currentCoverLora);
    careyUI->setCoverLoraScale(currentCoverLoraScale);
    careyUI->setCoverUseLora(currentCoverUseLora);
}

// Offer the LoRAs trained on the chosen model size, plus any whose size the
// backend does not say, and keep each mode's choice when it is still offered.
void Gary4juceAudioProcessorEditor::applyCareyModelFamily()
{
    // The chosen size may be one the backend does not have; switch to the one it has.
    if (!careyDits.isEmpty())
    {
        const auto hasFamily = [this](bool xl)
        {
            for (const auto* variant : { "base", "sft", "turbo" })
                if (carey_native::resolveDit(careyDits, variant, xl).isNotEmpty())
                    return true;
            return false;
        };
        if (!hasFamily(currentCareyXl) && hasFamily(!currentCareyXl))
            currentCareyXl = !currentCareyXl;
    }

    const juce::String family = currentCareyXl ? "xl" : "standard";
    juce::StringArray loras;
    for (const auto& adapter : careyAdapters)
        if (adapter.family.isEmpty() || adapter.family == family)
            loras.addIfNotAlreadyThere(adapter.name);
    loras.sortNatural();
    availableCareyLoras = loras;

    const auto keepChoice = [this](juce::String& choice)
    {
        for (const auto& lora : availableCareyLoras)
        {
            if (lora.equalsIgnoreCase(choice))
            {
                choice = lora;
                return;
            }
        }
        choice = availableCareyLoras.isEmpty() ? juce::String() : availableCareyLoras[0];
    };
    keepChoice(currentCareyLegoLora);
    keepChoice(currentCareyCompleteLora);
    keepChoice(currentCoverLora);

    if (availableCareyLoras.isEmpty())
    {
        currentCareyLegoUseLora = false;
        currentCareyCompleteUseLora = false;
        currentCoverUseLora = false;
    }

    syncCareyLoraUi();
}

void Gary4juceAudioProcessorEditor::refreshCareyAvailableLoras(bool force)
{
    const juce::String baseUrl = getServiceUrl(ServiceType::Carey, "");
    const auto nowMs = juce::Time::getCurrentTime().toMilliseconds();

    if (!force
        && baseUrl == careyLoraFetchBackendUrl
        && nowMs - careyLoraLastFetchMs < 2000)
        return;

    if (careyLoraFetchInFlight.exchange(true))
        return;

    careyLoraFetchBackendUrl = baseUrl;
    careyLoraLastFetchMs = nowMs;
    const int fetchNonce = careyLoraFetchNonce.fetch_add(1) + 1;
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, fetchNonce, baseUrl]()
    {
        carey_native::Props props;
        juce::String error;
        const bool success = carey_native::fetchProps(baseUrl, props, error);
        if (!success)
        {
            DBG("[carey] failed to fetch /props: " + error);
        }

        juce::MessageManager::callAsync([asyncAlive, editor, fetchNonce, baseUrl, props, success]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            editor->careyLoraFetchInFlight.store(false);

            if (editor->careyLoraFetchNonce.load() != fetchNonce)
                return;

            if (editor->getServiceUrl(ServiceType::Carey, "") != baseUrl)
                return;

            editor->careyDits = success ? props.dits : juce::StringArray();
            editor->careyAdapters = success ? props.adapters : std::vector<carey_native::Adapter>();
            editor->applyCareyModelFamily();
        });
    });
}

// One of the captions `lora` was trained with, or empty when the backend
// did not share them.
juce::String Gary4juceAudioProcessorEditor::randomCareyLoraCaption(const juce::String& lora) const
{
    for (const auto& adapter : careyAdapters)
    {
        if (adapter.name.equalsIgnoreCase(lora) && !adapter.captions.isEmpty())
            return adapter.captions[juce::Random::getSystemRandom().nextInt(adapter.captions.size())];
    }
    return {};
}

void Gary4juceAudioProcessorEditor::requestCareyCompleteCaption()
{
    if (!careyUI)
        return;

    const juce::String lora = getSelectedCareyCompleteLora();
    const juce::String caption = lora.isNotEmpty() ? randomCareyLoraCaption(lora) : juce::String();
    if (caption.isNotEmpty())
    {
        careyUI->setCompleteCaptionText(caption);
        currentCareyCompleteCaption = caption;
        return;
    }
    careyUI->applyRandomCompleteCaption();
}

void Gary4juceAudioProcessorEditor::requestCareyCoverCaption()
{
    if (!careyUI)
        return;

    const juce::String lora = getSelectedCareyCoverLora();
    const juce::String caption = lora.isNotEmpty() ? randomCareyLoraCaption(lora) : juce::String();
    if (caption.isNotEmpty())
    {
        careyUI->setCoverCaptionText(caption);
        currentCoverCaption = caption;
        return;
    }
    careyUI->applyRandomCoverCaption();
}

// Submit one Carey job and follow it to the end: progress on the waveform and
// status bar, the result saved like every other generation, and a stop that
// cancels it on the backend too.
void Gary4juceAudioProcessorEditor::startCareyJob(CareyJob job)
{
    if (job.needsSourceAudio
        && !validateAudioDurationForModel(getGaryBufferFile(), audioProcessor.getIsUsingLocalhost() ? CareyUI::kLocalMaximumDurationSeconds : CareyUI::kRemoteMaximumDurationSeconds, "carey"))
        return;
    if (!isServiceReachable(ServiceType::Carey))
    {
        showStatusMessage("carey not reachable - check connection first");
        return;
    }
    if (!ensureGaryDataDirectoryAvailable())
        return;
    if (job.needsSourceAudio && !getGaryBufferFile().existsAsFile())
    {
        showStatusMessage("missing myBuffer.wav - save your recording first");
        return;
    }

    const int requestNonce = careyRequestNonce.fetch_add(1) + 1;

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
    showStatusMessage("submitting carey " + job.label + " request...", 2500);

    const juce::String baseUrl = getServiceUrl(ServiceType::Carey, "");
    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis(this);
    const auto generationToken = beginGenerationAsyncWork();

    juce::Thread::launch([safeThis, generationToken, requestNonce, baseUrl, job]()
    {
        const auto isRequestCurrent = [safeThis, generationToken, requestNonce]()
        {
            return safeThis != nullptr
                && safeThis->isGenerationAsyncWorkCurrent(generationToken)
                && safeThis->careyRequestNonce.load() == requestNonce;
        };
        const auto onMessageThread = [safeThis, generationToken, requestNonce](std::function<void(Gary4juceAudioProcessorEditor&)> fn)
        {
            juce::MessageManager::callAsync([safeThis, generationToken, requestNonce, fn]()
            {
                if (safeThis != nullptr
                    && safeThis->isGenerationAsyncWorkCurrent(generationToken)
                    && safeThis->careyRequestNonce.load() == requestNonce)
                    fn(*safeThis);
            });
        };
        const auto finishWithError = [onMessageThread](const juce::String& error)
        {
            onMessageThread([error](Gary4juceAudioProcessorEditor& editor)
            {
                editor.showStatusMessage(error, 4500);
                if (editor.shouldShowGenerationFailureDialog(error))
                    editor.showGenerationFailureDialog(error);
                editor.isGenerating = false;
                editor.isCurrentlyQueued = false;
                editor.generationProgress = 0;
                editor.smoothProgressAnimation = false;
                editor.setCareyWaveformState(0, false);
                editor.setActiveOp(ActiveOp::None);
                editor.updateAllGenerationButtonStates();
                editor.repaint();
            });
        };

        carey_native::Props props;
        juce::String error;
        if (!carey_native::fetchProps(baseUrl, props, error))
        {
            finishWithError(error);
            return;
        }

        carey_native::Request request;
        double trimToSeconds = 0.0;
        if (!job.prepare(props, request, trimToSeconds, error))
        {
            finishWithError(error.isNotEmpty() ? error : "carey could not prepare the request");
            return;
        }
        const juce::String seed = request.json.getProperty("seed", {}).toString();
        DBG("[carey-" + job.label + "] request: " + juce::JSON::toString(request.json, true));

        if (!isRequestCurrent())
            return;

        int lastPercent = -1;
        juce::String lastText;
        const auto result = carey_native::run(baseUrl, request,
            [&](const carey_native::Progress& progress)
            {
                const int percent = carey_native::progressPercent(progress);
                const juce::String text = carey_native::progressText(progress);
                if (percent == lastPercent && text == lastText)
                    return;
                lastPercent = percent;
                lastText = text;
                const bool queued = progress.stage == "queued";
                onMessageThread([percent, text, queued](Gary4juceAudioProcessorEditor& editor)
                {
                    editor.smoothProgressAnimation = false;
                    editor.generationProgress = percent;
                    editor.isCurrentlyQueued = queued;
                    editor.setCareyWaveformState(percent, queued);
                    editor.showStatusMessage("carey: " + text, 2500);
                    editor.repaint();
                });
            },
            [&]() { return !isRequestCurrent(); });

        if (result.cancelled)
        {
            DBG("[carey-" + job.label + "] cancelled");
            return;
        }
        if (!result.succeeded)
        {
            finishWithError(result.error);
            return;
        }

        juce::MemoryBlock audio = result.audio;
        if (trimToSeconds > 0.0)
            careyTrimWav(audio, trimToSeconds);
        const juce::String audioBase64 = juce::Base64::toBase64(audio.getData(), audio.getSize());
        const juce::String label = job.label;

        onMessageThread([audioBase64, seed, label](Gary4juceAudioProcessorEditor& editor)
        {
            editor.isCurrentlyQueued = false;
            editor.generationProgress = 100;
            editor.smoothProgressAnimation = false;
            editor.setCareyWaveformState(100, false);
            editor.setActiveOp(ActiveOp::None);
            editor.setCareyLastSeed(seed);
            editor.saveGeneratedAudio(audioBase64);
            editor.showStatusMessage("carey " + label + " complete", 2500);
            editor.updateAllGenerationButtonStates();
        });
    });
}

// Lego: one new track, generated over the recording.
void Gary4juceAudioProcessorEditor::sendToCarey()
{
    juce::String trackName = currentCareyTrackName.trim().toLowerCase();
    if (trackName == "piano")
        trackName = "keyboard";
    static const juce::StringArray kTracks = {
        "vocals", "backing_vocals", "drums", "bass", "guitar", "keyboard",
        "percussion", "strings", "synth", "fx", "brass", "woodwinds"
    };
    if (!kTracks.contains(trackName))
        trackName = "vocals";

    // An empty caption takes one of the track's own, as the wrapper used to.
    juce::String caption = currentCareyCaption.trim();
    if (caption.isEmpty() && careyUI != nullptr)
        caption = careyUI->randomLegoCaptionForTrack(trackName);

    const int bpm = juce::jmax(1, juce::roundToInt(getCareyBpmForRequest()));
    const juce::int64 seed = careyUI != nullptr && careyUI->getSeed() >= 0 ? careyUI->getSeed() : carey_native::newSeed();
    const auto json = careyRequest("lego", caption, currentCareyLyrics, bpm, currentCareyKeyScale, currentCareyTimeSig,
                                   currentCareyLanguage, juce::jmax(1, currentCareySteps),
                                   juce::jlimit(3.0, 10.0, currentLegoCfg), seed);
    json->setProperty("track", trackName);
    json->setProperty("repainting_start", 0.0);
    json->setProperty("repainting_end", -1.0);

    const juce::String lora = getSelectedCareyLegoLora();
    const double loraScale = juce::jlimit(0.0, 1.0, currentCareyLegoLoraScale);
    const bool xl = currentCareyXl;
    const bool loopAssist = currentCareyLoopAssistEnabled;
    const bool trimToInput = currentCareyTrimToInputEnabled;
    const juce::File bufferFile = getGaryBufferFile();

    // Lego needs a base model: turbo and sft do not take the task.
    startCareyJob({ "lego", true, [=](const carey_native::Props& props, carey_native::Request& request,
                                double& trimToSeconds, juce::String& error)
    {
        if (!careySelectModels(props, "base", xl, lora, loraScale, *json, error))
            return false;
        double inputSeconds = 0.0, conditioningSeconds = 0.0;
        if (!careyConditioningAudio(bufferFile, bpm, loopAssist, request.sourceAudio, inputSeconds, conditioningSeconds, error))
            return false;
        json->setProperty("duration", conditioningSeconds);
        trimToSeconds = trimToInput ? inputSeconds : 0.0;
        request.json = juce::var(json.get());
        return true;
    } });
}

// Extract (hidden since v5.0.0): one track pulled out of the recording.
void Gary4juceAudioProcessorEditor::sendToCareyExtract()
{
    juce::String trackName = currentCareyExtractTrackName.trim().toLowerCase();
    static const juce::StringArray kTracks = {
        "drums", "vocals", "backing_vocals", "bass", "guitar", "keyboard",
        "synth", "strings", "percussion", "brass", "woodwinds", "fx"
    };
    if (!kTracks.contains(trackName))
        trackName = "drums";

    int bpm = juce::jlimit(20, 300, juce::roundToInt(getCareyBpmForRequest()));
    if (juce::JUCEApplicationBase::isStandaloneApp())
        bpm = juce::jlimit(20, 300, currentCareyExtractBpm);
    const juce::int64 seed = careyUI != nullptr && careyUI->getSeed() >= 0 ? careyUI->getSeed() : carey_native::newSeed();
    const auto json = careyRequest("extract", {}, {}, bpm, {}, {}, currentCareyLanguage,
                                   juce::jlimit(32, 100, currentCareyExtractSteps),
                                   juce::jlimit(3.0, 10.0, currentCareyExtractCfg), seed);
    json->setProperty("track", trackName);
    const bool xl = currentCareyXl;
    const juce::File bufferFile = getGaryBufferFile();

    startCareyJob({ "extract", true, [=](const carey_native::Props& props, carey_native::Request& request,
                                   double&, juce::String& error)
    {
        if (!careySelectModels(props, "base", xl, {}, 1.0, *json, error))
            return false;
        double inputSeconds = 0.0, conditioningSeconds = 0.0;
        if (!careyConditioningAudio(bufferFile, bpm, false, request.sourceAudio, inputSeconds, conditioningSeconds, error))
            return false;
        json->setProperty("duration", inputSeconds);
        request.json = juce::var(json.get());
        return true;
    } });
}

// Continue: the recording kept as it is, extended to the target length by
// repainting everything after it.
void Gary4juceAudioProcessorEditor::sendToCareyComplete()
{
    int bpm = juce::jmax(1, juce::roundToInt(getCareyBpmForRequest()));
    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        const int standaloneBpm = juce::jlimit(40, 240, currentCareyCompleteBpm);
        if (standaloneBpm > 0)
            bpm = standaloneBpm;
    }

    const juce::String variant = careyVariant(currentCareyCompleteModel, true);
    const bool turbo = variant == "turbo";
    const int steps = turbo ? CareyUI::kFixedCompleteTurboSteps : juce::jlimit(32, 100, currentCareyCompleteSteps);
    const double cfg = turbo ? CareyUI::kFixedCompleteTurboCfg : juce::jlimit(3.0, 10.0, currentCompleteCfg);
    const int maximumSeconds = isUsingLocalhost ? CareyUI::kLocalMaximumDurationSeconds : CareyUI::kRemoteMaximumDurationSeconds;
    const int targetSeconds = juce::jlimit(30, maximumSeconds, currentCareyCompleteDurationSeconds);
    const juce::int64 seed = careyUI != nullptr && careyUI->getSeed() >= 0 ? careyUI->getSeed() : carey_native::newSeed();
    const auto json = careyRequest("repaint", currentCareyCompleteCaption.trim(), currentCareyLyrics, bpm,
                                   currentCareyKeyScale, currentCareyTimeSig, currentCareyLanguage, steps, cfg, seed);

    const juce::String lora = getSelectedCareyCompleteLora();
    const double loraScale = juce::jlimit(0.0, 1.0, currentCareyCompleteLoraScale);
    const bool xl = currentCareyXl;
    const bool useSourceAsReference = currentCompleteUseSrcAsRef;
    const juce::File bufferFile = getGaryBufferFile();

    startCareyJob({ "continue", true, [=](const carey_native::Props& props, carey_native::Request& request,
                                    double&, juce::String& error)
    {
        if (!careySelectModels(props, variant, xl, lora, loraScale, *json, error))
            return false;
        double inputSeconds = 0.0, conditioningSeconds = 0.0;
        if (!careyConditioningAudio(bufferFile, bpm, false, request.sourceAudio, inputSeconds, conditioningSeconds, error))
            return false;
        if (inputSeconds >= (double) targetSeconds - 1.0)
        {
            error = "the target length must be longer than your audio (" + juce::String(inputSeconds, 1) + "s)";
            return false;
        }
        json->setProperty("duration", (double) targetSeconds);
        json->setProperty("repainting_start", inputSeconds);
        json->setProperty("repainting_end", (double) targetSeconds);
        if (useSourceAsReference)
            request.referenceAudio = request.sourceAudio;
        request.json = juce::var(json.get());
        return true;
    } });
}

// Generate: a new piece from the caption and lyrics alone, with continue's
// settings, which generate shares.
void Gary4juceAudioProcessorEditor::sendToCareyGenerate()
{
    int bpm = juce::jmax(1, juce::roundToInt(getCareyBpmForRequest()));
    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        const int standaloneBpm = juce::jlimit(40, 240, currentCareyCompleteBpm);
        if (standaloneBpm > 0)
            bpm = standaloneBpm;
    }

    const juce::String variant = careyVariant(currentCareyCompleteModel, true);
    const bool turbo = variant == "turbo";
    const int steps = turbo ? CareyUI::kFixedCompleteTurboSteps : juce::jlimit(32, 100, currentCareyCompleteSteps);
    const double cfg = turbo ? CareyUI::kFixedCompleteTurboCfg : juce::jlimit(3.0, 10.0, currentCompleteCfg);
    const int maximumSeconds = isUsingLocalhost ? CareyUI::kLocalMaximumDurationSeconds : CareyUI::kRemoteMaximumDurationSeconds;
    const int seconds = juce::jlimit(30, maximumSeconds, currentCareyCompleteDurationSeconds);
    const juce::int64 seed = careyUI != nullptr && careyUI->getSeed() >= 0 ? careyUI->getSeed() : carey_native::newSeed();
    const auto json = careyRequest("text2music", currentCareyCompleteCaption.trim(), currentCareyLyrics, bpm,
                                   currentCareyKeyScale, currentCareyTimeSig, currentCareyLanguage, steps, cfg, seed);
    json->setProperty("duration", (double) seconds);

    const juce::String lora = getSelectedCareyCompleteLora();
    const double loraScale = juce::jlimit(0.0, 1.0, currentCareyCompleteLoraScale);
    const bool xl = currentCareyXl;

    startCareyJob({ "generate", false, [=](const carey_native::Props& props, carey_native::Request& request,
                                           double&, juce::String& error)
    {
        if (!careySelectModels(props, variant, xl, lora, loraScale, *json, error))
            return false;
        request.json = juce::var(json.get());
        return true;
    } });
}

// Cover: the recording reimagined from its structure (cover-nofsq).
void Gary4juceAudioProcessorEditor::sendToCareyCover()
{
    const int bpm = juce::jmax(1, juce::roundToInt(getCareyBpmForRequest()));
    const juce::String variant = kCareyCoverModelExperimentEnabled ? careyVariant(currentCoverModel, false) : juce::String("turbo");
    const bool turbo = variant == "turbo";
    const int steps = turbo ? CareyUI::kFixedCoverSteps : juce::jlimit(8, 100, currentCoverSteps);
    const double cfg = turbo ? CareyUI::kFixedCoverCfg : juce::jlimit(3.0, 10.0, currentCoverCfg);
    const juce::int64 seed = careyUI != nullptr && careyUI->getSeed() >= 0 ? careyUI->getSeed() : carey_native::newSeed();
    const auto json = careyRequest("cover-nofsq", currentCoverCaption.trim(), currentCareyLyrics, bpm,
                                   currentCareyKeyScale, currentCareyTimeSig, currentCareyLanguage, steps, cfg, seed);
    json->setProperty("audio_cover_strength", juce::jlimit(0.0, 1.0, currentCoverAudioStrength));
    json->setProperty("cover_noise_strength", juce::jlimit(0.0, 1.0, currentCoverNoiseStrength));

    const juce::String lora = getSelectedCareyCoverLora();
    const double loraScale = juce::jlimit(0.0, 1.0, currentCoverLoraScale);
    const bool xl = currentCareyXl;
    const bool loopAssist = currentCoverLoopAssistEnabled;
    const bool trimToInput = currentCoverTrimToInputEnabled;
    const bool useSourceAsReference = currentCoverUseSrcAsRef;
    const juce::File bufferFile = getGaryBufferFile();

    startCareyJob({ "cover", true, [=](const carey_native::Props& props, carey_native::Request& request,
                                 double& trimToSeconds, juce::String& error)
    {
        if (!careySelectModels(props, variant, xl, lora, loraScale, *json, error))
            return false;
        double inputSeconds = 0.0, conditioningSeconds = 0.0;
        if (!careyConditioningAudio(bufferFile, bpm, loopAssist, request.sourceAudio, inputSeconds, conditioningSeconds, error))
            return false;
        json->setProperty("duration", conditioningSeconds);
        trimToSeconds = trimToInput ? inputSeconds : 0.0;
        if (useSourceAsReference)
            request.referenceAudio = request.sourceAudio;
        request.json = juce::var(json.get());
        return true;
    } });
}
