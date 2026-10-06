// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

/*
  ==============================================================================
    StemsRuntime.h

    The embedded stem separator: stems.cpp's runtime (stems.dll and its ggml
    libraries) and its GGUF models, downloaded on request into the gary4juce
    data folder, so they follow the user's storage location and survive plugin
    updates:

        <gary data>/stems/runtime/<tag>/   stems.dll + stems-ggml*.dll, or libstems.dylib
        <gary data>/stems/models/          *.gguf

    Every download is checked against a SHA-256 pinned below before it is used.
    StemsService runs one job at a time on a background thread; the settings
    panel polls getJob() rather than receiving callbacks, so nothing on the
    worker side has to outlive the window.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace stems
{
    struct PackageFile
    {
        juce::String fileName;
        juce::String sha256;
        juce::int64 sizeBytes = 0;
    };

    // The stems.cpp release this build of gary4juce uses.
    struct RuntimeRelease
    {
        juce::String tag;                  // "v0.1.0"
        juce::String downloadBaseUrl;      // ".../releases/download/v0.1.0/"
        std::vector<PackageFile> packages; // unzipped in order into one folder
    };

    struct ModelInfo
    {
        juce::String id;           // stems.cpp's model name, e.g. "htdemucs"
        juce::String stemsSummary; // "drums, bass, other, vocals"
        juce::String note;         // shown beside the name; may be empty
        juce::String url;
        PackageFile file;
    };

    const RuntimeRelease& pinnedRuntime();
    const std::vector<ModelInfo>& modelCatalog();
    const ModelInfo* findModel(const juce::String& id);
    juce::String defaultModelId();
    // The stems a model writes, in its order ("drums", "bass", ...), before it has run.
    juce::StringArray stemNamesFor(const juce::String& modelId);

    juce::String formatBytes(juce::int64 bytes);

    class StemsService : public std::enable_shared_from_this<StemsService>
    {
    public:
        enum class JobKind { None, InstallRuntime, DownloadModel, Test, Separate };

        struct Job
        {
            JobKind kind = JobKind::None;
            juce::String modelId;       // DownloadModel / Test / Separate
            bool running = false;
            double progress = -1.0;     // 0..1, or negative when unknown
            juce::String status;        // one short line for the panel
            bool succeeded = false;     // meaningful once running is false
            juce::String error;
            // Separate: one 24-bit WAV per stem, at the source's sample rate, in model order.
            juce::StringArray stemNames;
            juce::Array<juce::File> stemFiles;
        };

        explicit StemsService(juce::File garyDataDirectory);
        ~StemsService();

        // After a storage migration. A runtime already loaded keeps running from its old folder
        // (the migration keeps that folder as a backup); new loads come from the new one.
        void setDataDirectory(juce::File garyDataDirectory);

        juce::File getStemsDirectory() const;
        juce::File getRuntimeDirectory() const;   // for the pinned tag
        juce::File getModelsDirectory() const;

        bool isRuntimeAvailableForPlatform() const;
        bool isRuntimeInstalled() const;
        bool isModelInstalled(const juce::String& modelId) const;
        bool hasPendingRemoval() const;

        // Jobs. Each returns false, and changes nothing, while another job is running.
        bool startRuntimeInstall();
        bool startModelDownload(const juce::String& modelId);
        // Separates three seconds of test audio with the model on the GPU or the CPU. That proves the
        // whole path: the DLL loads from the data folder, finds its backends beside it, runs the model.
        bool startTest(const juce::String& modelId, bool useGpu);
        // Separates source (an audio file) and writes the stems to outputDirectory/<model>/.
        bool startSeparation(const juce::String& modelId, bool useGpu,
                             const juce::File& source, const juce::File& outputDirectory);
        void cancelJob();
        Job getJob() const;

        // Separated stems are ephemeral: each open stems popup gets its own session folder, and
        // deletes it when it closes. Only a stem the user drags out is copied, into dragged_audio.
        // Sessions older than a day (left by a crash) are swept when the service starts.
        juce::File createSessionDirectory();
        void deleteSessionDirectory(const juce::File& session);

        // Deletes what it can now. A runtime that is loaded in this session goes on the next.
        void removeRuntime();
        void removeModel(const juce::String& modelId);

    private:
        struct Engine;
        struct CallbackBridge;
        friend struct CallbackBridge;

        bool beginJob(JobKind kind, const juce::String& modelId, const juce::String& status);
        void finishJob(bool succeeded, const juce::String& message);
        void setProgress(double progress, const juce::String& status);
        bool cancelled() const { return cancelRequested.load(); }

        bool download(const juce::String& url, const PackageFile& expected, const juce::File& localSource,
                      const juce::File& destination, const juce::String& label,
                      double progressStart, double progressSpan, juce::String& error);
        void runRuntimeInstall();
        void runModelDownload(const juce::String& modelId);
        void runTest(const juce::String& modelId, bool useGpu);
        void runSeparation(const juce::String& modelId, bool useGpu, const juce::File& source,
                           const juce::File& outputDirectory);
        bool ensureEngine(juce::String& error);
        void deletePendingRemovals();
        void sweepStaleSessions();

        // GARY4JUCE_STEMS_PACKAGE_DIR: a folder holding the release zips and their SHA256SUMS, used
        // instead of GitHub, as gary4local's GARY4LOCAL_NATIVE_PACKAGE_DIR is. It is for testing a
        // stems.cpp release before it is published; its SHA256SUMS stands in for the pinned hashes.
        juce::File packageOverrideDirectory() const;

        mutable std::mutex mutex;
        juce::File dataDirectory;
        Job job;
        std::atomic<bool> cancelRequested { false };
        std::unique_ptr<Engine> engine;   // used only by the job thread, and there is one job at a time
    };
}
