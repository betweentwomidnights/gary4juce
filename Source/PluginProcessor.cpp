/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin processor.
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Gary4juceAudioProcessor::Gary4juceAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
{
    // Check backend health on startup
    checkBackendHealth();
}

Gary4juceAudioProcessor::~Gary4juceAudioProcessor()
{
    DBG("=== STOPPING PROCESSOR BACKGROUND OPERATIONS ===");
    stopHealthChecks();
}

void Gary4juceAudioProcessor::stopHealthChecks()
{
    DBG("Stopping health checks - setting background operations flag");
    shouldStopBackgroundOperations.store(true);
    
    // Wait briefly for any ongoing health check requests to notice the flag and abort
    juce::Thread::sleep(100);
    
    DBG("Health checks stopped - ongoing requests should abort");
}

void Gary4juceAudioProcessor::checkBackendHealth()
{
    // SAFETY: Don't start new health checks if stopping
    if (shouldStopBackgroundOperations.load()) {
        DBG("Health check aborted - background operations stopped");
        return;
    }

    DBG("Checking backend health at: " + backendBaseUrl);

    // Create health check request in background thread
    juce::Thread::launch([this]() {
        // SAFETY: Exit if background operations stopped
        if (shouldStopBackgroundOperations.load()) {
            DBG("Health check thread aborted - background operations stopped");
            return;
        }
        juce::URL healthUrl(backendBaseUrl + "/health");

        // Create input stream for the URL
        std::unique_ptr<juce::InputStream> stream(healthUrl.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(5000)  // 5 second timeout
        ));

        if (stream != nullptr)
        {
            // Read the entire response
            juce::String responseText = stream->readEntireStreamAsString();
            DBG("Backend health response: " + responseText);

            // Simple check - if we got any response, assume it's working
            bool isHealthy = !responseText.isEmpty();

            // Try to parse JSON for more detailed check
            if (isHealthy)
            {
                try {
                    auto json = juce::JSON::parse(responseText);
                    if (auto* jsonObj = json.getDynamicObject())
                    {
                        auto status = jsonObj->getProperty("status").toString();
                        isHealthy = (status == "live");
                        DBG("Backend status: " + status);
                    }
                }
                catch (...)
                {
                    DBG("Failed to parse health response JSON, but got response");
                    // Keep isHealthy = true since we got a response
                }
            }

            // Update connection status on message thread
            juce::MessageManager::callAsync([this, isHealthy]() {
                // SAFETY: Don't process if background operations stopped
                if (shouldStopBackgroundOperations.load()) {
                    DBG("Health check success callback aborted");
                    return;
                }
                setBackendConnectionStatus(isHealthy);
                });
        }
        else
        {
            DBG("Failed to create health check stream");
            juce::MessageManager::callAsync([this]() {
                // SAFETY: Don't process if background operations stopped
                if (shouldStopBackgroundOperations.load()) {
                    DBG("Health check failure callback aborted");
                    return;
                }
                setBackendConnectionStatus(false);
                });
        }
        });
}

void Gary4juceAudioProcessor::setBackendConnectionStatus(bool connected)
{
    if (backendConnected != connected)
    {
        backendConnected = connected;
        DBG("Backend connection status changed: " + juce::String(connected ? "Connected" : "Disconnected"));

        // Notify editor of status change
        if (auto* editor = getActiveEditor())
            if (auto* myEditor = dynamic_cast<Gary4juceAudioProcessorEditor*>(editor))
                myEditor->updateConnectionStatus(connected);
    }
}

//==============================================================================
// Backend URL management methods

juce::String Gary4juceAudioProcessor::getServiceUrl(ServiceType service, const juce::String& endpoint) const
{
    if (isUsingLocalhost) 
    {
        switch (service) 
        {
            case ServiceType::Gary:
            case ServiceType::Terry:
                return "http://localhost:8000" + endpoint;
            case ServiceType::Jerry:
                return "http://localhost:8005" + endpoint;
        }
    }
    
    // Remote backend - same domain for all services
    return "https://g4l.thecollabagepatch.com" + endpoint;
}

void Gary4juceAudioProcessor::setUsingLocalhost(bool useLocalhost)
{
    if (isUsingLocalhost != useLocalhost)
    {
        isUsingLocalhost = useLocalhost;

        // Update base URL for health checks (use Gary service as default)
        backendBaseUrl = getServiceUrl(ServiceType::Gary, "");

        DBG("Backend switched to: " + getCurrentBackendType());
        DBG("New base URL: " + backendBaseUrl);

        // FIRST: Reset connection state and notify editor immediately
        setBackendConnectionStatus(false);

        // SAFETY: Only check health if not stopping
        if (!shouldStopBackgroundOperations.load()) {
            // THEN: Check health with new URL (this will update to true if backend is running)
            checkBackendHealth();
        }
    }
}

//==============================================================================
// Session ID management methods

void Gary4juceAudioProcessor::setCurrentSessionId(const juce::String& sessionId)
{
    currentSessionId = sessionId;
    sessionTimestamp = juce::Time::getCurrentTime().toMilliseconds();  // ADD THIS
    DBG("Session ID stored in processor: " + sessionId);
    DBG("Session timestamp set to: " + juce::String(sessionTimestamp));
}

juce::String Gary4juceAudioProcessor::getCurrentSessionId() const
{
    // Check if session is still valid before returning it
    if (!isSessionValid())
    {
        DBG("getCurrentSessionId() called but session is stale - returning empty");
        return "";  // Return empty string for stale sessions
    }

    DBG("getCurrentSessionId() called, returning: '" + currentSessionId + "' (age: " +
        juce::String((juce::Time::getCurrentTime().toMilliseconds() - sessionTimestamp) / 1000) + "s)");
    return currentSessionId;
}

void Gary4juceAudioProcessor::clearCurrentSessionId()
{
    currentSessionId = "";
    sessionTimestamp = 0;  // ADD THIS
    DBG("Session ID and timestamp cleared from processor");
}

bool Gary4juceAudioProcessor::isSessionValid() const
{
    if (currentSessionId.isEmpty())
        return false;

    if (sessionTimestamp == 0)
        return false;  // No timestamp set

    auto currentTime = juce::Time::getCurrentTime().toMilliseconds();
    auto sessionAge = currentTime - sessionTimestamp;

    bool isValid = sessionAge < SESSION_TIMEOUT_MS;

    if (!isValid)
    {
        DBG("Session is stale - Age: " + juce::String(sessionAge / 1000) + "s, Timeout: " +
            juce::String(SESSION_TIMEOUT_MS / 1000) + "s");
    }

    return isValid;
}

//==============================================================================
// Recording buffer methods

void Gary4juceAudioProcessor::startRecording()
{
    DBG("Starting recording...");
    juce::ScopedLock lock(bufferLock);

    bufferWritePosition = 0;
    recordedSamples = 0;
    atomicRecordedSamples = 0;
    recording = true;
    atomicRecording = true;
}

void Gary4juceAudioProcessor::stopRecording()
{
    DBG("Stopping recording. Recorded " + juce::String(recordedSamples) + " samples");
    juce::ScopedLock lock(bufferLock);

    recording = false;
    atomicRecording = false;
    atomicRecordedSamples = recordedSamples;
}

void Gary4juceAudioProcessor::clearRecordingBuffer()
{
    juce::ScopedLock lock(bufferLock);

    recordingBuffer.clear();
    bufferWritePosition = 0;
    recordedSamples = 0;
    atomicRecordedSamples = 0;
    recording = false;
    atomicRecording = false;
    
    // CRITICAL: Clear saved samples too
    savedSamples = 0;
    
    DBG("Recording buffer cleared and saved samples reset");
}

void Gary4juceAudioProcessor::saveRecordingToFile(const juce::File& file)
{
    DBG("saveRecordingToFile called with: " + file.getFullPathName());

    // Take a thread-safe snapshot of the recording buffer
    juce::AudioBuffer<float> tempBuffer;
    int snapshotSamples = 0;
    int snapshotChannels = 0;
    double snapshotSampleRate = 0.0;

    {
        juce::ScopedLock lock(bufferLock);

        snapshotSamples = recordedSamples;
        snapshotChannels = recordingBuffer.getNumChannels();
        snapshotSampleRate = currentSampleRate;

        if (snapshotSamples <= 0)
        {
            DBG("No recorded samples to save");
            return;
        }

        DBG("Creating temp buffer with " + juce::String(snapshotSamples) + " samples, " +
            juce::String(snapshotChannels) + " channels");

        // Create temp buffer and copy data while holding the lock
        tempBuffer.setSize(snapshotChannels, snapshotSamples);

        for (int channel = 0; channel < snapshotChannels; ++channel)
        {
            tempBuffer.copyFrom(channel, 0, recordingBuffer, channel, 0, snapshotSamples);
        }
    }
    // Lock is released here, safe to do file I/O

    // CRITICAL FIX: Delete existing file first to prevent appending
    if (file.exists())
    {
        bool deleteSuccess = file.deleteFile();
        DBG("Existing file deleted: " + juce::String(deleteSuccess ? "success" : "failed"));
    }

    DBG("Creating file output stream...");

    // Create file output stream - this should now create a fresh file
    std::unique_ptr<juce::FileOutputStream> fileStream(file.createOutputStream());

    if (fileStream == nullptr)
    {
        DBG("Failed to create file output stream for: " + file.getFullPathName());
        return;
    }

    // ADDITIONAL FIX: Use 16-bit instead of 24-bit for smaller files
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer;

    DBG("Creating WAV writer...");
    writer.reset(wavFormat.createWriterFor(fileStream.release(), // Transfer ownership
        snapshotSampleRate,
        snapshotChannels,
        16, // CHANGED: 16-bit depth instead of 24-bit (smaller files)
        {},
        0));

    if (writer != nullptr)
    {
        DBG("Writing audio buffer to file...");
        bool writeSuccess = writer->writeFromAudioSampleBuffer(tempBuffer, 0, tempBuffer.getNumSamples());
        writer.reset();
        
        if (writeSuccess)
        {
            // CRITICAL: Store saved samples in processor for persistence
            savedSamples = snapshotSamples;
            DBG("Successfully saved and stored " + juce::String(snapshotSamples) + " samples in processor");
        }
        
        DBG("Write operation success: " + juce::String(writeSuccess ? "true" : "false"));

        // VERIFY FILE SIZE
        if (file.exists())
        {
            auto fileSize = file.getSize();
            auto expectedSize = snapshotSamples * snapshotChannels * 2 + 44; // 16-bit + WAV header
            DBG("Final file size: " + juce::String(fileSize) + " bytes (expected ~" + juce::String(expectedSize) + " bytes)");

            if (fileSize > expectedSize * 2) // If more than 2x expected size
            {
                DBG("WARNING: File size is unexpectedly large!");
            }
        }

        DBG("Successfully saved " + juce::String(snapshotSamples) + " samples to " + file.getFullPathName());
    }
    else
    {
        DBG("Failed to create audio writer for file: " + file.getFullPathName());
    }
}

// Thread-safe getters
bool Gary4juceAudioProcessor::isRecording() const
{
    return atomicRecording.load();
}

int Gary4juceAudioProcessor::getRecordedSamples() const
{
    return atomicRecordedSamples.load();
}

float Gary4juceAudioProcessor::getRecordingProgress() const
{
    if (maxRecordingSamples <= 0)
        return 0.0f;

    return juce::jmin(1.0f, (float)atomicRecordedSamples.load() / (float)maxRecordingSamples);
}

//==============================================================================
const juce::String Gary4juceAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Gary4juceAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool Gary4juceAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool Gary4juceAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double Gary4juceAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Gary4juceAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int Gary4juceAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Gary4juceAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String Gary4juceAudioProcessor::getProgramName(int index)
{
    return {};
}

void Gary4juceAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

//==============================================================================
void Gary4juceAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Calculate buffer size for 30 seconds of audio
    int newMaxRecordingSamples = (int)(recordingLengthSeconds * sampleRate);

    // Only reset recording state if sample rate changed or buffer needs resize
    bool needsBufferResize = (maxRecordingSamples != newMaxRecordingSamples) ||
        (recordingBuffer.getNumChannels() != juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels()));

    maxRecordingSamples = newMaxRecordingSamples;

    // Set up recording buffer - use same channel configuration as the plugin
    int numChannels = juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());

    if (needsBufferResize)
    {
        // THREAD SAFETY: Use lock when modifying buffer and state
        juce::ScopedLock lock(bufferLock);

        // Only clear the buffer if we actually need to resize it
        recordingBuffer.setSize(numChannels, maxRecordingSamples);
        recordingBuffer.clear();

        // Reset recording state only when buffer changes - UPDATE ATOMICS TOO
        bufferWritePosition = 0;
        recordedSamples = 0;
        atomicRecordedSamples = 0;  // ADD THIS
        recording = false;
        atomicRecording = false;    // ADD THIS

        DBG("Recording buffer resized: " + juce::String(numChannels) + " channels, " +
            juce::String(maxRecordingSamples) + " samples (" +
            juce::String(recordingLengthSeconds) + " seconds at " +
            juce::String(sampleRate) + " Hz)");
    }
    else
    {
        // THREAD SAFETY: Use lock when modifying recording state
        juce::ScopedLock lock(bufferLock);

        // Just ensure we're not recording when transport stops, but preserve data
        recording = false;
        atomicRecording = false;    // ADD THIS

        DBG("PrepareToPlay called - preserving " + juce::String(recordedSamples) + " recorded samples");
    }

    wasPlaying = false;
}

void Gary4juceAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    stopRecording();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Gary4juceAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void Gary4juceAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Get transport information
    juce::AudioPlayHead* playHead = getPlayHead();
    bool isCurrentlyPlaying = false;

    if (playHead != nullptr)
    {
        juce::Optional<juce::AudioPlayHead::PositionInfo> positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
        {
            isCurrentlyPlaying = positionInfo->getIsPlaying();

            // NEW: Get BPM from DAW
            if (auto bpm = positionInfo->getBpm())
            {
                currentBPM = *bpm;  // Store in atomic variable
            }
        }
    }

    // Detect when transport starts (transition from not playing to playing)
    if (isCurrentlyPlaying && !wasPlaying)
    {
        startRecording();
    }
    else if (!isCurrentlyPlaying && wasPlaying)
    {
        stopRecording();
    }

    wasPlaying = isCurrentlyPlaying;



    // Record audio if we're recording and have space in buffer
    // Use atomic check first (fast path)
    if (atomicRecording.load() && atomicRecordedSamples.load() < maxRecordingSamples)
    {
        // Try to acquire lock without blocking (audio thread must not block)
        if (bufferLock.tryEnter())
        {
            // Double-check conditions under lock
            if (recording && recordedSamples < maxRecordingSamples)
            {
                int samplesToRecord = juce::jmin(buffer.getNumSamples(),
                    maxRecordingSamples - recordedSamples);

                // Copy input audio to recording buffer
                for (int channel = 0; channel < juce::jmin(totalNumInputChannels, recordingBuffer.getNumChannels()); ++channel)
                {
                    recordingBuffer.copyFrom(channel, bufferWritePosition, buffer, channel, 0, samplesToRecord);
                }

                bufferWritePosition += samplesToRecord;
                recordedSamples += samplesToRecord;
                atomicRecordedSamples = recordedSamples; // Update atomic version

                // Stop recording if we've reached the maximum
                if (recordedSamples >= maxRecordingSamples)
                {
                    DBG("Recording buffer full - stopped recording");
                    recording = false;
                    atomicRecording = false;
                }
            }

            bufferLock.exit();
        }
        // If we can't get the lock, skip this block (better than blocking audio thread)
    }

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Pass audio through unchanged
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        // Audio processing would go here
    }
}
//==============================================================================
bool Gary4juceAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Gary4juceAudioProcessor::createEditor()
{
    return new Gary4juceAudioProcessorEditor(*this);
}

//==============================================================================
void Gary4juceAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::XmlElement xmlState("GARY_STATE");
    xmlState.setAttribute("savedSamples", savedSamples.load());
    xmlState.setAttribute("transformRecording", transformRecording.load());
    xmlState.setAttribute("currentSessionId", currentSessionId);
    xmlState.setAttribute("sessionTimestamp", juce::String(sessionTimestamp));  // ADD THIS
    xmlState.setAttribute("isUsingLocalhost", isUsingLocalhost);
    xmlState.setAttribute("undoTransformAvailable", undoTransformAvailable.load());
    xmlState.setAttribute("retryAvailable", retryAvailable.load());

    // DEBUG: Log what we're saving with age calculation
    DBG("=== SAVING STATE ===");
    DBG("savedSamples: " + juce::String(savedSamples.load()));
    DBG("currentSessionId: '" + currentSessionId + "'");
    DBG("sessionTimestamp: " + juce::String(sessionTimestamp));
    if (sessionTimestamp > 0)
    {
        auto age = juce::Time::getCurrentTime().toMilliseconds() - sessionTimestamp;
        DBG("Session age: " + juce::String(age / 1000) + " seconds");
    }
    DBG("undoTransformAvailable: " + juce::String(undoTransformAvailable.load() ? "true" : "false"));
    DBG("retryAvailable: " + juce::String(retryAvailable.load() ? "true" : "false"));

    copyXmlToBinary(xmlState, destData);
}

void Gary4juceAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName("GARY_STATE"))
    {
        savedSamples = xml->getIntAttribute("savedSamples");
        transformRecording = xml->getBoolAttribute("transformRecording");
        currentSessionId = xml->getStringAttribute("currentSessionId");
        sessionTimestamp = xml->getStringAttribute("sessionTimestamp").getLargeIntValue();  // ADD THIS
        isUsingLocalhost = xml->getBoolAttribute("isUsingLocalhost");
        undoTransformAvailable = xml->getBoolAttribute("undoTransformAvailable");
        retryAvailable = xml->getBoolAttribute("retryAvailable");
        backendBaseUrl = getServiceUrl(ServiceType::Gary, "");

        // DEBUG: Log what we're loading
        DBG("=== LOADING STATE ===");
        DBG("savedSamples: " + juce::String(savedSamples.load()));
        DBG("currentSessionId: '" + currentSessionId + "'");
        DBG("sessionTimestamp: " + juce::String(sessionTimestamp));
        DBG("undoTransformAvailable: " + juce::String(undoTransformAvailable.load() ? "true" : "false"));
        DBG("retryAvailable: " + juce::String(retryAvailable.load() ? "true" : "false"));

        // CRITICAL: Check if loaded session is stale and clean up if needed
        if (!currentSessionId.isEmpty() && sessionTimestamp > 0)
        {
            auto currentTime = juce::Time::getCurrentTime().toMilliseconds();
            auto sessionAge = currentTime - sessionTimestamp;

            DBG("Session age on load: " + juce::String(sessionAge / 1000) + " seconds");

            if (sessionAge >= SESSION_TIMEOUT_MS)
            {
                DBG("=== CLEANING UP STALE SESSION ===");
                DBG("Session is " + juce::String(sessionAge / 60000) + " minutes old, clearing...");

                // Clear stale session and all associated state
                currentSessionId = "";
                sessionTimestamp = 0;
                undoTransformAvailable.store(false);
                retryAvailable.store(false);

                DBG("Stale session cleaned up - all operation flags cleared");
            }
            else
            {
                DBG("Session is valid - " + juce::String((SESSION_TIMEOUT_MS - sessionAge) / 60000) +
                    " minutes remaining until timeout");
            }
        }
        else if (!currentSessionId.isEmpty() && sessionTimestamp == 0)
        {
            // Handle legacy state (no timestamp) - assume it's stale
            DBG("=== LEGACY SESSION WITHOUT TIMESTAMP - ASSUMING STALE ===");
            currentSessionId = "";
            undoTransformAvailable.store(false);
            retryAvailable.store(false);
            DBG("Legacy session cleared");
        }

        // Trigger a health check to restore connection status
        checkBackendHealth();
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Gary4juceAudioProcessor();
}