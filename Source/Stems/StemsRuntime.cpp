// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "StemsRuntime.h"
#include "libstems_v1.h"

#include <cmath>
#include <map>

#if JUCE_WINDOWS
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace stems
{
    namespace
    {
        constexpr auto kInstalledMarker = "installed.json";
        constexpr auto kPendingRemovalFile = "pending-removal.txt";
        constexpr int kDownloadChunkBytes = 1 << 20;

        juce::String huggingFaceUrl(const juce::String& repo, const juce::String& fileName)
        {
            return "https://huggingface.co/thepatch/" + repo + "/resolve/main/" + fileName;
        }

        // "a b  c" lines of a SHA256SUMS file -> file name -> lowercase hash.
        std::map<juce::String, juce::String> readSha256Sums(const juce::File& file)
        {
            std::map<juce::String, juce::String> sums;
            juce::StringArray lines;
            lines.addLines(file.loadFileAsString());
            for (const auto& line : lines)
            {
                const auto trimmed = line.trim();
                const auto space = trimmed.indexOfChar(' ');
                if (space <= 0)
                    continue;
                const auto hash = trimmed.substring(0, space).toLowerCase();
                const auto name = trimmed.substring(space).trim().trimCharactersAtStart("*");
                if (hash.length() == 64 && name.isNotEmpty())
                    sums[name] = hash;
            }
            return sums;
        }
    }

    const RuntimeRelease& pinnedRuntime()
    {
        // core = stems.dll, stems-ggml.dll, stems-ggml-base.dll, the CPU variants; vulkan =
        // stems-ggml-vulkan.dll. Vulkan covers NVIDIA, AMD and Intel GPUs without the CUDA runtime.
        // The hashes are filled in from the published release's SHA256SUMS; until then a build
        // installs only through GARY4JUCE_STEMS_PACKAGE_DIR.
        static const RuntimeRelease release = []
        {
            RuntimeRelease r;
#if JUCE_WINDOWS
            r.tag = "v0.1.0";
            r.downloadBaseUrl = "https://github.com/betweentwomidnights/stems.cpp/releases/download/v0.1.0/";
            r.packages = {
                { "stems-v0.1.0-windows-x64-core.zip", "", 0 },
                { "stems-v0.1.0-windows-x64-vulkan.zip", "", 0 },
            };
#endif
            return r;
        }();
        return release;
    }

    const std::vector<ModelInfo>& modelCatalog()
    {
        // From the thepatch/*-GGUF repositories. F16 where it was published (each model card has its
        // parity numbers); viperx's BS-RoFormer only as F32.
        static const std::vector<ModelInfo> catalog = {
            { "htdemucs", "drums, bass, other, vocals", "",
              huggingFaceUrl("htdemucs-GGUF", "htdemucs-42M-v1.0-F16.gguf"),
              { "htdemucs-42M-v1.0-F16.gguf",
                "0531c009533f69d292f71263815d447ce23224062fbf6335db19d3577efb87c7", 105072000 } },
            { "htdemucs_6s", "drums, bass, other, vocals, guitar, piano", "",
              huggingFaceUrl("htdemucs-GGUF", "htdemucs_6s-27M-v1.0-F16.gguf"),
              { "htdemucs_6s-27M-v1.0-F16.gguf",
                "37f766646feb272ab022d83f054f0d395f7326915b3edd72535adb7c432aba13", 74318816 } },
            { "htdemucs_ft", "drums, bass, other, vocals", "best quality, 4x slower",
              huggingFaceUrl("htdemucs-GGUF", "htdemucs_ft-4x42M-v1.0-F16.gguf"),
              { "htdemucs_ft-4x42M-v1.0-F16.gguf",
                "a3eb551fbdcd55f8f01a43984445e1675398b56d7fba3cba84f0b669c3da228a", 420284832 } },
            { "mel_band_roformer_kim", "vocals, instrumental", "",
              huggingFaceUrl("mel-band-roformer-kim-GGUF", "mel_band_roformer_kim-0.2B-v1.0-F16.gguf"),
              { "mel_band_roformer_kim-0.2B-v1.0-F16.gguf",
                "096e097b66367654c9b909979a55cf873a7a4d784f4599d4754ff420492ee139", 456987072 } },
            { "bs_roformer_viperx_317", "vocals, instrumental", "no stated upstream license",
              huggingFaceUrl("bs-roformer-viperx-317-GGUF", "bs_roformer_viperx_317-0.2B-v1.0-F32.gguf"),
              { "bs_roformer_viperx_317-0.2B-v1.0-F32.gguf",
                "57425b20ea63d886bcecf8e59a1374613d4a4bb81373262f9e7463d718c02162", 639074304 } },
        };
        return catalog;
    }

    const ModelInfo* findModel(const juce::String& id)
    {
        for (const auto& model : modelCatalog())
            if (model.id == id)
                return &model;
        return nullptr;
    }

    juce::String defaultModelId() { return "htdemucs"; }

    juce::StringArray stemNamesFor(const juce::String& modelId)
    {
        juce::StringArray names;
        if (const auto* model = findModel(modelId))
            names.addTokens(model->stemsSummary, ",", {});
        names.trim();
        names.removeEmptyStrings();
        return names;
    }

    juce::String formatBytes(juce::int64 bytes)
    {
        const double mb = (double) bytes / (1024.0 * 1024.0);
        if (mb >= 1024.0)
            return juce::String(mb / 1024.0, 2) + " GB";
        if (mb >= 100.0)
            return juce::String(juce::roundToInt(mb)) + " MB";
        return juce::String(mb, 1) + " MB";
    }

    // ---------------------------------------------------------------------------------------------
    // The loaded library. Never unloaded: FreeLibrary on ggml while its GPU backend's threads are
    // alive is a crash waiting to happen, and the DLL is small. Contexts (the model weights in VRAM)
    // are created per job and destroyed after it, so nothing stays resident between uses.

    struct StemsService::Engine
    {
        juce::File library;
        const stems_api_v1* api = nullptr;
    };

    // separate() calls these on the job thread, between segments; user is the StemsService.
    struct StemsService::CallbackBridge
    {
        static int32_t STEMS_CALL shouldCancel(void* user)
        {
            return static_cast<StemsService*>(user)->cancelled() ? 1 : 0;
        }

        static void STEMS_CALL onProgress(void* user, const stems_progress_v1* progress)
        {
            auto* self = static_cast<StemsService*>(user);
            if (progress != nullptr && progress->size >= STEMS_PROGRESS_V1_MIN_SIZE)
                self->setProgress((double) progress->fraction, {});
        }
    };

    namespace
    {
#if JUCE_WINDOWS
        const stems_api_v1* loadApi(const juce::File& library, juce::String& error)
        {
            HMODULE module = LoadLibraryExW(library.getFullPathName().toWideCharPointer(), nullptr,
                                            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (module == nullptr)
            {
                error = "could not load " + library.getFileName() + " (Windows error "
                    + juce::String((int) GetLastError()) + ")";
                return nullptr;
            }
            using GetApi = const stems_api_v1* (STEMS_CALL*)(uint32_t);
            const auto getApi = reinterpret_cast<GetApi>(GetProcAddress(module, "stems_get_api"));
            const stems_api_v1* api = getApi != nullptr ? getApi(STEMS_ABI_VERSION_1) : nullptr;
            if (api == nullptr || api->size < STEMS_API_V1_MIN_SIZE)
                error = library.getFileName() + " does not provide the stems C ABI v1";
            return error.isEmpty() ? api : nullptr;
        }
#else
        const stems_api_v1* loadApi(const juce::File&, juce::String& error)
        {
            error = "the stem separator is not available on this platform yet";
            return nullptr;
        }
#endif

        // Three seconds of something with drums, bass, a chord and a voice-like tone in it.
        std::vector<float> makeTestAudio(int sampleRate, int seconds)
        {
            const int n = sampleRate * seconds;
            std::vector<float> planar((size_t) n * 2);
            juce::Random random(4);
            for (int i = 0; i < n; ++i)
            {
                const double t = (double) i / sampleRate;
                const double beat = std::fmod(t, 0.5);
                const double kick = std::sin(2.0 * juce::MathConstants<double>::pi * 55.0 * beat) * std::exp(-beat * 18.0);
                const double hat = (random.nextFloat() * 2.0f - 1.0f) * std::exp(-std::fmod(t, 0.25) * 60.0) * 0.3;
                const double bass = std::sin(2.0 * juce::MathConstants<double>::pi * 110.0 * t) * 0.25;
                const double chord = (std::sin(2.0 * juce::MathConstants<double>::pi * 261.6 * t)
                                    + std::sin(2.0 * juce::MathConstants<double>::pi * 329.6 * t)
                                    + std::sin(2.0 * juce::MathConstants<double>::pi * 392.0 * t)) * 0.08;
                const double voice = std::sin(2.0 * juce::MathConstants<double>::pi * (440.0 + 6.0 * std::sin(2.0 * juce::MathConstants<double>::pi * 5.0 * t)) * t) * 0.12;
                const float mix = (float) (0.6 * (kick + hat + bass + chord + voice));
                planar[(size_t) i] = mix;
                planar[(size_t) n + (size_t) i] = mix * 0.9f;
            }
            return planar;
        }
    }

    // ---------------------------------------------------------------------------------------------

    StemsService::StemsService(juce::File garyDataDirectory)
        : dataDirectory(std::move(garyDataDirectory))
    {
        deletePendingRemovals();
        sweepStaleSessions();
    }

    StemsService::~StemsService() = default;

    void StemsService::setDataDirectory(juce::File garyDataDirectory)
    {
        std::lock_guard<std::mutex> lock(mutex);
        dataDirectory = std::move(garyDataDirectory);
    }

    juce::File StemsService::getStemsDirectory() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return dataDirectory.getChildFile("stems");
    }

    juce::File StemsService::getRuntimeDirectory() const
    {
        return getStemsDirectory().getChildFile("runtime").getChildFile(pinnedRuntime().tag);
    }

    juce::File StemsService::getModelsDirectory() const
    {
        return getStemsDirectory().getChildFile("models");
    }

    bool StemsService::isRuntimeAvailableForPlatform() const
    {
        return !pinnedRuntime().packages.empty();
    }

    bool StemsService::isRuntimeInstalled() const
    {
        if (!isRuntimeAvailableForPlatform())
            return false;
        const auto dir = getRuntimeDirectory();
        return dir.getChildFile("stems.dll").existsAsFile() && dir.getChildFile(kInstalledMarker).existsAsFile();
    }

    bool StemsService::isModelInstalled(const juce::String& modelId) const
    {
        const auto* model = findModel(modelId);
        if (model == nullptr)
            return false;
        const auto file = getModelsDirectory().getChildFile(model->file.fileName);
        return file.existsAsFile() && file.getSize() == model->file.sizeBytes;
    }

    bool StemsService::hasPendingRemoval() const
    {
        return getStemsDirectory().getChildFile(kPendingRemovalFile).existsAsFile();
    }

    juce::File StemsService::packageOverrideDirectory() const
    {
        const auto value = juce::SystemStats::getEnvironmentVariable("GARY4JUCE_STEMS_PACKAGE_DIR", {});
        return value.isNotEmpty() ? juce::File(value) : juce::File();
    }

    // --- jobs ------------------------------------------------------------------------------------

    bool StemsService::beginJob(JobKind kind, const juce::String& modelId, const juce::String& status)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (job.running)
            return false;
        job = Job();
        job.kind = kind;
        job.modelId = modelId;
        job.running = true;
        job.status = status;
        cancelRequested.store(false);
        return true;
    }

    void StemsService::finishJob(bool succeeded, const juce::String& message)
    {
        std::lock_guard<std::mutex> lock(mutex);
        job.running = false;
        job.succeeded = succeeded;
        job.progress = succeeded ? 1.0 : job.progress;
        if (succeeded)
            job.status = message;
        else
            job.error = message;
    }

    void StemsService::setProgress(double progress, const juce::String& status)
    {
        std::lock_guard<std::mutex> lock(mutex);
        job.progress = progress;
        if (status.isNotEmpty())
            job.status = status;
    }

    StemsService::Job StemsService::getJob() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return job;
    }

    void StemsService::cancelJob()
    {
        cancelRequested.store(true);
    }

    bool StemsService::startRuntimeInstall()
    {
        if (!isRuntimeAvailableForPlatform())
            return false;
        if (!beginJob(JobKind::InstallRuntime, {}, "starting"))
            return false;
        juce::Thread::launch([self = shared_from_this()] { self->runRuntimeInstall(); });
        return true;
    }

    bool StemsService::startModelDownload(const juce::String& modelId)
    {
        if (findModel(modelId) == nullptr)
            return false;
        if (!beginJob(JobKind::DownloadModel, modelId, "starting"))
            return false;
        juce::Thread::launch([self = shared_from_this(), modelId] { self->runModelDownload(modelId); });
        return true;
    }

    bool StemsService::startTest(const juce::String& modelId, bool useGpu)
    {
        if (findModel(modelId) == nullptr)
            return false;
        if (!beginJob(JobKind::Test, modelId, "loading"))
            return false;
        juce::Thread::launch([self = shared_from_this(), modelId, useGpu] { self->runTest(modelId, useGpu); });
        return true;
    }

    // Fetches url (or copies localSource, when it exists) to destination through a .part file, and
    // keeps it only if its size and SHA-256 match.
    bool StemsService::download(const juce::String& url, const PackageFile& expected, const juce::File& localSource,
                                const juce::File& destination, const juce::String& label,
                                double progressStart, double progressSpan, juce::String& error)
    {
        if (expected.sha256.length() != 64)
        {
            error = "no pinned SHA-256 for " + expected.fileName;
            return false;
        }

        const auto part = destination.getSiblingFile(destination.getFileName() + ".part");
        part.deleteFile();
        if (!destination.getParentDirectory().createDirectory())
        {
            error = "cannot create " + destination.getParentDirectory().getFullPathName();
            return false;
        }

        std::unique_ptr<juce::InputStream> in;
        if (localSource.existsAsFile())
        {
            in = localSource.createInputStream();
        }
        else
        {
            int statusCode = 0;
            in = juce::URL(url).createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(20000)
                    .withNumRedirectsToFollow(10)
                    .withStatusCode(&statusCode));
            if (in != nullptr && statusCode != 0 && statusCode != 200)
            {
                error = "download failed for " + expected.fileName + " (HTTP " + juce::String(statusCode) + ")";
                return false;
            }
        }
        if (in == nullptr)
        {
            error = "could not reach " + (localSource.existsAsFile() ? localSource.getFullPathName() : url);
            return false;
        }

        const juce::int64 total = expected.sizeBytes > 0 ? expected.sizeBytes : in->getTotalLength();
        juce::int64 done = 0;
        {
            juce::FileOutputStream out(part);
            if (!out.openedOk())
            {
                error = "cannot write " + part.getFullPathName();
                return false;
            }
            juce::HeapBlock<char> buffer(kDownloadChunkBytes);
            while (!in->isExhausted())
            {
                if (cancelled())
                {
                    out.flush();
                    error = "cancelled";
                    break;
                }
                const int got = in->read(buffer.get(), kDownloadChunkBytes);
                if (got <= 0)
                    break;
                if (!out.write(buffer.get(), (size_t) got))
                {
                    error = "could not write " + part.getFullPathName() + " (disk full?)";
                    break;
                }
                done += got;
                if (total > 0)
                    setProgress(progressStart + progressSpan * (double) done / (double) total,
                                label + "  " + formatBytes(done) + " / " + formatBytes(total));
            }
        }
        if (error.isEmpty() && expected.sizeBytes > 0 && done != expected.sizeBytes)
            error = expected.fileName + " is " + formatBytes(done) + ", expected " + formatBytes(expected.sizeBytes);

        if (error.isEmpty())
        {
            setProgress(progressStart + progressSpan, "checking " + expected.fileName);
            const auto actual = juce::SHA256(part).toHexString().toLowerCase();
            if (actual != expected.sha256.toLowerCase())
                error = expected.fileName + " failed its SHA-256 check";
        }
        if (error.isEmpty())
        {
            destination.deleteFile();
            if (!part.moveFileTo(destination))
                error = "could not move " + part.getFileName() + " into place";
        }
        if (error.isNotEmpty())
            part.deleteFile();
        return error.isEmpty();
    }

    void StemsService::runRuntimeInstall()
    {
        const auto& release = pinnedRuntime();
        const auto stemsDir = getStemsDirectory();
        const auto runtimeRoot = stemsDir.getChildFile("runtime");
        const auto staging = runtimeRoot.getChildFile(".staging-" + release.tag);
        const auto downloads = stemsDir.getChildFile("downloads");
        const auto overrideDir = packageOverrideDirectory();

        std::vector<PackageFile> packages = release.packages;
        if (overrideDir.isDirectory())
        {
            const auto sums = readSha256Sums(overrideDir.getChildFile("SHA256SUMS"));
            for (auto& package : packages)
            {
                const auto found = sums.find(package.fileName);
                package.sha256 = found != sums.end() ? found->second : juce::String();
                package.sizeBytes = overrideDir.getChildFile(package.fileName).getSize();
            }
        }

        juce::String error;
        staging.deleteRecursively();
        if (!staging.createDirectory())
            error = "cannot create " + staging.getFullPathName();

        const double span = 0.9 / (double) juce::jmax<size_t>(1, packages.size());
        for (size_t i = 0; error.isEmpty() && i < packages.size(); ++i)
        {
            const auto& package = packages[i];
            const auto zip = downloads.getChildFile(package.fileName);
            const auto local = overrideDir.isDirectory() ? overrideDir.getChildFile(package.fileName) : juce::File();
            if (!download(release.downloadBaseUrl + package.fileName, package, local, zip,
                          "downloading", span * (double) i, span, error))
                break;

            setProgress(span * (double) (i + 1), "unpacking " + package.fileName);
            juce::ZipFile archive(zip);
            const auto result = archive.uncompressTo(staging, true);
            if (result.failed())
                error = "could not unpack " + package.fileName + ": " + result.getErrorMessage();
            zip.deleteFile();
        }

        if (error.isEmpty() && !staging.getChildFile("stems.dll").existsAsFile())
            error = "the runtime package has no stems.dll";

        if (error.isEmpty())
        {
            auto* marker = new juce::DynamicObject();
            marker->setProperty("tag", release.tag);
            juce::Array<juce::var> files;
            for (const auto& package : packages)
            {
                auto* entry = new juce::DynamicObject();
                entry->setProperty("file", package.fileName);
                entry->setProperty("sha256", package.sha256);
                files.add(juce::var(entry));
            }
            marker->setProperty("packages", files);
            marker->setProperty("installed_utc", juce::Time::getCurrentTime().toISO8601(true));
            staging.getChildFile(kInstalledMarker).replaceWithText(juce::JSON::toString(juce::var(marker)));

            setProgress(0.97, "installing");
            const auto finalDir = getRuntimeDirectory();
            if (finalDir.exists() && !finalDir.deleteRecursively())
                error = "an earlier copy of " + release.tag + " is still in use; restart your DAW and install again";
            else if (!staging.moveFileTo(finalDir))
                error = "could not move the runtime into " + finalDir.getFullPathName();
        }

        if (error.isNotEmpty())
        {
            staging.deleteRecursively();
            finishJob(false, error);
            return;
        }

        // Older versions go now if they are not loaded, otherwise next session.
        juce::StringArray stale;
        for (const auto& dir : runtimeRoot.findChildFiles(juce::File::findDirectories, false))
            if (dir.getFileName() != release.tag && !dir.deleteRecursively())
                stale.add(dir.getFullPathName());
        if (!stale.isEmpty())
            stemsDir.getChildFile(kPendingRemovalFile).appendText(stale.joinIntoString("\n") + "\n");

        finishJob(true, "installed stems.cpp " + release.tag
            + (overrideDir.isDirectory() ? " from " + overrideDir.getFullPathName() : juce::String()));
    }

    void StemsService::runModelDownload(const juce::String& modelId)
    {
        const auto* model = findModel(modelId);
        juce::String error;
        const auto destination = getModelsDirectory().getChildFile(model->file.fileName);
        if (download(model->url, model->file, {}, destination, "downloading", 0.0, 1.0, error))
            finishJob(true, model->id + " ready");
        else
            finishJob(false, error);
    }

    void StemsService::runTest(const juce::String& modelId, bool useGpu)
    {
        const auto* model = findModel(modelId);
        if (!isRuntimeInstalled())
            return finishJob(false, "install the runtime first");
        if (!isModelInstalled(modelId))
            return finishJob(false, "download " + model->id + " first");

        juce::String loadError;
        if (!ensureEngine(loadError))
            return finishJob(false, loadError);
        const auto* api = engine->api;

        stems_context_config_v1 config {};
        config.size = sizeof config;
        api->context_config_init(&config);
        const auto modelPath = getModelsDirectory().getChildFile(model->file.fileName).getFullPathName();
        config.model_path = modelPath.toRawUTF8();
        config.device = useGpu ? nullptr : "cpu";

        stems_error_v1 err {};
        err.size = sizeof err;
        api->error_init(&err);

        setProgress(-1.0, "loading " + model->id + (useGpu ? " on the GPU" : " on the CPU"));
        const double start = juce::Time::getMillisecondCounterHiRes();
        stems_context* context = nullptr;
        if (api->context_create(&config, &context, &err) != STEMS_STATUS_OK_V1)
            return finishJob(false, juce::String::fromUTF8(err.message));

        stems_model_info_v1 info {};
        info.size = sizeof info;
        api->model_info_init(&info);
        api->model_info(context, &info, &err);
        const juce::String backend = info.backend != nullptr ? juce::String::fromUTF8(info.backend) : juce::String("?");

        const int sampleRate = 44100, seconds = 3;
        const auto audio = makeTestAudio(sampleRate, seconds);
        stems_request_v1 request {};
        request.size = sizeof request;
        api->request_init(&request);
        request.input.samples = audio.data();
        request.input.n_samples = (uint64_t) sampleRate * (uint64_t) seconds;
        request.input.n_channels = 2;
        request.input.sample_rate = (uint32_t) sampleRate;
        request.input.layout = STEMS_AUDIO_PLANAR_V1;
        request.should_cancel = CallbackBridge::shouldCancel;
        request.callback_user = this;

        stems_result_v1 result {};
        result.size = sizeof result;
        api->result_init(&result);

        setProgress(-1.0, "separating on " + backend);
        const auto status = api->separate(context, &request, &result, &err);
        bool finite = true;
        if (status == STEMS_STATUS_OK_V1)
            for (uint64_t i = 0; i < (uint64_t) result.n_sources * result.n_channels * result.n_samples; ++i)
                if (!std::isfinite(result.samples[i])) { finite = false; break; }
        const uint32_t sources = result.n_sources;
        api->result_free(&result);
        api->context_destroy(context);
        const double seconds_taken = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;

        if (status == STEMS_STATUS_CANCELLED_V1)
            return finishJob(false, "cancelled");
        if (status != STEMS_STATUS_OK_V1)
            return finishJob(false, juce::String::fromUTF8(err.message));
        if (!finite)
            return finishJob(false, model->id + " on " + backend + " produced invalid audio");
        finishJob(true, model->id + " on " + backend + ": " + juce::String((int) sources)
            + " stems in " + juce::String(seconds_taken, 1) + " s");
    }

    // --- separation -----------------------------------------------------------------------------

    bool StemsService::ensureEngine(juce::String& error)
    {
        const auto library = getRuntimeDirectory().getChildFile("stems.dll");
        if (engine != nullptr && engine->library == library)
            return true;
        const auto* api = loadApi(library, error);
        if (api == nullptr)
            return false;
        engine = std::make_unique<Engine>();
        engine->library = library;
        engine->api = api;
        return true;
    }

    bool StemsService::startSeparation(const juce::String& modelId, bool useGpu,
                                       const juce::File& source, const juce::File& outputDirectory)
    {
        if (findModel(modelId) == nullptr)
            return false;
        if (!beginJob(JobKind::Separate, modelId, "loading " + modelId))
            return false;
        juce::Thread::launch([self = shared_from_this(), modelId, useGpu, source, outputDirectory]
        {
            self->runSeparation(modelId, useGpu, source, outputDirectory);
        });
        return true;
    }

    void StemsService::runSeparation(const juce::String& modelId, bool useGpu, const juce::File& source,
                                     const juce::File& outputDirectory)
    {
        const auto* model = findModel(modelId);
        if (!isRuntimeInstalled())
            return finishJob(false, "install the stem separator runtime in settings first");
        if (!isModelInstalled(modelId))
            return finishJob(false, "download " + modelId + " in settings first");

        // The source, planar: samples[channel * n + i].
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels == 0)
            return finishJob(false, "could not read " + source.getFileName());
        const int channels = (int) reader->numChannels;
        const int length = (int) reader->lengthInSamples;
        const double sampleRate = reader->sampleRate;
        juce::AudioBuffer<float> input(channels, length);
        reader->read(&input, 0, length, 0, true, true);
        reader.reset();
        std::vector<float> planar((size_t) channels * (size_t) length);
        for (int c = 0; c < channels; ++c)
            std::copy(input.getReadPointer(c), input.getReadPointer(c) + length,
                      planar.begin() + (std::ptrdiff_t) c * length);

        juce::String loadError;
        if (!ensureEngine(loadError))
            return finishJob(false, loadError);
        const auto* api = engine->api;

        stems_context_config_v1 config {};
        config.size = sizeof config;
        api->context_config_init(&config);
        const auto modelPath = getModelsDirectory().getChildFile(model->file.fileName).getFullPathName();
        config.model_path = modelPath.toRawUTF8();
        config.device = useGpu ? nullptr : "cpu";

        stems_error_v1 err {};
        err.size = sizeof err;
        api->error_init(&err);

        stems_context* context = nullptr;
        if (api->context_create(&config, &context, &err) != STEMS_STATUS_OK_V1)
            return finishJob(false, juce::String::fromUTF8(err.message));

        stems_model_info_v1 info {};
        info.size = sizeof info;
        api->model_info_init(&info);
        api->model_info(context, &info, &err);
        juce::StringArray names;
        for (uint32_t s = 0; s < info.n_sources; ++s)
            names.add(info.source_names[s] != nullptr ? juce::String::fromUTF8(info.source_names[s])
                                                      : "stem " + juce::String((int) s + 1));

        stems_request_v1 request {};
        request.size = sizeof request;
        api->request_init(&request);
        request.input.samples = planar.data();
        request.input.n_samples = (uint64_t) length;
        request.input.n_channels = (uint32_t) channels;
        request.input.sample_rate = (uint32_t) juce::roundToInt(sampleRate);
        request.input.layout = STEMS_AUDIO_PLANAR_V1;
        request.on_progress = CallbackBridge::onProgress;
        request.should_cancel = CallbackBridge::shouldCancel;
        request.callback_user = this;

        stems_result_v1 result {};
        result.size = sizeof result;
        api->result_init(&result);

        setProgress(0.0, "separating");
        const auto status = api->separate(context, &request, &result, &err);
        const juce::String separateError = juce::String::fromUTF8(err.message);
        api->context_destroy(context);   // frees the weights now; nothing stays in VRAM
        if (status == STEMS_STATUS_CANCELLED_V1)
        {
            api->result_free(&result);
            return finishJob(false, "cancelled");
        }
        if (status != STEMS_STATUS_OK_V1)
        {
            api->result_free(&result);
            return finishJob(false, separateError);
        }

        // Stems can each exceed full scale where the mix did not. Scale them all by one factor, as
        // demucs' --clip-mode rescale does, so they still add up to the mix.
        const size_t total = (size_t) result.n_sources * result.n_channels * (size_t) result.n_samples;
        float peak = 0.0f;
        for (size_t i = 0; i < total; ++i)
            peak = juce::jmax(peak, std::abs(result.samples[i]));
        const float gain = peak > 0.999f ? 0.999f / peak : 1.0f;

        // The popup may have closed during the run: it cancels and deletes its session, which must
        // not come back as a folder of stems nobody asked to keep.
        if (cancelled() || !outputDirectory.isDirectory())
        {
            api->result_free(&result);
            return finishJob(false, "cancelled");
        }

        setProgress(1.0, "writing stems");
        const auto modelDir = outputDirectory.getChildFile(modelId);
        modelDir.deleteRecursively();
        juce::Array<juce::File> files;
        juce::String writeError;
        if (!modelDir.createDirectory())
            writeError = "cannot create " + modelDir.getFullPathName();
        for (uint32_t s = 0; writeError.isEmpty() && s < result.n_sources; ++s)
        {
            juce::AudioBuffer<float> stem((int) result.n_channels, (int) result.n_samples);
            for (uint32_t c = 0; c < result.n_channels; ++c)
            {
                const float* src = result.samples + ((size_t) s * result.n_channels + c) * (size_t) result.n_samples;
                juce::FloatVectorOperations::multiply(stem.getWritePointer((int) c), src, gain, (int) result.n_samples);
            }
            const auto file = modelDir.getChildFile(juce::File::createLegalFileName(names[(int) s]) + ".wav");
            juce::WavAudioFormat wav;
            auto stream = std::make_unique<juce::FileOutputStream>(file);
            std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(
                stream.get(), (double) result.sample_rate, result.n_channels, 24, {}, 0));
            if (writer != nullptr)
                stream.release();   // the writer owns it now; on failure it stays ours to delete
            if (writer == nullptr || !writer->writeFromAudioSampleBuffer(stem, 0, stem.getNumSamples()))
                writeError = "could not write " + file.getFileName();
            writer.reset();
            files.add(file);
        }
        api->result_free(&result);
        if (writeError.isNotEmpty())
        {
            modelDir.deleteRecursively();
            return finishJob(false, writeError);
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            job.stemNames = names;
            job.stemFiles = files;
        }
        finishJob(true, modelId + ": " + juce::String(names.size()) + " stems");
    }

    // --- sessions --------------------------------------------------------------------------------

    juce::File StemsService::createSessionDirectory()
    {
        const auto session = getStemsDirectory().getChildFile("session")
                                 .getChildFile(juce::Uuid().toString().substring(0, 12));
        session.createDirectory();
        return session;
    }

    void StemsService::deleteSessionDirectory(const juce::File& session)
    {
        // Only ever inside our own session folder.
        if (session.isAChildOf(getStemsDirectory().getChildFile("session")))
            session.deleteRecursively();
    }

    void StemsService::sweepStaleSessions()
    {
        const auto cutoff = juce::Time::getCurrentTime() - juce::RelativeTime::days(1.0);
        const auto root = getStemsDirectory().getChildFile("session");
        for (const auto& dir : root.findChildFiles(juce::File::findDirectories, false))
            if (dir.getLastModificationTime() < cutoff)
                dir.deleteRecursively();
    }

    // --- removal ---------------------------------------------------------------------------------

    void StemsService::removeRuntime()
    {
        const auto stemsDir = getStemsDirectory();
        const auto runtimeRoot = stemsDir.getChildFile("runtime");
        // Drop the marker first, so a half-deleted folder never looks installed.
        getRuntimeDirectory().getChildFile(kInstalledMarker).deleteFile();
        juce::StringArray stale;
        for (const auto& dir : runtimeRoot.findChildFiles(juce::File::findDirectories, false))
            if (!dir.deleteRecursively())
                stale.add(dir.getFullPathName());
        if (!stale.isEmpty())
            stemsDir.getChildFile(kPendingRemovalFile).appendText(stale.joinIntoString("\n") + "\n");
    }

    void StemsService::removeModel(const juce::String& modelId)
    {
        if (const auto* model = findModel(modelId))
        {
            const auto file = getModelsDirectory().getChildFile(model->file.fileName);
            file.deleteFile();
            file.getSiblingFile(file.getFileName() + ".part").deleteFile();
        }
    }

    void StemsService::deletePendingRemovals()
    {
        const auto listFile = getStemsDirectory().getChildFile(kPendingRemovalFile);
        if (!listFile.existsAsFile())
            return;
        juce::StringArray lines, remaining;
        lines.addLines(listFile.loadFileAsString());
        const auto stemsPath = getStemsDirectory().getFullPathName();
        for (const auto& line : lines)
        {
            const juce::File dir(line.trim());
            // Only ever delete inside our own stems folder.
            if (line.trim().isEmpty() || !dir.isAChildOf(juce::File(stemsPath)))
                continue;
            if (dir.exists() && !dir.deleteRecursively())
                remaining.add(dir.getFullPathName());
        }
        if (remaining.isEmpty())
            listFile.deleteFile();
        else
            listFile.replaceWithText(remaining.joinIntoString("\n") + "\n");
    }
}
