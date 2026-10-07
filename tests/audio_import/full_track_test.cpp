// SPDX-License-Identifier: AGPL-3.0-only
// Integration checks against the real plugin processor, editor and stems wrapper.
#include <JuceHeader.h>
#include "../../Source/PluginEditor.h"
#include "../../Source/Components/Stems/StemsDialog.h"
#include "../../Source/Stems/StemsRuntime.h"
#include <iostream>

namespace {
int failures = 0, checks = 0;
void check(bool ok, const char* message) {
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
    juce::FileOutputStream stream(file);
    stream.setPosition(0);
    stream.truncate();
    juce::PNGImageFormat().writeImageToStream(image, stream);
}
}

struct FullTrackAudioTest { static int run(int argc, char** argv); };
int FullTrackAudioTest::run(int argc, char** argv) {
    if (argc < 4) { std::cerr << "usage: full_track_test source-330s.wav test-folder installed-gary-data [--skip-stems]\n"; return 2; }
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File source(argv[1]), root(argv[2]), installedData(argv[3]);
    root.createDirectory();
    if (juce::SystemStats::getEnvironmentVariable("GARY4JUCE_STORAGE_TEST_DIRECTORY", {}) != root.getFullPathName())
        return 2; // never exercise an editor against the user's session/preferences
    Gary4juceAudioProcessor processor;
    processor.stopHealthChecks();
    processor.prepareToPlay(48000.0, 512);
    const int liveCapacity = processor.getMaxRecordingSamples();
    {
        juce::AudioBuffer<float> audio(2, 48000 * 600);
        audio.clear();
        audio.setSample(0, audio.getNumSamples() - 1, .75f);
        audio.setSample(1, audio.getNumSamples() - 1, -.5f);
        check(processor.loadAudioIntoRecordingBuffer(audio), "ten-minute import accepted");
        check(processor.getRecordedSamples() == audio.getNumSamples(), "full import retained");
        check(processor.getRecordingBuffer().getSample(0, audio.getNumSamples()-1) == .75f, "final left sample retained");
        check(processor.getRecordingBuffer().getSample(1, audio.getNumSamples()-1) == -.5f, "final right sample retained");
        check(processor.getSavedSamples() == 0, "new import is unsaved until disk commit");
        check(processor.getMaxRecordingSamples() == liveCapacity, "live recording limit unchanged");
        processor.prepareToPlay(48000.0, 512);
        check(processor.getRecordedSamples() == audio.getNumSamples(), "same-rate prepare preserves long import");
    }
    const auto longFile = root.getChildFile("ten-minute.wav");
    check(processor.saveRecordingToFile(longFile), "long recording saves");
    check(processor.getSavedSamples() == 48000*600, "long recording marked saved");
    check(processor.loadRecordingAudioForPlayback(), "long recording loads for playback");
    check(std::abs(processor.getOutputAudioDuration()-600.0) < .001, "playback covers whole import");
    {
        juce::AudioBuffer<float> tooLong(1, 48000*600+1);
        check(!processor.loadAudioIntoRecordingBuffer(tooLong), "over-limit import rejected");
        check(processor.getRecordedSamples() == 48000*600, "rejected import preserves audio");
        juce::AudioBuffer<float> shortAudio(2, 48000);
        shortAudio.clear();
        check(processor.loadAudioIntoRecordingBuffer(shortAudio), "short reload accepted");
        check(processor.getRecordingBuffer().getNumSamples() == liveCapacity, "short reload releases expanded allocation");
    }
    {
        Gary4juceAudioProcessorEditor editor(processor);
        editor.stopTimer();
        processor.stopHealthChecks();
        editor.loadAudioFileIntoBuffer(source);
        editor.updateRecordingStatus();
        check(processor.getRecordedSamples() == 48000*330, "editor imports 330s without generation crop");
        check(editor.savedSamples == 48000*330, "editor saves the full imported source");
        check(std::abs(editor.getInputWaveformDisplayDuration()-330.0)<.001, "waveform reaches end of long source");
        editor.currentTab = Gary4juceAudioProcessorEditor::ModelTab::Jerry;
        editor.jerrySubTab = Gary4juceAudioProcessorEditor::JerrySubTab::SA3;
        check(!editor.getInputDurationHint().isEmpty(), "persistent model limit hint for long buffer");
        check(!editor.validateAudioDurationForModel(editor.getGaryBufferFile(), 240, "sa3"), "SA3 rejects long input before upload");
        check(!editor.validateAudioDurationForModel(editor.getGaryBufferFile(), 300, "carey"), "local Carey rejects long input");
        check(!editor.validateAudioDurationForModel(editor.getGaryBufferFile(), 240, "carey"), "remote Carey retains limit");
        check(editor.validateAudioDurationForModel(editor.getGaryBufferFile(), 330, "boundary"), "exact duration accepted");
        editor.isGenerating = false;
        editor.sendToCareyCover();
        check(!editor.isGenerating && editor.statusMessage.contains("limited"), "cover guard creates no generation job");
        editor.sendToGary();
        check(!editor.isGenerating && editor.statusMessage.contains("limited"), "Gary guard creates no generation job");
        processor.setUsingLocalhost(true);
        editor.localSA3Online = true;
        editor.sa3UI->setTransformAudioSourceRecording(true);
        editor.sendSA3Transform();
        check(!editor.isGenerating && editor.statusMessage.contains("limited"), "SA3 transform guard creates no generation job");
        editor.currentTab = Gary4juceAudioProcessorEditor::ModelTab::Carey;
        processor.setUsingLocalhost(true);
        check(editor.getCurrentModelInputLimit() == 300, "local input hint uses local limit");
        processor.setUsingLocalhost(false);
        check(editor.getCurrentModelInputLimit() == 240, "backend switch updates input limit");
        editor.switchToTab(Gary4juceAudioProcessorEditor::ModelTab::Jerry);
        editor.hasStatusMessage = false;
        editor.setEditorLayoutMode(Gary4juceAudioProcessorEditor::EditorLayoutMode::Compact);
        writeImage(editor, root.getChildFile("compact-long-input.png"));
        editor.setEditorLayoutMode(Gary4juceAudioProcessorEditor::EditorLayoutMode::Wide);
        editor.hasStatusMessage = false;
        writeImage(editor, root.getChildFile("wide-long-input.png"));
    }
    {
        // Block-backed waveform includes the final sample even with an uneven bin length.
        const auto peaks = StemsDialog::computeFilePeaks(longFile, 701);
        check(peaks.size() == 701, "file-backed waveform bins generated");
        check(!peaks.empty() && peaks.back().second > .12f, "waveform retains final averaged-channel peak");
    }
    if (argc > 4) { std::cout << checks << " checks, " << failures << " failures (stems skipped)\n"; return failures ? 1 : 0; }
    auto service = std::make_shared<stems::StemsService>(installedData);
    const auto output = root.getChildFile("separated");
    output.createDirectory();
    check(service->startSeparation("htdemucs", true, source, output), "long wrapper job starts");
    const auto deadline = juce::Time::getMillisecondCounterHiRes()+240000;
    while (service->getJob().running && juce::Time::getMillisecondCounterHiRes()<deadline)
        juce::Thread::sleep(100);
    if (service->getJob().running) { service->cancelJob(); return 3; }
    const auto job = service->getJob();
    std::cout << "separation: " << job.status << " error: " << job.error << '\n';
    check(job.succeeded && job.stemFiles.size() == 4, "JUCE wrapper separates full track");
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    for (const auto& file : job.stemFiles) {
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        check(reader && reader->lengthInSamples == 44100*330, "stem frame count matches source");
        check(reader && reader->sampleRate == 44100 && reader->numChannels == 2, "stem format retained");
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}

int main(int argc, char** argv) { return FullTrackAudioTest::run(argc, argv); }
