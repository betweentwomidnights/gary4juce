#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>

/**
 * InstrumentPrompts - Instrument/genre prompt generation
 * Ported from InstrumentPrompts.swift with verified combinations
 */
class InstrumentPrompts
{
public:
    InstrumentPrompts();

    /**
     * Main method: Weighted selection prioritizing verified prompts
     * 40% chance: verified standalone prompts
     * 35% chance: verified combinations
     * 25% chance: base genre + descriptor
     */
    juce::String getRandomGenrePrompt();

    /**
     * Weighted selection (prioritizes proven prompts)
     * 50% chance: verified combinations
     * 35% chance: standalone reliable prompts
     * 15% chance: new combination from verified parts
     */
    juce::String getWeightedGenrePrompt();

    /**
     * Category-specific selection
     */
    enum class Category
    {
        Standalone,
        Bass,
        Chords,
        Melody,
        Pads,
        All
    };

    juce::String getRandomPrompt(Category category);

    /**
     * Target specific instrument types
     */
    juce::String getBassPrompt();
    juce::String getChordsPrompt();
    juce::String getMelodyPrompt();
    juce::String getPadsPrompt();

    /**
     * Legacy method for compatibility
     */
    juce::String getCleanInstrumentPrompt();

    /**
     * Get all prompts combined (for "all" category)
     */
    std::vector<juce::String> getAllPrompts() const;

private:
    // Utility functions
    bool chance(double probability);
    template<typename T>
    T oneOf(const std::vector<T>& arr);

    // Data pools (matching Swift version)
    std::vector<juce::String> verifiedBaseGenres;
    std::vector<juce::String> standalonePrompts;
    std::vector<juce::String> instrumentDescriptors;
    std::vector<juce::String> verifiedCombinations;

    juce::Random random;
};
