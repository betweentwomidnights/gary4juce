#include "BeatPrompts.h"

BeatPrompts::BeatPrompts()
{
    // === Genre-Based Prompts ===
    genrePrompts = {
        // Hip-hop
        "trap beat", "boom bap hip hop", "UK drill beat", "old school hip hop",
        "lo-fi hip hop", "jazz hip hop", "phonk beat", "drill beat",
        "cloud rap beat", "underground hip hop",

        // EDM/Electronic
        "house beat", "deep house", "tech house", "progressive house",
        "techno beat", "minimal techno", "acid techno", "drum and bass",
        "liquid drum and bass", "neurofunk", "dubstep beat", "future bass",
        "trap EDM", "garage beat", "UK garage", "trance beat",
        "progressive trance", "psytrance", "ambient techno", "breakbeat",
        "big beat", "jungle beat", "hardcore techno", "gabber beat",
        "synthwave", "chillwave", "downtempo", "trip-hop", "glitch hop",
        "electro funk"
    };

    // === Drum-Specific Descriptors ===
    drumDescriptors = {
        "crispy drums", "punchy drums", "hard-hitting drums",
        "vintage drums", "analog drums", "digital drums",
        "compressed drums", "reverb drums", "dry drums",
        "filtered drums", "pitched drums", "chopped drums",
        "layered drums", "minimal drums", "complex drums",
        "bouncy drums", "tight drums", "loose drums",
        "heavy drums", "light drums", "driving drums",
        "snappy drums", "booming drums", "clean drums",
        "gritty drums", "warm drums", "cold drums",
        "fat drums", "thin drums", "wide drums"
    };

    // === Drum Techniques ===
    drumTechniques = {
        "side-chained", "compressed", "saturated",
        "bit-crushed", "filtered", "pitched down",
        "pitched up", "reversed", "chopped", "stuttered",
        "gated", "distorted", "overdrive", "tape-saturated"
    };

    // === Specific Drum Elements ===
    drumElements = {
        "kick pattern", "snare hits", "hi-hat rolls",
        "808 slides", "rim shots", "clap pattern",
        "cymbal crashes", "tom fills", "percussion loop",
        "drum fills", "beat drops", "drum breaks"
    };

    // === Simple Drum-Focused Prompts ===
    simpleDrumPrompts = {
        "hard drums", "soft drums", "punchy beat", "bouncy rhythm",
        "driving beat", "laid-back drums", "aggressive drums", "smooth beat",
        "tight rhythm", "loose groove", "minimal drums", "complex beat",
        "simple rhythm", "drum loop", "beat pattern", "percussion"
    };

    // === Rhythm Prompts ===
    rhythmPrompts = {
        "syncopated drum pattern", "straight drum beat", "polyrhythmic drums",
        "shuffle rhythm", "half-time drums", "double-time beat",
        "triplet groove", "ghost note pattern", "tight drum programming",
        "loose drum feel", "quantized drums", "swing drums",
        "four-on-the-floor", "breakbeat pattern", "complex rhythm"
    };

    // === Instrumentation Prompts ===
    instrumentationPrompts = {
        "808 drums", "analog drums", "live drums", "vintage drums",
        "electronic drums", "trap 808s", "heavy 808 bass", "punchy kick drum",
        "crisp snare", "vinyl samples", "jazz samples", "soul samples",
        "orchestral samples", "synthesizer bass", "analog synth"
    };

    // === Production Prompts ===
    productionPrompts = {
        "heavy compression", "analog warmth", "digital crisp", "vinyl crackle",
        "tape saturation", "clean production", "gritty texture", "reverb-heavy",
        "dry mix", "stereo-wide", "mono drums", "distorted drums",
        "filtered drums", "pitched drums", "chopped samples"
    };
}

juce::String BeatPrompts::getRandomPrompt()
{
    int randomSeed = random.nextInt(juce::Range<int>(1, 101));

    // 50% chance: drum descriptor + genre
    if (randomSeed <= 50)
    {
        auto drumDesc = oneOf(drumDescriptors);
        if (drumDesc.isEmpty()) drumDesc = "punchy drums";

        auto genre = oneOf(genrePrompts);
        if (genre.isEmpty()) genre = "hip hop";

        return drumDesc + " " + genre;
    }
    // 25% chance: technique + drum descriptor
    else if (randomSeed <= 75)
    {
        auto technique = oneOf(drumTechniques);
        if (technique.isEmpty()) technique = "compressed";

        auto drumDesc = oneOf(drumDescriptors);
        if (drumDesc.isEmpty()) drumDesc = "drums";

        return technique + " " + drumDesc;
    }
    // 15% chance: genre + specific element
    else if (randomSeed <= 90)
    {
        auto genre = oneOf(genrePrompts);
        if (genre.isEmpty()) genre = "trap";

        auto element = oneOf(drumElements);
        if (element.isEmpty()) element = "kick pattern";

        return genre + " " + element;
    }
    // 10% chance: complex (3 elements)
    else
    {
        auto technique = oneOf(drumTechniques);
        if (technique.isEmpty()) technique = "compressed";

        auto genre = oneOf(genrePrompts);
        if (genre.isEmpty()) genre = "hip hop";

        auto drumDesc = oneOf(drumDescriptors);
        if (drumDesc.isEmpty()) drumDesc = "drums";

        return technique + " " + genre + " " + drumDesc;
    }
}

juce::String BeatPrompts::getRandomDrumPrompt()
{
    auto basePrompt = getRandomPrompt();

    // Check if prompt contains drum-related words
    juce::StringArray drumWords = { "drum", "beat", "kick", "snare", "808", "percussion" };
    bool containsDrumWord = false;

    for (const auto& word : drumWords)
    {
        if (basePrompt.toLowerCase().contains(word))
        {
            containsDrumWord = true;
            break;
        }
    }

    // If no drum word found, append one
    if (!containsDrumWord)
    {
        juce::StringArray drumEnders = { "drums", "beat", "percussion" };
        auto drumEnder = oneOf(drumEnders);
        if (drumEnder.isEmpty()) drumEnder = "drums";

        return basePrompt + " " + drumEnder;
    }

    return basePrompt;
}

juce::String BeatPrompts::getRandomPrompt(Category category)
{
    switch (category)
    {
        case Category::Genre:
            return oneOf(genrePrompts);

        case Category::Rhythm:
            return oneOf(rhythmPrompts);

        case Category::Instrumentation:
            return oneOf(instrumentationPrompts);

        case Category::Production:
            return oneOf(productionPrompts);

        case Category::Hybrid:
            return getRandomPrompt();  // Use weighted logic

        case Category::Simple:
            return oneOf(simpleDrumPrompts);

        case Category::Drums:
            return oneOf(drumDescriptors);

        case Category::All:
            return getRandomDrumPrompt();  // Always ensure drums

        default:
            return "hip hop beat";
    }
}

std::vector<juce::String> BeatPrompts::getAllPrompts() const
{
    std::vector<juce::String> all;

    // Combine all prompt pools
    all.insert(all.end(), genrePrompts.begin(), genrePrompts.end());
    all.insert(all.end(), drumDescriptors.begin(), drumDescriptors.end());
    all.insert(all.end(), drumTechniques.begin(), drumTechniques.end());
    all.insert(all.end(), drumElements.begin(), drumElements.end());
    all.insert(all.end(), simpleDrumPrompts.begin(), simpleDrumPrompts.end());
    all.insert(all.end(), rhythmPrompts.begin(), rhythmPrompts.end());
    all.insert(all.end(), instrumentationPrompts.begin(), instrumentationPrompts.end());
    all.insert(all.end(), productionPrompts.begin(), productionPrompts.end());

    return all;
}

bool BeatPrompts::chance(double probability)
{
    probability = juce::jlimit(0.0, 1.0, probability);
    return random.nextDouble() < probability;
}

template<typename T>
T BeatPrompts::oneOf(const std::vector<T>& arr)
{
    if (arr.empty())
        return T();

    int idx = random.nextInt((int)arr.size());
    return arr[idx];
}
