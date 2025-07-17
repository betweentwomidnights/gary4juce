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
    setSize(400, 400);

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

    // Start timer to update recording status (refresh every 100ms)
    startTimer(100);

    // Initial status update
    updateRecordingStatus();
}

Gary4juceAudioProcessorEditor::~Gary4juceAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void Gary4juceAudioProcessorEditor::timerCallback()
{
    updateRecordingStatus();
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

    // Repaint if status changed
    if (wasRecording != isRecording ||
        std::abs(wasProgress - recordingProgress) > 0.01f ||
        wasSamples != recordedSamples)
    {
        repaint();
    }
}

void Gary4juceAudioProcessorEditor::saveRecordingBuffer()
{
    if (recordedSamples <= 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "No Recording",
            "There's no recorded audio to save. Press play in your DAW to start recording.");
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

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Recording Saved",
        "Recording saved to:\n" + recordingFile.getFullPathName());
}

void Gary4juceAudioProcessorEditor::clearRecordingBuffer()
{
    audioProcessor.clearRecordingBuffer();
    updateRecordingStatus();
}

void Gary4juceAudioProcessorEditor::updateConnectionStatus(bool connected)
{
    if (isConnected != connected)
    {
        isConnected = connected;
        DBG("Backend connection status updated: " + juce::String(connected ? "Connected" : "Disconnected"));
        repaint(); // Trigger a redraw

        // Re-enable check button if it was disabled
        if (!checkConnectionButton.isEnabled())
        {
            checkConnectionButton.setButtonText("Check Backend Connection");
            checkConnectionButton.setEnabled(true);
        }
    }
}

void Gary4juceAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

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
        g.drawFittedText("? Backend Connected",
            juce::Rectangle<int>(0, 50, getWidth(), 25),
            juce::Justification::centred, 1);
    }
    else
    {
        g.setColour(juce::Colours::orange);
        g.drawFittedText("? Backend Disconnected",
            juce::Rectangle<int>(0, 50, getWidth(), 25),
            juce::Justification::centred, 1);
    }

    // Recording status section
    auto recordingArea = juce::Rectangle<int>(20, 85, getWidth() - 40, 120);

    // Recording status background
    g.setColour(juce::Colour(0x20, 0x20, 0x20));
    g.fillRoundedRectangle(recordingArea.toFloat(), 5.0f);

    // Recording status border
    if (isRecording)
    {
        g.setColour(juce::Colours::red);
        g.drawRoundedRectangle(recordingArea.toFloat(), 5.0f, 2.0f);
    }
    else
    {
        g.setColour(juce::Colour(0x40, 0x40, 0x40));
        g.drawRoundedRectangle(recordingArea.toFloat(), 5.0f, 1.0f);
    }

    // Recording indicator
    auto indicatorArea = recordingArea.removeFromTop(30).reduced(10, 5);
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));

    if (isRecording)
    {
        // Animated red dot
        g.setColour(juce::Colours::red);
        auto dotArea = indicatorArea.removeFromLeft(20);
        g.fillEllipse(dotArea.getCentreX() - 5, dotArea.getCentreY() - 5, 10, 10);

        g.setColour(juce::Colours::white);
        g.drawText("RECORDING", indicatorArea.reduced(5, 0), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour(juce::Colour(0x60, 0x60, 0x60));
        auto dotArea = indicatorArea.removeFromLeft(20);
        g.fillEllipse(dotArea.getCentreX() - 5, dotArea.getCentreY() - 5, 10, 10);

        g.setColour(juce::Colours::lightgrey);
        g.drawText("READY", indicatorArea.reduced(5, 0), juce::Justification::centredLeft);
    }

    // Progress bar
    auto progressArea = recordingArea.removeFromTop(20).reduced(10, 0);

    // Progress bar background
    g.setColour(juce::Colour(0x30, 0x30, 0x30));
    g.fillRoundedRectangle(progressArea.toFloat(), 3.0f);

    // Progress bar fill
    if (recordingProgress > 0.0f)
    {
        auto fillWidth = (int)(progressArea.getWidth() * recordingProgress);
        auto fillArea = progressArea.withWidth(fillWidth);

        juce::Colour progressColor = isRecording ? juce::Colours::red : juce::Colours::green;
        g.setColour(progressColor);
        g.fillRoundedRectangle(fillArea.toFloat(), 3.0f);
    }

    // Progress text
    auto textArea = recordingArea.removeFromTop(25).reduced(10, 0);
    g.setFont(juce::FontOptions(12.0f));
    g.setColour(juce::Colours::white);

    juce::String progressText;
    if (recordedSamples > 0)
    {
        double recordedSeconds = (double)recordedSamples / 44100.0; // Assuming 44.1kHz for display
        progressText = juce::String::formatted("%.1f / 30.0 seconds (%d samples)",
            recordedSeconds, recordedSamples);
    }
    else
    {
        progressText = "Press PLAY in DAW to start recording";
    }

    g.drawText(progressText, textArea, juce::Justification::centred);

    // Instructions
    auto instructionArea = recordingArea;
    g.setFont(juce::FontOptions(11.0f));
    g.setColour(juce::Colours::lightgrey);
    g.drawText("Recording starts automatically when DAW transport plays.\nPlace plugin on master track to record full mix.",
        instructionArea, juce::Justification::centred);
}

void Gary4juceAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Reserve space for buttons at bottom
    auto buttonArea = bounds.removeFromBottom(100).reduced(20);

    // Arrange buttons in rows
    auto buttonRow1 = buttonArea.removeFromTop(30);
    auto buttonRow2 = buttonArea.removeFromTop(30);

    // First row: Save and Clear buttons
    auto saveArea = buttonRow1.removeFromLeft(buttonRow1.getWidth() / 2).reduced(5, 0);
    auto clearArea = buttonRow1.reduced(5, 0);

    saveBufferButton.setBounds(saveArea);
    clearBufferButton.setBounds(clearArea);

    // Second row: Connection check button
    checkConnectionButton.setBounds(buttonRow2.reduced(5, 0));
}