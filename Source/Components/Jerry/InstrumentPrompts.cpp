#include "InstrumentPrompts.h"

InstrumentPrompts::InstrumentPrompts()
{
    // === Verified Base Genres (Tested & Reliable) ===
    verifiedBaseGenres = {
        "aggressive techno",
        "ambient electronic",
        "experimental electronic",
        "future bass",
        "liquid dnb",
        "synthwave",
        "chillwave",
        "neurofunk",
        "drone",
        "melodic dubstep"
    };

    // === Standalone Reliable Prompts ===
    standalonePrompts = {
        "aggressive techno",
        "melodic rap",
        "ambient electronic",
        "ethereal",
        "experimental electronic",
        "future bass",
        "synthwave",
        "chillwave",
        "melodic dubstep"
    };

    // === Instrument Descriptors ===
    instrumentDescriptors = {
        "bass",
        "chords",
        "melody",
        "pads"
    };

    // === Verified Combinations (Pre-tested) ===
    verifiedCombinations = {
        "drone bass",
        "neurofunk bass",
        "liquid dnb chords",
        "liquid dnb melody",
        "chillwave chords",
        "chillwave pads"
    };
}

juce::String InstrumentPrompts::getRandomGenrePrompt()
{
    int randomSeed = random.nextInt(juce::Range<int>(1, 101));

    // 40% chance: verified standalone prompts
    if (randomSeed <= 40)
    {
        auto prompt = oneOf(standalonePrompts);
        return prompt.isEmpty() ? "aggressive techno" : prompt;
    }
    // 35% chance: verified combinations
    else if (randomSeed <= 75)
    {
        auto prompt = oneOf(verifiedCombinations);
        return prompt.isEmpty() ? "drone bass" : prompt;
    }
    // 25% chance: base genre + descriptor
    else
    {
        auto baseGenre = oneOf(verifiedBaseGenres);
        if (baseGenre.isEmpty()) baseGenre = "synthwave";

        auto descriptor = oneOf(instrumentDescriptors);
        if (descriptor.isEmpty()) descriptor = "melody";

        return baseGenre + " " + descriptor;
    }
}

juce::String InstrumentPrompts::getWeightedGenrePrompt()
{
    int randomSeed = random.nextInt(juce::Range<int>(1, 101));

    // 50% chance: verified combinations (highest reliability)
    if (randomSeed <= 50)
    {
        auto prompt = oneOf(verifiedCombinations);
        return prompt.isEmpty() ? "drone bass" : prompt;
    }
    // 35% chance: standalone reliable prompts
    else if (randomSeed <= 85)
    {
        auto prompt = oneOf(standalonePrompts);
        return prompt.isEmpty() ? "aggressive techno" : prompt;
    }
    // 15% chance: generate new combination
    else
    {
        auto baseGenre = oneOf(verifiedBaseGenres);
        if (baseGenre.isEmpty()) baseGenre = "synthwave";

        auto descriptor = oneOf(instrumentDescriptors);
        if (descriptor.isEmpty()) descriptor = "melody";

        return baseGenre + " " + descriptor;
    }
}

juce::String InstrumentPrompts::getRandomPrompt(Category category)
{
    switch (category)
    {
        case Category::Standalone:
        {
            auto prompt = oneOf(standalonePrompts);
            return prompt.isEmpty() ? "aggressive techno" : prompt;
        }

        case Category::Bass:
            return getBassPrompt();

        case Category::Chords:
            return getChordsPrompt();

        case Category::Melody:
            return getMelodyPrompt();

        case Category::Pads:
            return getPadsPrompt();

        case Category::All:
            return getRandomGenrePrompt();

        default:
            return "aggressive techno";
    }
}

juce::String InstrumentPrompts::getBassPrompt()
{
    std::vector<juce::String> bassOptions;

    // Add verified bass combinations
    bassOptions.push_back("drone bass");
    bassOptions.push_back("neurofunk bass");

    // Add generated bass prompts from verified genres
    for (const auto& genre : verifiedBaseGenres)
        bassOptions.push_back(genre + " bass");

    auto prompt = oneOf(bassOptions);
    return prompt.isEmpty() ? "drone bass" : prompt;
}

juce::String InstrumentPrompts::getChordsPrompt()
{
    std::vector<juce::String> chordOptions;

    // Add verified chord combinations
    chordOptions.push_back("liquid dnb chords");
    chordOptions.push_back("chillwave chords");

    // Add generated chord prompts from verified genres
    for (const auto& genre : verifiedBaseGenres)
        chordOptions.push_back(genre + " chords");

    auto prompt = oneOf(chordOptions);
    return prompt.isEmpty() ? "liquid dnb chords" : prompt;
}

juce::String InstrumentPrompts::getMelodyPrompt()
{
    std::vector<juce::String> melodyOptions;

    // Add verified melody combination
    melodyOptions.push_back("liquid dnb melody");

    // Add generated melody prompts from verified genres
    for (const auto& genre : verifiedBaseGenres)
        melodyOptions.push_back(genre + " melody");

    auto prompt = oneOf(melodyOptions);
    return prompt.isEmpty() ? "liquid dnb melody" : prompt;
}

juce::String InstrumentPrompts::getPadsPrompt()
{
    std::vector<juce::String> padOptions;

    // Add verified pad combination
    padOptions.push_back("chillwave pads");

    // Add generated pad prompts from verified genres
    for (const auto& genre : verifiedBaseGenres)
        padOptions.push_back(genre + " pads");

    auto prompt = oneOf(padOptions);
    return prompt.isEmpty() ? "chillwave pads" : prompt;
}

juce::String InstrumentPrompts::getCleanInstrumentPrompt()
{
    // Use weighted selection for maximum reliability
    return getWeightedGenrePrompt();
}

std::vector<juce::String> InstrumentPrompts::getAllPrompts() const
{
    std::vector<juce::String> all;

    // Add all standalone prompts
    all.insert(all.end(), standalonePrompts.begin(), standalonePrompts.end());

    // Add all verified combinations
    all.insert(all.end(), verifiedCombinations.begin(), verifiedCombinations.end());

    // Add all possible genre + descriptor combinations
    for (const auto& genre : verifiedBaseGenres)
    {
        for (const auto& descriptor : instrumentDescriptors)
        {
            all.push_back(genre + " " + descriptor);
        }
    }

    return all;
}

bool InstrumentPrompts::chance(double probability)
{
    probability = juce::jlimit(0.0, 1.0, probability);
    return random.nextDouble() < probability;
}

template<typename T>
T InstrumentPrompts::oneOf(const std::vector<T>& arr)
{
    if (arr.empty())
        return T();

    int idx = random.nextInt((int)arr.size());
    return arr[idx];
}
