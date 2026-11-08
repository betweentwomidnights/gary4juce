// SPDX-License-Identifier: AGPL-3.0-only
#include <JuceHeader.h>
#include "../../Source/PluginEditor.h"
#include "../../Source/Utils/GenerationMetadata.h"
#include <iostream>
#include <cmath>

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::cout << "FAIL: " << message << '\n'; }
}
juce::var request(const char* model, const char* prompt, int seed)
{
    auto value = juce::DynamicObject::Ptr(new juce::DynamicObject());
    value->setProperty("model_name", model);
    value->setProperty("description", prompt);
    value->setProperty("seed", seed);
    value->setProperty("audio_data", "uploaded audio must not appear in JSON");
    value->setProperty("hf_token", "private token must not appear in JSON");
    value->setProperty("max_tokens", 512);
    return juce::var(value.get());
}
juce::var exported(const juce::File& audio)
{
    return juce::JSON::parse(audio.withFileExtension(".json").loadFileAsString());
}
}

struct OutputMetadataTest { static int run(const juce::File& root); };
int OutputMetadataTest::run(const juce::File& root)
{
    if (juce::SystemStats::getEnvironmentVariable("GARY4JUCE_STORAGE_TEST_DIRECTORY", {}) != root.getFullPathName()) return 2;
    root.createDirectory();
    Gary4juceAudioProcessor processor;
    processor.stopHealthChecks();
    processor.prepareToPlay(48000, 512);
    {
        Gary4juceAudioProcessorEditor editor(processor);
        editor.stopAllBackgroundOperations();
        check(!editor.saveDraggedAudioMetadata, "metadata export defaults off in isolated preferences");
        juce::AudioBuffer<float> tone(1, 48000 * 2);
        for (int i = 0; i < tone.getNumSamples(); ++i) tone.setSample(0, i, 0.2f * std::sin(i * 0.03f));
        const auto fixture = root.getChildFile("fixture.wav");
        check(editor.writeAudioBufferToFileSafely(tone, 48000, fixture), "audio fixture created");
        juce::MemoryBlock bytes;
        fixture.loadFileAsData(bytes);
        const auto audio64 = juce::Base64::toBase64(bytes.getData(), bytes.getSize());

        const auto token = editor.beginGenerationAsyncWork();
        auto original = request("model-one", "original prompt", -1);
        editor.captureGenerationRequest(token, "gary", "generate", original);
        original.getDynamicObject()->setProperty("description", "changed before render completed");
        auto response = juce::DynamicObject::Ptr(new juce::DynamicObject());
        response->setProperty("generation_seed", juce::int64(1234567890));
        check(editor.saveGeneratedAudio(audio64, juce::var(response.get())), "successful render saved");
        auto stored = editor.loadOutputGenerationMetadata();
        check(stored["prompt"].toString() == "original prompt", "request snapshot is independent of later edits");
        check(stored["seed"].toString() == "1234567890", "actual random seed saved from completion");
        check(!stored["parameters"].hasProperty("audio_data") && !stored["parameters"].hasProperty("hf_token"), "uploaded audio and credentials excluded");
        check((int)stored["parameters"]["max_tokens"] == 512, "generation token counts retained");

        const auto audioOnly = root.getChildFile("audio-only.wav");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), audioOnly), "ordinary drag export works with setting off");
        check(!audioOnly.withFileExtension(".json").exists(), "setting off produces no exported JSON");
        editor.setSaveDraggedAudioMetadata(true);
        const auto firstExport = root.getChildFile("first.wav");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), firstExport), "WAV plus JSON exported");
        const auto firstJson = firstExport.withFileExtension(".json").loadFileAsString();
        check(exported(firstExport)["audio_file"].toString() == "first.wav", "JSON names exact dragged audio file");
        check(exported(firstExport)["export_audio_sha256"].toString() == juce::SHA256(firstExport).toHexString(), "export hash matches actual WAV");

        // A failed later request must leave both the old audio and its details.
        const auto failedToken = editor.beginGenerationAsyncWork();
        editor.captureGenerationRequest(failedToken, "sa3", "generate", request("new model", "new prompt", 99));
        check(!editor.saveGeneratedAudio("not valid base64!?"), "invalid render rejected");
        const juce::String corrupt = "base64 is valid but the audio is not";
        check(!editor.saveGeneratedAudio(juce::Base64::toBase64(corrupt.toRawUTF8(), corrupt.getNumBytesAsUTF8())), "unreadable audio response rejected before output replacement");
        const auto afterFailure = root.getChildFile("after-failure.wav");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), afterFailure), "previous output still exports after failure");
        check(exported(afterFailure)["model"].toString() == "model-one", "failed request cannot relabel previous output");

        editor.currentPlaybackPosition = 0.6;
        editor.cropAudioAtCurrentPosition();
        stored = editor.loadOutputGenerationMetadata();
        check(stored["seed"].toString() == "1234567890", "crop preserves successful render seed");
        check(stored["edits"].size() == 1 && std::abs((double)stored["edits"][0]["duration_seconds"] - 0.6) < 0.001, "crop records retained range");
        check(std::abs(editor.totalAudioDuration - 0.6) < 0.001, "actual crop and recorded duration agree");
        const auto cropped = root.getChildFile("cropped.wav");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), cropped), "cropped output exports with JSON");
        check(exported(cropped)["prompt"].toString() == "original prompt", "crop keeps original prompt");
        check(firstExport.withFileExtension(".json").loadFileAsString() == firstJson, "later crops never rewrite previous exports");

        // Retain provenance through a transform and restore it on undo.
        const auto beforeTransform = stored.clone();
        editor.terrySourceGenerationMetadata = beforeTransform;
        auto transformToken = editor.beginGenerationAsyncWork();
        editor.captureGenerationRequest(transformToken, "terry", "transform", request("terry-model", "transform prompt", 22));
        check(editor.saveGeneratedAudio(audio64), "transformed audio saved");
        check(editor.loadOutputGenerationMetadata()["source_render"]["prompt"].toString() == "original prompt", "transform remembers source provenance");
        editor.beginGenerationAsyncWork(); // undo begins with no new generation request
        check(editor.saveGeneratedAudio(audio64, {}, beforeTransform), "undo restores source provenance");
        check(editor.loadOutputGenerationMetadata()["service"].toString() == "gary", "undo restores source service, not current tab");

       #if JUCE_USE_FLAC
        editor.setDraggedAudioFormat(Gary4juceAudioProcessorEditor::DraggedAudioFormat::Flac);
        const auto flac = root.getChildFile("drag.flac");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), flac), "FLAC plus JSON exported");
        check(exported(flac)["export_audio_sha256"].toString() == juce::SHA256(flac).toHexString(), "FLAC metadata binds to encoded export");
        check(exported(flac)["seed"].toString() == "1234567890", "FLAC preserves generation seed");
       #endif
        editor.setDraggedAudioFormat(Gary4juceAudioProcessorEditor::DraggedAudioFormat::Wav);
        const auto freshToken = editor.beginGenerationAsyncWork();
        editor.captureGenerationRequest(freshToken, "gary", "generate", request("latest", "latest prompt", 42));
        editor.captureGenerationRequest(token, "gary", "generate", request("stale", "stale prompt", 999));
        check(editor.saveGeneratedAudio(audio64), "newer request completes");
        check(editor.loadOutputGenerationMetadata()["model"].toString() == "latest", "late request cannot overwrite a newer snapshot");
    }
    {
        Gary4juceAudioProcessorEditor editor(processor);
        editor.stopAllBackgroundOperations();
        check(editor.saveDraggedAudioMetadata, "metadata setting survives editor recreation");
        const auto reopened = root.getChildFile("reopened.wav");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), reopened), "persisted output exports after editor recreation");
        check(exported(reopened)["model"].toString() == "latest", "output provenance survives editor recreation");
        // Replacing audio outside generation must not inherit old settings.
        juce::AudioBuffer<float> external(1, 48000);
        external.clear();
        editor.writeAudioBufferToFileSafely(external, 48000, editor.getGaryOutputFile());
        const auto unknown = root.getChildFile("unknown.wav");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), unknown), "untracked audio still exports");
        check(exported(unknown)["generation_details_available"].isBool()
            && !(bool)exported(unknown)["generation_details_available"], "changed audio explicitly has unknown provenance");
        check(!exported(unknown).hasProperty("model"), "untracked audio never borrows stale settings");
        editor.clearOutputAudio();
        check(!editor.getGaryOutputFile().withFileExtension(".metadata.json").exists(), "clearing output clears provenance");
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    juce::ScopedJuceInitialiser_GUI gui;
    return OutputMetadataTest::run(juce::File(argv[1]));
}
