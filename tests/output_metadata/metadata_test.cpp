// SPDX-License-Identifier: AGPL-3.0-only
#include <JuceHeader.h>
#include "../../Source/PluginEditor.h"
#include "../../Source/Utils/GenerationMetadata.h"
#include "../../Source/Utils/EmbeddedAudioMetadata.h"
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
    value->setProperty("lyrics", juce::String::fromUTF8("[verse]\nΚαλημέρα κόσμε\n夜の歌 🎵"));
    return juce::var(value.get());
}
juce::StringPairArray embedded(const juce::File& audio)
{
    juce::StringPairArray result;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(audio));
    if (!reader) return result;
    if (audio.hasFileExtension("wav"))
    {
        result.set("GARY4JUCE_METADATA", reader->metadataValues[juce::WavAudioFormat::riffInfoComment2]);
        return result;
    }
    // JUCE's FLAC reader also ignores comments; inspect the exported block.
    auto input = audio.createInputStream();
    input->skipNextBytes(4);
    bool last = false;
    while (!last && !input->isExhausted())
    {
        const auto type = static_cast<unsigned char>(input->readByte());
        last = (type & 0x80) != 0;
        unsigned char length[3]{};
        if (input->read(length, 3) != 3) break;
        const auto size = (length[0] << 16) | (length[1] << 8) | length[2];
        juce::MemoryBlock block;
        if (input->readIntoMemoryBlock(block, size) != size) break;
        if ((type & 0x7f) != 4) continue;
        juce::MemoryInputStream comments(block, false);
        const auto vendorLength = comments.readInt();
        comments.skipNextBytes(vendorLength);
        const auto count = comments.readInt();
        for (int i = 0; i < count && !comments.isExhausted(); ++i)
        {
            const auto fieldLength = comments.readInt();
            juce::MemoryBlock text;
            if (fieldLength < 0 || comments.readIntoMemoryBlock(text, fieldLength) != fieldLength) break;
            const auto field = juce::String::fromUTF8(static_cast<const char*>(text.getData()), fieldLength);
            result.set(field.upToFirstOccurrenceOf("=", false, false), field.fromFirstOccurrenceOf("=", false, false));
        }
    }
    return result;
}

bool sameAudio(const juce::File& a, const juce::File& b)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> left(formats.createReaderFor(a)), right(formats.createReaderFor(b));
    if (!left || !right || left->sampleRate != right->sampleRate || left->numChannels != right->numChannels
        || left->lengthInSamples != right->lengthInSamples || left->bitsPerSample != right->bitsPerSample) return false;
    juce::AudioBuffer<float> x(static_cast<int>(left->numChannels), 4096), y(static_cast<int>(right->numChannels), 4096);
    for (juce::int64 start = 0; start < left->lengthInSamples; start += 4096)
    {
        const auto size = static_cast<int>(juce::jmin<juce::int64>(4096, left->lengthInSamples - start));
        if (!left->read(&x, 0, size, start, true, true) || !right->read(&y, 0, size, start, true, true)) return false;
        for (int channel = 0; channel < x.getNumChannels(); ++channel)
            for (int i = 0; i < size; ++i)
                if (x.getSample(channel, i) != y.getSample(channel, i)) return false;
    }
    return true;
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
        check(editor.draggedAudioMetadata == Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Off, "metadata export defaults off in isolated preferences");
#if JUCE_LINUX
        check(JucePlugin_EditorRequiresKeyboardFocus == 1, "Linux plugin requests host keyboard focus");
        editor.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        editor.setVisible(true);
        std::function<juce::TextEditor*(juce::Component&)> findPrompt = [&](juce::Component& component) -> juce::TextEditor*
        {
            if (auto* text = dynamic_cast<juce::TextEditor*>(&component))
                if (text->isShowing() && !text->isReadOnly()) return text;
            for (auto* child : component.getChildren())
                if (auto* text = findPrompt(*child)) return text;
            return nullptr;
        };
        auto* prompt = findPrompt(editor);
        check(prompt != nullptr, "Linux editor exposes a visible editable prompt box");
        if (prompt != nullptr)
        {
            const auto before = prompt->getText();
            prompt->grabKeyboardFocus();
            prompt->keyPressed(juce::KeyPress('x', {}, 'x'));
            check(prompt->getText() != before, "Linux prompt box accepts typed characters");
            prompt->setText(before, false);
        }
        const auto image = editor.createComponentSnapshot(editor.getLocalBounds());
        auto screenshot = root.getChildFile("linux-preview.png").createOutputStream();
        if (screenshot != nullptr) juce::PNGImageFormat().writeImageToStream(image, *screenshot);
#endif

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
        editor.setDraggedAudioMetadata(Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Json);
        const auto firstExport = root.getChildFile("first.wav");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), firstExport), "WAV plus JSON exported");
        const auto firstJson = firstExport.withFileExtension(".json").loadFileAsString();
        check(exported(firstExport)["audio_file"].toString() == "first.wav", "JSON names exact dragged audio file");
        check(exported(firstExport)["export_audio_sha256"].toString() == juce::SHA256(firstExport).toHexString(), "export hash matches actual WAV");

        const auto sourceHash = juce::SHA256(editor.getGaryOutputFile()).toHexString();
        editor.setDraggedAudioMetadata(Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Embedded);
        const auto taggedWav = root.getChildFile("tagged.wav");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), taggedWav), "WAV with embedded tags exported");
        const auto wavDetails = juce::JSON::parse(embedded(taggedWav)["GARY4JUCE_METADATA"]);
        check(wavDetails["prompt"].toString() == "original prompt" && wavDetails["seed"].toString() == "1234567890", "WAV comment retains render snapshot");
        check(wavDetails["parameters"]["lyrics"].toString() == original["lyrics"].toString(), "WAV retains multiline Unicode lyrics");
        check(!wavDetails["parameters"].hasProperty("hf_token") && !wavDetails["parameters"].hasProperty("audio_data"), "embedded metadata excludes secrets and audio payloads");
        check(wavDetails["source_audio_sha256"].toString() == sourceHash && !wavDetails.hasProperty("export_audio_sha256"), "embedded tags identify source without a circular file hash");
        check(!taggedWav.withFileExtension(".json").exists(), "embedded mode writes no JSON sidecar");
        check(sameAudio(editor.getGaryOutputFile(), taggedWav), "WAV tagging preserves sample format and every sample");
        check(juce::SHA256(editor.getGaryOutputFile()).toHexString() == sourceHash, "tagging leaves source file untouched");
        editor.setDraggedAudioMetadata(Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Json);

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
        editor.setDraggedAudioMetadata(Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Embedded);
        const auto taggedCrop = root.getChildFile("tagged-crop.wav");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), taggedCrop), "cropped WAV exports embedded details");
        check(juce::JSON::parse(embedded(taggedCrop)["GARY4JUCE_METADATA"])["edits"].size() == 1, "embedded details retain crop history");
        editor.setDraggedAudioMetadata(Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Json);

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
        editor.setDraggedAudioMetadata(Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Embedded);
        const auto taggedFlac = root.getChildFile("tagged.flac");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), taggedFlac), "FLAC with embedded tags exported");
        const auto flacTags = embedded(taggedFlac);
        check(flacTags["PROMPT"] == "original prompt" && flacTags["SEED"] == "1234567890", "FLAC exposes prompt and seed as ordinary tags");
        check(flacTags["LYRICS"] == original["lyrics"].toString(), "FLAC exposes full Unicode lyrics tag");
        check(juce::JSON::parse(flacTags["GARY4JUCE_METADATA"])["service"].toString() == "gary", "FLAC retains complete structured snapshot");
        check(!taggedFlac.withFileExtension(".json").exists(), "FLAC embedded mode writes no sidecar");
        check(sameAudio(flac, taggedFlac), "FLAC tagging preserves sample format and every sample");
        editor.setDraggedAudioMetadata(Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Json);
       #endif
        editor.setDraggedAudioFormat(Gary4juceAudioProcessorEditor::DraggedAudioFormat::Wav);
        // Long prompts/settings must not hit the short Broadcast WAV description limit.
        const auto longToken = editor.beginGenerationAsyncWork();
        auto longRequest = request("long-model", "long prompt", 73);
        const auto longPrompt = juce::String::repeatedString("long prompt with \"quotes\" and line breaks\n", 2000);
        longRequest.getDynamicObject()->setProperty("description", longPrompt);
        editor.captureGenerationRequest(longToken, "carey", "generate", longRequest);
        check(editor.saveGeneratedAudio(audio64), "render with long prompt saved");
        editor.setDraggedAudioMetadata(Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Embedded);
        const auto longWav = root.getChildFile("long.wav");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), longWav), "long WAV metadata exports");
        check(juce::JSON::parse(embedded(longWav)["GARY4JUCE_METADATA"])["prompt"].toString() == longPrompt, "large WAV snapshot is not truncated");
       #if JUCE_USE_FLAC
        editor.setDraggedAudioFormat(Gary4juceAudioProcessorEditor::DraggedAudioFormat::Flac);
        const auto longFlac = root.getChildFile("long.flac");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), longFlac), "long FLAC metadata exports");
        check(embedded(longFlac)["PROMPT"] == longPrompt, "large FLAC tag is not truncated");
        editor.setDraggedAudioFormat(Gary4juceAudioProcessorEditor::DraggedAudioFormat::Wav);
       #endif
        const auto malformed = root.getChildFile("malformed.wav");
        malformed.replaceWithText("invalid audio container");
        const auto rejected = root.getChildFile("rejected.wav");
        check(!editor.createDraggedAudioFile(malformed, rejected) && !rejected.exists(), "tagging failure removes incomplete export");
        const auto malformedFlac = root.getChildFile("malformed.flac");
        const unsigned char invalidFlac[] = { 'f', 'L', 'a', 'C', 0x80, 0, 0, 34, 0 };
        malformedFlac.replaceWithData(invalidFlac, sizeof(invalidFlac));
        const auto malformedHash = juce::SHA256(malformedFlac).toHexString();
        check(!generation_metadata::embedFlac(malformedFlac, generation_metadata::embeddedTags({}))
            && juce::SHA256(malformedFlac).toHexString() == malformedHash, "truncated FLAC is rejected without modifying it");
        editor.setDraggedAudioMetadata(Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Json);
        const auto freshToken = editor.beginGenerationAsyncWork();
        editor.captureGenerationRequest(freshToken, "gary", "generate", request("latest", "latest prompt", 42));
        editor.captureGenerationRequest(token, "gary", "generate", request("stale", "stale prompt", 999));
        check(editor.saveGeneratedAudio(audio64), "newer request completes");
        check(editor.loadOutputGenerationMetadata()["model"].toString() == "latest", "late request cannot overwrite a newer snapshot");
        // Simulate preferences written by the original JSON-only release.
        editor.getUpdatePreferences().removeValue("draggedAudioMetadataMode");
        editor.getUpdatePreferences().setValue("draggedAudioMetadata", true);
        editor.getUpdatePreferences().saveIfNeeded();
    }
    {
        Gary4juceAudioProcessorEditor editor(processor);
        editor.stopAllBackgroundOperations();
        check(editor.draggedAudioMetadata == Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Json, "legacy enabled preference migrates to JSON");
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
        editor.setDraggedAudioMetadata(Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Embedded);
        const auto unknownEmbedded = root.getChildFile("unknown-embedded.wav");
        check(editor.createDraggedAudioFile(editor.getGaryOutputFile(), unknownEmbedded), "untracked audio exports with embedded details");
        const auto unknownDetails = juce::JSON::parse(embedded(unknownEmbedded)["GARY4JUCE_METADATA"]);
        check(unknownDetails.hasProperty("generation_details_available") && !(bool)unknownDetails["generation_details_available"]
            && !unknownDetails.hasProperty("model") && !unknownDetails.hasProperty("source_audio_sha256"), "embedded unknown provenance cannot inherit stale settings");
        editor.clearOutputAudio();
        check(!editor.getGaryOutputFile().withFileExtension(".metadata.json").exists(), "clearing output clears provenance");
    }
    {
        Gary4juceAudioProcessorEditor editor(processor);
        editor.stopAllBackgroundOperations();
        check(editor.draggedAudioMetadata == Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Embedded, "embedded choice survives editor recreation");
        editor.setDraggedAudioMetadata(Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Off);
        const auto off = root.getChildFile("off-again.wav");
        check(editor.createDraggedAudioFile(root.getChildFile("fixture.wav"), off), "switching metadata off still exports audio");
        check(!off.withFileExtension(".json").exists() && embedded(off)["GARY4JUCE_METADATA"].isEmpty(), "switching off produces neither JSON nor embedded generation tags");
    }
    {
        Gary4juceAudioProcessorEditor editor(processor);
        editor.stopAllBackgroundOperations();
        check(editor.draggedAudioMetadata == Gary4juceAudioProcessorEditor::DraggedAudioMetadata::Off, "explicit off overrides legacy enabled preference");
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
