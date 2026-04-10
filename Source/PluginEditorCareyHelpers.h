#pragma once

#include <JuceHeader.h>
#include "PluginEditorTextHelpers.h"

namespace plugin_editor_detail
{
    struct CareyFailureInfo
    {
        juce::String userMessage;
        juce::String popupDetail;
    };

    inline CareyFailureInfo parseCareyFailureResponse(const juce::var& responseVar,
                                                      const juce::String& fallbackMessage)
    {
        CareyFailureInfo info;
        info.userMessage = fallbackMessage;
        info.popupDetail = fallbackMessage;

        auto* responseObj = responseVar.getDynamicObject();
        if (responseObj == nullptr)
            return info;

        const juce::String errorText = responseObj->getProperty("error").toString().trim();
        const juce::String technicalError = responseObj->getProperty("technical_error").toString().trim();
        const juce::String errorCode = responseObj->getProperty("error_code").toString().trim().toLowerCase();
        const bool retryable = static_cast<bool>(responseObj->getProperty("retryable"));
        const juce::String backendName = responseObj->getProperty("backend_name").toString().trim();

        info.userMessage = errorText.isNotEmpty() ? errorText : fallbackMessage;
        info.popupDetail = technicalError.isNotEmpty()
            ? technicalError
            : (errorText.isNotEmpty() ? errorText : fallbackMessage);

        if (errorCode == "acestep_backend_restarting")
        {
            juce::String backendLabel = "carey backend";
            if (backendName.equalsIgnoreCase("turbo"))
                backendLabel = "carey turbo backend";
            else if (backendName.equalsIgnoreCase("base"))
                backendLabel = "carey base backend";

            info.userMessage = backendLabel + " hit a GPU runtime error and is restarting. try again in about a minute.";
        }

        juce::String metadata;
        if (errorCode.isNotEmpty())
            metadata << "\nerror_code=" << errorCode;
        if (backendName.isNotEmpty())
            metadata << "\nbackend_name=" << backendName;
        if (retryable)
            metadata << "\nretryable=true";

        if (metadata.isNotEmpty() && !info.popupDetail.contains(metadata.trimStart()))
            info.popupDetail << metadata;

        return info;
    }

    inline int parseCareyProgressValue(const juce::var& rawProgressVar)
    {
        if (rawProgressVar.isInt())
        {
            return juce::jlimit(0, 99, static_cast<int>(rawProgressVar));
        }

        if (rawProgressVar.isDouble())
        {
            const double rawProgress = static_cast<double>(rawProgressVar);
            if (rawProgress <= 0.0)
                return 0;

            const double normalized = (rawProgress < 1.0) ? rawProgress : rawProgress / 100.0;
            return juce::jlimit(0, 99, juce::roundToInt(normalized * 100.0));
        }

        if (rawProgressVar.isBool())
            return static_cast<bool>(rawProgressVar) ? 99 : 0;

        const juce::String rawText = rawProgressVar.toString().trim();
        if (rawText.isEmpty())
            return -1;

        const double rawProgress = rawText.getDoubleValue();
        if (rawProgress <= 0.0)
            return 0;

        const bool looksFractional = rawText.containsChar('.');
        const double normalized = (looksFractional && rawProgress < 1.0) ? rawProgress : rawProgress / 100.0;
        return juce::jlimit(0, 99, juce::roundToInt(normalized * 100.0));
    }

    inline int parseCareyStepProgressFromText(const juce::String& text)
    {
        int bestPercent = -1;
        int searchFrom = 0;
        while (searchFrom < text.length())
        {
            const int slash = text.indexOfChar(searchFrom, '/');
            if (slash < 0)
                break;

            int leftStart = slash - 1;
            while (leftStart >= 0 && juce::CharacterFunctions::isDigit(text[leftStart]))
                --leftStart;

            int rightEnd = slash + 1;
            while (rightEnd < text.length() && juce::CharacterFunctions::isDigit(text[rightEnd]))
                ++rightEnd;

            if ((leftStart + 1) < slash && (slash + 1) < rightEnd)
            {
                const int currentStep = text.substring(leftStart + 1, slash).getIntValue();
                const int totalSteps = text.substring(slash + 1, rightEnd).getIntValue();
                if (totalSteps > 0 && currentStep >= 0 && currentStep <= totalSteps)
                {
                    bestPercent = juce::jmax(bestPercent,
                                             juce::jlimit(0, 99,
                                                          juce::roundToInt((double) currentStep * 100.0 / (double) totalSteps)));
                }
            }

            searchFrom = slash + 1;
        }

        return bestPercent;
    }

    inline int resolveCareyProgressPercent(const juce::var& rawProgressVar,
                                           const juce::String& queueMessage,
                                           bool allowTextFallback,
                                           int fallbackProgress)
    {
        const int parsedProgress = parseCareyProgressValue(rawProgressVar);
        if (parsedProgress >= 0)
            return parsedProgress;

        if (allowTextFallback)
        {
            const int textStepProgress = parseCareyStepProgressFromText(queueMessage);
            if (textStepProgress >= 0)
                return textStepProgress;
        }

        return juce::jlimit(0, 99, fallbackProgress);
    }
}
