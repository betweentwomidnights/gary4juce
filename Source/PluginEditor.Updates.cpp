/*
  ==============================================================================
    Update-related PluginEditor method definitions.
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    static constexpr auto kDefaultUpdateManifestUrl =
        "https://betweentwomidnights.github.io/gary4juce/updates/gary4juce/stable-mac.json";
    static constexpr auto kUpdateManifestOverrideEnv = "GARY4JUCE_UPDATE_MANIFEST_URL";
    static constexpr auto kUpdatePrefsFolder = "gary4juce";
    static constexpr auto kUpdatePrefsAppName = "gary4juce-updater";
    static constexpr auto kUpdatePrefsSuffix = ".settings";
    static constexpr auto kLastUpdateCheckKey = "lastUpdateCheckMs";
    static constexpr auto kSkippedUpdateVersionKey = "skippedUpdateVersion";
    static constexpr auto kStartupUpdateCheckEnabledKey = "startupUpdateCheckEnabled";
    static constexpr juce::int64 kAutoUpdateCheckCooldownMs = 12LL * 60LL * 60LL * 1000LL;
    static constexpr int kUpdateCheckTimeoutMs = 4000;

    struct SemanticVersion
    {
        int major = 0;
        int minor = 0;
        int patch = 0;
        juce::String prerelease;
        bool valid = false;

        static SemanticVersion parse(const juce::String& value)
        {
            SemanticVersion version;
            auto normalized = value.trim().trimCharactersAtStart("vV");

            if (normalized.isEmpty())
                return version;

            const auto hyphenIndex = normalized.indexOfChar('-');
            const auto core = hyphenIndex >= 0 ? normalized.substring(0, hyphenIndex) : normalized;
            version.prerelease = hyphenIndex >= 0 ? normalized.substring(hyphenIndex + 1).trim() : juce::String();

            auto parts = juce::StringArray::fromTokens(core, ".", "");
            parts.removeEmptyStrings();

            if (parts.isEmpty() || parts.size() > 3)
                return version;

            int components[3] = { 0, 0, 0 };

            for (int i = 0; i < parts.size(); ++i)
            {
                const auto part = parts[i].trim();

                if (part.isEmpty())
                    return version;

                for (int j = 0; j < part.length(); ++j)
                    if (!juce::CharacterFunctions::isDigit(part[j]))
                        return version;

                components[i] = part.getIntValue();
            }

            version.major = components[0];
            version.minor = components[1];
            version.patch = components[2];
            version.valid = true;
            return version;
        }

        int compareTo(const SemanticVersion& other) const noexcept
        {
            if (major != other.major) return major < other.major ? -1 : 1;
            if (minor != other.minor) return minor < other.minor ? -1 : 1;
            if (patch != other.patch) return patch < other.patch ? -1 : 1;

            if (prerelease.isEmpty() && other.prerelease.isNotEmpty()) return 1;
            if (prerelease.isNotEmpty() && other.prerelease.isEmpty()) return -1;

            return prerelease.compareNatural(other.prerelease);
        }
    };

    struct PluginUpdateManifest
    {
        juce::String channel = "stable";
        juce::String latestVersion;
        juce::String downloadUrl;
        juce::String publishedAt;
        juce::StringArray notes;
    };

    struct PluginUpdateCheckResult
    {
        juce::String currentVersion;
        juce::String manifestUrl;
        juce::String channel = "stable";
        juce::String latestVersion;
        juce::String downloadUrl;
        juce::String publishedAt;
        juce::StringArray notes;
        juce::String error;
        juce::int64 checkedAtMs = 0;
        bool updateAvailable = false;
        bool shouldPrompt = false;
    };

    class UpdatePromptOptions final : public juce::Component
    {
    public:
        explicit UpdatePromptOptions(const bool checkOnStartup)
        {
            startupUpdateToggle.setButtonText("check for updates on startup");
            startupUpdateToggle.setToggleState(checkOnStartup, juce::dontSendNotification);
            startupUpdateToggle.setMouseCursor(juce::MouseCursor::PointingHandCursor);
            addAndMakeVisible(startupUpdateToggle);

            setSize(320, 24);
        }

        bool shouldCheckOnStartup() const
        {
            return startupUpdateToggle.getToggleState();
        }

        void resized() override
        {
            startupUpdateToggle.setBounds(getLocalBounds());
        }

    private:
        juce::ToggleButton startupUpdateToggle;
    };

    void removeCustomComponent(juce::AlertWindow& alertWindow, juce::Component& component)
    {
        for (int i = alertWindow.getNumCustomComponents(); --i >= 0;)
        {
            if (alertWindow.getCustomComponent(i) == &component)
            {
                alertWindow.removeCustomComponent(i);
                return;
            }
        }
    }

    juce::String normalizeVersionString(const juce::String& value)
    {
        return value.trim().trimCharactersAtStart("vV");
    }

    bool versionsMatch(const juce::String& left, const juce::String& right)
    {
        const auto lhs = SemanticVersion::parse(left);
        const auto rhs = SemanticVersion::parse(right);

        if (lhs.valid && rhs.valid)
            return lhs.compareTo(rhs) == 0;

        return normalizeVersionString(left).equalsIgnoreCase(normalizeVersionString(right));
    }

    bool isVersionNewer(const juce::String& latest, const juce::String& current)
    {
        const auto latestVersion = SemanticVersion::parse(latest);
        const auto currentVersion = SemanticVersion::parse(current);

        if (latestVersion.valid && currentVersion.valid)
            return latestVersion.compareTo(currentVersion) > 0;

        return normalizeVersionString(latest).compareNatural(normalizeVersionString(current), false) > 0;
    }

    juce::String configuredUpdateManifestUrl()
    {
        const auto overrideUrl = juce::SystemStats::getEnvironmentVariable(kUpdateManifestOverrideEnv, {}).trim();

        if (overrideUrl.isNotEmpty())
            return overrideUrl;

        return juce::String(kDefaultUpdateManifestUrl);
    }

    bool readUpdateManifest(const juce::String& jsonText,
                            PluginUpdateManifest& manifest,
                            juce::String& error)
    {
        const auto manifestVar = juce::JSON::parse(jsonText);
        auto* manifestObj = manifestVar.getDynamicObject();

        if (manifestObj == nullptr)
        {
            error = "invalid update manifest json";
            return false;
        }

        manifest.channel = manifestObj->getProperty("channel").toString().trim();
        if (manifest.channel.isEmpty())
            manifest.channel = "stable";

        manifest.latestVersion = manifestObj->getProperty("latest_version").toString().trim();
        manifest.downloadUrl = manifestObj->getProperty("download_url").toString().trim();
        manifest.publishedAt = manifestObj->getProperty("published_at").toString().trim();

        if (auto* notesArray = manifestObj->getProperty("notes").getArray())
        {
            for (const auto& note : *notesArray)
            {
                const auto trimmed = note.toString().trim();
                if (trimmed.isNotEmpty())
                    manifest.notes.add(trimmed);
            }
        }
        else
        {
            const auto notesText = manifestObj->getProperty("notes").toString().trim();
            if (notesText.isNotEmpty())
                manifest.notes.add(notesText);
        }

        if (manifest.latestVersion.isEmpty())
        {
            error = "update manifest missing latest_version";
            return false;
        }

        return true;
    }

    PluginUpdateCheckResult runPluginUpdateCheck(const juce::String& currentVersion,
                                                 const juce::String& skippedVersion,
                                                 const bool includeSkippedVersion)
    {
        PluginUpdateCheckResult result;
        result.currentVersion = currentVersion.trim();
        result.manifestUrl = configuredUpdateManifestUrl();
        result.checkedAtMs = juce::Time::getCurrentTime().toMilliseconds();

        int statusCode = 0;
        juce::StringPairArray responseHeaders;

        juce::URL manifestUrl(result.manifestUrl);
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(kUpdateCheckTimeoutMs)
            .withExtraHeaders("Accept: application/json\r\nUser-Agent: gary4juce/" + result.currentVersion + "\r\n")
            .withResponseHeaders(&responseHeaders)
            .withStatusCode(&statusCode);

        std::unique_ptr<juce::InputStream> stream(manifestUrl.createInputStream(options));

        if (stream == nullptr)
        {
            result.error = "update manifest unavailable";
            return result;
        }

        const auto responseText = stream->readEntireStreamAsString();

        if (statusCode >= 400)
        {
            result.error = "update manifest returned http " + juce::String(statusCode);
            return result;
        }

        PluginUpdateManifest manifest;
        if (!readUpdateManifest(responseText, manifest, result.error))
            return result;

        result.channel = manifest.channel;
        result.latestVersion = manifest.latestVersion;
        result.downloadUrl = manifest.downloadUrl;
        result.publishedAt = manifest.publishedAt;
        result.notes = manifest.notes;
        result.updateAvailable = isVersionNewer(manifest.latestVersion, currentVersion);

        if (result.updateAvailable && manifest.downloadUrl.isEmpty())
        {
            result.error = "update manifest missing download_url";
            return result;
        }

        result.shouldPrompt = result.updateAvailable
            && (includeSkippedVersion || !versionsMatch(skippedVersion, manifest.latestVersion));

        return result;
    }

    juce::String buildUpdatePromptMessage(const juce::String& currentVersion,
                                          const juce::String& latestVersion,
                                          const juce::StringArray& notes,
                                          const juce::String& channel,
                                          const juce::String& publishedAt)
    {
        juce::String message;
        message << "you're on v" << currentVersion << ".\n";
        message << "v" << latestVersion << " is available";

        if (channel.isNotEmpty() && !channel.equalsIgnoreCase("stable"))
            message << " (" << channel << ")";

        message << ".";

        if (publishedAt.isNotEmpty())
            message << "\npublished: " << publishedAt;

        if (!notes.isEmpty())
        {
            message << "\n\nwhat's new:";

            const auto maxNotes = juce::jmin(6, notes.size());
            for (int i = 0; i < maxNotes; ++i)
            {
                auto line = notes[i].trim();
                if (line.length() > 120)
                    line = line.substring(0, 117).trimEnd() + "...";

                if (line.isNotEmpty())
                    message << "\n- " << line;
            }
        }

        message << "\n\nclose your DAW before replacing the plugin files.";
        return message;
    }
}

void Gary4juceAudioProcessorEditor::maybeAutoCheckForUpdates()
{
    if (hasCheckedForUpdatesThisEditorSession)
        return;

    hasCheckedForUpdatesThisEditorSession = true;

    if (!getStartupUpdateCheckEnabled())
    {
        DBG("Skipping auto update check; startup checks disabled");
        return;
    }

    const auto nowMs = juce::Time::getCurrentTime().toMilliseconds();
    const auto lastCheckMs = getLastUpdateCheckTimeMs();

    if (lastCheckMs > 0 && (nowMs - lastCheckMs) < kAutoUpdateCheckCooldownMs)
    {
        DBG("Skipping auto update check; last check was " + juce::String((nowMs - lastCheckMs) / 1000) + "s ago");
        return;
    }

    checkForPluginUpdates(false);
}

void Gary4juceAudioProcessorEditor::checkForPluginUpdates(bool manual, bool includeSkippedVersion)
{
    if (updateCheckInFlight.exchange(true))
    {
        if (manual)
            showStatusMessage("update check already running", 2000);
        return;
    }

    if (manual)
    {
        checkUpdatesButton.setEnabled(false);
        showStatusMessage("checking for updates...", 2500);
    }

    const auto currentVersion = juce::String(ProjectInfo::versionString);
    const auto skippedVersion = includeSkippedVersion ? juce::String() : getSkippedUpdateVersion();
    const std::weak_ptr<std::atomic<bool>> asyncAlive = editorAsyncAlive;
    auto* editor = this;

    juce::Thread::launch([asyncAlive, editor, currentVersion, skippedVersion, manual, includeSkippedVersion]()
    {
        const auto result = runPluginUpdateCheck(currentVersion, skippedVersion, includeSkippedVersion);

        juce::MessageManager::callAsync([asyncAlive, editor, result, manual]()
        {
            const auto alive = asyncAlive.lock();
            if (alive == nullptr || !alive->load(std::memory_order_acquire))
                return;

            editor->updateCheckInFlight.store(false);
            editor->checkUpdatesButton.setEnabled(true);
            editor->setLastUpdateCheckTimeMs(result.checkedAtMs > 0
                                                   ? result.checkedAtMs
                                                   : juce::Time::getCurrentTime().toMilliseconds());

            if (result.error.isNotEmpty())
            {
                DBG("Plugin update check failed: " + result.error);
                if (manual)
                    editor->showStatusMessage("update check failed", 3500);
                return;
            }

            if (!result.updateAvailable)
            {
                if (manual)
                {
                    editor->setSkippedUpdateVersion({});
                    editor->showStatusMessage("you're already on the latest gary4juce", 3000);
                }
                return;
            }

            if (!result.shouldPrompt && !manual)
                return;

            if (editor->updatePromptVisible)
            {
                if (manual)
                    editor->showStatusMessage("update prompt already open", 2500);
                return;
            }

            if (editor->isGenerating)
            {
                editor->deferredUpdatePromptReady = true;
                editor->deferredUpdateVersion = result.latestVersion;
                editor->deferredUpdateDownloadUrl = result.downloadUrl;
                editor->deferredUpdateChannel = result.channel;
                editor->deferredUpdatePublishedAt = result.publishedAt;
                editor->deferredUpdateNotes = result.notes;

                if (manual)
                    editor->showStatusMessage("update ready after this render", 3500);

                return;
            }

            if (auto* modalManager = juce::ModalComponentManager::getInstanceWithoutCreating())
            {
                if (modalManager->getNumModalComponents() > 0)
                {
                    editor->deferredUpdatePromptReady = true;
                    editor->deferredUpdateVersion = result.latestVersion;
                    editor->deferredUpdateDownloadUrl = result.downloadUrl;
                    editor->deferredUpdateChannel = result.channel;
                    editor->deferredUpdatePublishedAt = result.publishedAt;
                    editor->deferredUpdateNotes = result.notes;

                    if (manual)
                        editor->showStatusMessage("update ready when this dialog closes", 3500);

                    return;
                }
            }

            editor->showPluginUpdatePrompt(result.latestVersion,
                                           result.downloadUrl,
                                           result.notes,
                                           result.channel,
                                           result.publishedAt);
        });
    });
}

void Gary4juceAudioProcessorEditor::maybeShowDeferredUpdatePrompt()
{
    if (!deferredUpdatePromptReady || updatePromptVisible || isGenerating)
        return;

    if (auto* modalManager = juce::ModalComponentManager::getInstanceWithoutCreating())
        if (modalManager->getNumModalComponents() > 0)
            return;

    if (deferredUpdateDownloadUrl.isEmpty() || deferredUpdateVersion.isEmpty())
    {
        clearDeferredUpdatePrompt();
        return;
    }

    const auto latestVersion = deferredUpdateVersion;
    const auto downloadUrl = deferredUpdateDownloadUrl;
    const auto notes = deferredUpdateNotes;
    const auto channel = deferredUpdateChannel;
    const auto publishedAt = deferredUpdatePublishedAt;

    clearDeferredUpdatePrompt();
    showPluginUpdatePrompt(latestVersion, downloadUrl, notes, channel, publishedAt);
}

void Gary4juceAudioProcessorEditor::showPluginUpdatePrompt(const juce::String& latestVersion,
                                                           const juce::String& downloadUrl,
                                                           const juce::StringArray& notes,
                                                           const juce::String& channel,
                                                           const juce::String& publishedAt)
{
    if (updatePromptVisible || latestVersion.isEmpty() || downloadUrl.isEmpty())
        return;

    updatePromptVisible = true;

    auto* alertWindow = new juce::AlertWindow("update available",
                                              buildUpdatePromptMessage(ProjectInfo::versionString,
                                                                       latestVersion,
                                                                       notes,
                                                                       channel,
                                                                       publishedAt),
                                              juce::MessageBoxIconType::InfoIcon,
                                              this);

    alertWindow->addButton("download update", 1);
    alertWindow->addButton("not now", 0);
    alertWindow->addButton("skip this version", 2);

    auto* optionsComponent = new UpdatePromptOptions(getStartupUpdateCheckEnabled());
    alertWindow->addCustomComponent(optionsComponent);

    juce::Component::SafePointer<Gary4juceAudioProcessorEditor> safeThis(this);
    alertWindow->enterModalState(true,
        juce::ModalCallbackFunction::create([safeThis, alertWindow, optionsComponent, latestVersion, downloadUrl](int result)
        {
            const auto checkOnStartup = optionsComponent == nullptr || optionsComponent->shouldCheckOnStartup();

            if (optionsComponent != nullptr)
            {
                removeCustomComponent(*alertWindow, *optionsComponent);
                delete optionsComponent;
            }

            std::unique_ptr<juce::AlertWindow> cleanup(alertWindow);

            if (safeThis == nullptr)
                return;

            safeThis->updatePromptVisible = false;
            safeThis->setStartupUpdateCheckEnabled(checkOnStartup);

            if (result == 1)
            {
                safeThis->setSkippedUpdateVersion({});
                if (!juce::URL(downloadUrl).launchInDefaultBrowser())
                    safeThis->showStatusMessage("couldn't open the release page", 3500);
            }
            else if (result == 2)
            {
                safeThis->setSkippedUpdateVersion(latestVersion);
                safeThis->showStatusMessage("will skip v" + latestVersion + " for now", 3000);
            }
        }));
}

void Gary4juceAudioProcessorEditor::clearDeferredUpdatePrompt()
{
    deferredUpdatePromptReady = false;
    deferredUpdateVersion.clear();
    deferredUpdateDownloadUrl.clear();
    deferredUpdateChannel = "stable";
    deferredUpdatePublishedAt.clear();
    deferredUpdateNotes.clear();
}

juce::PropertiesFile& Gary4juceAudioProcessorEditor::getUpdatePreferences()
{
    if (updatePreferences == nullptr)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = kUpdatePrefsAppName;
        options.filenameSuffix = kUpdatePrefsSuffix;
        options.folderName = kUpdatePrefsFolder;
        options.osxLibrarySubFolder = "Application Support";
        options.commonToAllUsers = false;
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        updatePreferences = std::make_unique<juce::PropertiesFile>(options);
    }

    return *updatePreferences;
}

juce::String Gary4juceAudioProcessorEditor::getSkippedUpdateVersion()
{
    return getUpdatePreferences().getValue(kSkippedUpdateVersionKey).trim();
}

void Gary4juceAudioProcessorEditor::setSkippedUpdateVersion(const juce::String& version)
{
    getUpdatePreferences().setValue(kSkippedUpdateVersionKey, version.trim());
    getUpdatePreferences().saveIfNeeded();
}

juce::int64 Gary4juceAudioProcessorEditor::getLastUpdateCheckTimeMs()
{
    return getUpdatePreferences().getValue(kLastUpdateCheckKey).getLargeIntValue();
}

void Gary4juceAudioProcessorEditor::setLastUpdateCheckTimeMs(juce::int64 timeMs)
{
    getUpdatePreferences().setValue(kLastUpdateCheckKey, juce::String(timeMs));
    getUpdatePreferences().saveIfNeeded();
}

bool Gary4juceAudioProcessorEditor::getStartupUpdateCheckEnabled()
{
    return getUpdatePreferences().getBoolValue(kStartupUpdateCheckEnabledKey, true);
}

void Gary4juceAudioProcessorEditor::setStartupUpdateCheckEnabled(bool enabled)
{
    getUpdatePreferences().setValue(kStartupUpdateCheckEnabledKey, enabled);
    getUpdatePreferences().saveIfNeeded();
}
