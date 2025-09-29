// BarTrim.h
#pragma once
#include <cstdint>
#include <juce_audio_formats/juce_audio_formats.h>

// ====== CONFIG ======
#ifndef BARTRIM_DEBUG
#define BARTRIM_DEBUG 1   // 1 = emit DBG logs
#endif
#ifndef BARTRIM_DRY_RUN
#define BARTRIM_DRY_RUN 0 // 1 = no writes; only logs decisions
#endif
#ifndef BARTRIM_SILENCE_THRESH
#define BARTRIM_SILENCE_THRESH 1.0e-6f // RMS below this considered silence
#endif

#if BARTRIM_DEBUG
#define BTLOG(msg) DBG(juce::String("[BarTrim] ") + msg)
#else
#define BTLOG(msg) do{}while(0)
#endif

// --- Helpers ---
inline bool waitForFileQuiescent(const juce::File& f, int timeoutMs = 800, int pollMs = 80)
{
    if (!f.existsAsFile()) return false;
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;
    int64_t last = f.getSize();
    for (;;)
    {
        juce::Thread::sleep(pollMs);
        const int64_t now = f.getSize();
        if (now == last) return true;
        last = now;
        if (juce::Time::getMillisecondCounterHiRes() > deadline) return false;
    }
}

inline std::unique_ptr<juce::AudioFormatReader> openReader(const juce::File& f)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    return std::unique_ptr<juce::AudioFormatReader>(fm.createReaderFor(f));
}

inline float rmsAll(const juce::AudioBuffer<float>& buf)
{
    if (buf.getNumSamples() <= 0) return 0.0f;
    double sum = 0.0;
    const int nCh = buf.getNumChannels();
    const int nSm = buf.getNumSamples();
    for (int ch = 0; ch < nCh; ++ch)
    {
        const float* d = buf.getReadPointer(ch);
        double s = 0.0;
        for (int i = 0; i < nSm; ++i) { const float v = d[i]; s += (double)v * (double)v; }
        sum += s;
    }
    const double denom = (double)nCh * (double)nSm;
    return denom > 0.0 ? (float)std::sqrt(sum / denom) : 0.0f;
}

inline void logBufferRMS(const char* tag, const juce::AudioBuffer<float>& buf)
{
#if BARTRIM_DEBUG
    juce::String msg; msg << tag << " RMS (len=" << buf.getNumSamples() << "): ";
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        msg << "ch" << ch << "=" << buf.getRMSLevel(ch, 0, buf.getNumSamples()) << " ";
    msg << "all=" << rmsAll(buf);
    BTLOG(msg);
#endif
}

// True if totalSamples is an exact multiple of samplesPerBar
inline bool isBarAligned(int64_t totalSamples, int64_t samplesPerBar)
{
    return (samplesPerBar > 0) && (totalSamples % samplesPerBar == 0);
}

// ===== Basic "trim to whole bars (end on bar)" =====
inline juce::File makeBarAlignedCopy(const juce::File& srcFile, double bpm, int beatsPerBar)
{
    if (!srcFile.existsAsFile() || bpm <= 0.0 || beatsPerBar <= 0)
        return srcFile;

    if (!waitForFileQuiescent(srcFile))
    {
        BTLOG("source not quiescent -> skip trim: " + srcFile.getFullPathName());
        return srcFile;
    }

    auto reader = openReader(srcFile);
    if (!reader) { BTLOG("openReader failed"); return srcFile; }

    const double sr = reader->sampleRate;
    const int64_t totalSamples = (int64_t)reader->lengthInSamples;
    if (sr <= 0.0 || totalSamples <= 0) { BTLOG("bad SR or empty"); return srcFile; }

    const double secondsPerBar = (60.0 / bpm) * (double)beatsPerBar;
    const int64_t samplesPerBar = (int64_t)std::floor(secondsPerBar * sr + 0.5);

    BTLOG("SR=" + juce::String(sr, 2) + " total=" + juce::String((int)totalSamples)
        + " spb=" + juce::String((int)samplesPerBar)
        + " bars=" + juce::String((int)(samplesPerBar > 0 ? totalSamples / samplesPerBar : 0))
        + " aligned=" + juce::String(isBarAligned(totalSamples, samplesPerBar) ? "yes" : "no"));

    if (samplesPerBar <= 0 || totalSamples < samplesPerBar)
    {
        BTLOG("not enough for one bar or invalid spb -> skip trim");
        return srcFile;
    }

    const int64_t fullSamples = (totalSamples / samplesPerBar) * samplesPerBar;
    if (fullSamples == totalSamples)
    {
        BTLOG("already bar-aligned -> use original");
        return srcFile;
    }
    if (fullSamples <= 0)
    {
        BTLOG("fullSamples <= 0 -> skip trim");
        return srcFile;
    }

    juce::AudioBuffer<float> buffer((int)reader->numChannels, (int)fullSamples);
    const bool okRead = reader->read(&buffer, 0, (int)fullSamples, 0, true, true);
    if (!okRead) { BTLOG("reader->read failed"); return srcFile; }

    logBufferRMS("SRC(fullbars)", buffer);

#if BARTRIM_DRY_RUN
    BTLOG("DRY_RUN: would write bartrim.wav here; skipping write and returning original.");
    return srcFile;
#else
    auto trimmed = srcFile.getSiblingFile(srcFile.getFileNameWithoutExtension() + ".bartrim.wav");
    if (trimmed.existsAsFile()) trimmed.deleteFile();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> out(trimmed.createOutputStream());
    if (!out) { BTLOG("createOutputStream failed"); return srcFile; }

    const int bitsPerSample = 24;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(out.release(), sr, (unsigned int)reader->numChannels, bitsPerSample, {}, 0));
    if (!writer) { BTLOG("createWriterFor failed"); return srcFile; }

    const bool okWrite = writer->writeFromAudioSampleBuffer(buffer, 0, (int)fullSamples);
    writer.reset();

    if (!okWrite || !trimmed.existsAsFile() || trimmed.getSize() <= 0)
    {
        BTLOG("write failed or empty; size=" + juce::String((int)trimmed.getSize()) + " -> use original");
        if (trimmed.existsAsFile()) trimmed.deleteFile();
        return srcFile;
    }

    // Verify and print RMS of the written file too
    if (auto r2 = openReader(trimmed))
    {
        juce::AudioBuffer<float> b2((int)r2->numChannels, (int)r2->lengthInSamples);
        const bool okRead2 = r2->read(&b2, 0, (int)r2->lengthInSamples, 0, true, true);
        if (okRead2) logBufferRMS("TRIMMED", b2);
        else BTLOG("verify read of written file failed");
    }

    BTLOG("trimmed: total=" + juce::String((int)totalSamples) + " -> full=" + juce::String((int)fullSamples)
        + " secs=" + juce::String((double)fullSamples / sr, 3)
        + " path=" + trimmed.getFullPathName());

    return trimmed;
#endif
}

// ===== Trim to whole bars AND <= maxSeconds, ending on a bar =====
inline juce::File makeBarAlignedMaxSecondsCopy(const juce::File& srcFile,
    double bpm, int beatsPerBar, double maxSeconds)
{
    if (!srcFile.existsAsFile() || bpm <= 0.0 || beatsPerBar <= 0 || maxSeconds <= 0.0)
        return srcFile;

    if (!waitForFileQuiescent(srcFile))
    {
        BTLOG("source not quiescent -> skip trim(max): " + srcFile.getFullPathName());
        return srcFile;
    }

    auto reader = openReader(srcFile);
    if (!reader) { BTLOG("openReader failed"); return srcFile; }

    const double sr = reader->sampleRate;
    const int64_t totalSamples = (int64_t)reader->lengthInSamples;
    if (sr <= 0.0 || totalSamples <= 0) { BTLOG("bad SR or empty"); return srcFile; }

    const double secondsPerBar = (60.0 / bpm) * (double)beatsPerBar;
    const int64_t samplesPerBar = (int64_t)std::floor(secondsPerBar * sr + 0.5);
    const int64_t maxSamples = (int64_t)std::floor(maxSeconds * sr);

    BTLOG("MAX mode: SR=" + juce::String(sr, 2)
        + " total=" + juce::String((int)totalSamples)
        + " spb=" + juce::String((int)samplesPerBar)
        + " maxS=" + juce::String((int)maxSamples));

    if (samplesPerBar <= 0 || totalSamples < samplesPerBar)
    {
        BTLOG("not enough for one bar or invalid spb -> skip trim(max)");
        return srcFile;
    }

    const int64_t fullBarsTotal = totalSamples / samplesPerBar;
    const int64_t fullBarsMax = std::max<int64_t>(1, maxSamples / samplesPerBar);
    const int64_t barsToKeep = std::min(fullBarsTotal, fullBarsMax);
    const int64_t fullSamples = barsToKeep * samplesPerBar;

    if (fullSamples == totalSamples)
    {
        BTLOG("already <= max and bar-aligned -> use original");
        return srcFile;
    }
    if (fullSamples <= 0 || barsToKeep <= 0)
    {
        BTLOG("computed 0 bars to keep -> use original");
        return srcFile;
    }

    juce::AudioBuffer<float> buffer((int)reader->numChannels, (int)fullSamples);
    const bool okRead = reader->read(&buffer, 0, (int)fullSamples, 0, true, true);
    if (!okRead) { BTLOG("reader->read failed (max)"); return srcFile; }

    logBufferRMS("SRC(full<=max)", buffer);

#if BARTRIM_DRY_RUN
    BTLOG("DRY_RUN: would write bartrim.wav (max) here; skipping write and returning original.");
    return srcFile;
#else
    auto trimmed = srcFile.getSiblingFile(srcFile.getFileNameWithoutExtension() + ".bartrim.wav");
    if (trimmed.existsAsFile()) trimmed.deleteFile();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> out(trimmed.createOutputStream());
    if (!out) { BTLOG("createOutputStream failed (max)"); return srcFile; }

    const int bitsPerSample = 24;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(out.release(), sr, (unsigned int)reader->numChannels, bitsPerSample, {}, 0));
    if (!writer) { BTLOG("createWriterFor failed (max)"); return srcFile; }

    const bool okWrite = writer->writeFromAudioSampleBuffer(buffer, 0, (int)fullSamples);
    writer.reset();

    if (!okWrite || !trimmed.existsAsFile() || trimmed.getSize() <= 0)
    {
        BTLOG("write failed or empty (max); size=" + juce::String((int)trimmed.getSize()) + " -> use original");
        if (trimmed.existsAsFile()) trimmed.deleteFile();
        return srcFile;
    }

    if (auto r2 = openReader(trimmed))
    {
        juce::AudioBuffer<float> b2((int)r2->numChannels, (int)r2->lengthInSamples);
        const bool okRead2 = r2->read(&b2, 0, (int)r2->lengthInSamples, 0, true, true);
        if (okRead2) logBufferRMS("TRIMMED(max)", b2);
        else BTLOG("verify read of written file failed (max)");
    }

    BTLOG("trimmed(max): keptBars=" + juce::String((int)barsToKeep)
        + " full=" + juce::String((int)fullSamples)
        + " secs=" + juce::String((double)fullSamples / sr, 3)
        + " path=" + trimmed.getFullPathName());

    return trimmed;
#endif
}
