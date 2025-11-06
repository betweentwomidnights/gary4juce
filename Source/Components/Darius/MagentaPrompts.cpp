#include "MagentaPrompts.h"

MagentaPrompts::MagentaPrompts()
{
    // Initialize categories for cycling
    categories = { Category::Instrument, Category::Vibe, Category::Genre };

    // Initialize data pools (matching JS version)
    instruments = {
        "electric guitar", "acoustic guitar", "flamenco guitar", "bass guitar",
        "electric piano", "grand piano", "synth lead", "synth arpeggio",
        "violin", "cello", "trumpet", "saxophone", "clarinet",
        "drums", "808 drums", "live drums",
        "strings", "brass section", "hammond organ", "wurlitzer", "moog bass", "analog synth"
    };

    vibes = {
        "warmup", "afterglow", "sunrise", "midnight", "dusk", "twilight", "daybreak", "nocturne", "aurora", "ember",
        "neon", "chrome", "velvet", "glass", "granite", "desert", "oceanic", "skyline", "underground", "warehouse",
        "dreamy", "nostalgic", "moody", "uplifting", "mysterious", "energetic", "chill", "dark", "bright", "atmospheric",
        "spacey", "groovy", "ethereal", "glitchy", "dusty", "tape", "vintage", "hazy", "crystalline", "shimmer",
        "magnetic", "luminous", "starlit", "shadow", "smolder", "static", "drift", "bloom", "horizon"
    };

    genres = {
        "synthwave", "death metal", "lofi hiphop", "acid house", "techno", "ambient",
        "jazz", "blues", "rock", "pop", "electronic", "hip hop", "reggae", "folk",
        "classical", "funk", "soul", "disco", "dubstep", "drum and bass", "trance", "garage"
    };

    microGenres = {
        "breakbeat", "boom bap", "uk garage", "two step", "dub techno", "deep house",
        "lofi house", "minimal techno", "progressive house", "psytrance", "goa",
        "liquid dnb", "neurofunk", "glitch hop", "idm", "electro", "footwork",
        "phonk", "dark trap", "hyperpop", "darksynth", "chillwave", "vaporwave", "future garage"
    };

    genreQualifiers = {
        "deep", "dub", "dark", "melodic", "minimal", "uplifting", "lofi", "industrial", "retro", "neo"
    };

    genericTechniques = {
        "arpeggio", "ostinato", "staccato", "legato", "tremolo", "harmonics", "plucks", "pad", "chops"
    };

    // Initialize instrument descriptors map
    instrumentDescriptors = {
        {"electric guitar", {"palm-muted", "tremolo", "shoegaze", "chorused", "lead", "octave"}},
        {"acoustic guitar", {"fingerstyle", "nylon", "arpeggio", "strummed"}},
        {"flamenco guitar", {"rasgueado", "picado"}},
        {"bass guitar", {"slap", "picked", "sub", "syncopated"}},
        {"moog bass", {"sub", "resonant", "rubbery"}},
        {"analog synth", {"pad", "plucks", "supersaw", "arpeggio"}},
        {"synth lead", {"portamento", "supersaw", "mono"}},
        {"electric piano", {"rhodes", "chorused", "tine"}},
        {"wurlitzer", {"dirty", "tremolo"}},
        {"grand piano", {"felt", "upright", "arpeggio"}},
        {"hammond organ", {"leslie", "drawbar"}},
        {"strings", {"pizzicato", "ostinato", "legato"}},
        {"violin", {"pizzicato", "legato", "tremolo"}},
        {"cello", {"sul tasto", "legato", "pizzicato"}},
        {"trumpet", {"muted", "harmon"}},
        {"saxophone", {"breathy", "subtone"}},
        {"clarinet", {"staccato", "legato"}},
        {"drums", {"brushed", "breakbeat", "rimshot"}},
        {"808 drums", {"808", "trap"}},
        {"live drums", {"brushed", "tight", "roomy"}},
        {"brass section", {"stabs", "swell"}}
    };
}

juce::String MagentaPrompts::getNextCyclingStyle()
{
    auto cat = categories[currentCategoryIndex];
    currentCategoryIndex = (currentCategoryIndex + 1) % (int)categories.size();

    switch (cat)
    {
        case Category::Instrument: return getRandomInstrument();
        case Category::Vibe:       return getRandomVibe();
        case Category::Genre:      return getRandomGenre();
    }

    return getRandomInstrument();
}

juce::String MagentaPrompts::getRandomStyle()
{
    int r = random.nextInt(3);
    switch (r)
    {
        case 0: return getRandomInstrument();
        case 1: return getRandomVibe();
        case 2: return getRandomGenre();
        default: return getRandomInstrument();
    }
}

void MagentaPrompts::resetCycle()
{
    currentCategoryIndex = 0;
}

juce::String MagentaPrompts::getRandomInstrument()
{
    auto inst = oneOf(instruments);
    if (inst.isEmpty())
        inst = "electric guitar";

    if (chance(0.45))
    {
        // Try to get instrument-specific descriptor
        auto it = instrumentDescriptors.find(inst);
        if (it != instrumentDescriptors.end() && !it->second.empty())
        {
            auto specific = oneOf(it->second);
            if (specific.isNotEmpty())
            {
                juce::StringArray words;
                words.add(specific);
                words.add(inst);
                return clip1to3(words);
            }
        }

        // Fall back to generic technique
        auto tech = oneOf(genericTechniques);
        if (tech.isEmpty())
            tech = "arpeggio";

        juce::StringArray words;
        words.add(tech);
        words.add(inst);
        return clip1to3(words);
    }

    return inst;
}

juce::String MagentaPrompts::getRandomVibe()
{
    auto vibe = oneOf(vibes);
    return vibe.isEmpty() ? "warmup" : vibe;
}

juce::String MagentaPrompts::getRandomGenre()
{
    if (chance(0.65))
    {
        auto micro = oneOf(microGenres);
        return micro.isEmpty() ? "breakbeat" : micro;
    }
    else
    {
        // Filter out "jazz" from genres (matching JS logic)
        std::vector<juce::String> filteredGenres;
        for (const auto& g : genres)
            if (g.toLowerCase() != "jazz")
                filteredGenres.push_back(g);

        auto base = oneOf(filteredGenres);
        if (base.isEmpty())
            base = "electronic";

        if (chance(0.30))
        {
            auto q = oneOf(genreQualifiers);
            if (q.isEmpty())
                q = "deep";

            juce::StringArray words;
            words.add(q);
            words.add(base);
            return clip1to3(words);
        }

        return base;
    }
}

bool MagentaPrompts::chance(double probability)
{
    probability = juce::jlimit(0.0, 1.0, probability);
    return random.nextDouble() < probability;
}

template<typename T>
T MagentaPrompts::oneOf(const std::vector<T>& arr)
{
    if (arr.empty())
        return T();

    int idx = random.nextInt((int)arr.size());
    return arr[idx];
}

juce::String MagentaPrompts::clip1to3(const juce::StringArray& words, int maxWords)
{
    juce::StringArray result;

    // Split each word by spaces and flatten
    for (const auto& word : words)
    {
        auto parts = juce::StringArray::fromTokens(word, " ", "");
        for (const auto& part : parts)
            result.add(part);
    }

    // Take only first maxWords
    juce::StringArray clipped;
    for (int i = 0; i < juce::jmin(maxWords, result.size()); ++i)
        clipped.add(result[i]);

    return clipped.joinIntoString(" ");
}
