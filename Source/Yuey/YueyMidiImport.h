// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

// Reads a .mid file into the plain ticks YueyMidiScore works from. Everything that can only
// be known from the file itself is decided here: more than one part, drums, bends and
// controllers, a meter that changes. Whatever isn't clean enough to write down exactly comes
// back as a short error, not as a quietly thinned-out clip.

#pragma once

#include <JuceHeader.h>

#include "YueyMidiScore.h"

namespace yueymidi
{
struct ReadResult
{
    bool ok = false;
    juce::String error;
    Clip clip;
};

ReadResult readMidiFile(const juce::File& file);

// "10 bars · 20s at 120 BPM", the one line a slot shows under a loaded file.
juce::String summaryText(const Score& score, int bpm);
}
