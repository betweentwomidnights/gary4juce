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

    A model that splits a stem further (DrumSep, for "drums") separates either
    the popup's audio or that stem of a separation already made here: a "from"
    menu beside the model menu picks which, and "split kit" on a drums stem
    goes straight there.
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
    friend struct StemsModelsTest;

public:
    // What the dialog needs from the editor: the shared output player, and dragging into the DAW.
    struct Host
    {
        std::function<void(const juce::File&)> togglePlayback;          // play, pause or resume a stem
        std::function<void(const juce::File&)> stopPlayback;            // and back to its start
        std::function<void(const juce::File&, double)> seek;            // seconds
        std::function<juce::File()> activeStem;                         // loaded in the player, or {}
        std::function<bool()> isPlaying;                                // the active stem is playing
        std::function<double(const juce::File&)> position;              // each stem keeps its own
        std::function<void(const juce::File&, const juce::String&)> drag; // stem file, stem name
        std::function<bool()> isGenerating;
        std::function<void()> releasePlayback;                          // before the session goes
    };

    // writeSource puts the audio to separate (the output, or the recording buffer) into a WAV file
    // in the popup's session; sourceTitle titles the popup ("output", "recording buffer").
    StemsDialog(std::shared_ptr<stems::StemsService> service, juce::PropertiesFile& preferences, Host host,
                const std::function<bool(const juce::File&)>& writeSource, const juce::String& sourceTitle);
    ~StemsDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class StemCell;
    struct Separation
    {
        juce::StringArray names;
        juce::Array<juce::File> files;
        int order = 0;                 // later separations are higher
    };

    void timerCallback() override;
    void rebuildCells();
    void rebuildInputs();
    void separate();
    juce::String currentModelId() const;
    // A model's separation of the popup's audio is keyed by its id; a split of another model's
    // stem by "<model><<parent>", e.g. "mdx23c_drumsep<htdemucs".
    juce::String currentKey() const;
    bool currentModelSplits() const;
    const stems::ModelInfo* splitterFor(const juce::String& stemName) const;   // installed, or null
    void splitStem(const juce::String& stemName);
    juce::String emptyCellHint() const;
    void layoutWindow();
    void updateControls();

    std::shared_ptr<stems::StemsService> service;
    juce::PropertiesFile& preferences;
    Host host;

    juce::File session;
    juce::File source;                 // the audio being separated, as it was when the popup opened
    bool sourceReady = false;

    juce::Label title;
    CustomComboBox modelBox;
    juce::StringArray modelIds;        // modelBox item i + 1
    // What a splitting model separates: the popup's audio (""), or the stem of a parent model's
    // separation. Shown only for such a model.
    CustomComboBox inputBox;
    juce::StringArray inputParents;    // inputBox item i + 1
    juce::String inputParent;
    bool inputChosen = false;          // picked by the user or by split kit, not defaulted
    juce::String sourceName;
    CustomButton separateButton { "separate" };
    std::vector<std::unique_ptr<StemCell>> cells;
    juce::Label status, hint;

    std::map<juce::String, Separation> separations;   // by currentKey(), for this popup only
    int separationCount = 0;
    bool separating = false;
    juce::String separatingKey;
    double targetProgress = 0.0, shownProgress = 0.0;
    bool loadingModel = false;

public:
    // Min/max per pixel column, averaged across channels, as the output waveform draws them.
    static std::vector<std::pair<float, float>> computePeaks(const juce::AudioBuffer<float>& audio, int width);
    static std::vector<std::pair<float, float>> computeFilePeaks(const juce::File& file, int width);
    static void drawPeaks(juce::Graphics& g, juce::Rectangle<int> area,
                          const std::vector<std::pair<float, float>>& peaks, float opacity);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemsDialog)
};
