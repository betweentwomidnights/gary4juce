// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

// Turns imported MIDI (a melody clip and/or a chord clip) into YuE2's two-voice ABC.
//
// This is the C++ form of the upstream event compiler that the MIDI-import spikes used
// (YuE's skills/yue2-music/instrumental/scripts/compile_score.py), reading ticks instead of
// event lists. It deliberately depends on nothing but the standard library, so it can be
// tested without JUCE. Reading the .mid files is YueyMidiImport's job.
//
// What it accepts is narrow on purpose. It rejects what it can't write down exactly rather
// than guess: overlapping melody notes, rhythms off the power-of-two grid, chord shapes other
// than root-position major and minor triads, lanes of different length or meter.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace yueymidi
{
struct Note
{
    std::int64_t onset = 0;     // ticks
    std::int64_t duration = 0;  // ticks
    int pitch = 60;             // MIDI note number
};

// One imported file as the reader found it: ticks, not seconds, so the tempo of the file
// never matters. Beats are what we keep.
struct Clip
{
    int ppq = 0;
    std::vector<Note> notes;
    // The later of the file's end-of-track event and its last note. The length the user
    // exported is the length we use; there is no cropping and no looping.
    std::int64_t endTick = 0;
    // 0 when the file carries no time signature.
    int meterNumerator = 0;
    int meterDenominator = 0;
};

struct Score
{
    bool ok = false;
    std::string error;       // short and actionable; empty when ok
    std::string abc;
    int bars = 0;
    int meterNumerator = 4;
    int meterDenominator = 4;
    double seconds = 0.0;    // at the tempo it was built for
    bool hasMelody = false;
    bool hasChords = false;
};

// Builds the ABC for whichever lanes are given (either may be null, not both). bpm is the
// quarter-note tempo the score is written at; the notes keep their place in beats, so a
// different bpm changes how long the score lasts and nothing else. The fallback meter is
// used only for a lane whose file has none and which has no other lane to take one from.
Score buildScore(const Clip* melody, const Clip* chords, int bpm,
                 int fallbackMeterNumerator, int fallbackMeterDenominator);
}
