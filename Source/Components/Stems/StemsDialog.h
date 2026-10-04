// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

/*
  ==============================================================================
    StemsDialog.h

    The popup behind the "stems" button on the output waveform. Pick an
    installed model, press separate, then audition each stem on its own
    waveform and drag the ones you want into the DAW. Playback, seeking,
    progress and drag-out behave like the output waveform's.

    Everything here is ephemeral: the popup copies the output into its own
    session folder when it opens, separates into that folder, and deletes it
    when it closes. Only a stem you drag out is kept, as a copy in
    dragged_audio.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Base/CustomButton.h"
#include "../Base/CustomComboBox.h"
#include "../../Stems/StemsRuntime.h"

#include <map>
#include <memory>

class StemsDialog : public juce::Component,
                    private juce::Timer
{
public:
    // What the dialog needs from the editor: the shared output player, and dragging into the DAW.
    struct Host
    {
        std::function<void(const juce::File&)> togglePlayback;          // play, pause or resume a stem
        std::function<void()> stopPlayback;
        std::function<void(const juce::File&, double)> seek;            // seconds
        std::function<juce::File()> activeStem;                         // loaded in the player, or {}
        std::function<bool()> isPlaying;                                // the active stem is playing
        std::function<double()> position;                               // of the active stem, seconds
        std::function<void(const juce::File&, const juce::String&)> drag; // stem file, stem name
        std::function<bool()> isGenerating;
        std::function<void()> releasePlayback;                          // before the session goes
    };

    StemsDialog(std::shared_ptr<stems::StemsService> service, juce::PropertiesFile& preferences,
                Host host, const juce::File& outputAudio);
    ~StemsDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class StemCell;
    struct Separation
    {
        juce::StringArray names;
        juce::Array<juce::File> files;
    };

    void timerCallback() override;
    void rebuildCells();
    void separate();
    juce::String currentModelId() const;
    void layoutWindow();
    void updateControls();

    std::shared_ptr<stems::StemsService> service;
    juce::PropertiesFile& preferences;
    Host host;

    juce::File session;
    juce::File source;                 // the output, copied into the session
    juce::AudioBuffer<float> sourceAudio;
    double sourceSampleRate = 44100.0;

    juce::Label title;
    CustomComboBox modelBox;
    juce::StringArray modelIds;        // modelBox item i + 1
    CustomButton separateButton { "separate" };
    std::vector<std::unique_ptr<StemCell>> cells;
    juce::Label status, hint;

    std::map<juce::String, Separation> separations;   // by model, for this popup only
    bool separating = false;
    juce::String separatingModel;
    double targetProgress = 0.0, shownProgress = 0.0;
    bool loadingModel = false;
    std::vector<std::pair<float, float>> sourcePeaks;   // for the current cell width
    int sourcePeaksWidth = 0;

public:
    // Min/max per pixel column, averaged across channels, as the output waveform draws them.
    static std::vector<std::pair<float, float>> computePeaks(const juce::AudioBuffer<float>& audio, int width);
    static void drawPeaks(juce::Graphics& g, juce::Rectangle<int> area,
                          const std::vector<std::pair<float, float>>& peaks, float opacity);
    const std::vector<std::pair<float, float>>& getSourcePeaks(int width);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemsDialog)
};
