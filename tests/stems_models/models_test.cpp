// SPDX-License-Identifier: AGPL-3.0-only
// The pinned stems.cpp runtime and every catalog model, through the real StemsService, settings
// panel and stems popup: install from the published release, separate with each model, check each
// writes the stems the catalog names in its order, and split an htdemucs drums stem with DrumSep.
// Writes PNGs of the panels into the test folder.
#include <JuceHeader.h>
#include "../../Source/PluginEditor.h"
#include "../../Source/Components/Stems/StemsDialog.h"
#include "../../Source/Components/Stems/StemsSettings.h"
#include "../../Source/Stems/StemsRuntime.h"
#include <iostream>

namespace {
int failures = 0, checks = 0;
void check(bool ok, const juce::String& message) {
    ++checks;
    if (!ok) { ++failures; std::cout << "FAIL: " << message << '\n'; }
}
void writeImage(juce::Component& component, const juce::File& file) {
    component.setVisible(true);  // logical visibility only; no desktop window
    juce::Image image(juce::Image::ARGB, component.getWidth(), component.getHeight(), true, juce::SoftwareImageType());
    {
        juce::Graphics graphics(image);
        component.paintEntireComponent(graphics, true);
    }
    file.deleteFile();
    juce::FileOutputStream stream(file);
    juce::PNGImageFormat().writeImageToStream(image, stream);
}
bool waitFor(stems::StemsService& service, double seconds) {
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + seconds * 1000.0;
    while (service.getJob().running && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::Thread::sleep(50);
    if (!service.getJob().running)
        return true;
    service.cancelJob();
    return false;
}
juce::int64 frames(const juce::File& file) {
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    return reader != nullptr ? reader->lengthInSamples : -1;
}
// The visible "split kit" buttons on the popup's stem cells (StemCell is private to the dialog).
std::vector<juce::Button*> splitButtons(juce::Component& dialog) {
    std::vector<juce::Button*> found;
    for (auto* cell : dialog.getChildren())
        for (auto* child : cell->getChildren())
            if (auto* button = dynamic_cast<juce::Button*>(child); button != nullptr && button->isVisible()
                && button->getButtonText() == "split kit")
                found.push_back(button);
    return found;
}
}

struct StemsModelsTest { static int run(int argc, char** argv); };
int StemsModelsTest::run(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: models_test test-folder input.wav [seed-models-folder]\n"
                     "  test-folder is a throwaway gary data folder; input.wav is a short stereo mix.\n"
                     "  seed-models-folder holds catalog GGUFs to copy in instead of downloading them;\n"
                     "  each is still checked against its pinned SHA-256. uvr_denoise_lite always downloads.\n";
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File root(argv[1]), input(argv[2]);
    const juce::File seed = argc > 3 ? juce::File(argv[3]) : juce::File();
    if (!input.existsAsFile()) { std::cerr << "no " << input.getFullPathName() << '\n'; return 2; }
    root.createDirectory();
    const auto data = root.getChildFile("gary-data");

    auto service = std::make_shared<stems::StemsService>(data);
    const auto& release = stems::pinnedRuntime();

    // An earlier gary4juce's runtime: the panel offers to update it, and installing replaces it.
    const auto old = service->getStemsDirectory().getChildFile("runtime").getChildFile("v0.1.2");
    if (!service->isRuntimeInstalled()) {
        old.createDirectory();
        old.getChildFile("stems.dll").replaceWithText("stand-in");
        old.getChildFile("installed.json").replaceWithText("{}");
        check(service->installedOlderRuntimeTag() == "v0.1.2", "older runtime detected");
        juce::PropertiesFile::Options options;
        options.applicationName = "stems-models-test";
        juce::PropertiesFile prefs(root.getChildFile("update.settings"), options);
        StemsSettings panel(service, prefs);
        check(panel.runtimeButton.getButtonText() == "update", "panel offers update over v0.1.2");
        check(panel.runtimeStatus.getText().contains("v0.1.2 installed. update to " + release.tag),
              "panel names both runtimes: " + panel.runtimeStatus.getText());
        writeImage(panel, root.getChildFile("settings-update.png"));

        check(service->startRuntimeInstall(), "runtime install starts");
        check(waitFor(*service, 600.0), "runtime install finishes");
        const auto job = service->getJob();
        std::cout << "runtime: " << job.status << job.error << '\n';
        check(job.succeeded && service->isRuntimeInstalled(), "pinned runtime installs from the release");
        check(!old.exists() && service->installedOlderRuntimeTag().isEmpty(), "update removes v0.1.2");
    }

    // Models: download the smallest through the service's pinned-hash path; seed the rest.
    const auto models = service->getModelsDirectory();
    models.createDirectory();
    for (const auto& model : stems::modelCatalog()) {
        const auto file = models.getChildFile(model.file.fileName);
        if (model.id != "uvr_denoise_lite" && !service->isModelInstalled(model.id) && seed.isDirectory())
            seed.getChildFile(model.file.fileName).copyFileTo(file);
        if (!service->isModelInstalled(model.id)) {
            check(service->startModelDownload(model.id), model.id + " download starts");
            check(waitFor(*service, 1800.0), model.id + " download finishes");
            const auto job = service->getJob();
            check(job.succeeded, model.id + " downloads: " + job.error);
        }
        check(juce::SHA256(file).toHexString() == model.file.sha256, model.id + " matches its pinned SHA-256");
    }

    // Every model separates through the C ABI wrapper, writing the stems the catalog promises.
    const auto separated = root.getChildFile("separated");
    separated.deleteRecursively();
    separated.createDirectory();
    const auto inputFrames = frames(input);
    for (const auto& model : stems::modelCatalog()) {
        const double start = juce::Time::getMillisecondCounterHiRes();
        check(service->startSeparation(model.id, true, input, separated), model.id + " separation starts");
        check(waitFor(*service, 900.0), model.id + " separation finishes");
        const auto job = service->getJob();
        std::cout << model.id << ": " << (job.succeeded ? job.status : "FAILED " + job.error) << " ("
                  << juce::String((juce::Time::getMillisecondCounterHiRes() - start) / 1000.0, 1) << " s)\n";
        check(job.succeeded, model.id + " separates");
        check(job.stemNames == stems::stemNamesFor(model.id),
              model.id + " stems " + job.stemNames.joinIntoString(", ") + " match the catalog");
        for (const auto& file : job.stemFiles)
            check(frames(file) == inputFrames, model.id + " " + file.getFileName() + " keeps the input length");
    }

    // The popup: sectioned model menu, htdemucs, then split kit on its drums.
    juce::PropertiesFile::Options options;
    options.applicationName = "stems-models-test";
    juce::PropertiesFile prefs(root.getChildFile("popup.settings"), options);
    prefs.setValue(StemsSettings::kModelPreference, "htdemucs");
    StemsDialog::Host host;
    host.togglePlayback = [](const juce::File&) {};
    host.stopPlayback = [](const juce::File&) {};
    host.seek = [](const juce::File&, double) {};
    host.activeStem = [] { return juce::File(); };
    host.isPlaying = [] { return false; };
    host.position = [](const juce::File&) { return 0.0; };
    host.drag = [](const juce::File&, const juce::String&) {};
    host.isGenerating = [] { return false; };
    host.releasePlayback = [] {};
    {
        StemsDialog dialog(service, prefs, host, [&](const juce::File& to) { return input.copyFileTo(to); }, "output");
        const auto finish = [&] {
            const auto deadline = juce::Time::getMillisecondCounterHiRes() + 900000.0;
            while (dialog.separating && juce::Time::getMillisecondCounterHiRes() < deadline) {
                dialog.timerCallback();
                juce::Thread::sleep(30);
            }
            return !dialog.separating;
        };

        juce::StringArray headings;
        for (juce::PopupMenu::MenuItemIterator it(*dialog.modelBox.getRootMenu()); it.next();)
            if (it.getItem().isSectionHeader)
                headings.add(it.getItem().text);
        check(headings == juce::StringArray { "stems", "vocals and instrumental", "noise, echo and reverb", "drum kit" },
              "model menu sections: " + headings.joinIntoString(" | "));
        check(dialog.currentModelId() == "htdemucs" && !dialog.inputBox.isVisible(), "opens on htdemucs, no from menu");

        // Before htdemucs has run, DrumSep can only take the whole output, and says what it is for.
        dialog.modelBox.setSelectedId(dialog.modelIds.indexOf("mdx23c_drumsep") + 1, juce::sendNotificationSync);
        check(dialog.inputBox.isVisible() && dialog.inputParents.size() == 1 && dialog.inputParent.isEmpty(),
              "drumsep alone offers only the output");
        check(dialog.emptyCellHint().startsWith("made for drums"), "drumsep explains its input");
        check(dialog.cells.size() == 6, "drumsep shows six cells before separating");
        writeImage(dialog, root.getChildFile("popup-drumsep-empty.png"));

        dialog.modelBox.setSelectedId(dialog.modelIds.indexOf("htdemucs") + 1, juce::sendNotificationSync);
        dialog.separate();
        check(finish() && dialog.separations.count("htdemucs") == 1, "popup separates with htdemucs");
        check(dialog.cells.size() == 4, "htdemucs shows four cells");
        const auto offered = splitButtons(dialog);
        check(offered.size() == 1 && offered[0]->getTooltip().startsWith("split these drums into kick"),
              "only the drums cell offers split kit");
        auto* split = offered.empty() ? nullptr : offered[0];
        writeImage(dialog, root.getChildFile("popup-htdemucs.png"));

        if (split != nullptr && split->onClick)
            split->onClick();   // what a click runs; triggerClick would need the message loop
        check(dialog.separating && dialog.separatingKey == "mdx23c_drumsep<htdemucs", "split kit splits htdemucs drums");
        check(finish() && dialog.separations.count("mdx23c_drumsep<htdemucs") == 1, "drums split into a kit");
        check(dialog.cells.size() == 6 && dialog.currentKey() == "mdx23c_drumsep<htdemucs", "kit shows six cells");
        check(dialog.inputBox.getText() == "from htdemucs drums", "from menu shows the drums stem: " + dialog.inputBox.getText());
        check(prefs.getValue(StemsSettings::kModelPreference) == "htdemucs", "split kit leaves the remembered model");
        const auto kit = dialog.separations["mdx23c_drumsep<htdemucs"];
        check(kit.files.size() == 6 && kit.files[0].getFullPathName().contains("from-htdemucs"), "kit written beside htdemucs");
        writeImage(dialog, root.getChildFile("popup-kit.png"));

        // The whole output is a separate DrumSep result; the split stays until htdemucs runs again.
        dialog.inputBox.setSelectedId(1, juce::sendNotificationSync);
        check(dialog.currentKey() == "mdx23c_drumsep" && dialog.cells.size() == 6, "from the output is its own view");
        dialog.inputBox.setSelectedId(2, juce::sendNotificationSync);
        check(dialog.currentKey() == "mdx23c_drumsep<htdemucs", "back to the kit without splitting again");
        dialog.modelBox.setSelectedId(dialog.modelIds.indexOf("htdemucs") + 1, juce::sendNotificationSync);
        dialog.separate();
        check(dialog.separations.count("mdx23c_drumsep<htdemucs") == 0, "running htdemucs again drops its kit");
        check(finish(), "htdemucs reruns");
        dialog.modelBox.setSelectedId(dialog.modelIds.indexOf("mdx23c_drumsep") + 1, juce::sendNotificationSync);
        check(dialog.inputParent == "htdemucs", "drumsep defaults to the newest drums stem");
    }

    // The waveforms' stems handles: shown whenever there is audio, dimmed with a pointer to settings
    // until the runtime and a model are installed.
    if (juce::SystemStats::getEnvironmentVariable("GARY4JUCE_STORAGE_TEST_DIRECTORY", {}) != root.getFullPathName()) {
        check(false, "GARY4JUCE_STORAGE_TEST_DIRECTORY must be the test folder for the editor checks");
    } else {
        Gary4juceAudioProcessor processor;
        processor.stopHealthChecks();
        processor.prepareToPlay(48000.0, 512);
        processor.setUsingLocalhost(false);
        Gary4juceAudioProcessorEditor editor(processor);
        editor.stopTimer();
        processor.stopHealthChecks();

        // A data folder holding only a stand-in runtime of the given tag (stems.dll and its marker).
        const auto fakeData = [&](const juce::String& name, const juce::String& tag) {
            const auto dir = root.getChildFile("handle-" + name);
            dir.deleteRecursively();
            if (tag.isNotEmpty()) {
                const auto runtime = dir.getChildFile("stems").getChildFile("runtime").getChildFile(tag);
                runtime.createDirectory();
                runtime.getChildFile("stems.dll").replaceWithText("stand-in");
                runtime.getChildFile("installed.json").replaceWithText("{}");
            }
            return dir;
        };
        const auto handleState = [&](const juce::File& dataDirectory, bool generating) {
            editor.stemsService = std::make_shared<stems::StemsService>(dataDirectory);
            editor.isGenerating = generating;
            editor.updateStemsButtonState();
        };
        auto& handle = editor.stemsButton;
        const auto needsSetup = [&] { return (bool) handle.getProperties()["needsSetup"]; };
        // A click on the handle: true when it opened the stem separator settings window.
        const auto clickOpensSettings = [&] {
            const auto settingsWindows = [] {
                int count = 0;
                for (int i = 0; i < juce::TopLevelWindow::getNumTopLevelWindows(); ++i)
                    if (juce::TopLevelWindow::getTopLevelWindow(i)->getName() == "gary settings > stem separator")
                        ++count;
                return count;
            };
            const int before = settingsWindows();
            if (handle.onClick)
                handle.onClick();
            const bool opened = settingsWindows() > before;
            for (int i = juce::TopLevelWindow::getNumTopLevelWindows(); --i >= 0;)
                if (auto* window = juce::TopLevelWindow::getTopLevelWindow(i); window->getName() == "gary settings > stem separator")
                    delete window;
            return opened;
        };
        editor.hasOutputAudio = true;
        editor.outputAudioFile = input;

        handleState(fakeData("none", {}), false);
        check(handle.isVisible() && handle.isEnabled() && needsSetup(), "handle shows, dimmed and clickable, before setup");
        check(handle.getTooltip() == "click to install the stem separator (also in gary settings > stem separator...)",
              "tooltip offers installing: " + handle.getTooltip());
        writeImage(editor, root.getChildFile("editor-stems-setup.png"));
        check(clickOpensSettings(), "clicking it opens the stem separator settings");

        handleState(fakeData("old", "v0.1.2"), false);
        check(needsSetup() && handle.getTooltip().startsWith("click to update the stem separator runtime"),
              "tooltip offers updating v0.1.2: " + handle.getTooltip());

        handleState(fakeData("nomodel", release.tag), false);
        check(needsSetup() && handle.getTooltip().startsWith("click to download a stem separation model"),
              "tooltip offers downloading a model: " + handle.getTooltip());

        handleState(fakeData("none", {}), true);
        check(handle.isEnabled() && needsSetup(), "setup stays a click away during a generation");

        handleState(data, false);
        check(handle.isVisible() && handle.isEnabled() && !needsSetup()
              && handle.getTooltip() == "separate this output into stems",
              "handle ready once a model is installed: " + handle.getTooltip());
        writeImage(editor, root.getChildFile("editor-stems-enabled.png"));

        handleState(data, true);
        check(!handle.isEnabled() && handle.getTooltip() == "wait for the current generation to finish",
              "handle disabled during a generation");
        check(!clickOpensSettings(), "a ready handle does not open settings");

        editor.isGenerating = false;
        editor.hasOutputAudio = false;
        editor.updateStemsButtonState();
        check(!handle.isVisible(), "no handle without output audio");
        check(!editor.stemsInputButton.isVisible(), "no recording-buffer handle without recorded audio");
        editor.stemsService = nullptr;
    }

    {
        StemsSettings panel(service, prefs);
        check(panel.runtimeStatus.getText() == "stems.cpp " + release.tag + " installed", "panel shows the pinned runtime");
        check(panel.groupHeadings.size() == 4 && panel.modelRows.size() == stems::modelCatalog().size(), "panel lists every model by group");
        check(panel.modelsContent.getHeight() > panel.modelsViewport.getHeight(), "model list scrolls");
        writeImage(panel, root.getChildFile("settings.png"));
    }

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}

int main(int argc, char** argv) { return StemsModelsTest::run(argc, argv); }
