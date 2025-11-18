/*
  ==============================================================================
    AudioSelectionDialog.cpp
  ==============================================================================
*/

#include "AudioSelectionDialog.h"
#include "../Utils/Theme.h"

AudioSelectionDialog::AudioSelectionDialog()
{
    // Register audio formats
    formatManager.registerBasicFormats();

    // Initialize audio device for playback
    juce::String audioError = deviceManager.initialiseWithDefaultDevices(0, 2); // 0 inputs, 2 outputs
    if (audioError.isNotEmpty())
    {
        DBG("Audio device error: " + audioError);
    }

    // Set up the audio source player
    deviceManager.addAudioCallback(&sourcePlayer);
    sourcePlayer.setSource(&transportSource);

    // Title label
    titleLabel.setText("Select Audio Segment", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    // Duration label
    durationLabel.setFont(juce::FontOptions(14.0f));
    durationLabel.setJustificationType(juce::Justification::centred);
    durationLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(durationLabel);

    // Instruction label
    instructionLabel.setText("Drag the white selection window to choose 30 seconds, then click Confirm",
                            juce::dontSendNotification);
    instructionLabel.setFont(juce::FontOptions(12.0f));
    instructionLabel.setJustificationType(juce::Justification::centred);
    instructionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(instructionLabel);

    // Load icons
    playIcon = IconFactory::createPlayIcon();
    pauseIcon = IconFactory::createPauseIcon();
    stopIcon = IconFactory::createStopIcon();

    // Play button
    playButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    updatePlayButtonIcon();
    playButton.setTooltip("play/pause audio");
    playButton.onClick = [this]() { playAudio(); };
    playButton.setEnabled(false); // Initially disabled until audio is loaded
    addAndMakeVisible(playButton);

    // Stop button
    if (stopIcon)
        stopButton.setIcon(stopIcon->createCopy());
    stopButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    stopButton.setTooltip("stop playback");
    stopButton.onClick = [this]() { stopAudio(); };
    stopButton.setEnabled(false); // Initially disabled
    addAndMakeVisible(stopButton);

    // Confirm button
    confirmButton.setButtonText("Confirm");
    confirmButton.setButtonStyle(CustomButton::ButtonStyle::Gary);  // Use Gary style for primary action
    confirmButton.setTooltip("use selected 30 seconds");
    confirmButton.onClick = [this]() { confirmSelection(); };
    confirmButton.setEnabled(false); // Initially disabled until audio is loaded
    addAndMakeVisible(confirmButton);

    // Cancel button
    cancelButton.setButtonText("Cancel");
    cancelButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    cancelButton.setTooltip("close without selecting");
    cancelButton.onClick = [this]()
    {
        // Stop playback before closing
        if (isPlaying)
            stopAudio();

        if (onCancel)
            onCancel();
    };
    addAndMakeVisible(cancelButton);

    // Start timer for playback cursor updates (50ms = 20 FPS)
    startTimer(50);

    // Set reasonable initial size
    setSize(800, 500);
}

AudioSelectionDialog::~AudioSelectionDialog()
{
    stopTimer();

    // Stop playback and clean up audio
    transportSource.setSource(nullptr);
    sourcePlayer.setSource(nullptr);
    deviceManager.removeAudioCallback(&sourcePlayer);

    readerSource.reset();
}

bool AudioSelectionDialog::loadAudioFile(const juce::File& audioFile)
{
    if (!audioFile.existsAsFile())
    {
        DBG("AudioSelectionDialog: File does not exist");
        return false;
    }

    // Create reader for the audio file
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));

    if (!reader)
    {
        DBG("AudioSelectionDialog: Could not create reader for file");
        return false;
    }

    // Store audio properties
    audioSampleRate = reader->sampleRate;
    totalAudioDuration = (double)reader->lengthInSamples / audioSampleRate;

    DBG("AudioSelectionDialog: Loaded " + juce::String(totalAudioDuration, 2) + "s audio at " +
        juce::String(audioSampleRate) + "Hz");

    // Load entire file into buffer for waveform display
    audioBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&audioBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

    // Set up transport source for playback
    // Need to re-create the reader for the transport source (reader can only be used once)
    reader.reset(formatManager.createReaderFor(audioFile));
    if (reader)
    {
        readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
        transportSource.setSource(readerSource.get(), 0, nullptr, audioSampleRate);
    }

    // Update duration label
    int minutes = (int)(totalAudioDuration / 60.0);
    int seconds = (int)totalAudioDuration % 60;
    durationLabel.setText("Duration: " + juce::String(minutes) + "m " + juce::String(seconds) + "s",
                         juce::dontSendNotification);

    // Enable playback controls
    playButton.setEnabled(true);
    stopButton.setEnabled(true);
    confirmButton.setEnabled(true);

    // Initialize selection window to first 30 seconds
    selectionStartTime = 0.0;

    repaint();
    return true;
}

void AudioSelectionDialog::paint(juce::Graphics& g)
{
    // Dark background
    g.fillAll(juce::Colour(0x1e, 0x1e, 0x1e));

    // Draw waveform
    if (!waveformArea.isEmpty())
    {
        drawWaveform(g, waveformArea);

        // Draw selection window overlay
        if (audioBuffer.getNumSamples() > 0)
        {
            drawSelectionWindow(g, waveformArea);
        }
    }
}

void AudioSelectionDialog::resized()
{
    auto bounds = getLocalBounds();
    const int margin = 20;
    const int buttonHeight = 40;
    const int buttonWidth = 120;
    const int playStopButtonWidth = 50;

    // Title at top
    titleLabel.setBounds(bounds.removeFromTop(50).reduced(margin, 10));

    // Duration label
    durationLabel.setBounds(bounds.removeFromTop(30).reduced(margin, 0));

    // Instruction label
    instructionLabel.setBounds(bounds.removeFromTop(30).reduced(margin, 0));

    bounds.removeFromTop(margin); // spacing

    // Waveform area (main central area)
    waveformArea = bounds.removeFromTop(bounds.getHeight() - 80).reduced(margin, 0);

    bounds.removeFromTop(margin); // spacing

    // Bottom controls - centered
    auto controlArea = bounds.removeFromTop(buttonHeight);

    // Calculate total width of controls: play + spacing + stop + spacing + confirm + spacing + cancel
    int totalControlWidth = playStopButtonWidth + 10 + playStopButtonWidth + 30 + buttonWidth + 10 + buttonWidth;
    int startX = (controlArea.getWidth() - totalControlWidth) / 2;

    auto controlRow = controlArea.withX(startX);

    // Playback controls on the left
    playButton.setBounds(controlRow.removeFromLeft(playStopButtonWidth));
    controlRow.removeFromLeft(10); // spacing
    stopButton.setBounds(controlRow.removeFromLeft(playStopButtonWidth));

    controlRow.removeFromLeft(30); // larger spacing

    // Confirm button
    confirmButton.setBounds(controlRow.removeFromLeft(buttonWidth));
    controlRow.removeFromLeft(10); // spacing

    // Cancel button on the right
    cancelButton.setBounds(controlRow.removeFromLeft(buttonWidth));
}

void AudioSelectionDialog::timerCallback()
{
    // Update playback position
    if (isPlaying && transportSource.isPlaying())
    {
        currentPlaybackPosition = transportSource.getCurrentPosition();

        // Stop playback at the end of the selection window
        double selectionEndTime = juce::jmin(selectionStartTime + selectionDuration, totalAudioDuration);
        if (currentPlaybackPosition >= selectionEndTime - 0.1)
        {
            stopAudio();
        }

        repaint();
    }
}

void AudioSelectionDialog::playAudio()
{
    if (audioBuffer.getNumSamples() == 0)
        return;

    if (isPlaying)
    {
        // Pause
        transportSource.stop();
        isPlaying = false;
        isPaused = true;
        pausedPosition = currentPlaybackPosition;
        updatePlayButtonIcon();
    }
    else if (isPaused)
    {
        // Resume from paused position
        transportSource.setPosition(pausedPosition);
        transportSource.start();
        isPlaying = true;
        isPaused = false;
        updatePlayButtonIcon();
    }
    else
    {
        // Start from beginning of selection window
        transportSource.setPosition(selectionStartTime);
        transportSource.start();
        isPlaying = true;
        isPaused = false;
        currentPlaybackPosition = selectionStartTime;
        updatePlayButtonIcon();
    }
}

void AudioSelectionDialog::stopAudio()
{
    transportSource.stop();
    transportSource.setPosition(selectionStartTime);
    isPlaying = false;
    isPaused = false;
    currentPlaybackPosition = selectionStartTime;
    pausedPosition = selectionStartTime;
    updatePlayButtonIcon();
    repaint();
}

void AudioSelectionDialog::updatePlayButtonIcon()
{
    if (isPlaying)
    {
        if (pauseIcon)
            playButton.setIcon(pauseIcon->createCopy());
    }
    else
    {
        if (playIcon)
            playButton.setIcon(playIcon->createCopy());
    }
}

void AudioSelectionDialog::drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    // Black background
    g.setColour(juce::Colours::black);
    g.fillRect(area);

    // Draw border
    g.setColour(juce::Colour(0x40, 0x40, 0x40));
    g.drawRect(area, 1);

    if (audioBuffer.getNumSamples() == 0)
    {
        // No audio loaded
        g.setFont(juce::FontOptions(14.0f));
        g.setColour(juce::Colours::darkgrey);
        g.drawText("No audio loaded", area, juce::Justification::centred);
        return;
    }

    // Draw waveform (following existing pattern from PluginEditor)
    const int waveWidth = area.getWidth() - 2;
    const int waveHeight = area.getHeight() - 2;
    const int centerY = area.getCentreY();

    if (waveWidth <= 0)
        return;

    // Calculate samples per pixel
    const int samplesPerPixel = juce::jmax(1, audioBuffer.getNumSamples() / waveWidth);

    // Draw waveform in red (following existing pattern)
    g.setColour(juce::Colours::red);

    for (int x = 0; x < waveWidth; ++x)
    {
        const int startSample = x * samplesPerPixel;
        const int endSample = juce::jmin(startSample + samplesPerPixel, audioBuffer.getNumSamples());

        if (endSample > startSample)
        {
            // Find min/max in this pixel's worth of samples
            float minVal = 0.0f, maxVal = 0.0f;

            for (int sample = startSample; sample < endSample; ++sample)
            {
                // Average across channels
                float sampleValue = 0.0f;
                for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
                {
                    sampleValue += audioBuffer.getSample(ch, sample);
                }
                sampleValue /= audioBuffer.getNumChannels();

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

    // Draw playback cursor
    if ((isPlaying || isPaused || currentPlaybackPosition > 0.0) && totalAudioDuration > 0.0)
    {
        // Calculate cursor position as a percentage of total duration
        double progressPercent = currentPlaybackPosition / totalAudioDuration;
        progressPercent = juce::jlimit(0.0, 1.0, progressPercent);

        // Convert to pixel position
        int cursorX = area.getX() + 1 + (int)(progressPercent * waveWidth);

        // Different cursor appearance for different states
        if (isPlaying)
        {
            // Playing cursor - bright white
            g.setColour(juce::Colours::white.withAlpha(0.9f));
        }
        else if (isPaused)
        {
            // Paused cursor - slightly dimmer
            g.setColour(juce::Colours::white.withAlpha(0.7f));
        }
        else
        {
            // Seek position cursor - dimmer still
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

    // Draw timestamp at cursor position (if playing or paused)
    if ((isPlaying || isPaused) && totalAudioDuration > 0.0)
    {
        int minutes = (int)(currentPlaybackPosition / 60.0);
        int seconds = (int)currentPlaybackPosition % 60;
        juce::String timeString = juce::String(minutes) + ":" +
                                  juce::String(seconds).paddedLeft('0', 2);

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(juce::Colours::white);

        // Draw timestamp in top-left corner of waveform
        juce::Rectangle<int> timeRect(area.getX() + 5, area.getY() + 5, 60, 20);
        g.fillRect(timeRect.toFloat());
        g.setColour(juce::Colours::black);
        g.drawText(timeString, timeRect, juce::Justification::centred);
    }
}

// ============================================================================
// Stage 2B: Selection Window Methods
// ============================================================================

void AudioSelectionDialog::drawSelectionWindow(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    if (totalAudioDuration <= 0.0)
        return;

    auto selectionRect = getSelectionRectangle();

    if (selectionRect.isEmpty())
        return;

    // Draw semi-transparent overlay for unselected regions
    g.setColour(juce::Colours::black.withAlpha(0.5f));

    // Left unselected region
    if (selectionRect.getX() > area.getX())
    {
        juce::Rectangle<int> leftRegion(area.getX(), area.getY(),
                                        selectionRect.getX() - area.getX(), area.getHeight());
        g.fillRect(leftRegion);
    }

    // Right unselected region
    if (selectionRect.getRight() < area.getRight())
    {
        juce::Rectangle<int> rightRegion(selectionRect.getRight(), area.getY(),
                                         area.getRight() - selectionRect.getRight(), area.getHeight());
        g.fillRect(rightRegion);
    }

    // Draw white border around selection
    g.setColour(juce::Colours::white);
    g.drawRect(selectionRect.toFloat(), 2.0f);

    // Draw selection time label
    int startMin = (int)(selectionStartTime / 60.0);
    int startSec = (int)selectionStartTime % 60;
    double endTime = juce::jmin(selectionStartTime + selectionDuration, totalAudioDuration);
    int endMin = (int)(endTime / 60.0);
    int endSec = (int)endTime % 60;

    juce::String timeLabel = juce::String(startMin) + ":" + juce::String(startSec).paddedLeft('0', 2) +
                             " - " + juce::String(endMin) + ":" + juce::String(endSec).paddedLeft('0', 2);

    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);

    // Draw label at top of selection window
    juce::Rectangle<int> labelRect(selectionRect.getX(), selectionRect.getY() + 5,
                                   selectionRect.getWidth(), 20);
    g.drawText(timeLabel, labelRect, juce::Justification::centred);
}

juce::Rectangle<int> AudioSelectionDialog::getSelectionRectangle() const
{
    if (waveformArea.isEmpty() || totalAudioDuration <= 0.0)
        return juce::Rectangle<int>();

    const int waveWidth = waveformArea.getWidth() - 2;

    // Calculate pixel position of selection window
    double selectionStartPercent = selectionStartTime / totalAudioDuration;
    double selectionEndPercent = juce::jmin(selectionStartTime + selectionDuration, totalAudioDuration) / totalAudioDuration;

    int startX = waveformArea.getX() + 1 + (int)(selectionStartPercent * waveWidth);
    int endX = waveformArea.getX() + 1 + (int)(selectionEndPercent * waveWidth);

    return juce::Rectangle<int>(startX, waveformArea.getY() + 1,
                                endX - startX, waveformArea.getHeight() - 2);
}

bool AudioSelectionDialog::isMouseOverSelection(const juce::Point<int>& pos) const
{
    auto selectionRect = getSelectionRectangle();
    return selectionRect.contains(pos);
}

void AudioSelectionDialog::mouseDown(const juce::MouseEvent& event)
{
    if (!waveformArea.contains(event.getPosition()))
        return;

    if (isMouseOverSelection(event.getPosition()))
    {
        isDraggingSelection = true;
        dragStartX = event.getPosition().x;
        dragStartSelectionTime = selectionStartTime;

        // Stop playback when starting to drag (Option A - safest UX)
        if (isPlaying)
        {
            stopAudio();
        }

        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
}

void AudioSelectionDialog::mouseDrag(const juce::MouseEvent& event)
{
    if (!isDraggingSelection)
        return;

    updateSelectionFromMouseDrag(event.getPosition().x);
    repaint();
}

void AudioSelectionDialog::mouseUp(const juce::MouseEvent& event)
{
    if (isDraggingSelection)
    {
        isDraggingSelection = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

void AudioSelectionDialog::updateSelectionFromMouseDrag(int mouseX)
{
    if (waveformArea.isEmpty() || totalAudioDuration <= 0.0)
        return;

    const int waveWidth = waveformArea.getWidth() - 2;
    int deltaX = mouseX - dragStartX;

    // Convert pixel delta to time delta
    double deltaTime = (deltaX / (double)waveWidth) * totalAudioDuration;
    double newSelectionStart = dragStartSelectionTime + deltaTime;

    // Clamp to valid range (selection window must stay fully within audio)
    double maxStartTime = totalAudioDuration - selectionDuration;
    selectionStartTime = juce::jlimit(0.0, maxStartTime, newSelectionStart);
}

void AudioSelectionDialog::confirmSelection()
{
    if (audioBuffer.getNumSamples() == 0 || totalAudioDuration <= 0.0)
        return;

    // Calculate sample range for the selected 30 seconds
    int startSample = (int)(selectionStartTime * audioSampleRate);
    int numSamples = (int)(selectionDuration * audioSampleRate);

    // Clamp to buffer bounds
    startSample = juce::jlimit(0, audioBuffer.getNumSamples() - 1, startSample);
    numSamples = juce::jmin(numSamples, audioBuffer.getNumSamples() - startSample);

    // Extract the selected segment into a new buffer
    juce::AudioBuffer<float> selectedSegment(audioBuffer.getNumChannels(), numSamples);

    for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
    {
        selectedSegment.copyFrom(ch, 0, audioBuffer, ch, startSample, numSamples);
    }

    DBG("Extracted selection: " + juce::String(selectionStartTime, 1) + "s to " +
        juce::String(selectionStartTime + selectionDuration, 1) + "s (" +
        juce::String(numSamples) + " samples)");

    // Stop playback before closing
    if (isPlaying)
        stopAudio();

    // Call the confirm callback with the extracted segment
    if (onConfirm)
        onConfirm(selectedSegment);
}
