#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>

/**
 * BeatPrompts - Drum-focused prompt generation
 * Ported from BeatPrompts.swift with weighted randomization logic
 */
class BeatPrompts
{
public:
    BeatPrompts();

    /**
     * Main method: Weighted random selection favoring drum-focused prompts
     * 50% chance: drum descriptor + genre
     * 25% chance: technique + drum descriptor
     * 15% chance: genre + specific element
     * 10% chance: complex (technique + genre + drum descriptor)
     */
    juce::String getRandomPrompt();

    /**
     * Fallback method: Always ensures drums mentioned
     */
    juce::String getRandomDrumPrompt();

    /**
     * Category-specific selection
     */
    enum class Category
    {
        Genre,
        Rhythm,
        Instrumentation,
        Production,
        Hybrid,
        Simple,
        Drums,
        All
    };

    juce::String getRandomPrompt(Category category);

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
    std::vector<juce::String> genrePrompts;
    std::vector<juce::String> drumDescriptors;
    std::vector<juce::String> drumTechniques;
    std::vector<juce::String> drumElements;
    std::vector<juce::String> simpleDrumPrompts;
    std::vector<juce::String> rhythmPrompts;
    std::vector<juce::String> instrumentationPrompts;
    std::vector<juce::String> productionPrompts;

    juce::Random random;
};
