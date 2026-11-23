// SPDX-License-Identifier: AGPL-3.0-only
// Drive real processor callbacks explicitly so source switches occur before playback confirmation.
#include <JuceHeader.h>
#include "../../Source/PluginEditor.h"
#include <cmath>
#include <iostream>

namespace {
int failures = 0, checks = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
}

struct PlaybackStateTest
{
    static int run(int argc, char** argv)
    {
       #if !JUCE_DEBUG
        std::cerr << "Playback regression requires Debug storage isolation" << std::endl;
        return 2;
       #endif
        if (argc != 2) return 2;
        const juce::File root(argv[1]);
        if (juce::SystemStats::getEnvironmentVariable("GARY4JUCE_STORAGE_TEST_DIRECTORY", {}) != root.getFullPathName())
            return 2; // The Debug storage override must isolate editor preferences and audio.
        juce::ScopedJuceInitialiser_GUI gui;
        if (root.createDirectory().failed()) return 2;
        const auto wave = root.getChildFile("playback.wav");
        wave.deleteFile();
        {
            juce::AudioBuffer<float> samples(2, 96000);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < samples.getNumSamples(); ++i)
                    samples.setSample(ch, i, .1f * std::sin(i * .03f));
            juce::WavAudioFormat format;
            auto stream = wave.createOutputStream();
            std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 48000, 2, 16, {}, 0));
            if (!writer || !writer->writeFromAudioSampleBuffer(samples, 0, samples.getNumSamples())) return 2;
        }
        Gary4juceAudioProcessor processor;
        processor.stopHealthChecks();
        processor.prepareToPlay(48000, 512);
        processor.setUsingLocalhost(false);
        Gary4juceAudioProcessorEditor editor(processor);
        editor.stopTimer();
        processor.stopHealthChecks();
        editor.hasOutputAudio = true;
        editor.outputAudioFile = wave;
        editor.totalAudioDuration = 2.0;
        if (!wave.copyFileTo(editor.getGaryBufferFile())) return 2;
        editor.savedSamples = 96000;

        const auto beginPendingOutput = [&]
        {
            editor.releaseStemPlayback();
            editor.stopInputPlayback();
            editor.stopOutputPlayback();
            editor.playOutputAudio();
            check(editor.isOutputPlaybackPending && !editor.isPlayingOutput,
                  "output waits for a rendered block before showing as playing");
        };
        const auto render = [&]
        {
            juce::AudioBuffer<float> block(2, 512);
            block.clear();
            juce::MidiBuffer midi;
            processor.processBlock(block, midi);
            editor.checkPlaybackStatus();
        };
        const auto outputWasCancelled = [&]
        {
            return !editor.isOutputPlaybackPending && !editor.isPlayingOutput
                && !editor.hasShownPlaybackPendingHint && editor.outputPlaybackRequestTimeMs == 0
                && editor.playOutputButton.getTooltip() != "waiting for host audio callback";
        };

        beginPendingOutput();
        render();
        check(editor.isPlayingOutput && !editor.isOutputPlaybackPending, "render confirms normal output playback");

        beginPendingOutput();
        editor.toggleStemPlayback(wave);
        check(outputWasCancelled(), "stem preview cancels the pending output request immediately");
        render();
        check(editor.activePlaybackSource == Gary4juceAudioProcessorEditor::PlaybackSource::Stem
                  && editor.stemPlaybackRunning && !editor.isPlayingOutput && editor.currentPlaybackPosition == 0.0,
              "a stem callback cannot activate the output button or cursor");

        beginPendingOutput();
        editor.playInputAudio();
        check(outputWasCancelled(), "saved-buffer playback cancels the pending output request");
        render();
        check(editor.isPlayingInput && !editor.isPlayingOutput, "a saved-buffer callback leaves output stopped");

        beginPendingOutput();
        editor.seekInputToPosition(.25);
        check(outputWasCancelled() && editor.isPausedInput && !processor.getIsPlayingOutput(),
              "seeking the saved buffer cancels pending output without starting playback");

        beginPendingOutput();
        editor.seekStem(wave, .5);
        check(outputWasCancelled() && !processor.getIsPlayingOutput(), "seeking a stem cancels pending output");
        editor.checkPlaybackStatus();
        check(!editor.isPlayingOutput && editor.currentPlaybackPosition == 0.0, "seeking a stem leaves output cursor at zero");

        beginPendingOutput();
        render();
        check(editor.isPlayingOutput && !editor.isOutputPlaybackPending, "output can start again after changing sources");
        editor.stopOutputPlayback();
        std::cout << checks << " checks, " << failures << " failures" << std::endl;
        return failures ? 1 : 0;
    }
};
int main(int argc, char** argv) { return PlaybackStateTest::run(argc, argv); }
