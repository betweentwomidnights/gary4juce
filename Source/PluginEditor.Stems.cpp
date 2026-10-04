// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Stems/StemsRuntime.h"
#include "Components/Stems/StemsSettings.h"
#include "Components/Stems/StemsDialog.h"

stems::StemsService& Gary4juceAudioProcessorEditor::getStemsService()
{
    if (stemsService == nullptr)
        stemsService = std::make_shared<stems::StemsService>(activeGaryDataDirectory);
    return *stemsService;
}

void Gary4juceAudioProcessorEditor::showStemsSettings()
{
    if (!ensureGaryDataDirectoryAvailable(true))
        return;

    getStemsService();
    auto* panel = new StemsSettings(stemsService, getUpdatePreferences());

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(panel);
    options.dialogTitle = "stem separator";
    options.dialogBackgroundColour = juce::Colour(0x1e, 0x1e, 0x1e);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    trackEditorModalWindow(options.launchAsync());
}

// --- the "stems" handle --------------------------------------------------------------------------

void Gary4juceAudioProcessorEditor::updateStemsButtonState()
{
    bool ready = hasOutputAudio && outputAudioFile.existsAsFile();
    if (ready)
    {
        auto& service = getStemsService();
        ready = service.isRuntimeInstalled();
        if (ready)
        {
            ready = false;
            for (const auto& model : stems::modelCatalog())
                if (service.isModelInstalled(model.id)) { ready = true; break; }
        }
    }

    if (stemsButton.isVisible() != ready || stemsButton.isEnabled() == isGenerating)
    {
        stemsButton.setVisible(ready);
        stemsButton.setEnabled(!isGenerating);
        repaint(stemsButton.getBounds());
    }
}

void Gary4juceAudioProcessorEditor::showStemsDialog()
{
    if (isGenerating)
    {
        showStatusMessage("wait for the current generation to finish", 2500);
        return;
    }
    if (!hasOutputAudio || !outputAudioFile.existsAsFile())
    {
        showStatusMessage("no output audio to separate", 2500);
        return;
    }
    if (!ensureGaryDataDirectoryAvailable(true))
        return;

    StemsDialog::Host host;
    host.togglePlayback = [this](const juce::File& stem) { toggleStemPlayback(stem); };
    host.stopPlayback = [this] { stopStemPlayback(); };
    host.seek = [this](const juce::File& stem, double seconds) { seekStem(stem, seconds); };
    host.activeStem = [this]
    {
        return activePlaybackSource == PlaybackSource::Stem ? activeStemFile : juce::File();
    };
    host.isPlaying = [this]
    {
        return activePlaybackSource == PlaybackSource::Stem && audioProcessor.getIsPlayingOutput();
    };
    host.position = [this]
    {
        if (activePlaybackSource != PlaybackSource::Stem)
            return 0.0;
        return audioProcessor.getIsPlayingOutput() ? audioProcessor.getOutputPlaybackPosition()
                                                   : stemPausedPosition;
    };
    host.drag = [this](const juce::File& stem, const juce::String& name) { startStemDrag(stem, name); };
    host.isGenerating = [this] { return isGenerating; };
    host.releasePlayback = [this] { releaseStemPlayback(); };

    getStemsService();
    auto* dialog = new StemsDialog(stemsService, getUpdatePreferences(), std::move(host), outputAudioFile);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = "stems";
    options.dialogBackgroundColour = juce::Colour(0x1e, 0x1e, 0x1e);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    trackEditorModalWindow(options.launchAsync());
}

// --- stem playback, through the same player as the output ----------------------------------------

namespace
{
    // How far a finished or stopped stem is from the start, so play resumes where it should.
    constexpr double kNoPosition = 0.0;
}

void Gary4juceAudioProcessorEditor::loadStemIntoPlayer(const juce::File& stem)
{
    if (activePlaybackSource != PlaybackSource::Stem || activeStemFile != stem)
    {
        // Take the player over, as playOutputAudio does from the input.
        audioProcessor.stopOutputPlayback();
        isPlayingInput = false;
        isPausedInput = false;
        currentInputPlaybackPosition = 0.0;
        updateInputPlayButtonIcon();
        isPlayingOutput = false;
        isPausedOutput = false;
        currentPlaybackPosition = 0.0;
        pausedPosition = 0.0;
        updatePlayButtonIcon();

        audioProcessor.loadOutputAudioForPlayback(stem);
        activePlaybackSource = PlaybackSource::Stem;
        activeStemFile = stem;
        stemPausedPosition = kNoPosition;
        stemPlaybackRunning = false;
    }
}

void Gary4juceAudioProcessorEditor::toggleStemPlayback(const juce::File& stem)
{
    loadStemIntoPlayer(stem);
    if (audioProcessor.getIsPlayingOutput())
    {
        audioProcessor.pauseOutputPlayback();
        stemPausedPosition = audioProcessor.getOutputPlaybackPosition();
        stemPlaybackRunning = false;
    }
    else
    {
        audioProcessor.startOutputPlayback(stemPausedPosition);
        stemPlaybackRunning = true;
    }
    repaint();
}

void Gary4juceAudioProcessorEditor::stopStemPlayback()
{
    if (activePlaybackSource == PlaybackSource::Stem)
        audioProcessor.stopOutputPlayback();
    stemPausedPosition = kNoPosition;
    stemPlaybackRunning = false;
}

void Gary4juceAudioProcessorEditor::seekStem(const juce::File& stem, double seconds)
{
    // Seeking a stem that is not loaded loads it, without playing, at that point; as the output
    // does, play then starts there, and a stem already playing carries on from it.
    loadStemIntoPlayer(stem);
    audioProcessor.seekOutputPlayback(seconds);
    stemPausedPosition = seconds;
}

void Gary4juceAudioProcessorEditor::releaseStemPlayback()
{
    if (activePlaybackSource != PlaybackSource::Stem)
        return;
    audioProcessor.stopOutputPlayback();
    activePlaybackSource = PlaybackSource::None;
    activeStemFile = juce::File();
    stemPausedPosition = kNoPosition;
    stemPlaybackRunning = false;
}

void Gary4juceAudioProcessorEditor::checkStemPlaybackStatus()
{
    // Played to the end: the next play starts from the beginning, as the output's does.
    if (activePlaybackSource == PlaybackSource::Stem && stemPlaybackRunning
        && !audioProcessor.getIsPlayingOutput())
    {
        stemPlaybackRunning = false;
        stemPausedPosition = kNoPosition;
    }
}

// --- drag a stem into the DAW, the way the output is dragged -------------------------------------

void Gary4juceAudioProcessorEditor::startStemDrag(const juce::File& stem, const juce::String& stemName)
{
    if (isDragInProgress.load() || !isEditorValid.load() || !stem.existsAsFile())
        return;
    if (!ensureGaryDataDirectoryAvailable())
        return;

    isDragInProgress.store(true);
    juce::File dragFile;
    {
        juce::ScopedLock lock(fileLock);
        const auto draggedAudioDir = getGaryDraggedAudioDirectory();
        if (!draggedAudioDir.exists() && !draggedAudioDir.createDirectory().wasOk())
        {
            showStatusMessage("drag failed - folder creation error", 2000);
            isDragInProgress.store(false);
            return;
        }

        // Only a dragged stem is kept: this copy, in the user's dragged audio format.
        const auto timestamp = juce::String(juce::Time::getCurrentTime().toMilliseconds());
        dragFile = draggedAudioDir.getChildFile("gary4juce_" + juce::File::createLegalFileName(stemName)
                                                + "_" + timestamp + getDraggedAudioFileExtension());
        if (!createDraggedAudioFile(stem, dragFile) || !dragFile.existsAsFile() || dragFile.getSize() <= 0)
        {
            dragFile.deleteFile();
            showStatusMessage("drag failed - could not create "
                + getDraggedAudioFileExtension().substring(1).toUpperCase() + " file", 3000);
            isDragInProgress.store(false);
            return;
        }
    }

    juce::StringArray files;
    files.add(dragFile.getFullPathName());

    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;
    const bool started = performExternalDragDropOfFiles(files, true, nullptr, [asyncAlive, editor, stemName]()
    {
        juce::MessageManager::callAsync([asyncAlive, editor, stemName]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;
            editor->showStatusMessage(stemName + " dragged", 2000);
            editor->isDragInProgress.store(false);
        });
    });
    if (!started)
    {
        showStatusMessage("drag failed - try again", 2000);
        juce::ScopedLock lock(fileLock);
        dragFile.deleteFile();
        isDragInProgress.store(false);
    }
}
