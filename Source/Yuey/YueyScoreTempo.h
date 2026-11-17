// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <JuceHeader.h>

namespace yuey
{
// Replace the header tempo, preserving the beat unit, notes and line endings.
inline juce::String retimeAbcTempo(const juce::String& abc, double bpm)
{
    if (!std::isfinite(bpm) || bpm <= 0.0)
        return abc;
    for (int start = 0; start < abc.length();)
    {
        const int newline = abc.indexOfChar(start, '\n');
        const int end = newline < 0 ? abc.length() : newline;
        const auto line = abc.substring(start, end);
        const int equals = line.indexOfChar('=');
        if (line.trimStart().startsWith("Q:") && equals >= 0)
        {
            const int valueEnd = line.endsWithChar('\r') ? end - 1 : end;
            return abc.substring(0, start + equals + 1)
                + juce::String(juce::roundToInt(bpm)) + abc.substring(valueEnd);
        }
        start = end + 1;
    }
    return abc;
}
} // namespace yuey
