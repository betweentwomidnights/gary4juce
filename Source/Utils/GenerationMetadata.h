// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <JuceHeader.h>

namespace generation_metadata
{
// Keep request settings, never the uploaded audio or credentials. Audio hashes
// identify conditioning material without duplicating it in every JSON file.
inline juce::var sanitise(const juce::var& value)
{
    if (const auto* object = value.getDynamicObject())
    {
        auto result = juce::DynamicObject::Ptr(new juce::DynamicObject());
        for (const auto& property : object->getProperties())
        {
            const auto key = property.name.toString();
            const auto lower = key.toLowerCase();
            if (lower == "token" || lower == "hf_token" || lower == "access_token"
                || lower == "password" || lower == "authorization" || lower == "api_key")
                continue;
            if (lower == "audio_data" || lower == "audio_base64" || lower == "src_audio"
                || lower == "reference_audio" || lower == "init_audio" || lower == "audio"
                || lower == "source_audio" || lower == "input_audio")
            {
                const auto bytes = property.value.toString().toUTF8();
                result->setProperty(key + "_base64_sha256", juce::SHA256(bytes.getAddress(), bytes.sizeInBytes() - 1).toHexString());
            }
            else
                result->setProperty(property.name, sanitise(property.value));
        }
        return juce::var(result.get());
    }
    if (const auto* array = value.getArray())
    {
        juce::Array<juce::var> result;
        for (const auto& entry : *array) result.add(sanitise(entry));
        return result;
    }
    return value.clone();
}

inline juce::var snapshot(const juce::String& service, const juce::String& operation, const juce::var& request)
{
    auto result = juce::DynamicObject::Ptr(new juce::DynamicObject());
    result->setProperty("schema_version", 1);
    result->setProperty("application", "gary4juce");
    result->setProperty("application_version", ProjectInfo::versionString);
    result->setProperty("requested_at", juce::Time::getCurrentTime().toISO8601(true));
    result->setProperty("service", service);
    result->setProperty("operation", operation);
    const auto parameters = sanitise(request);
    result->setProperty("parameters", parameters);
    for (auto key : { "model_name", "synth_model", "model", "model_type" })
        if (parameters.getProperty(key, {}).toString().isNotEmpty()) { result->setProperty("model", parameters[key]); break; }
    if (!result->hasProperty("model"))
    {
        const auto model = service == "terry" ? "facebook/melodyflow-t24-30secs"
            : service == "sa3" ? "stabilityai/stable-audio-3-medium"
            : service == "foundation" ? "Foundation-1"
            : service == "yuey" ? "YuE2-3.6B" : "";
        result->setProperty("model", model);
        result->setProperty("model_source", "service_default");
    }
    for (auto key : { "prompt", "description", "custom_prompt", "caption" })
        if (parameters.getProperty(key, {}).toString().isNotEmpty()) { result->setProperty("prompt", parameters[key]); break; }
    const auto seed = parameters.getProperty("seed", juce::var(-1));
    result->setProperty("seed", seed.toString().getLargeIntValue() >= 0 ? seed : juce::var());
    result->setProperty("seed_source", seed.toString().getLargeIntValue() >= 0 ? "request" : "unreported");
    result->setProperty("edits", juce::Array<juce::var>());
    return juce::var(result.get());
}

inline void setSeed(juce::var& metadata, const juce::var& seed)
{
    const auto text = seed.toString().trim();
    if (auto* object = metadata.getDynamicObject())
        if (text.isNotEmpty() && text.containsOnly("0123456789"))
        {
            object->setProperty("seed", seed);
            object->setProperty("seed_source", "backend");
        }
}

inline juce::var cropped(const juce::var& original, double start, double duration)
{
    auto result = original.clone();
    if (auto* object = result.getDynamicObject())
    {
        auto edits = object->getProperty("edits").clone();
        auto edit = juce::DynamicObject::Ptr(new juce::DynamicObject());
        edit->setProperty("operation", "crop");
        edit->setProperty("relative_to", "previous_output");
        edit->setProperty("start_seconds", start);
        edit->setProperty("duration_seconds", duration);
        edits.append(juce::var(edit.get()));
        object->setProperty("edits", edits);
    }
    return result;
}
}
