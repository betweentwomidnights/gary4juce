// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <JuceHeader.h>

namespace generation_metadata
{
// Use ordinary text tags for browsing and retain the complete, structured
// snapshot in a separate tag. Lyrics are also in parameters in that snapshot.
inline juce::StringPairArray embeddedTags(const juce::var& metadata)
{
    juce::StringPairArray tags;
    tags.set("SOFTWARE", "gary4juce " + juce::String(ProjectInfo::versionString));
    tags.set("GARY4JUCE_METADATA", juce::JSON::toString(metadata));
    for (auto key : { "model", "prompt", "seed" })
    {
        const auto value = metadata.getProperty(key, {}).toString();
        if (value.isNotEmpty()) tags.set(juce::String(key).toUpperCase(), value);
    }
    const auto lyrics = metadata.getProperty("parameters", {}).getProperty("lyrics", {}).toString();
    if (lyrics.isNotEmpty()) tags.set("LYRICS", lyrics);
    return tags;
}

inline void writeSyncSafe(juce::OutputStream& out, size_t size)
{
    for (int shift : { 21, 14, 7, 0 })
        out.writeByte(static_cast<char>((size >> shift) & 0x7f));
}

inline void writeRiffText(juce::OutputStream& out, const char* id, const juce::String& text)
{
    const auto size = text.getNumBytesAsUTF8() + 1;
    out.write(id, 4);
    out.writeInt(static_cast<int>(size));
    out.write(text.toRawUTF8(), size);
    if ((size & 1) != 0) out.writeByte(0);
}

// Append RIFF INFO and ID3v2.4 chunks without decoding/re-encoding the audio.
// This operates on the disposable export copy, never on myOutput.wav.
inline bool embedWav(const juce::File& file, const juce::StringPairArray& tags)
{
    auto input = file.createInputStream();
    if (input == nullptr || input->readInt() != 0x46464952) return false; // RIFF
    const auto riffSize = static_cast<juce::uint32>(input->readInt());
    if (input->readInt() != 0x45564157 || file.getSize() != static_cast<juce::int64>(riffSize) + 8)
        return false; // WAVE; refuse malformed/unsupported containers.
    input.reset();

    juce::MemoryOutputStream info;
    info.write("INFO", 4);
    writeRiffText(info, "ICMT", tags["GARY4JUCE_METADATA"]);
    writeRiffText(info, "ISFT", tags["SOFTWARE"]);

    juce::MemoryOutputStream frames;
    for (int i = 0; i < tags.size(); ++i)
    {
        const auto key = tags.getAllKeys()[i];
        const auto value = tags.getAllValues()[i];
        juce::MemoryOutputStream body;
        body.writeByte(3); // ID3v2.4 UTF-8, including multiline/non-Latin lyrics.
        if (key == "LYRICS") body.write("und", 3);
        else body.write(key.toRawUTF8(), key.getNumBytesAsUTF8());
        body.writeByte(0); // Empty USLT description, or TXXX field-name terminator.
        body.write(value.toRawUTF8(), value.getNumBytesAsUTF8());
        if (body.getDataSize() > 0x0fffffff) return false;
        frames.write(key == "LYRICS" ? "USLT" : "TXXX", 4);
        writeSyncSafe(frames, body.getDataSize());
        frames.writeShort(0);
        frames.write(body.getData(), body.getDataSize());
    }
    if (frames.getDataSize() > 0x0fffffff) return false;
    juce::MemoryOutputStream id3;
    id3.write("ID3\x04\x00\x00", 6);
    writeSyncSafe(id3, frames.getDataSize());
    id3.write(frames.getData(), frames.getDataSize());

    const auto newSize = file.getSize() + static_cast<juce::int64>(8 + info.getDataSize() + 8
        + id3.getDataSize() + (id3.getDataSize() & 1));
    if (newSize - 8 > 0xffffffffLL) return false;
    auto out = file.createOutputStream();
    if (out == nullptr || !out->openedOk() || !out->setPosition(file.getSize())) return false;
    out->write("LIST", 4);
    out->writeInt(static_cast<int>(info.getDataSize()));
    out->write(info.getData(), info.getDataSize());
    out->write("id3 ", 4);
    out->writeInt(static_cast<int>(id3.getDataSize()));
    out->write(id3.getData(), id3.getDataSize());
    if ((id3.getDataSize() & 1) != 0) out->writeByte(0);
    if (!out->setPosition(4)) return false;
    out->writeInt(static_cast<int>(newSize - 8));
    out->flush();
    return out->getStatus().wasOk() && file.getSize() == newSize;
}

// JUCE's FLAC encoder ignores metadataValues. Replace its Vorbis comment block
// in a temporary file, retaining STREAMINFO, other blocks and encoded frames.
inline bool embedFlac(const juce::File& file, const juce::StringPairArray& tags)
{
    juce::MemoryOutputStream comments;
    const auto vendor = tags["SOFTWARE"];
    comments.writeInt(static_cast<int>(vendor.getNumBytesAsUTF8()));
    comments.write(vendor.toRawUTF8(), vendor.getNumBytesAsUTF8());
    comments.writeInt(tags.size());
    for (int i = 0; i < tags.size(); ++i)
    {
        const auto field = tags.getAllKeys()[i] + "=" + tags.getAllValues()[i];
        comments.writeInt(static_cast<int>(field.getNumBytesAsUTF8()));
        comments.write(field.toRawUTF8(), field.getNumBytesAsUTF8());
    }
    if (comments.getDataSize() > 0xffffff) return false;

    auto input = file.createInputStream();
    if (input == nullptr || input->readInt() != 0x43614c66) return false; // fLaC
    juce::TemporaryFile temporary(file);
    auto out = temporary.getFile().createOutputStream();
    if (out == nullptr || !out->openedOk()) return false;
    out->write("fLaC", 4);
    bool last = false, first = true;
    while (!last)
    {
        unsigned char header[4]{};
        if (input->read(header, 4) != 4) return false;
        last = (header[0] & 0x80) != 0;
        const auto type = header[0] & 0x7f;
        const auto size = (header[1] << 16) | (header[2] << 8) | header[3];
        if ((first && (type != 0 || size != 34)) || type == 127
            || input->getPosition() + size > input->getTotalLength()) return false;
        first = false;
        if (type == 4)
        {
            if (!input->setPosition(input->getPosition() + size)) return false;
            continue;
        }
        header[0] &= 0x7f; // Our new comment block will be last.
        if (!out->write(header, 4) || out->writeFromInputStream(*input, size) != size) return false;
    }
    const auto size = comments.getDataSize();
    const unsigned char header[] = { 0x84, static_cast<unsigned char>(size >> 16),
        static_cast<unsigned char>(size >> 8), static_cast<unsigned char>(size) };
    out->write(header, 4);
    out->write(comments.getData(), size);
    const auto remaining = input->getTotalLength() - input->getPosition();
    if (remaining <= 0 || out->writeFromInputStream(*input, remaining) != remaining) return false;
    out->flush();
    const bool ok = out->getStatus().wasOk();
    out.reset();
    input.reset();
    return ok && temporary.overwriteTargetFileWithTemporary();
}
}
