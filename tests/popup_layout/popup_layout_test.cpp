// SPDX-License-Identifier: AGPL-3.0-only
// Offline layout/lifetime checks. Uses isolated storage; no runtime or model download is needed.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <JuceHeader.h>
#include "../../Source/PluginEditor.h"
#include <iostream>

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::cout << "FAIL: " << message << '\n'; }
}
void snapshot(juce::Component& component, const juce::File& file)
{
    juce::Image image(juce::Image::ARGB, component.getWidth(), component.getHeight(), true,
                      juce::SoftwareImageType());
    { juce::Graphics g(image); component.paintEntireComponent(g, true); }
    file.deleteFile();
    juce::FileOutputStream stream(file);
    juce::PNGImageFormat().writeImageToStream(image, stream);
}
void dispatch()
{
    // The plugin library disables JUCE modal loops; pump the Windows queue for async close callbacks.
    const auto deadline = GetTickCount64() + 30;
    do
    {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(1);
    } while (GetTickCount64() < deadline);
}
}

struct PopupLayoutTest { static int run(const juce::File&); };
int PopupLayoutTest::run(const juce::File& root)
{
    if (juce::SystemStats::getEnvironmentVariable("GARY4JUCE_STORAGE_TEST_DIRECTORY", {})
        != root.getFullPathName()) return 2;
    root.createDirectory();
    Gary4juceAudioProcessor processor;
    processor.prepareToPlay(48000, 512);
    juce::File finalSession;
    {
        Gary4juceAudioProcessorEditor editor(processor);
        editor.stopTimer();
        editor.stopAllBackgroundOperations();
        editor.setVisible(true);
        juce::AudioBuffer<float> tone(1, 4800);
        tone.clear();
        const auto fixture = root.getChildFile("source.wav");
        check(editor.writeAudioBufferToFileSafely(tone, 48000, fixture), "WAV fixture created");
        editor.hasOutputAudio = true;
        editor.outputAudioFile = fixture;
        editor.currentTab = Gary4juceAudioProcessorEditor::ModelTab::Jerry;
        editor.jerrySubTab = Gary4juceAudioProcessorEditor::JerrySubTab::SA3;
        editor.sa3Lanes.open = true;
        editor.setEditorLayoutMode(Gary4juceAudioProcessorEditor::EditorLayoutMode::Compact);
        editor.showStemsDialog(false);
        auto* panel = editor.stemsPanel.get();
        check(panel != nullptr && panel->getParentComponent() == &editor && !panel->isOnDesktop()
              && !panel->isCurrentlyModal(), "stems is an ordinary editor child, with no desktop/modal window");
        if (panel == nullptr) return 1;
        const auto session = panel->session;
        check(editor.getWidth() == 880 && panel->getX() >= 400, "compact attaches on right at 880 pixels");
        check(editor.sa3Lanes.open && !editor.sa3LanePanelShown(), "stems borrows automation dock without clearing its state");
        editor.showStemsDialog(false);
        check(editor.stemsPanel.get() == panel && panel->session == session, "reopening preserves results and session");
        editor.showStemsDialog(true);
        check(editor.stemsPanel.get() == panel, "another source cannot silently discard the open stems");

        // Supply menu entries directly: layout should not depend on downloading hundreds of MB.
        panel->modelIds = { "htdemucs", "htdemucs_6s", "mdx23c_drumsep" };
        panel->modelBox.clear(juce::dontSendNotification);
        for (int i = 0; i < panel->modelIds.size(); ++i)
            panel->modelBox.addItem(panel->modelIds[i], i + 1);
        panel->modelBox.setSelectedId(2, juce::sendNotificationSync);
        check(panel->cellsContent.getNumChildComponents() == 6
              && panel->cellsViewport.getVerticalScrollBar().isVisible(), "six stems fit through scrolling");
        panel->timerCallback();
        check(editor.getWidth() == 880, "six-stem model does not grow editor width");
        snapshot(editor, root.getChildFile("compact-stems.png"));
        panel->cellsViewport.setViewPosition(0, 10000);
        auto* last = panel->cellsContent.getChildComponent(5);
        check(last != nullptr && panel->cellsViewport.getViewArea().intersects(last->getBounds()),
              "last stem is reachable by scrolling");
        panel->modelBox.setSelectedId(3, juce::sendNotificationSync);
        check(panel->inputBox.isVisible() && panel->inputBox.getWidth() > 300,
              "split-model source menu has its own full-width row");
        snapshot(editor, root.getChildFile("compact-drumsep.png"));
        editor.isGenerating = true;
        panel->timerCallback();
        check(!panel->separateButton.isEnabled(), "generation prevents starting a separation");
        editor.isGenerating = false;
        panel->timerCallback();
        check(panel->separateButton.isEnabled(), "separation re-enables after generation");

        editor.setEditorLayoutMode(Gary4juceAudioProcessorEditor::EditorLayoutMode::Wide);
        check(editor.getWidth() == 1040 && panel->getX() >= 480 && panel->getRight() <= 1040,
              "wide stems uses the existing model column at 1040 pixels");
        check(!editor.jerryTabButton.isVisible(), "covered model controls are excluded from keyboard navigation");
        snapshot(editor, root.getChildFile("wide-stems.png"));
        editor.switchToTab(Gary4juceAudioProcessorEditor::ModelTab::Terry);
        editor.setEditorLayoutMode(Gary4juceAudioProcessorEditor::EditorLayoutMode::Compact);
        check(editor.terryUI->isVisible() && !editor.sa3UI->isVisible(),
              "changing tabs behind a wide stems panel restores only the selected model");
        editor.switchToTab(Gary4juceAudioProcessorEditor::ModelTab::Jerry);
        check(editor.jerryTabButton.isVisible() && editor.stemsPanel.get() == panel,
              "layout switches restore controls and retain the same stems session");
        editor.migrateGaryDataDirectory(root.getChildFile("other-storage"));
        check(panel->session == session && session.exists(), "storage migration cannot strand an open stems session");
        editor.toggleStemPlayback(panel->source);
        check(editor.activePlaybackSource == Gary4juceAudioProcessorEditor::PlaybackSource::Stem,
              "stem preview uses the shared audio player");
        panel->closeButton.onClick();
        check(editor.stemsPanel != nullptr, "close button waits until its callback unwinds");
        dispatch();
        check(editor.stemsPanel == nullptr
              && editor.activePlaybackSource == Gary4juceAudioProcessorEditor::PlaybackSource::None,
              "close callback releases playback and destroys its panel");
        check(!session.exists() && editor.sa3Lanes.open && editor.sa3LanePanelShown(),
              "closing deletes temporary stems and restores automation");
        editor.sa3Lanes.open = false;
        editor.updateSA3LanePanel();
        check(editor.getWidth() == 400, "compact returns to normal width after both panels close");

        editor.showStorageSettings();
        auto* storage = dynamic_cast<juce::AlertWindow*>(editor.editorModalWindows.back().getComponent());
        check(storage != nullptr && storage->findColour(juce::AlertWindow::backgroundColourId)
              == Theme::Colors::Background, "real storage popup uses Gary background");
        if (storage != nullptr)
        {
            check(storage->getComboBoxComponent("draggedAudioFormat") != nullptr
                  && storage->getComboBoxComponent("draggedAudioFormat")->findColour(juce::ComboBox::backgroundColourId)
                     == juce::Colour(0xff151515), "popup controls use the dark palette");
            snapshot(*storage, root.getChildFile("storage-popup.png"));
            storage->exitModalState(0);
            dispatch();
        }
        editor.showSupportDialog("generation failed", "the backend could not finish this render", "test details", false);
        auto* support = dynamic_cast<juce::AlertWindow*>(editor.editorModalWindows.back().getComponent());
        check(support != nullptr && support->findColour(juce::AlertWindow::outlineColourId)
              == Theme::Colors::AccentRed, "real error popup uses Gary outline");
        if (support != nullptr)
        {
            snapshot(*support, root.getChildFile("error-popup.png"));
            support->exitModalState(0);
            dispatch();
        }
        editor.showStemsDialog(false);
        check(editor.stemsPanel != nullptr, "stems can reopen after closing");
        finalSession = editor.stemsPanel->session;
    }
    check(!finalSession.exists(), "editor teardown cleans the remaining stems session");
    dispatch();  // allow independently owned UI to finish its close callbacks after editor teardown
    processor.releaseResources();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    juce::ScopedJuceInitialiser_GUI gui;
    return PopupLayoutTest::run(juce::File(argv[1]));
}
