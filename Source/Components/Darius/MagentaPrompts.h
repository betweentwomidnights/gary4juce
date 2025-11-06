#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>

/**
 * MagentaPrompts - C++ port of magenta_prompts.js
 * Generates semi-random music prompts with cycling categories:
 * instrument → vibe → genre
 */
class MagentaPrompts
{
public:
    MagentaPrompts();

    /**
     * Get the next prompt in the cycling sequence.
     * Cycles through: instrument → vibe → genre
     */
    juce::String getNextCyclingStyle();

    /**
     * Get a completely random style (ignores cycling)
     */
    juce::String getRandomStyle();

    /**
     * Reset the cycling index to start from instruments
     */
    void resetCycle();

private:
    // Category generators
    juce::String getRandomInstrument();
    juce::String getRandomVibe();
    juce::String getRandomGenre();

    // Utility functions
    bool chance(double probability);
    template<typename T>
    T oneOf(const std::vector<T>& arr);
    juce::String clip1to3(const juce::StringArray& words, int maxWords = 3);

    // Data pools
    std::vector<juce::String> instruments;
    std::vector<juce::String> vibes;
    std::vector<juce::String> genres;
    std::vector<juce::String> microGenres;
    std::vector<juce::String> genreQualifiers;
    std::vector<juce::String> genericTechniques;
    std::map<juce::String, std::vector<juce::String>> instrumentDescriptors;

    // Cycling state
    enum class Category { Instrument, Vibe, Genre };
    int currentCategoryIndex = 0;
    std::vector<Category> categories;

    juce::Random random;
};
