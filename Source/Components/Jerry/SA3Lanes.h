// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <JuceHeader.h>

#include "SA3LaneModel.h"
#include "SA3UI.h"

#include <functional>
#include <vector>

// SA3 LoRA automation lanes: the parts that aren't drawing. A lane belongs to one LoRA, by its
// catalog name (kept in SA3Lane::path, which the shared model uses as the lane's identity).
namespace sa3lanes
{
    struct State
    {
        std::vector<gary::SA3Lane> lanes;
        gary::SA3LaneBlend blend = gary::SA3LaneBlend::OneSong;
        gary::SA3LaneSnap snap = gary::SA3LaneSnap::Off;
        bool open = false;
    };

    // Gives every active LoRA a lane, in list order. A new lane starts flat at its slider's value, so
    // it changes nothing until you draw. A lane whose LoRA was switched off or removed is kept after
    // the others, so turning it back on returns its curve.
    void sync(std::vector<gary::SA3Lane>& lanes, const std::vector<SA3UI::LoraSelection>& active);

    bool isActive(const gary::SA3Lane& lane, const std::vector<SA3UI::LoraSelection>& active);
    double strengthOf(const gary::SA3Lane& lane, const std::vector<SA3UI::LoraSelection>& active);

    // Adds lora_lanes and the blend to a request whose "loras" already holds the fixed-strength LoRAs.
    // A lane replaces its LoRA's fixed strength, which would otherwise run in every pass too. A curve
    // that stays at 0 is left out, since it adds nothing but would still encode its prompt.
    // decoratePrompt adds the tempo and key, as the request's own prompt has them. Envelope times
    // start at startSeconds, which a continuation sets to the source's length. Returns the number
    // of lanes sent.
    int addToRequest(juce::DynamicObject& request,
                     const State& state,
                     const std::vector<SA3UI::LoraSelection>& active,
                     const std::function<juce::String(const juce::String&)>& decoratePrompt,
                     double startSeconds,
                     double seconds);

    juce::var toVar(const State& state);
    State fromVar(const juce::var& value);

    // Min/max pairs across the audio from startSeconds on, for drawing behind the curves.
    std::vector<float> peaks(const juce::AudioBuffer<float>& audio, double sampleRate,
                             double startSeconds, size_t columns = 512);
}
