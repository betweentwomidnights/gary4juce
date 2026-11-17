// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

/*
  ==============================================================================
    AudioSelectionDialog.cpp
  ==============================================================================
*/

#include "AudioSelectionDialog.h"
#include "../Utils/Theme.h"
#include <cmath>

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
    instructionLabel.setFont(juce::FontOptions(12.0f));
    instructionLabel.setJustificationType(juce::Justification::centred);
    instructionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(instructionLabel);
    updateInstructionText();

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

    // Zoom, to place the window's edges precisely.
    zoomOutButton.setIcon(IconFactory::createZoomOutIcon());
    zoomOutButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    zoomOutButton.setTooltip("zoom out");
    zoomOutButton.onClick = [this]() { zoomFromButton(0.5); };
    zoomOutButton.setEnabled(false);
    addAndMakeVisible(zoomOutButton);

    zoomInButton.setIcon(IconFactory::createZoomInIcon());
    zoomInButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    zoomInButton.setTooltip("zoom in (ctrl + mouse wheel zooms around the mouse)");
    zoomInButton.onClick = [this]() { zoomFromButton(2.0); };
    zoomInButton.setEnabled(false);
    addAndMakeVisible(zoomInButton);

    scrollBar.setAutoHide(false);
    scrollBar.setLookAndFeel(&scrollBarLookAndFeel);   // the same scrollbar as the main UI's
    scrollBar.addListener(this);
    addChildComponent(scrollBar);

    // Confirm button
    confirmButton.setButtonText("Confirm");
    confirmButton.setButtonStyle(CustomButton::ButtonStyle::Gary);  // Use Gary style for primary action
    confirmButton.setTooltip("use selected segment");
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
    scrollBar.removeListener(this);
    scrollBar.setLookAndFeel(nullptr);

    // Stop playback and clean up audio
    transportSource.setSource(nullptr);
    sourcePlayer.setSource(nullptr);
    deviceManager.removeAudioCallback(&sourcePlayer);

    readerSource.reset();
}

void AudioSelectionDialog::setSelectionWindowConstraints(double minDurationSeconds,
                                                         double maxDurationSeconds,
                                                         double preferredDurationSeconds)
{
    const double normalizedMin = juce::jmax(1.0, juce::jmin(minDurationSeconds, maxDurationSeconds));
    const double normalizedMax = juce::jmax(normalizedMin, juce::jmax(minDurationSeconds, maxDurationSeconds));
    const double normalizedPreferred = juce::jlimit(normalizedMin, normalizedMax, preferredDurationSeconds);

    selectionMinDuration = normalizedMin;
    selectionMaxDuration = normalizedMax;
    selectionPreferredDuration = normalizedPreferred;
    updateInstructionText();

    if (totalAudioDuration > 0.0)
        setInitialSelectionStartTime(selectionStartTime);
}

void AudioSelectionDialog::updateInstructionText()
{
    juce::String rangeText;
    if (std::abs(selectionMinDuration - selectionMaxDuration) < 0.001)
        rangeText = juce::String(selectionMaxDuration, 0) + "s";
    else
        rangeText = juce::String(selectionMinDuration, 0) + "-" + juce::String(selectionMaxDuration, 0) + "s";

    const auto instruction = std::abs(selectionMinDuration - selectionMaxDuration) < 0.001
        ? "Drag the selection window to choose your starting point (" + rangeText + "), then click Confirm."
          " Click to listen from any point; zoom in to place it precisely"
        : "Drag the window or its handles to set the start and end (" + rangeText + "), then click Confirm."
          " Click to listen from any point; zoom in to place the edges precisely";
    instructionLabel.setText(instruction, juce::dontSendNotification);
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

    // Show the whole file to begin with.
    setView(0.0, totalAudioDuration);

    // Initialize selection window at start of file with current constraints.
    setInitialSelectionStartTime(0.0);

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

    // Waveform area (main central area), with the zoom scrollbar's row beneath it
    waveformArea = bounds.removeFromTop(bounds.getHeight() - 80).reduced(margin, 0);
    scrollArea = waveformArea.removeFromBottom(14);
    waveformArea.removeFromBottom(4);
    scrollBar.setBounds(scrollArea);
    if (totalAudioDuration > 0.0)
        setView(viewStart, viewDuration);   // the narrowest zoom depends on the width

    bounds.removeFromTop(margin); // spacing

    // Bottom controls - centered
    auto controlArea = bounds.removeFromTop(buttonHeight);

    // play, stop | zoom out, zoom in | confirm, cancel
    int totalControlWidth = playStopButtonWidth + 10 + playStopButtonWidth + 30
                          + playStopButtonWidth + 10 + playStopButtonWidth + 30
                          + buttonWidth + 10 + buttonWidth;
    int startX = (controlArea.getWidth() - totalControlWidth) / 2;

    auto controlRow = controlArea.withX(startX);

    // Playback controls on the left
    playButton.setBounds(controlRow.removeFromLeft(playStopButtonWidth));
    controlRow.removeFromLeft(10); // spacing
    stopButton.setBounds(controlRow.removeFromLeft(playStopButtonWidth));

    controlRow.removeFromLeft(30); // larger spacing

    zoomOutButton.setBounds(controlRow.removeFromLeft(playStopButtonWidth));
    controlRow.removeFromLeft(10);
    zoomInButton.setBounds(controlRow.removeFromLeft(playStopButtonWidth));

    controlRow.removeFromLeft(30);

    // Confirm button
    confirmButton.setBounds(controlRow.removeFromLeft(buttonWidth));
    controlRow.removeFromLeft(10); // spacing

    // Cancel button on the right
    cancelButton.setBounds(controlRow.removeFromLeft(buttonWidth));
}

void AudioSelectionDialog::timerCallback()
{
    if (!isPlaying)
        return;

    // Played off the end of the file: the transport stops itself.
    if (!transportSource.isPlaying())
    {
        stopAudio();
        return;
    }

    currentPlaybackPosition = transportSource.getCurrentPosition();

    // Stop at the end of the selection window (or of the file, after a seek past the window).
    if (currentPlaybackPosition >= playStopTime - 0.1)
    {
        stopAudio();
        return;
    }

    // Zoomed in, the view pages along with the cursor so it never runs off the edge.
    if (isZoomed() && (currentPlaybackPosition < viewStart || currentPlaybackPosition > viewStart + viewDuration))
        setView(currentPlaybackPosition, viewDuration);

    repaint();
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
        return;
    }

    // Resume from a pause or a seek, or start at the selection.
    const double from = isPaused ? pausedPosition : selectionStartTime;
    const double selectionEnd = juce::jmin(selectionStartTime + selectionDuration, totalAudioDuration);
    playStopTime = from < selectionEnd - 0.1 ? selectionEnd : totalAudioDuration;
    transportSource.setPosition(from);
    transportSource.start();
    isPlaying = true;
    isPaused = false;
    currentPlaybackPosition = from;
    updatePlayButtonIcon();
}

void AudioSelectionDialog::seekTo(double timeSeconds)
{
    const double t = juce::jlimit(0.0, totalAudioDuration, timeSeconds);
    currentPlaybackPosition = t;
    transportSource.setPosition(t);
    if (isPlaying)
    {
        const double selectionEnd = juce::jmin(selectionStartTime + selectionDuration, totalAudioDuration);
        playStopTime = t < selectionEnd - 0.1 ? selectionEnd : totalAudioDuration;
    }
    else
    {
        // As on the output waveform: play then starts here.
        isPaused = true;
        pausedPosition = t;
        updatePlayButtonIcon();
    }
    repaint();
}

// --- zoom ----------------------------------------------------------------------------------------

bool AudioSelectionDialog::isZoomed() const
{
    return totalAudioDuration > 0.0 && viewDuration < totalAudioDuration - 1.0e-6;
}

double AudioSelectionDialog::minViewDuration() const
{
    // About one sample per pixel at the deepest zoom.
    const int width = juce::jmax(1, waveformArea.getWidth() - 2);
    return juce::jmin(totalAudioDuration, juce::jmax(0.01, width / juce::jmax(1.0, audioSampleRate)));
}

void AudioSelectionDialog::setView(double start, double duration)
{
    if (totalAudioDuration <= 0.0)
        return;
    viewDuration = juce::jlimit(minViewDuration(), totalAudioDuration, duration);
    viewStart = juce::jlimit(0.0, totalAudioDuration - viewDuration, start);

    scrollBar.setRangeLimits(0.0, totalAudioDuration, juce::dontSendNotification);
    scrollBar.setCurrentRange(viewStart, viewDuration, juce::dontSendNotification);
    scrollBar.setSingleStepSize(viewDuration * 0.1);
    scrollBar.setVisible(isZoomed());
    zoomOutButton.setEnabled(isZoomed());
    zoomInButton.setEnabled(viewDuration > minViewDuration() + 1.0e-9);
    repaint();
}

void AudioSelectionDialog::zoomAround(double factor, double anchorTime, double anchorFraction)
{
    const double newDuration = viewDuration / factor;
    setView(anchorTime - anchorFraction * newDuration, newDuration);
}

void AudioSelectionDialog::zoomFromButton(double factor)
{
    // Centre on the playback cursor once there is one (click near an edge, then zoom in to place
    // it), otherwise on the selection.
    const bool haveCursor = isPlaying || isPaused || currentPlaybackPosition > 0.0;
    const double anchor = haveCursor ? currentPlaybackPosition : selectionStartTime + selectionDuration * 0.5;
    zoomAround(factor, anchor, 0.5);
}

void AudioSelectionDialog::scrollBarMoved(juce::ScrollBar*, double newRangeStart)
{
    setView(newRangeStart, viewDuration);
}

void AudioSelectionDialog::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (!waveformArea.contains(event.getPosition()) || totalAudioDuration <= 0.0)
        return;

    if (event.mods.isCtrlDown() || event.mods.isCommandDown())
    {
        // Zoom around the time under the mouse, keeping it under the mouse.
        if (wheel.deltaY == 0.0f)
            return;
        const double fraction = juce::jlimit(0.0, 1.0,
            (double)(event.x - waveformArea.getX() - 1) / juce::jmax(1, waveformArea.getWidth() - 2));
        zoomAround(wheel.deltaY > 0.0f ? 2.0 : 0.5, mouseXToTime(event.x), fraction);
        return;
    }

    if (isZoomed())
    {
        const float delta = std::abs(wheel.deltaX) > std::abs(wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
        setView(viewStart - delta * viewDuration * 0.5, viewDuration);
    }
}

double AudioSelectionDialog::timeToX(double timeSeconds) const
{
    const int waveWidth = waveformArea.getWidth() - 2;
    if (viewDuration <= 0.0)
        return (double)waveformArea.getX() + 1.0;
    return waveformArea.getX() + 1 + (timeSeconds - viewStart) / viewDuration * waveWidth;
}

juce::String AudioSelectionDialog::formatTime(double timeSeconds) const
{
    const int minutes = (int)(timeSeconds / 60.0);
    const double seconds = timeSeconds - minutes * 60.0;
    if (!isZoomed())
        return juce::String(minutes) + ":" + juce::String((int)seconds).paddedLeft('0', 2);
    // Zoomed in, edges are placed to the hundredth of a second, so show it.
    auto text = juce::String(seconds, 2);
    if (seconds < 10.0)
        text = "0" + text;
    return juce::String(minutes) + ":" + text;
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

    // The visible span of the file: all of it, or less when zoomed in.
    const double firstVisibleSample = viewStart * audioSampleRate;
    const double visibleSamples = juce::jmax(1.0, viewDuration * audioSampleRate);
    const int totalSamples = audioBuffer.getNumSamples();

    // Draw waveform in red (following existing pattern)
    g.setColour(juce::Colours::red);

    for (int x = 0; x < waveWidth; ++x)
    {
        const int startSample = juce::jlimit(0, totalSamples,
            (int)(firstVisibleSample + visibleSamples * x / waveWidth));
        const int endSample = juce::jlimit(0, totalSamples,
            juce::jmax(startSample + 1, (int)(firstVisibleSample + visibleSamples * (x + 1) / waveWidth)));

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

    // Draw playback cursor, when it is in view
    const bool cursorInView = currentPlaybackPosition >= viewStart
                           && currentPlaybackPosition <= viewStart + viewDuration;
    if ((isPlaying || isPaused || currentPlaybackPosition > 0.0) && totalAudioDuration > 0.0 && cursorInView)
    {
        int cursorX = (int)timeToX(currentPlaybackPosition);

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
        const juce::String timeString = formatTime(currentPlaybackPosition);

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(juce::Colours::white);

        // Draw timestamp in top-left corner of waveform
        juce::Rectangle<int> timeRect(area.getX() + 5, area.getY() + 5, isZoomed() ? 76 : 60, 20);
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

    // The selection in pixels, before clipping: zoomed in, either edge (or both) can be off screen.
    const double endTime = juce::jmin(selectionStartTime + selectionDuration, totalAudioDuration);
    const int startX = (int)timeToX(selectionStartTime);
    const int endX = (int)timeToX(endTime);
    const juce::Rectangle<int> fullRect(startX, area.getY() + 1, endX - startX, area.getHeight() - 2);
    const auto visibleRect = fullRect.getIntersection(area);
    const bool leftEdgeVisible = startX >= area.getX() && startX <= area.getRight();
    const bool rightEdgeVisible = endX >= area.getX() && endX <= area.getRight();

    juce::Graphics::ScopedSaveState clip(g);
    g.reduceClipRegion(area);

    // Draw semi-transparent overlay for unselected regions
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    if (startX > area.getX())
        g.fillRect(area.getX(), area.getY(), juce::jmin(startX, area.getRight()) - area.getX(), area.getHeight());
    if (endX < area.getRight())
    {
        const int from = juce::jmax(endX, area.getX());
        g.fillRect(from, area.getY(), area.getRight() - from, area.getHeight());
    }

    if (visibleRect.isEmpty())
        return;

    // White border around the selection; the clip hides edges that are off screen.
    g.setColour(juce::Colours::white);
    g.drawRect(fullRect.toFloat(), 2.0f);

    if (canResizeSelection())
    {
        // Edge handles for resize affordance.
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        const int handleHeight = juce::jmin(26, fullRect.getHeight() - 8);
        const int handleY = fullRect.getCentreY() - handleHeight / 2;
        if (leftEdgeVisible)
            g.fillRect(fullRect.getX() - 1, handleY, 3, handleHeight);
        if (rightEdgeVisible)
            g.fillRect(fullRect.getRight() - 2, handleY, 3, handleHeight);
    }

    // Include duration in the label to show the window size
    const juce::String timeLabel = formatTime(selectionStartTime) + " - " + formatTime(endTime)
                                 + " (" + juce::String(selectionDuration, isZoomed() ? 2 : 1) + "s)";

    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);

    // Draw label at top of the visible part of the selection window
    juce::Rectangle<int> labelRect(visibleRect.getX(), visibleRect.getY() + 5, visibleRect.getWidth(), 20);
    g.drawText(timeLabel, labelRect, juce::Justification::centred);
}

// The part of the selection that is in view (empty when it is scrolled out of view).
juce::Rectangle<int> AudioSelectionDialog::getSelectionRectangle() const
{
    if (waveformArea.isEmpty() || totalAudioDuration <= 0.0)
        return juce::Rectangle<int>();

    const int startX = (int)timeToX(selectionStartTime);
    const int endX = (int)timeToX(juce::jmin(selectionStartTime + selectionDuration, totalAudioDuration));
    return juce::Rectangle<int>(startX, waveformArea.getY() + 1, endX - startX, waveformArea.getHeight() - 2)
        .getIntersection(waveformArea);
}

std::pair<double, double> AudioSelectionDialog::getEffectiveDurationRange() const
{
    if (totalAudioDuration <= 0.0)
        return { 0.0, 0.0 };

    const double effectiveMin = juce::jmin(selectionMinDuration, totalAudioDuration);
    const double effectiveMax = juce::jmin(selectionMaxDuration, totalAudioDuration);
    return { effectiveMin, effectiveMax };
}

bool AudioSelectionDialog::canResizeSelection() const
{
    const auto [effectiveMin, effectiveMax] = getEffectiveDurationRange();
    return (effectiveMax - effectiveMin) > 0.001;
}

double AudioSelectionDialog::mouseXToTime(int mouseX) const
{
    if (waveformArea.isEmpty() || totalAudioDuration <= 0.0)
        return 0.0;

    const int waveWidth = waveformArea.getWidth() - 2;
    if (waveWidth <= 0)
        return 0.0;

    const int clampedX = juce::jlimit(waveformArea.getX() + 1, waveformArea.getRight() - 1, mouseX);
    const double percent = (double)(clampedX - (waveformArea.getX() + 1)) / (double)waveWidth;
    return juce::jlimit(0.0, totalAudioDuration, viewStart + percent * viewDuration);
}

// A handle can be grabbed only while its edge is on screen.
bool AudioSelectionDialog::isMouseNearLeftHandle(int mouseX) const
{
    if (!canResizeSelection() || totalAudioDuration <= 0.0)
        return false;

    const double edgeX = timeToX(selectionStartTime);
    if (edgeX < waveformArea.getX() || edgeX > waveformArea.getRight())
        return false;

    constexpr int handleHitRadius = 8;
    return std::abs(mouseX - edgeX) <= handleHitRadius;
}

bool AudioSelectionDialog::isMouseNearRightHandle(int mouseX) const
{
    if (!canResizeSelection() || totalAudioDuration <= 0.0)
        return false;

    const double edgeX = timeToX(juce::jmin(selectionStartTime + selectionDuration, totalAudioDuration));
    if (edgeX < waveformArea.getX() || edgeX > waveformArea.getRight())
        return false;

    constexpr int handleHitRadius = 8;
    return std::abs(mouseX - edgeX) <= handleHitRadius;
}

bool AudioSelectionDialog::isMouseOverSelection(const juce::Point<int>& pos) const
{
    auto selectionRect = getSelectionRectangle();
    return selectionRect.contains(pos);
}

void AudioSelectionDialog::mouseMove(const juce::MouseEvent& event)
{
    const auto hitArea = waveformArea.expanded(2, 0);
    if (!hitArea.contains(event.getPosition()))
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

    if (isMouseNearLeftHandle(event.getPosition().x) || isMouseNearRightHandle(event.getPosition().x))
    {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        return;
    }

    if (isMouseOverSelection(event.getPosition()))
    {
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        return;
    }

    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void AudioSelectionDialog::mouseDown(const juce::MouseEvent& event)
{
    pendingMove = false;
    mouseMovedSinceDown = false;
    mouseDownX = event.getPosition().x;

    const auto hitArea = waveformArea.expanded(2, 0);
    if (!hitArea.contains(event.getPosition()))
        return;

    // A handle resizes at once. Inside the window, a press only becomes a move once the mouse
    // travels a few pixels; released in place, it is a click, which seeks.
    if (isMouseNearLeftHandle(event.getPosition().x))
        selectionDragMode = SelectionDragMode::ResizeLeft;
    else if (isMouseNearRightHandle(event.getPosition().x))
        selectionDragMode = SelectionDragMode::ResizeRight;
    else
    {
        selectionDragMode = SelectionDragMode::None;
        pendingMove = isMouseOverSelection(event.getPosition());
        return;
    }

    isDraggingSelection = true;
    dragStartX = event.getPosition().x;
    dragStartSelectionTime = selectionStartTime;
    dragStartSelectionDuration = selectionDuration;

    // Stop playback when starting to drag (Option A - safest UX)
    if (isPlaying)
        stopAudio();

    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
}

void AudioSelectionDialog::mouseDrag(const juce::MouseEvent& event)
{
    if (std::abs(event.getPosition().x - mouseDownX) > 3)
        mouseMovedSinceDown = true;

    if (pendingMove && mouseMovedSinceDown)
    {
        pendingMove = false;
        selectionDragMode = SelectionDragMode::Move;
        isDraggingSelection = true;
        dragStartX = mouseDownX;
        dragStartSelectionTime = selectionStartTime;
        dragStartSelectionDuration = selectionDuration;
        if (isPlaying)
            stopAudio();
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }

    if (!isDraggingSelection)
        return;

    if (selectionDragMode == SelectionDragMode::ResizeLeft ||
        selectionDragMode == SelectionDragMode::ResizeRight)
        updateSelectionFromResizeDrag(event.getPosition().x);
    else
        updateSelectionFromMouseDrag(event.getPosition().x);

    repaint();
}

void AudioSelectionDialog::mouseUp(const juce::MouseEvent& event)
{
    if (isDraggingSelection)
    {
        isDraggingSelection = false;
        selectionDragMode = SelectionDragMode::None;
        mouseMove(event);
        return;
    }

    // A click on the waveform (not a drag): seek there, as on the output waveform.
    const bool wasClick = !mouseMovedSinceDown && waveformArea.contains(event.getPosition());
    pendingMove = false;
    if (wasClick && totalAudioDuration > 0.0)
        seekTo(mouseXToTime(event.getPosition().x));
}

void AudioSelectionDialog::updateSelectionFromMouseDrag(int mouseX)
{
    if (waveformArea.isEmpty() || totalAudioDuration <= 0.0)
        return;

    const int waveWidth = waveformArea.getWidth() - 2;
    int deltaX = mouseX - dragStartX;

    // Convert pixel delta to time delta, at the current zoom
    double deltaTime = (deltaX / (double)waveWidth) * viewDuration;
    double newSelectionStart = dragStartSelectionTime + deltaTime;

    const auto [effectiveMinDuration, effectiveMaxDuration] = getEffectiveDurationRange();
    const double preferredDuration = juce::jlimit(effectiveMinDuration, effectiveMaxDuration, selectionPreferredDuration);

    if (effectiveMaxDuration <= 0.0)
        return;

    if (std::abs(effectiveMaxDuration - effectiveMinDuration) < 0.001)
    {
        // Fixed-length selection window (for example, 30s or 180s exactly).
        selectionDuration = effectiveMaxDuration;
        const double maxStartTime = juce::jmax(0.0, totalAudioDuration - selectionDuration);
        selectionStartTime = juce::jlimit(0.0, maxStartTime, newSelectionStart);
        return;
    }

    if (userResizedSelection)
    {
        // Preserve manually resized duration when moving the window.
        selectionDuration = juce::jlimit(effectiveMinDuration, effectiveMaxDuration, dragStartSelectionDuration);
        const double maxStartTime = juce::jmax(0.0, totalAudioDuration - selectionDuration);
        selectionStartTime = juce::jlimit(0.0, maxStartTime, newSelectionStart);
        return;
    }

    const double availableDuration = totalAudioDuration - newSelectionStart;
    if (availableDuration >= preferredDuration)
    {
        selectionDuration = preferredDuration;
        selectionStartTime = juce::jmax(0.0, newSelectionStart);
    }
    else if (availableDuration >= effectiveMinDuration)
    {
        selectionDuration = juce::jmin(effectiveMaxDuration, availableDuration);
        selectionStartTime = juce::jmax(0.0, newSelectionStart);
    }
    else
    {
        selectionDuration = effectiveMinDuration;
        const double maxStartTime = juce::jmax(0.0, totalAudioDuration - selectionDuration);
        selectionStartTime = juce::jlimit(0.0, maxStartTime, newSelectionStart);
    }
}

void AudioSelectionDialog::updateSelectionFromResizeDrag(int mouseX)
{
    if (waveformArea.isEmpty() || totalAudioDuration <= 0.0)
        return;

    const auto [effectiveMinDuration, effectiveMaxDuration] = getEffectiveDurationRange();
    if ((effectiveMaxDuration - effectiveMinDuration) < 0.001)
        return;

    const double draggedTime = mouseXToTime(mouseX);

    if (selectionDragMode == SelectionDragMode::ResizeRight)
    {
        const double fixedLeft = dragStartSelectionTime;
        const double minRight = juce::jmin(totalAudioDuration, fixedLeft + effectiveMinDuration);
        const double maxRight = juce::jmin(totalAudioDuration, fixedLeft + effectiveMaxDuration);
        const double newRight = juce::jlimit(minRight, maxRight, draggedTime);

        selectionStartTime = fixedLeft;
        selectionDuration = juce::jmax(effectiveMinDuration, newRight - fixedLeft);
        userResizedSelection = true;
    }
    else if (selectionDragMode == SelectionDragMode::ResizeLeft)
    {
        const double fixedRight = juce::jlimit(0.0, totalAudioDuration,
            dragStartSelectionTime + dragStartSelectionDuration);
        const double minLeft = juce::jmax(0.0, fixedRight - effectiveMaxDuration);
        const double maxLeft = juce::jmax(0.0, fixedRight - effectiveMinDuration);
        const double newLeft = juce::jlimit(minLeft, maxLeft, draggedTime);

        selectionStartTime = newLeft;
        selectionDuration = juce::jmax(effectiveMinDuration, fixedRight - newLeft);
        userResizedSelection = true;
    }
}

void AudioSelectionDialog::confirmSelection()
{
    if (audioBuffer.getNumSamples() == 0 || totalAudioDuration <= 0.0)
        return;

    // Calculate sample range for the selected segment.
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
        juce::String(numSamples) + " samples at " + juce::String(audioSampleRate) + " Hz)");

    // Stop playback before closing
    if (isPlaying)
        stopAudio();

    // Call the confirm callback with the extracted segment, sample rate, and selection start time
    if (onConfirm)
        onConfirm(selectedSegment, audioSampleRate, selectionStartTime);
}

void AudioSelectionDialog::setInitialSelectionStartTime(double startTime)
{
    // Set the selection start time, ensuring it's within valid bounds
    if (totalAudioDuration > 0.0)
    {
        const auto [effectiveMinDuration, effectiveMaxDuration] = getEffectiveDurationRange();
        const double preferredDuration = juce::jlimit(effectiveMinDuration, effectiveMaxDuration, selectionPreferredDuration);

        if (effectiveMaxDuration <= 0.0)
            return;

        if (std::abs(effectiveMaxDuration - effectiveMinDuration) < 0.001)
        {
            selectionDuration = effectiveMaxDuration;
            const double maxAllowedStart = juce::jmax(0.0, totalAudioDuration - selectionDuration);
            selectionStartTime = juce::jlimit(0.0, maxAllowedStart, startTime);
        }
        else
        {
            const double maxAllowedStart = juce::jmax(0.0, totalAudioDuration - effectiveMinDuration);
            selectionStartTime = juce::jlimit(0.0, maxAllowedStart, startTime);

            const double availableDuration = totalAudioDuration - selectionStartTime;
            if (availableDuration >= preferredDuration)
                selectionDuration = preferredDuration;
            else if (availableDuration >= effectiveMinDuration)
                selectionDuration = juce::jmin(effectiveMaxDuration, availableDuration);
            else
                selectionDuration = effectiveMinDuration;
        }

        DBG("Set initial selection start time: " + juce::String(selectionStartTime, 2) + "s");
        userResizedSelection = false;
        repaint();
    }
}
