// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

// Draws the yuey tab to PNGs, offscreen, in the states the MIDI import adds. A look at the layout
// without opening the plugin: the two audio-source states must look as they always did, and the
// midi states are where the new slots appear. Usage: ui_snapshot.exe OUTPUT_FOLDER

#include "../../Source/Components/Yuey/YueyUI.h"

#include <iostream>

namespace
{
void snapshot(YueyUI&, const juce::File&, const juce::String&, int, int);
int failures = 0, checks = 0;
void check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { ++failures; std::cout << "FAIL: " << message << "\n"; }
}

void tempoCases(YueyUI& ui, const juce::File& folder)
{
    auto* wheel = dynamic_cast<BpmControl*>(ui.findChildWithID("yuey-bpm-control"));
    auto* display = dynamic_cast<juce::Label*>(ui.findChildWithID("yuey-host-bpm"));
    check(wheel != nullptr && display != nullptr, "tempo components exist");
    if (wheel == nullptr || display == nullptr) return;
    int changes = 0;
    ui.onPlanningChanged = [&] { ++changes; };
    const YueyUI::SubTab tabs[] = { YueyUI::SubTab::Create, YueyUI::SubTab::Remix, YueyUI::SubTab::Continue };
    ui.setIsStandalone(true);
    ui.setContinuationMethod(YueyUI::ContinuationMethod::Score);
    ui.setBpm(120);
    for (int i = 0; i < 3; ++i)
    {
        ui.setCurrentSubTab(tabs[i]);
        check(wheel->isVisible() && wheel->isEnabled() && !display->isVisible(), "standalone tabs show editable tempo");
        wheel->setValue(121 + i);
        check(ui.getBpm() == 121 + i, "standalone wheel updates the render tempo");
    }
    check(changes == 3, "standalone tempo changes notify planning once each");
    ui.setCurrentSubTab(YueyUI::SubTab::Create);
    snapshot(ui, folder, "9-create-standalone", 400, 560);
    ui.setIsStandalone(false);
    ui.setBpm(123.45);
    for (int i = 0; i < 3; ++i)
    {
        ui.setCurrentSubTab(tabs[i]);
        check(!wheel->isVisible() && display->isVisible() && !display->isEditable(), "hosted tabs show read-only project tempo");
        check(juce::approximatelyEqual(ui.getBpm(), 123.45) && display->getText() == "123.45 bpm",
              "fractional project tempo is displayed and retained");
        snapshot(ui, folder, "host-" + juce::String(i), 400, 560);
    }
    check(changes == 3, "host synchronization does not send standalone edit callbacks");
    ui.setBpm(320);
    check(ui.getBpm() == 320 && display->getText() == "320 bpm", "host tempo is not clamped to the wheel range");
    ui.setIsStandalone(true);
    ui.setBpm(120);
    ui.setContinuationMethod(YueyUI::ContinuationMethod::Audio);
    check(wheel->isVisible() && !wheel->isEnabled(), "audio continuation shows source-owned tempo");
    ui.setMidiSourceSelected(true);
    check(wheel->isEnabled(), "MIDI continuation restores editable tempo even with stored audio method");
    ui.onPlanningChanged = nullptr;
    ui.setMidiSourceSelected(false);
    ui.setContinuationMethod(YueyUI::ContinuationMethod::Score);
}

void snapshot(YueyUI& ui, const juce::File& folder, const juce::String& name, int width, int height)
{
    ui.setBounds(0, 0, width, height);
    ui.setVisible(true);
    ui.resized();
    juce::Image image(juce::Image::ARGB, width, height, true);
    {
        juce::Graphics graphics(image);
        graphics.fillAll(juce::Colour(0xff151515));  // the editor's model panel
        ui.paintEntireComponent(graphics, true);
    }
    const auto file = folder.getChildFile(name + ".png");
    file.deleteFile();
    juce::FileOutputStream out(file);
    juce::PNGImageFormat().writeImageToStream(image, out);
    std::cout << "wrote " << file.getFullPathName().toStdString() << "\n";
}

void layoutCases(const juce::File& folder)
{
    YueyUI ui;
    ui.setIsStandalone(true);
    ui.setVisible(true);
    const int width = 650;
    int changes = 0;
    ui.onLayoutHeightChanged = [&]
    {
        ++changes;
        ui.setSize(width, ui.getPreferredHeight(width));
    };
    ui.setSize(width, ui.getPreferredHeight(width));
    juce::Viewport* viewport = nullptr;
    for (auto* child : ui.getChildren())
        if (auto* candidate = dynamic_cast<juce::Viewport*>(child)) viewport = candidate;
    check(viewport != nullptr, "Yuey viewport exists");
    if (viewport == nullptr) return;
    const auto fits = [&]
    {
        check(!viewport->getVerticalScrollBar().isVisible(), "wide preferred height fits every visible row");
        check(viewport->getViewPositionY() == 0, "fitting content starts at the prompt");
        check(viewport->getViewedComponent()->getWidth() == viewport->getWidth(),
              "fitting content uses full width without a scrollbar gutter");
    };
    ui.setCurrentSubTab(YueyUI::SubTab::Continue);
    ui.setRemixInstrumental(false);
    fits();
    snapshot(ui, folder, "wide-continue-score", width, ui.getHeight());
    ui.setRemixInstrumental(true);
    fits();
    ui.setContinuationMethod(YueyUI::ContinuationMethod::Audio);
    fits();
    ui.setMidiSourceSelected(true);
    fits();
    snapshot(ui, folder, "wide-continue-midi-empty", width, ui.getHeight());
    const int emptyHeight = ui.getHeight();
    ui.showMidiLoaded(YueyUI::MidiLane::Melody, juce::File("C:/Music/melody.alc"), "10 bars, 20s at 120 BPM");
    fits();
    check(ui.getHeight() > emptyHeight, "loading a MIDI summary grows the wide panel");
    ui.showMidiInvalid(YueyUI::MidiLane::Chords, juce::File("C:/Music/chords.mid"),
                       "this file has pitch bends, which can't be written into the score; remove them and export again");
    ui.setMidiNote("the melody and chords have different lengths; crop them to the same length in your DAW");
    fits();
    snapshot(ui, folder, "wide-continue-midi-error", width, ui.getHeight());
    check(changes > 5, "layout changes notify the editor as visible rows change");
    ui.onLayoutHeightChanged = nullptr;
    ui.setSize(400, 330);
    check(viewport->getVerticalScrollBar().isVisible(), "short compact panels retain scrolling when needed");
    viewport->setViewPosition(0, 100);
    ui.setSize(width, ui.getPreferredHeight(width));
    fits();
    // Yuey must leave the shared panel colour intact, including its viewport.
    juce::Image image(juce::Image::ARGB, width, ui.getHeight(), true);
    const juce::Colour panel(0xff151515);
    {
        juce::Graphics graphics(image);
        graphics.fillAll(panel);
        ui.paintEntireComponent(graphics, true);
    }
    check(image.getPixelAt(1, 1) == panel, "Yuey chrome preserves the shared grey background");
    check(image.getPixelAt(viewport->getX() + 1, viewport->getY() + 1) == panel,
          "Yuey viewport preserves the shared grey background");
    const auto file = folder.getChildFile("wide-panel-background.png");
    file.deleteFile();
    juce::FileOutputStream out(file);
    juce::PNGImageFormat().writeImageToStream(image, out);
}
}  // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File folder = argc > 1 ? juce::File(argv[1])
                                       : juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("yuey-snapshots");
    folder.createDirectory();

    const int width = 400, height = 560;  // the compact editor's width; the tab scrolls past this
    YueyUI ui;
    ui.setIsStandalone(true);
    ui.setAudioSourceAvailability(true, true);
    ui.setRemixInstrumental(true);
    ui.setRemixPrompt("warm lofi piano, brushed drums");

    // Exercise the same file-drop callbacks used by OS drags, including .alc.
    YueyMidiSlot slot("melody");
    int drops = 0;
    slot.onFile = [&](const juce::File& file) { if (file.getFileName() == "test.ALC") ++drops; };
    check(slot.isInterestedInFileDrag({ "C:/Music/test.ALC" }), "slot accepts case-insensitive Live Clip extension");
    check(slot.isInterestedInFileDrag({ "C:/Music/test.mid" })
          && slot.isInterestedInFileDrag({ "C:/Music/test.midi" }), "standard MIDI drops remain accepted");
    check(!slot.isInterestedInFileDrag({ "C:/Music/test.wav" })
          && !slot.isInterestedInFileDrag({ "C:/Music/a.alc", "C:/Music/b.mid" }), "unsupported and multiple-file drops are refused");
    slot.filesDropped({ "C:/Music/test.ALC" }, 0, 0);
    slot.filesDropped({ "C:/Music/test.wav" }, 0, 0);
    check(drops == 1, "accepted Live Clip drop reaches the import callback exactly once");

    // The audio sources, which must look as they did before MIDI existed.
    ui.setCurrentSubTab(YueyUI::SubTab::Remix);
    snapshot(ui, folder, "1-remix-audio", width, height);
    ui.setCurrentSubTab(YueyUI::SubTab::Continue);
    snapshot(ui, folder, "2-continue-audio", width, height);

    // MIDI, remix: nothing loaded, then one file, then a file the importer refused plus a note.
    ui.setCurrentSubTab(YueyUI::SubTab::Remix);
    ui.setMidiSourceSelected(true);
    snapshot(ui, folder, "3-remix-midi-empty", width, height);

    ui.showMidiLoaded(YueyUI::MidiLane::Melody, juce::File("C:/Users/me/Music/melody_test_short.mid"), juce::String::fromUTF8("10 bars \xc2\xb7 20s at 120 BPM"));
    snapshot(ui, folder, "4-remix-midi-melody", width, height);

    ui.showMidiInvalid(YueyUI::MidiLane::Chords, juce::File("C:/Users/me/Music/pads_with_bends.mid"),
                       "this file has pitch bends, which can't be written into the score; remove them and export again");
    snapshot(ui, folder, "5-remix-midi-error", width, height);

    ui.showMidiLoaded(YueyUI::MidiLane::Chords, juce::File("C:/Users/me/Music/chords_test_short.mid"), juce::String::fromUTF8("8 bars \xc2\xb7 16s at 120 BPM"));
    ui.setMidiNote("the melody is 10 bars and the chords are 8. trim them to the same length in your DAW and export again");
    snapshot(ui, folder, "6-remix-midi-mismatch", width, height);

    // MIDI, continue: no method row, the length choice stays.
    ui.setMidiNote(juce::String());
    ui.showMidiLoaded(YueyUI::MidiLane::Chords, juce::File("C:/Users/me/Music/chords_test_short.mid"), juce::String::fromUTF8("10 bars \xc2\xb7 20s at 120 BPM"));
    ui.setCurrentSubTab(YueyUI::SubTab::Continue);
    snapshot(ui, folder, "7-continue-midi", width, height);

    // Back to an audio source: everything returns, and the midi files stay put underneath.
    ui.setMidiSourceSelected(false);
    snapshot(ui, folder, "8-continue-audio-again", width, height);
    ui.setCurrentSubTab(YueyUI::SubTab::Remix);
    ui.setMidiSourceSelected(true);
    ui.showMidiLoaded(YueyUI::MidiLane::Melody, juce::File("C:/Music/Instrument Melody.alc"),
                      juce::String::fromUTF8("10 bars \xc2\xb7 20s at 120 BPM"));
    snapshot(ui, folder, "alc-loaded", width, height);
    ui.setMidiSourceSelected(false);
    tempoCases(ui, folder);
    layoutCases(folder);
    std::cout << checks << " UI checks, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
