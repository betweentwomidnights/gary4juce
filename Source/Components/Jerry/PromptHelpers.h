#pragma once

#include <JuceHeader.h>

namespace PromptHelpers
{
    /**
     * Removes duplicate words from a prompt string (case-insensitive).
     * Example: "trap trap drums" -> "trap drums"
     */
    static juce::String deduplicateWords(juce::String s)
    {
        juce::StringArray tokens;
        tokens.addTokens(s, " \t", "");
        tokens.removeEmptyStrings(true);

        juce::StringArray unique;
        for (const auto& token : tokens)
        {
            // Case-insensitive check to catch "Trap trap" style duplicates
            bool alreadyHas = false;
            for (const auto& existing : unique)
            {
                if (existing.equalsIgnoreCase(token))
                {
                    alreadyHas = true;
                    break;
                }
            }
            if (!alreadyHas)
                unique.add(token);
        }

        return unique.joinIntoString(" ");
    }

    /**
     * Filters out inappropriate words based on loop type.
     * - Drums mode (loopTypeIndex=1): removes instrument words like "synth", "bass", "guitar"
     * - Instruments mode (loopTypeIndex=2): removes drum words like "drums", "kick", "snare"
     * - Auto mode (loopTypeIndex=0) or smart loop disabled: no filtering
     */
    static juce::String filterWordsForLoopType(juce::String s, int loopTypeIndex, bool smartLoopEnabled)
    {
        // Only filter if smart loop is enabled and we're in drums or instr mode
        if (!smartLoopEnabled || loopTypeIndex == 0)
            return s;

        juce::StringArray tokens;
        tokens.addTokens(s, " \t", "");
        tokens.removeEmptyStrings(true);

        juce::StringArray filtered;

        // Define filter lists - add more as needed
        static const juce::StringArray instrumentWords =
            {"synth", "bass", "guitar", "piano", "keys", "keyboard", "pad", "lead",
             "melody", "melodic", "chord", "chords", "arp", "arpeggiated", "arpeggiation"};

        static const juce::StringArray drumWords =
            {"drums", "drum", "kick", "snare", "hihat", "hi-hat", "hat", "hats",
             "cymbal", "cymbals", "percussion", "percussive", "beat", "beats"};

        for (const auto& token : tokens)
        {
            bool shouldKeep = true;
            auto lowerToken = token.toLowerCase();

            if (loopTypeIndex == 1) // drums mode - filter out instrument words
            {
                shouldKeep = !instrumentWords.contains(lowerToken);
            }
            else if (loopTypeIndex == 2) // instruments mode - filter out drum words
            {
                shouldKeep = !drumWords.contains(lowerToken);
            }

            if (shouldKeep)
                filtered.add(token);
        }

        return filtered.joinIntoString(" ");
    }

    /**
     * Removes very short words (2 chars or less) that might be orphaned fragments.
     * Example: "hip hop drums" -> keeps all; "drums hip bass" -> "drums bass"
     * This catches cases where "hip" appears without "hop" or similar fragments.
     */
    static juce::String removeShortOrphans(juce::String s)
    {
        juce::StringArray tokens;
        tokens.addTokens(s, " \t", "");
        tokens.removeEmptyStrings(true);

        // Only remove short words if the prompt has enough other words
        // Don't want to return empty if we only had short words
        juce::StringArray longWords;
        for (const auto& token : tokens)
        {
            if (token.length() > 2)
                longWords.add(token);
        }

        // If we'd end up with nothing, just return original
        if (longWords.isEmpty() && !tokens.isEmpty())
            return s;

        return longWords.joinIntoString(" ");
    }

    /**
     * Main cleanup function - applies all cleanup steps in order.
     * Call this on the final prompt string before returning to user.
     */
    static juce::String cleanupPrompt(juce::String prompt, int loopTypeIndex, bool smartLoopEnabled)
    {
        if (prompt.isEmpty())
            return prompt;

        // 1. Filter inappropriate words based on loop type
        prompt = filterWordsForLoopType(prompt, loopTypeIndex, smartLoopEnabled);

        // 2. Deduplicate words
        prompt = deduplicateWords(prompt);

        // 3. Remove very short orphan words (optional - comment out if too aggressive)
        prompt = removeShortOrphans(prompt);

        return prompt.trim();
    }

} // namespace PromptHelpers
