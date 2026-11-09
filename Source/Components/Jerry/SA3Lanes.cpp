// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "SA3Lanes.h"

#include <algorithm>

namespace sa3lanes
{
namespace
{
    const SA3UI::LoraSelection* find(const gary::SA3Lane& lane, const std::vector<SA3UI::LoraSelection>& active)
    {
        const auto name = juce::String(lane.path);
        const auto it = std::find_if(active.begin(), active.end(),
                                     [&name](const SA3UI::LoraSelection& lora) { return lora.name == name; });
        return it == active.end() ? nullptr : &*it;
    }
}

void sync(std::vector<gary::SA3Lane>& lanes, const std::vector<SA3UI::LoraSelection>& active)
{
    std::vector<gary::SA3Lane> synced;
    std::vector<bool> taken(lanes.size(), false);
    for (const auto& lora : active)
    {
        const auto path = lora.name.toStdString();
        const auto existing = std::find_if(lanes.begin(), lanes.end(),
                                           [&path](const gary::SA3Lane& lane) { return lane.path == path; });
        if (existing != lanes.end())
        {
            taken[static_cast<size_t>(existing - lanes.begin())] = true;
            synced.push_back(*existing);
            float amount = 0.f;
            if (flatAmount(synced.back(), amount) && amount > 0.f)
            {
                const auto strength = static_cast<float>(juce::jlimit(0.0, (double)gary::kSA3MaxLaneAmount, lora.strength));
                for (auto& point : synced.back().points)
                    point.amount = strength;
            }
            continue;
        }
        if (synced.size() >= gary::kSA3MaxLanes)
            continue;
        gary::SA3Lane lane;
        lane.name = path;
        lane.path = path;
        const auto strength = static_cast<float>(juce::jlimit(0.0, (double)gary::kSA3MaxLaneAmount, lora.strength));
        lane.points = { { 0.f, strength }, { 1.f, strength } };
        synced.push_back(std::move(lane));
    }
    for (size_t i = 0; i < lanes.size(); ++i)
        if (!taken[i] && !lanes[i].path.empty() && synced.size() < gary::kSA3MaxLanes)
            synced.push_back(std::move(lanes[i]));
    lanes = std::move(synced);
}

bool isActive(const gary::SA3Lane& lane, const std::vector<SA3UI::LoraSelection>& active)
{
    return find(lane, active) != nullptr;
}

double strengthOf(const gary::SA3Lane& lane, const std::vector<SA3UI::LoraSelection>& active)
{
    const auto* lora = find(lane, active);
    return lora != nullptr ? lora->strength : 0.0;
}

bool flatAmount(const gary::SA3Lane& lane, float& amount)
{
    const auto& points = lane.points;
    if (points.size() != 2 || points[0].position > 0.f || points[1].position < 1.f
        || points[0].amount != points[1].amount)
        return false;
    amount = points[0].amount;
    return true;
}

bool isDrawn(const gary::SA3Lane& lane)
{
    float amount = 0.f;
    return !flatAmount(lane, amount) || amount <= 0.f;
}

int addToRequest(juce::DynamicObject& request,
                 const State& state,
                 const std::vector<SA3UI::LoraSelection>& active,
                 const std::function<juce::String(const juce::String&)>& decoratePrompt,
                 double startSeconds,
                 double seconds)
{
    juce::StringArray laned;
    juce::Array<juce::var> lanes;
    for (const auto& lane : state.lanes)
    {
        if (!isActive(lane, active))
            continue;
        laned.add(juce::String(lane.path));
        if (std::none_of(lane.points.begin(), lane.points.end(),
                         [](const gary::SA3LanePoint& point) { return point.amount > 0.f; }))
            continue;

        juce::Array<juce::var> envelope;
        const auto flat = gary::LaneEnvelopeSeconds(lane.points, startSeconds, seconds);
        for (size_t i = 0; i + 1 < flat.size(); i += 2)
            envelope.add(juce::Array<juce::var> { (double)flat[i], (double)flat[i + 1] });

        // The curve is the strength: sa3.cpp multiplies the adapter's strength by it.
        juce::DynamicObject::Ptr lora = new juce::DynamicObject();
        lora->setProperty("name", juce::String(lane.path));
        lora->setProperty("strength", 1.0);

        juce::DynamicObject::Ptr entry = new juce::DynamicObject();
        entry->setProperty("loras", juce::Array<juce::var> { juce::var(lora.get()) });
        entry->setProperty("envelope", envelope);
        const auto prompt = juce::String(lane.prompt).trim();
        if (prompt.isNotEmpty())
            entry->setProperty("prompt", decoratePrompt ? decoratePrompt(prompt) : prompt);
        lanes.add(juce::var(entry.get()));
    }

    if (laned.isEmpty())
        return 0;

    juce::Array<juce::var> fixed;
    if (auto* loras = request.getProperty("loras").getArray())
        for (const auto& item : *loras)
            if (auto* lora = item.getDynamicObject(); lora == nullptr || !laned.contains(lora->getProperty("name").toString()))
                fixed.add(item);
    request.setProperty("loras", fixed);

    if (lanes.isEmpty())
        return 0;

    // Only a transform has a top-level strength: where its noise starts. Everything else starts at 1.
    const auto sigmaMax = request.hasProperty("strength")
        ? juce::jlimit(0.01f, 1.f, (float)(double)request.getProperty("strength")) : 1.f;
    const auto sigmas = gary::SA3StepSigmas((int)request.getProperty("steps"), sigmaMax,
                                            request.getProperty("shift").toString().toLowerCase().toStdString(),
                                            startSeconds + seconds);
    const auto blend = gary::LaneBlendSettings(state.blend, sigmas);
    request.setProperty("lora_lanes", lanes);
    request.setProperty("lane_isolation", (double)blend.isolation);
    request.setProperty("lane_isolation_until", (double)blend.isolationUntil);
    request.setProperty("lane_average_until", (double)blend.averageUntil);
    return lanes.size();
}

juce::var toVar(const State& state)
{
    juce::Array<juce::var> lanes;
    for (const auto& lane : state.lanes)
    {
        juce::Array<juce::var> points;
        for (const auto& point : lane.points)
            points.add(juce::Array<juce::var> { (double)point.position, (double)point.amount });
        juce::DynamicObject::Ptr entry = new juce::DynamicObject();
        entry->setProperty("name", juce::String(lane.path));
        entry->setProperty("prompt", juce::String(lane.prompt));
        entry->setProperty("points", points);
        lanes.add(juce::var(entry.get()));
    }
    juce::DynamicObject::Ptr object = new juce::DynamicObject();
    object->setProperty("open", state.open);
    object->setProperty("blend", (double)state.blend);
    object->setProperty("snap", static_cast<int>(state.snap));
    object->setProperty("lanes", lanes);
    return juce::var(object.get());
}

State fromVar(const juce::var& value)
{
    State state;
    auto* object = value.getDynamicObject();
    if (object == nullptr)
        return state;
    state.open = (bool)object->getProperty("open");
    state.blend = juce::jlimit(0.f, 1.f, (float)(double)object->getProperty("blend"));
    state.snap = gary::LaneSnapFromIndex((int)object->getProperty("snap"));
    if (auto* lanes = object->getProperty("lanes").getArray())
    {
        for (const auto& item : *lanes)
        {
            auto* entry = item.getDynamicObject();
            if (entry == nullptr || state.lanes.size() >= gary::kSA3MaxLanes)
                continue;
            gary::SA3Lane lane;
            lane.path = entry->getProperty("name").toString().trim().toStdString();
            lane.name = lane.path;
            lane.prompt = entry->getProperty("prompt").toString().toStdString();
            if (lane.path.empty())
                continue;
            if (auto* points = entry->getProperty("points").getArray())
            {
                for (const auto& point : *points)
                {
                    auto* pair = point.getArray();
                    if (pair == nullptr || pair->size() != 2 || lane.points.size() >= gary::kSA3MaxLanePoints)
                        continue;
                    lane.points.push_back({ juce::jlimit(0.f, 1.f, (float)(double)(*pair)[0]),
                                            juce::jlimit(0.f, gary::kSA3MaxLaneAmount, (float)(double)(*pair)[1]) });
                }
            }
            gary::SortLanePoints(lane.points);
            state.lanes.push_back(std::move(lane));
        }
    }
    return state;
}

std::vector<float> peaks(const juce::AudioBuffer<float>& audio, double sampleRate, double startSeconds, size_t columns)
{
    std::vector<float> result;
    const int total = audio.getNumSamples();
    if (audio.getNumChannels() <= 0 || total <= 0 || sampleRate <= 0.0 || columns == 0)
        return result;
    const int first = juce::jlimit(0, total, static_cast<int>(startSeconds * sampleRate));
    const int count = total - first;
    if (count <= 0)
        return result;
    result.assign(columns * 2, 0.f);
    for (size_t column = 0; column < columns; ++column)
    {
        const int from = first + static_cast<int>(static_cast<juce::int64>(count) * (juce::int64)column / (juce::int64)columns);
        const int to = first + static_cast<int>(static_cast<juce::int64>(count) * (juce::int64)(column + 1) / (juce::int64)columns);
        float low = 0.f;
        float high = 0.f;
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        {
            if (to <= from)
                continue;
            const auto range = juce::FloatVectorOperations::findMinAndMax(audio.getReadPointer(channel, from), to - from);
            low = std::min(low, range.getStart());
            high = std::max(high, range.getEnd());
        }
        result[column * 2] = juce::jlimit(-1.f, 1.f, low);
        result[column * 2 + 1] = juce::jlimit(-1.f, 1.f, high);
    }
    return result;
}
}
