// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <array>
#include <unordered_map>

namespace
{
    constexpr auto kDataDirectoryKey = "dataDirectory";
    constexpr auto kFallbackDirtyKey = "dataDirectoryFallbackDirty";
    constexpr auto kStorageInitializedKey = "dataDirectoryInitialized";

    juce::File storageTestDirectoryOverride()
    {
       #if JUCE_DEBUG
        const auto path = juce::SystemStats::getEnvironmentVariable(
            "GARY4JUCE_STORAGE_TEST_DIRECTORY", {}).trim();
        if (path.isNotEmpty())
            return juce::File(path);
       #endif
        return {};
    }

    bool isStorageTestMode()
    {
        return storageTestDirectoryOverride() != juce::File{};
    }

    int storageTestDelayMilliseconds()
    {
       #if JUCE_DEBUG
        if (isStorageTestMode())
            return juce::jlimit(0, 500, juce::SystemStats::getEnvironmentVariable(
                "GARY4JUCE_STORAGE_TEST_DELAY_MS", {}).getIntValue());
       #endif
        return 0;
    }

    struct DirectoryCopyResult
    {
        bool ok = true;
        bool cancelled = false;
        int filesCopied = 0;
        int filesProcessed = 0;
        int totalFiles = 0;
        juce::String error;
    };

    using DirectoryProgress = std::function<void(double processed, int total, int copied)>;
    using CancellationCheck = std::function<bool()>;

    struct FileSnapshot
    {
        juce::int64 size = 0;
        juce::int64 modifiedMilliseconds = 0;
    };

    using DirectorySnapshot = std::unordered_map<std::string, FileSnapshot>;

    juce::File defaultGaryDataDirectory()
    {
        const auto testDirectory = storageTestDirectoryOverride();
        if (testDirectory != juce::File{})
            return testDirectory.getSiblingFile("fallback-gary4juce");

        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("gary4juce");
    }

    juce::File appDataRecoveryDirectory()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("gary4juce")
            .getChildFile("recovery-data");
    }

    juce::File temporaryRecoveryDirectory()
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("gary4juce-recovery");
    }

    bool ensureDirectoryWritable(const juce::File& directory, juce::String& error)
    {
        if (directory == juce::File{})
        {
            error = "the selected path is empty";
            return false;
        }

        if (directory.exists() && !directory.isDirectory())
        {
            error = "the selected path is a file, not a folder";
            return false;
        }

        if (!directory.isDirectory())
        {
            const auto result = directory.createDirectory();
            if (!result.wasOk() || !directory.isDirectory())
            {
                error = result.getErrorMessage().isNotEmpty()
                    ? result.getErrorMessage() : "the folder could not be created";
                return false;
            }
        }

        const auto probe = directory.getNonexistentChildFile(
            ".gary4juce-write-test", ".tmp", false);
        if (!probe.replaceWithText("gary4juce storage check"))
        {
            error = "the folder is not writable";
            return false;
        }

        if (!probe.deleteFile())
        {
            DBG("Could not remove storage write-test file: " + probe.getFullPathName());
        }

        return true;
    }

    bool streamRangesMatch(juce::InputStream& source,
                           juce::InputStream& destination,
                           juce::int64 position,
                           int bytesToCompare)
    {
        if (!source.setPosition(position) || !destination.setPosition(position))
            return false;

        std::array<char, 64 * 1024> sourceBytes{};
        std::array<char, 64 * 1024> destinationBytes{};
        auto remaining = bytesToCompare;
        while (remaining > 0)
        {
            const auto requested = juce::jmin(remaining, (int) sourceBytes.size());
            const auto sourceRead = source.read(sourceBytes.data(), requested);
            const auto destinationRead = destination.read(destinationBytes.data(), requested);
            if (sourceRead != requested || destinationRead != requested
                || std::memcmp(sourceBytes.data(), destinationBytes.data(), (size_t) requested) != 0)
                return false;
            remaining -= requested;
        }
        return true;
    }

    bool filesAppearIdentical(const juce::File& source,
                              const juce::File& destination,
                              const CancellationCheck& shouldCancel = {},
                              bool* wasCancelled = nullptr)
    {
        const auto sourceSize = source.getSize();
        if (!destination.existsAsFile() || sourceSize != destination.getSize())
            return false;

        const auto sourceModified = source.getLastModificationTime().toMilliseconds();
        const auto destinationModified = destination.getLastModificationTime().toMilliseconds();
        const auto timestampDifference = sourceModified >= destinationModified
            ? sourceModified - destinationModified : destinationModified - sourceModified;
        if (timestampDifference > 2000)
            return false;

        if (sourceSize == 0)
            return true;

        if (shouldCancel && shouldCancel())
        {
            if (wasCancelled != nullptr)
                *wasCancelled = true;
            return false;
        }

        auto sourceStream = source.createInputStream();
        auto destinationStream = destination.createInputStream();
        if (sourceStream == nullptr || destinationStream == nullptr)
            return false;

        constexpr int sampleSize = 64 * 1024;
        if (sourceSize <= sampleSize * 2)
            return streamRangesMatch(*sourceStream, *destinationStream, 0, (int) sourceSize);

        return streamRangesMatch(*sourceStream, *destinationStream, 0, sampleSize)
            && streamRangesMatch(*sourceStream, *destinationStream,
                                 sourceSize - sampleSize, sampleSize);
    }

    FileSnapshot getFileSnapshot(const juce::File& file)
    {
        return { file.getSize(), file.getLastModificationTime().toMilliseconds() };
    }

    bool snapshotsMatch(const FileSnapshot& first, const FileSnapshot& second)
    {
        return first.size == second.size
            && first.modifiedMilliseconds == second.modifiedMilliseconds;
    }

    juce::String normalizedRelativePath(const juce::File& file, const juce::File& root)
    {
        return file.getRelativePathFrom(root).replaceCharacter('\\', '/');
    }

    bool isInternalMigrationFile(const juce::String& relativePath)
    {
        const auto topLevelName = relativePath.upToFirstOccurrenceOf("/", false, false);
        return topLevelName.equalsIgnoreCase("migration_conflicts")
            || topLevelName.equalsIgnoreCase("migration_backups");
    }

    bool copyFileSafely(const juce::File& source,
                        const juce::File& destination,
                        const CancellationCheck& shouldCancel = {},
                        const std::function<void(double)>& reportFileProgress = {})
    {
        const auto temporaryFile = destination.getParentDirectory().getNonexistentChildFile(
            destination.getFileNameWithoutExtension() + ".migrating",
            destination.getFileExtension(), true);
        auto sourceStream = source.createInputStream();
        auto destinationStream = temporaryFile.createOutputStream();
        if (sourceStream == nullptr || destinationStream == nullptr || !destinationStream->openedOk())
        {
            temporaryFile.deleteFile();
            return false;
        }

        const auto sourceSize = source.getSize();
        std::array<char, 64 * 1024> bytes{};
        for (;;)
        {
            if (shouldCancel && shouldCancel())
            {
                sourceStream.reset();
                destinationStream.reset();
                temporaryFile.deleteFile();
                return false;
            }

            const auto bytesRead = sourceStream->read(bytes.data(), (int) bytes.size());
            if (bytesRead <= 0)
                break;
            if (!destinationStream->write(bytes.data(), (size_t) bytesRead))
            {
                sourceStream.reset();
                destinationStream.reset();
                temporaryFile.deleteFile();
                return false;
            }

            if (reportFileProgress && sourceSize > 0)
                reportFileProgress((double) sourceStream->getPosition() / (double) sourceSize);
        }

        destinationStream->flush();
        sourceStream.reset();
        destinationStream.reset();
        if (temporaryFile.getSize() != sourceSize)
        {
            temporaryFile.deleteFile();
            return false;
        }
        temporaryFile.setLastModificationTime(source.getLastModificationTime());
        if (reportFileProgress)
            reportFileProgress(1.0);

        juce::File previousFile;
        if (destination.existsAsFile())
        {
            previousFile = destination.getParentDirectory().getNonexistentChildFile(
                destination.getFileNameWithoutExtension() + ".previous",
                destination.getFileExtension(), true);
            if (!destination.moveFileTo(previousFile))
            {
                temporaryFile.deleteFile();
                return false;
            }
        }

        if (!temporaryFile.moveFileTo(destination))
        {
            if (previousFile.existsAsFile() && !previousFile.moveFileTo(destination))
            {
                DBG("Could not restore migration target: " + previousFile.getFullPathName());
            }
            temporaryFile.deleteFile();
            return false;
        }

        if (previousFile.existsAsFile() && !previousFile.deleteFile())
        {
            DBG("Could not remove migration staging file: " + previousFile.getFullPathName());
        }
        return true;
    }

    DirectoryCopyResult copyDataDirectoryContents(
        const juce::File& source,
        const juce::File& destination,
        const DirectoryProgress& reportProgress = {},
        const CancellationCheck& shouldCancel = {},
        DirectorySnapshot* capturedSnapshot = nullptr,
        const DirectorySnapshot* changedSinceSnapshot = nullptr,
        bool preserveDestinationConflicts = true)
    {
        DirectoryCopyResult result;

        if (source == destination || !source.isDirectory())
            return result;

        if (destination.isAChildOf(source))
        {
            result.ok = false;
            result.error = "the new folder cannot be inside the current gary4juce folder";
            return result;
        }

        juce::String validationError;
        if (!ensureDirectoryWritable(destination, validationError))
        {
            result.ok = false;
            result.error = validationError;
            return result;
        }

        juce::Array<juce::File> discoveredFiles;
        source.findChildFiles(discoveredFiles, juce::File::findFiles, true);
        juce::Array<juce::File> sourceFiles;
        for (const auto& sourceFile : discoveredFiles)
        {
            const auto relativePath = normalizedRelativePath(sourceFile, source);
            if (isInternalMigrationFile(relativePath))
                continue;

            const auto currentSnapshot = getFileSnapshot(sourceFile);
            const auto snapshotKey = relativePath.toStdString();
            if (capturedSnapshot != nullptr)
                (*capturedSnapshot)[snapshotKey] = currentSnapshot;

            if (changedSinceSnapshot != nullptr)
            {
                const auto previous = changedSinceSnapshot->find(snapshotKey);
                if (previous != changedSinceSnapshot->end()
                    && snapshotsMatch(previous->second, currentSnapshot))
                    continue;
            }

            sourceFiles.add(sourceFile);
        }
        result.totalFiles = sourceFiles.size();
        if (reportProgress)
            reportProgress(0, result.totalFiles, 0);
        const auto migrationStamp = juce::String(juce::Time::getCurrentTime().toMilliseconds());

        for (const auto& sourceFile : sourceFiles)
        {
            if (shouldCancel && shouldCancel())
            {
                result.ok = false;
                result.cancelled = true;
                return result;
            }

            const auto relativePath = normalizedRelativePath(sourceFile, source);
            auto destinationFile = destination.getChildFile(relativePath);

            bool comparisonCancelled = false;
            if (filesAppearIdentical(sourceFile, destinationFile, shouldCancel,
                                     &comparisonCancelled))
            {
                ++result.filesProcessed;
                if (reportProgress)
                    reportProgress(result.filesProcessed, result.totalFiles, result.filesCopied);
                continue;
            }
            if (comparisonCancelled)
            {
                result.ok = false;
                result.cancelled = true;
                return result;
            }

            const auto parentResult = destinationFile.getParentDirectory().createDirectory();
            if (!parentResult.wasOk())
            {
                result.ok = false;
                result.error = "could not create " + destinationFile.getParentDirectory().getFullPathName()
                    + ": " + parentResult.getErrorMessage();
                return result;
            }

            const bool isTemporaryWorkingAudio = destinationFile.getFileName()
                .equalsIgnoreCase("myBuffer.wav")
                || destinationFile.getFileName().equalsIgnoreCase("myOutput.wav");
            if (destinationFile.existsAsFile() && preserveDestinationConflicts
                && !isTemporaryWorkingAudio)
            {
                const auto backupFile = destination
                    .getChildFile("migration_conflicts")
                    .getChildFile(migrationStamp)
                    .getChildFile(relativePath);
                const auto backupResult = backupFile.getParentDirectory().createDirectory();
                if (!backupResult.wasOk()
                    || !copyFileSafely(destinationFile, backupFile, shouldCancel))
                {
                    if (shouldCancel && shouldCancel())
                    {
                        result.ok = false;
                        result.cancelled = true;
                        return result;
                    }
                    result.ok = false;
                    result.error = "could not back up existing " + relativePath;
                    return result;
                }
            }

            if (!copyFileSafely(
                    sourceFile, destinationFile, shouldCancel,
                    [&result, &reportProgress](double fileFraction)
                    {
                        if (reportProgress)
                            reportProgress((double) result.filesProcessed + fileFraction,
                                           result.totalFiles, result.filesCopied);
                    }))
            {
                if (shouldCancel && shouldCancel())
                {
                    result.ok = false;
                    result.cancelled = true;
                    return result;
                }
                result.ok = false;
                result.error = "failed to copy " + relativePath;
                return result;
            }

            ++result.filesCopied;
            ++result.filesProcessed;
            if (reportProgress)
                reportProgress(result.filesProcessed, result.totalFiles, result.filesCopied);
            if (const auto delay = storageTestDelayMilliseconds(); delay > 0)
                juce::Thread::sleep((unsigned int) delay);
        }

        return result;
    }

    juce::File normalizeSelectedStorageFolder(const juce::File& selected)
    {
        if (selected.getFileName().equalsIgnoreCase("gary4juce"))
            return selected;

        return selected.getChildFile("gary4juce");
    }

    class StorageMigrationTask final : public juce::ThreadWithProgressWindow
    {
    public:
        using Completion = std::function<void(bool cancelled,
                                              const DirectoryCopyResult& firstPass,
                                              const DirectoryCopyResult& catchUpPass)>;

        StorageMigrationTask(const juce::File& sourceDirectory,
                             const juce::File& destinationDirectory,
                             juce::Component* parent,
                             Completion completionCallback)
            : juce::ThreadWithProgressWindow("migrating gary4juce storage",
                                             true, true, 10000, "cancel", parent),
              source(sourceDirectory),
              destination(destinationDirectory),
              completion(std::move(completionCallback))
        {
            setProgress(0.0);
            setStatusMessage("preparing file list...");
        }

        void run() override
        {
            firstPass = copyDataDirectoryContents(
                source, destination,
                [this](double processed, int total, int copied)
                {
                    const auto fraction = total > 0 ? processed / (double) total : 1.0;
                    setProgress(fraction);
                    setStatusMessage("copying storage: " + juce::String((int) processed)
                        + " of " + juce::String(total) + " files ("
                        + juce::String(copied) + " copied)");
                },
                [this]() { return threadShouldExit(); },
                &initialSnapshot);

            if (!firstPass.ok || threadShouldExit())
                return;

            setProgress(0.0);
            setStatusMessage("checking for files changed during migration...");
            catchUpPass = copyDataDirectoryContents(
                source, destination,
                [this](double processed, int total, int copied)
                {
                    const auto fraction = total > 0 ? processed / (double) total : 1.0;
                    setProgress(fraction);
                    if (total == 0)
                        setStatusMessage("no additional changes found");
                    else
                        setStatusMessage("copying changes: " + juce::String((int) processed)
                            + " of " + juce::String(total) + " files"
                            + (copied > 0 ? " (" + juce::String(copied) + " updated)"
                                          : juce::String{}));
                },
                [this]() { return threadShouldExit(); },
                nullptr, &initialSnapshot, false);

            if (catchUpPass.ok)
            {
                setProgress(1.0);
                setStatusMessage("migration complete");
            }
        }

        void threadComplete(bool userPressedCancel) override
        {
            completion(userPressedCancel || firstPass.cancelled || catchUpPass.cancelled,
                       firstPass, catchUpPass);
        }

    private:
        juce::File source;
        juce::File destination;
        Completion completion;
        DirectorySnapshot initialSnapshot;
        DirectoryCopyResult firstPass;
        DirectoryCopyResult catchUpPass;
    };
}

void Gary4juceAudioProcessorEditor::initializeGaryDataDirectory()
{
    auto& preferences = getUpdatePreferences();
    const auto testDirectory = storageTestDirectoryOverride();
    const bool testMode = testDirectory != juce::File{};
    const auto configuredPath = testMode
        ? testDirectory.getFullPathName()
        : preferences.getValue(kDataDirectoryKey).trim();
    const bool storageWasInitialized = testMode
        || preferences.getBoolValue(kStorageInitializedKey, false);
    configuredGaryDataDirectory = configuredPath.isNotEmpty()
        ? juce::File(configuredPath) : defaultGaryDataDirectory();
    garyDataFallbackDirty = !testMode
        && preferences.getBoolValue(kFallbackDirtyKey, false);

    juce::String configuredError;
    bool configuredAvailable = false;
    if (!storageWasInitialized || configuredGaryDataDirectory.isDirectory())
    {
        configuredAvailable = ensureDirectoryWritable(
            configuredGaryDataDirectory, configuredError);
    }
    else
    {
        configuredError = "the configured folder no longer exists";
    }

    if (!testMode)
    {
        preferences.setValue(kStorageInitializedKey, true);
        preferences.saveIfNeeded();
    }

    if (configuredAvailable && !(garyDataFallbackDirty
        && configuredGaryDataDirectory != defaultGaryDataDirectory()))
    {
        activateGaryDataDirectory(configuredGaryDataDirectory, false);
        return;
    }

    const juce::File fallbacks[] = {
        defaultGaryDataDirectory(),
        appDataRecoveryDirectory(),
        temporaryRecoveryDirectory()
    };

    for (const auto& fallback : fallbacks)
    {
        if (fallback == configuredGaryDataDirectory && !configuredAvailable)
            continue;

        juce::String fallbackError;
        if (ensureDirectoryWritable(fallback, fallbackError))
        {
            activateGaryDataDirectory(fallback, true);
            garyDataFallbackDirty = true;
            if (!testMode)
            {
                preferences.setValue(kFallbackDirtyKey, true);
                preferences.saveIfNeeded();
            }

            if (configuredAvailable)
                showStatusMessage("storage recovered - open storage settings to merge files", 8000);
            else
                showStatusMessage("configured storage unavailable - using recovery folder", 8000);
            return;
        }
    }

    activeGaryDataDirectory = defaultGaryDataDirectory();
    usingGaryDataFallback = true;
    updateStorageButtonState();
    showStatusMessage("no writable storage folder is currently available", 10000);
    DBG("No writable gary4juce data directory. Configured error: " + configuredError);
}

bool Gary4juceAudioProcessorEditor::ensureGaryDataDirectoryAvailable(bool notifyUser)
{
    juce::String error;
    if (activeGaryDataDirectory.isDirectory()
        && ensureDirectoryWritable(activeGaryDataDirectory, error))
        return true;

    const auto unavailableDirectory = activeGaryDataDirectory;
    const juce::File fallbacks[] = {
        defaultGaryDataDirectory(),
        appDataRecoveryDirectory(),
        temporaryRecoveryDirectory()
    };

    for (const auto& fallback : fallbacks)
    {
        if (fallback == unavailableDirectory)
            continue;

        juce::String fallbackError;
        if (!ensureDirectoryWritable(fallback, fallbackError))
            continue;

        activateGaryDataDirectory(fallback, true);
        garyDataFallbackDirty = true;
        if (!isStorageTestMode())
        {
            auto& preferences = getUpdatePreferences();
            preferences.setValue(kFallbackDirtyKey, true);
            preferences.saveIfNeeded();
        }
        recoverCurrentAudioFiles();

        if (notifyUser)
            showStatusMessage("storage disconnected - audio recovered to fallback folder", 9000);
        return true;
    }

    if (notifyUser)
        showStatusMessage("storage error - no writable fallback folder available", 10000);
    return false;
}

void Gary4juceAudioProcessorEditor::activateGaryDataDirectory(const juce::File& directory,
                                                              bool isFallback)
{
    const auto previousOutputFile = outputAudioFile;
    activeGaryDataDirectory = directory;
    usingGaryDataFallback = isFallback;
    outputAudioFile = getGaryOutputFile();

    if (lastDraggedAudioFile == previousOutputFile)
        lastDraggedAudioFile = outputAudioFile;

    if (foundationUI != nullptr)
        foundationUI->setDataDirectory(activeGaryDataDirectory);

    updateStorageButtonState();
}

void Gary4juceAudioProcessorEditor::recoverCurrentAudioFiles()
{
    if (!activeGaryDataDirectory.isDirectory())
        return;

    const auto bufferFile = getGaryBufferFile();
    if (!isRecording && recordedSamples > 0
        && !audioProcessor.saveRecordingToFile(bufferFile))
    {
        DBG("Could not recover the in-memory input buffer to fallback storage");
    }

    outputAudioFile = getGaryOutputFile();
    if (outputAudioBuffer.getNumSamples() > 0)
        writeCurrentOutputToFile(outputAudioFile);

    updateAllGenerationButtonStates();
    repaint();
}

bool Gary4juceAudioProcessorEditor::writeDataToFileSafely(const juce::File& file,
                                                          const void* data,
                                                          size_t dataSize) const
{
    const auto parentResult = file.getParentDirectory().createDirectory();
    if (!parentResult.wasOk())
        return false;

    const auto temporaryFile = file.getParentDirectory().getNonexistentChildFile(
        file.getFileNameWithoutExtension() + ".writing", file.getFileExtension(), true);
    if (!temporaryFile.replaceWithData(data, dataSize))
    {
        temporaryFile.deleteFile();
        return false;
    }

    const bool installed = copyFileSafely(temporaryFile, file);
    temporaryFile.deleteFile();
    return installed;
}

bool Gary4juceAudioProcessorEditor::writeCurrentOutputToFile(const juce::File& file) const
{
    if (outputAudioBuffer.getNumChannels() <= 0 || outputAudioBuffer.getNumSamples() <= 0)
        return false;

    const auto parentResult = file.getParentDirectory().createDirectory();
    if (!parentResult.wasOk())
        return false;

    const auto temporaryFile = file.getParentDirectory().getNonexistentChildFile(
        file.getFileNameWithoutExtension() + ".writing", file.getFileExtension(), true);

    std::unique_ptr<juce::FileOutputStream> stream(temporaryFile.createOutputStream());
    if (stream == nullptr || !stream->openedOk())
    {
        temporaryFile.deleteFile();
        return false;
    }

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(
        stream.release(), juce::jmax(1.0, currentAudioSampleRate),
        outputAudioBuffer.getNumChannels(), 16, {}, 0));
    if (writer == nullptr)
    {
        temporaryFile.deleteFile();
        return false;
    }

    const bool written = writer->writeFromAudioSampleBuffer(
        outputAudioBuffer, 0, outputAudioBuffer.getNumSamples());
    writer.reset();
    if (!written)
    {
        temporaryFile.deleteFile();
        return false;
    }

    if (!copyFileSafely(temporaryFile, file))
    {
        temporaryFile.deleteFile();
        return false;
    }

    temporaryFile.deleteFile();
    return true;
}

void Gary4juceAudioProcessorEditor::updateStorageButtonState()
{
    storageButton.setButtonStyle(usingGaryDataFallback
        ? CustomButton::ButtonStyle::Terry
        : CustomButton::ButtonStyle::Standard);
    storageButton.setTooltip((usingGaryDataFallback ? "using recovery storage: " : "audio storage: ")
        + activeGaryDataDirectory.getFullPathName());
}

void Gary4juceAudioProcessorEditor::showStorageSettings()
{
    auto message = "Current folder:\n" + activeGaryDataDirectory.getFullPathName();
    if (usingGaryDataFallback)
        message += "\n\nConfigured folder:\n" + configuredGaryDataDirectory.getFullPathName()
            + "\n\nThe configured folder is unavailable or has unmerged recovery files."
              " Gary is staying on the recovery folder so no work is stranded.";
    else
        message += "\n\nChanging location copies the complete gary4juce folder."
            " The old folder is retained as a backup.";

    auto* alert = new juce::AlertWindow("gary4juce storage", message,
        usingGaryDataFallback ? juce::MessageBoxIconType::WarningIcon
                              : juce::MessageBoxIconType::InfoIcon,
        this);
    alert->addButton("change / migrate", 1);
    if (usingGaryDataFallback && configuredGaryDataDirectory != juce::File{})
        alert->addButton("retry configured", 2);
    if (activeGaryDataDirectory != defaultGaryDataDirectory())
        alert->addButton("use default", 3);
    alert->addButton("open folder", 4);
    alert->addButton("close", 0);
    trackEditorModalWindow(alert);

    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;
    alert->enterModalState(true, juce::ModalCallbackFunction::create(
        [asyncAlive, editor, alert](int result)
        {
            std::unique_ptr<juce::AlertWindow> cleanup(alert);
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            if (result == 1)
                editor->chooseGaryDataDirectory();
            else if (result == 2)
                editor->migrateGaryDataDirectory(editor->configuredGaryDataDirectory);
            else if (result == 3)
                editor->migrateGaryDataDirectory(defaultGaryDataDirectory());
            else if (result == 4)
                editor->activeGaryDataDirectory.startAsProcess();
        }));
}

void Gary4juceAudioProcessorEditor::chooseGaryDataDirectory()
{
    if (storageMigrationInProgress.load())
    {
        showStatusMessage("storage migration already in progress", 3000);
        return;
    }

    storageFolderChooser = std::make_unique<juce::FileChooser>(
        "choose where the gary4juce folder should live",
        activeGaryDataDirectory.getParentDirectory(), juce::String{}, true, false, this);

    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;
    storageFolderChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [asyncAlive, editor](const juce::FileChooser& chooser)
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            const auto selected = chooser.getResult();
            if (selected == juce::File{})
                return;

            const auto destination = normalizeSelectedStorageFolder(selected);
            if (destination == editor->activeGaryDataDirectory)
            {
                editor->showStatusMessage("that folder is already active", 2500);
                return;
            }

            auto* confirm = new juce::AlertWindow(
                "migrate gary4juce storage",
                "Copy everything from:\n" + editor->activeGaryDataDirectory.getFullPathName()
                    + "\n\nto:\n" + destination.getFullPathName()
                    + "\n\nThe original folder will not be deleted.",
                juce::MessageBoxIconType::QuestionIcon, editor);
            confirm->addButton("copy and use", 1);
            confirm->addButton("cancel", 0);
            editor->trackEditorModalWindow(confirm);
            confirm->enterModalState(true, juce::ModalCallbackFunction::create(
                [asyncAlive, editor, confirm, destination](int result)
                {
                    std::unique_ptr<juce::AlertWindow> cleanup(confirm);
                    const auto stillAlive = asyncAlive.lock();
                    if (stillAlive == nullptr || !stillAlive->load(std::memory_order_acquire))
                        return;
                    if (result == 1)
                        editor->migrateGaryDataDirectory(destination);
                }));
        });
}

void Gary4juceAudioProcessorEditor::migrateGaryDataDirectory(const juce::File& destination)
{
    if (storageMigrationInProgress.exchange(true))
    {
        showStatusMessage("storage migration already in progress", 3000);
        return;
    }

    juce::String validationError;
    if (!ensureDirectoryWritable(destination, validationError))
    {
        storageMigrationInProgress.store(false);
        showStatusMessage("storage migration failed: " + validationError, 9000);
        return;
    }

    const auto source = activeGaryDataDirectory;
    if (source == destination)
    {
        storageMigrationInProgress.store(false);
        return;
    }

    showStatusMessage("copying gary4juce storage...", 60000);
    storageButton.setEnabled(false);

    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;
    auto completion = [destination, asyncAlive, editor](
        bool cancelled,
        const DirectoryCopyResult& firstPass,
        const DirectoryCopyResult& catchUpPass)
    {
        const auto alive = asyncAlive.lock();
        if (alive == nullptr || !alive->load(std::memory_order_acquire))
            return;

        auto finishWithError = [editor](const juce::String& error)
        {
            editor->storageMigrationInProgress.store(false);
            editor->storageButton.setEnabled(true);
            editor->showStatusMessage(
                "migration incomplete - original folder remains active: " + error, 12000);
        };

        if (cancelled)
        {
            finishWithError("cancelled; partial destination copy retained for safe retry");
            return;
        }

        if (!firstPass.ok)
        {
            finishWithError(firstPass.error);
            return;
        }

        if (!catchUpPass.ok)
        {
            finishWithError(catchUpPass.error);
            return;
        }

        if (!isStorageTestMode())
        {
            auto& preferences = editor->getUpdatePreferences();
            if (destination == defaultGaryDataDirectory())
                preferences.removeValue(kDataDirectoryKey);
            else
                preferences.setValue(kDataDirectoryKey, destination.getFullPathName());
            preferences.setValue(kFallbackDirtyKey, false);
            preferences.saveIfNeeded();
        }

        editor->configuredGaryDataDirectory = destination;
        editor->garyDataFallbackDirty = false;
        editor->activateGaryDataDirectory(destination, false);
        editor->recoverCurrentAudioFiles();
        editor->storageMigrationInProgress.store(false);
        editor->storageButton.setEnabled(true);
        editor->showStatusMessage("storage migration complete - original folder retained", 7000);
    };

    storageMigrationTask = std::make_unique<StorageMigrationTask>(
        source, destination, this, std::move(completion));
    storageMigrationTask->launchThread();
}
