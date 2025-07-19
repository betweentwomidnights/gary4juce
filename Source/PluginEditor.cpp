/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Gary4juceAudioProcessorEditor::Gary4juceAudioProcessorEditor(Gary4juceAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(400, 850);  // Made taller to accommodate Gary controls

    // Check initial backend connection status
    isConnected = audioProcessor.isBackendConnected();
    DBG("Editor created, backend connection status: " + juce::String(isConnected ? "Connected" : "Disconnected"));

    // Set up the "Check Connection" button
    checkConnectionButton.setButtonText("Check Backend Connection");
    checkConnectionButton.onClick = [this]() {
        DBG("Manual backend health check requested");
        audioProcessor.checkBackendHealth();

        // Show immediate feedback
        checkConnectionButton.setButtonText("Checking...");
        checkConnectionButton.setEnabled(false);

        // Re-enable button after a delay
        juce::Timer::callAfterDelay(3000, [this]() {
            checkConnectionButton.setButtonText("Check Backend Connection");
            checkConnectionButton.setEnabled(true);
            });
        };

    // Set up the "Save Buffer" button
    saveBufferButton.setButtonText("Save Recording Buffer");
    saveBufferButton.onClick = [this]() {
        saveRecordingBuffer();
        };
    saveBufferButton.setEnabled(false); // Initially disabled

    // Set up the "Clear Buffer" button
    clearBufferButton.setButtonText("Clear Buffer");
    clearBufferButton.onClick = [this]() {
        clearRecordingBuffer();
        };

    addAndMakeVisible(checkConnectionButton);
    addAndMakeVisible(saveBufferButton);
    addAndMakeVisible(clearBufferButton);

    // Set up Gary (MusicGen) controls
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
    sendToGaryButton.onClick = [this]() {
        sendToGary();
        };
    sendToGaryButton.setEnabled(false); // Initially disabled until we have audio
    addAndMakeVisible(sendToGaryButton);

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
    playOutputButton.onClick = [this]() {
        playOutputAudio();  // Let the smart function handle play/pause/resume logic
        };
    playOutputButton.setEnabled(false);
    addAndMakeVisible(playOutputButton);

    // Clear output button
    clearOutputButton.setButtonText("Clear Output");
    clearOutputButton.onClick = [this]() {
        clearOutputAudio();
        };
    clearOutputButton.setEnabled(false); // Initially disabled
    addAndMakeVisible(clearOutputButton);

    // Stop output button  
    stopOutputButton.setButtonText("Stop");
    stopOutputButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    stopOutputButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    stopOutputButton.onClick = [this]() {
        fullStopOutputPlayback();
        };
    stopOutputButton.setEnabled(false); // Initially disabled
    addAndMakeVisible(stopOutputButton);

    // Initialize output file path
    auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto garyDir = documentsDir.getChildFile("gary4juce");
    outputAudioFile = garyDir.getChildFile("myOutput.wav");

    // Check if output file already exists and load it
    if (outputAudioFile.exists())
    {
        loadOutputAudioFile();
    }
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

//==============================================================================
// Updated timerCallback() with smooth progress animation:
void Gary4juceAudioProcessorEditor::timerCallback()
{
    updateRecordingStatus();

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

    // Update Gary button state - need saved audio to send to Gary
    sendToGaryButton.setEnabled(savedSamples > 0 && isConnected);

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
    currentSessionId = "";
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

                showStatusMessage("Generating: " + juce::String(serverProgress) + "%", 1000);
                DBG("Server progress: " + juce::String(serverProgress) + "%, animating from " +
                    juce::String(lastKnownProgress));
                return;
            }

            // ... rest of the function stays the same for completion handling
            auto audioData = responseObj->getProperty("audio_data").toString();
            if (audioData.isNotEmpty())
            {
                stopPolling();
                showStatusMessage("Audio generation complete!", 3000);
                saveGeneratedAudio(audioData);
                DBG("Successfully received generated audio: " + juce::String(audioData.length()) + " chars");
            }
            else
            {
                auto status = responseObj->getProperty("status").toString();
                if (status == "failed")
                {
                    auto error = responseObj->getProperty("error").toString();
                    stopPolling();
                    showStatusMessage("Generation failed: " + error, 5000);
                }
                else if (status == "completed")
                {
                    stopPolling();
                    showStatusMessage("Generation completed but no audio received", 3000);
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
    if (savedSamples <= 0)
    {
        showStatusMessage("Please save your recording first!");
        return;
    }

    if (!isConnected)
    {
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

void Gary4juceAudioProcessorEditor::clearRecordingBuffer()
{
    audioProcessor.clearRecordingBuffer();
    savedSamples = 0;  // Reset saved samples too
    updateRecordingStatus();
}

void Gary4juceAudioProcessorEditor::updateConnectionStatus(bool connected)
{
    if (isConnected != connected)
    {
        isConnected = connected;
        DBG("Backend connection status updated: " + juce::String(connected ? "Connected" : "Disconnected"));

        // Update Gary button state when connection changes
        sendToGaryButton.setEnabled(savedSamples > 0 && isConnected);

        repaint(); // Trigger a redraw to update Gary section border

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

        // Seek to that position
        seekToPosition(seekTime);

        DBG("Click-to-seek: " + juce::String(clickPercent * 100.0, 1) + "% = " +
            juce::String(seekTime, 2) + "s");

        return; // We handled this click
    }

    // If we didn't handle it, pass to parent
    juce::Component::mouseDown(event);
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


void Gary4juceAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colours::black);

    // Title
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawFittedText("Gary4JUCE",
        juce::Rectangle<int>(0, 10, getWidth(), 40),
        juce::Justification::centred, 1);

    // Connection status display
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    if (isConnected)
    {
        g.setColour(juce::Colours::lightgreen);
        g.drawFittedText("CONNECTED",
            juce::Rectangle<int>(0, 50, getWidth(), 25),
            juce::Justification::centred, 1);
    }
    else
    {
        g.setColour(juce::Colours::orange);
        g.drawFittedText("DISCONNECTED",
            juce::Rectangle<int>(0, 50, getWidth(), 25),
            juce::Justification::centred, 1);
    }

    // Recording status section header
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawFittedText("Recording Buffer",
        juce::Rectangle<int>(0, 85, getWidth(), 25),
        juce::Justification::centred, 1);

    // Draw the INPUT waveform
    drawWaveform(g, waveformArea);

    // Status text below input waveform
    g.setFont(juce::FontOptions(12.0f));
    auto statusArea = juce::Rectangle<int>(0, waveformArea.getBottom() + 10, getWidth(), 20);

    if (hasStatusMessage && !statusMessage.isEmpty())
    {
        g.setColour(juce::Colours::lightgreen);
        g.drawText(statusMessage, statusArea, juce::Justification::centred);
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
        g.drawText(statusText, statusArea, juce::Justification::centred);
    }

    // Sample count info for input
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
        auto infoArea = juce::Rectangle<int>(0, statusArea.getBottom() + 5, getWidth(), 15);
        g.drawText(infoText, infoArea, juce::Justification::centred);
    }

    // Draw Gary section background
    auto garyBounds = juce::Rectangle<int>(20, 320, getWidth() - 40, 150);
    g.setColour(juce::Colour(0x15, 0x15, 0x15));
    g.fillRoundedRectangle(garyBounds.toFloat(), 5.0f);

    // Gary section border
    if (savedSamples > 0 && isConnected)
    {
        g.setColour(juce::Colours::darkred.withAlpha(0.6f));
    }
    else
    {
        g.setColour(juce::Colour(0x30, 0x30, 0x30));
    }
    g.drawRoundedRectangle(garyBounds.toFloat(), 5.0f, 1.0f);

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
        auto outputInfoArea = juce::Rectangle<int>(0, outputWaveformArea.getBottom() + 5, getWidth(), 15);
        g.drawText(outputInfo, outputInfoArea, juce::Justification::centred);
    }
}

void Gary4juceAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Reserve space for recording waveform area (top section)
    auto topSection = bounds.removeFromTop(300);
    waveformArea = topSection.removeFromBottom(150).reduced(20, 10);

    // Reserve space for Gary controls section
    auto garySection = bounds.removeFromTop(160).reduced(20, 10);

    // Gary section title
    auto garyTitleArea = garySection.removeFromTop(25);
    garyLabel.setBounds(garyTitleArea);

    // Prompt duration controls
    auto promptRow = garySection.removeFromTop(30);
    auto promptLabelArea = promptRow.removeFromLeft(100);
    promptDurationLabel.setBounds(promptLabelArea);
    promptDurationSlider.setBounds(promptRow.reduced(5, 0));

    // Model selection controls  
    auto modelRow = garySection.removeFromTop(30);
    auto modelLabelArea = modelRow.removeFromLeft(100);
    modelLabel.setBounds(modelLabelArea);
    modelComboBox.setBounds(modelRow.reduced(5, 0));

    // Send to Gary button
    auto garyButtonRow = garySection.removeFromTop(40);
    sendToGaryButton.setBounds(garyButtonRow.reduced(50, 5));

    // NEW: Output waveform section
    auto outputSection = bounds.removeFromTop(200).reduced(20, 10);

    // Output label
    auto outputLabelArea = outputSection.removeFromTop(25);
    outputLabel.setBounds(outputLabelArea);

    // Output waveform area
    outputWaveformArea = outputSection.removeFromTop(120);

    // Output control buttons - now three buttons
    auto outputButtonArea = outputSection.removeFromTop(35);
    auto buttonWidth = outputButtonArea.getWidth() / 3;

    auto playArea = outputButtonArea.removeFromLeft(buttonWidth).reduced(2, 0);
    auto stopArea = outputButtonArea.removeFromLeft(buttonWidth).reduced(2, 0);
    auto clearArea = outputButtonArea.reduced(2, 0);

    playOutputButton.setBounds(playArea);
    stopOutputButton.setBounds(stopArea);
    clearOutputButton.setBounds(clearArea);

    // Reserve space for input control buttons at bottom
    auto buttonArea = bounds.removeFromBottom(100).reduced(20);

    // Arrange input control buttons in rows
    auto buttonRow1 = buttonArea.removeFromTop(30);
    auto buttonRow2 = buttonArea.removeFromTop(30);

    // First row: Save and Clear input buttons
    auto saveArea = buttonRow1.removeFromLeft(buttonRow1.getWidth() / 2).reduced(5, 0);
    auto clearInputArea = buttonRow1.reduced(5, 0);
    saveBufferButton.setBounds(saveArea);
    clearBufferButton.setBounds(clearInputArea);

    // Second row: Connection check button
    checkConnectionButton.setBounds(buttonRow2.reduced(5, 0));
}