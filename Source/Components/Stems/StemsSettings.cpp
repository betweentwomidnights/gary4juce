// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "StemsSettings.h"
#include "../../Utils/Theme.h"

namespace
{
    constexpr int kMargin = 16;
    constexpr int kRowHeight = 46;
    constexpr int kHeadingHeight = 22;

    void styleLabel(juce::Label& label, juce::FontOptions font, juce::Colour colour)
    {
        label.setFont(font);
        label.setColour(juce::Label::textColourId, colour);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setInterceptsMouseClicks(false, false);
    }

    void styleHeading(juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        styleLabel(label, Theme::Fonts::Header, Theme::Colors::TextPrimary);
    }
}

// One model: pick it, see what it separates into and how big it is, download or delete it.
class StemsSettings::ModelRow : public juce::Component
{
public:
    explicit ModelRow(const stems::ModelInfo& info) : model(info)
    {
        select.setRadioGroupId(1001);
        select.setColour(juce::ToggleButton::tickColourId, Theme::Colors::PrimaryRed);
        select.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff555555));
        select.onClick = [this] { if (onSelect) onSelect(model.id); };
        addAndMakeVisible(select);

        name.setText(model.id, juce::dontSendNotification);
        styleLabel(name, Theme::Fonts::Header, Theme::Colors::TextPrimary);
        addAndMakeVisible(name);

        summary.setText(model.stemsSummary + (model.note.isNotEmpty() ? "  -  " + model.note : juce::String()),
                        juce::dontSendNotification);
        styleLabel(summary, Theme::Fonts::Body, Theme::Colors::TextSecondary);
        addAndMakeVisible(summary);

        size.setText(stems::formatBytes(model.file.sizeBytes), juce::dontSendNotification);
        styleLabel(size, Theme::Fonts::Body, Theme::Colors::TextSecondary);
        size.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(size);

        action.onClick = [this]
        {
            if (installed) { if (onDelete) onDelete(model.id); }
            else if (onDownload) onDownload(model.id);
        };
        addAndMakeVisible(action);
    }

    void update(bool isInstalled, bool isSelected, bool jobRunning, bool isThisJob)
    {
        installed = isInstalled;
        select.setEnabled(installed && !jobRunning);
        select.setToggleState(isSelected && installed, juce::dontSendNotification);
        action.setButtonText(isThisJob ? "..." : installed ? "delete" : "download");
        action.setButtonStyle(installed ? CustomButton::ButtonStyle::Standard : CustomButton::ButtonStyle::Gary);
        action.setEnabled(!jobRunning);
        name.setColour(juce::Label::textColourId,
                       installed ? Theme::Colors::TextPrimary : Theme::Colors::TextSecondary);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(0, 4);
        select.setBounds(area.removeFromLeft(28));
        action.setBounds(area.removeFromRight(86).reduced(0, 4));
        area.removeFromRight(8);
        size.setBounds(area.removeFromRight(70));
        name.setBounds(area.removeFromTop(area.getHeight() / 2));
        summary.setBounds(area);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawHorizontalLine(getHeight() - 1, 0.0f, (float) getWidth());
    }

    std::function<void(const juce::String&)> onSelect, onDownload, onDelete;
    const stems::ModelInfo& model;

private:
    juce::ToggleButton select;
    juce::Label name, summary, size;
    CustomButton action { "download" };
    bool installed = false;
};

StemsSettings::StemsSettings(std::shared_ptr<stems::StemsService> s, juce::PropertiesFile& prefs)
    : service(std::move(s)), preferences(prefs)
{
    title.setText("stem separator", juce::dontSendNotification);
    styleLabel(title, Theme::Fonts::HeaderLarge, Theme::Colors::TextPrimary);
    addAndMakeVisible(title);

    subtitle.setText("splits gary's output into stems on this computer, with stems.cpp. "
                     "nothing is uploaded, and it works with the remote backend too.",
                     juce::dontSendNotification);
    styleLabel(subtitle, Theme::Fonts::Body, Theme::Colors::TextSecondary);
    subtitle.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(subtitle);

    styleHeading(runtimeHeading, "runtime");
    addAndMakeVisible(runtimeHeading);
    styleLabel(runtimeStatus, Theme::Fonts::Body, Theme::Colors::TextSecondary);
    addAndMakeVisible(runtimeStatus);
    runtimeButton.onClick = [this]
    {
        if (service->isRuntimeInstalled())
        {
            service->removeRuntime();
            lastMessage = service->hasPendingRemoval()
                ? "the runtime is in use; it finishes uninstalling when gary4juce next loads"
                : "runtime removed";
            lastMessageIsError = false;
        }
        else
        {
            service->startRuntimeInstall();
        }
        refresh();
    };
    addAndMakeVisible(runtimeButton);

    styleHeading(processingHeading, "processing");
    addAndMakeVisible(processingHeading);
    gpuToggle.setColour(juce::ToggleButton::textColourId, Theme::Colors::TextPrimary);
    gpuToggle.setColour(juce::ToggleButton::tickColourId, Theme::Colors::PrimaryRed);
    gpuToggle.setToggleState(useGpu(), juce::dontSendNotification);
    gpuToggle.onClick = [this]
    {
        preferences.setValue(kUseGpuPreference, gpuToggle.getToggleState());
        preferences.saveIfNeeded();
        refresh();
    };
    addAndMakeVisible(gpuToggle);
    gpuHint.setText("works on NVIDIA, AMD and Intel GPUs. off runs on the CPU: slower, but always available.",
                    juce::dontSendNotification);
    styleLabel(gpuHint, Theme::Fonts::Small, Theme::Colors::TextSecondary);
    addAndMakeVisible(gpuHint);

    styleHeading(modelsHeading, "models");
    addAndMakeVisible(modelsHeading);
    for (const auto& model : stems::modelCatalog())
    {
        auto row = std::make_unique<ModelRow>(model);
        row->onSelect = [this](const juce::String& id) { selectModel(id); };
        row->onDownload = [this](const juce::String& id)
        {
            service->startModelDownload(id);
            refresh();
        };
        row->onDelete = [this](const juce::String& id)
        {
            const auto* info = stems::findModel(id);
            juce::Component::SafePointer<StemsSettings> safeThis(this);
            juce::AlertWindow::showAsync(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::QuestionIcon)
                    .withTitle("delete " + id + "?")
                    .withMessage("this frees " + stems::formatBytes(info->file.sizeBytes)
                                 + ". you can download it again any time.")
                    .withButton("delete")
                    .withButton("cancel")
                    .withAssociatedComponent(this),
                [safeThis, id](int result)
                {
                    if (safeThis == nullptr || result != 1)
                        return;
                    safeThis->service->removeModel(id);
                    safeThis->refresh();
                });
        };
        addAndMakeVisible(*row);
        modelRows.push_back(std::move(row));
    }

    testButton.setButtonStyle(CustomButton::ButtonStyle::Gary);
    testButton.setTooltip("separates three seconds of test audio with the selected model");
    testButton.onClick = [this]
    {
        service->startTest(selectedModelId(), useGpu());
        refresh();
    };
    addAndMakeVisible(testButton);

    cancelButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    cancelButton.onClick = [this] { service->cancelJob(); };
    addAndMakeVisible(cancelButton);

    progressBar.setColour(juce::ProgressBar::foregroundColourId, Theme::Colors::PrimaryRed);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xff2a2a2a));
    progressBar.setTextToDisplay({});
    addAndMakeVisible(progressBar);

    styleLabel(jobStatus, Theme::Fonts::Body, Theme::Colors::TextSecondary);
    addAndMakeVisible(jobStatus);

    styleLabel(storageLabel, Theme::Fonts::Small, Theme::Colors::TextSecondary);
    addAndMakeVisible(storageLabel);

    // The sum of what resized() lays out, top to bottom.
    setSize(560, kMargin + 30 + 36                                // title, subtitle
                 + 8 + kHeadingHeight + 34                        // runtime
                 + 8 + kHeadingHeight + 34 + 18                   // processing
                 + 8 + kHeadingHeight + (int) modelRows.size() * kRowHeight
                 + 12 + 32 + 24                                   // test / progress, status
                 + 8 + 18 + kMargin);                             // storage line
    refresh();
    startTimerHz(10);
}

StemsSettings::~StemsSettings()
{
    stopTimer();
}

bool StemsSettings::useGpu() const
{
    return preferences.getBoolValue(kUseGpuPreference, true);
}

juce::String StemsSettings::selectedModelId() const
{
    const auto stored = preferences.getValue(kModelPreference, stems::defaultModelId());
    if (service->isModelInstalled(stored))
        return stored;
    for (const auto& model : stems::modelCatalog())
        if (service->isModelInstalled(model.id))
            return model.id;
    return stored;
}

void StemsSettings::selectModel(const juce::String& modelId)
{
    preferences.setValue(kModelPreference, modelId);
    preferences.saveIfNeeded();
    refresh();
}

void StemsSettings::timerCallback()
{
    const auto job = service->getJob();
    if (job.running)
    {
        progressValue = job.progress;   // negative shows the spinning bar
        refresh();
    }
    else if (renderedRunning)
    {
        // The panel last drew a running job, which has finished since, possibly before any tick
        // saw it running (a job that fails at once does). Show how it ended.
        lastMessage = job.succeeded ? job.status : job.error;
        lastMessageIsError = !job.succeeded;
        progressValue = 0.0;
        refresh();
    }
}

void StemsSettings::refresh()
{
    const auto job = service->getJob();
    const bool running = job.running;
    renderedRunning = running;
    const bool available = service->isRuntimeAvailableForPlatform();
    const bool runtimeInstalled = service->isRuntimeInstalled();
    const auto& release = stems::pinnedRuntime();

    if (!available)
        runtimeStatus.setText("not available on this platform yet", juce::dontSendNotification);
    else if (runtimeInstalled)
        runtimeStatus.setText("stems.cpp " + release.tag + " installed", juce::dontSendNotification);
    else if (running && job.kind == stems::StemsService::JobKind::InstallRuntime)
        runtimeStatus.setText("installing stems.cpp " + release.tag, juce::dontSendNotification);
    else
        runtimeStatus.setText("not installed. stems.cpp " + release.tag + " for GPU and CPU, about 19 MB",
                              juce::dontSendNotification);

    runtimeButton.setButtonText(runtimeInstalled ? "remove" : "install");
    runtimeButton.setButtonStyle(runtimeInstalled ? CustomButton::ButtonStyle::Standard
                                                  : CustomButton::ButtonStyle::Gary);
    runtimeButton.setEnabled(available && !running);

    gpuToggle.setEnabled(!running);

    const auto selected = selectedModelId();
    for (auto& row : modelRows)
        row->update(service->isModelInstalled(row->model.id), row->model.id == selected, running,
                    running && job.kind == stems::StemsService::JobKind::DownloadModel
                        && job.modelId == row->model.id);

    testButton.setEnabled(!running && runtimeInstalled && service->isModelInstalled(selected));
    testButton.setButtonText(running && job.kind == stems::StemsService::JobKind::Test ? "testing" : "test " + selected);
    cancelButton.setVisible(running && job.kind != stems::StemsService::JobKind::None);
    progressBar.setVisible(running);

    if (running)
    {
        jobStatus.setText(job.status, juce::dontSendNotification);
        jobStatus.setColour(juce::Label::textColourId, Theme::Colors::TextSecondary);
    }
    else
    {
        jobStatus.setText(lastMessage, juce::dontSendNotification);
        jobStatus.setColour(juce::Label::textColourId,
                            lastMessageIsError ? Theme::Colors::PrimaryRed : Theme::Colors::TextPrimary);
    }

    storageLabel.setText("stored in " + service->getStemsDirectory().getFullPathName()
                         + "  (moves with gary's audio storage)", juce::dontSendNotification);
    storageLabel.setTooltip(service->getStemsDirectory().getFullPathName());
}

void StemsSettings::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));
}

void StemsSettings::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    title.setBounds(area.removeFromTop(30));
    subtitle.setBounds(area.removeFromTop(36));

    area.removeFromTop(8);
    runtimeHeading.setBounds(area.removeFromTop(kHeadingHeight));
    {
        auto line = area.removeFromTop(34);
        runtimeButton.setBounds(line.removeFromRight(110).reduced(0, 3));
        runtimeStatus.setBounds(line);
    }

    area.removeFromTop(8);
    processingHeading.setBounds(area.removeFromTop(kHeadingHeight));
    gpuToggle.setBounds(area.removeFromTop(34).removeFromLeft(260));
    gpuHint.setBounds(area.removeFromTop(18));

    area.removeFromTop(8);
    modelsHeading.setBounds(area.removeFromTop(kHeadingHeight));
    for (auto& row : modelRows)
        row->setBounds(area.removeFromTop(kRowHeight));

    area.removeFromTop(12);
    {
        auto line = area.removeFromTop(32);
        testButton.setBounds(line.removeFromRight(170));
        line.removeFromRight(8);
        cancelButton.setBounds(line.removeFromRight(90));
        line.removeFromRight(8);
        progressBar.setBounds(line.reduced(0, 9));
    }
    jobStatus.setBounds(area.removeFromTop(24));
    storageLabel.setBounds(area.removeFromBottom(18));
}
