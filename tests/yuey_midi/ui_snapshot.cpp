// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

// Draws the yuey tab to PNGs, offscreen, in the states the MIDI import adds. A look at the layout
// without opening the plugin: the two audio-source states must look as they always did, and the
// midi states are where the new slots appear. Usage: ui_snapshot.exe OUTPUT_FOLDER

#include "../../Source/Components/Yuey/YueyUI.h"

#include <iostream>

namespace
{
void snapshot(YueyUI& ui, const juce::File& folder, const juce::String& name, int width, int height)
{
    ui.setBounds(0, 0, width, height);
    ui.setVisible(true);
    ui.resized();
    const auto image = ui.createComponentSnapshot(ui.getLocalBounds(), true, 1.0f);
    const auto file = folder.getChildFile(name + ".png");
    file.deleteFile();
    juce::FileOutputStream out(file);
    juce::PNGImageFormat().writeImageToStream(image, out);
    std::cout << "wrote " << file.getFullPathName().toStdString() << "\n";
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
    ui.setAudioSourceAvailability(true, true);
    ui.setRemixInstrumental(true);
    ui.setRemixPrompt("warm lofi piano, brushed drums");

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
    return 0;
}
