// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

/*
  ==============================================================================
    StemsSettings.h

    Settings panel for the embedded stem separator: install or remove the
    stems.cpp runtime, choose GPU or CPU, download and pick a model, and run a
    short test separation. Shown in a DialogWindow from the settings menu.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Base/CustomButton.h"
#include "../../Stems/StemsRuntime.h"

#include <memory>

class StemsSettings : public juce::Component,
                      private juce::Timer
{
public:
    static constexpr auto kModelPreference = "stemsModel";
    static constexpr auto kUseGpuPreference = "stemsUseGpu";

    StemsSettings(std::shared_ptr<stems::StemsService> service, juce::PropertiesFile& preferences);
    ~StemsSettings() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class ModelRow;

    void timerCallback() override;
    void refresh();
    void selectModel(const juce::String& modelId);
    juce::String selectedModelId() const;
    bool useGpu() const;

    std::shared_ptr<stems::StemsService> service;
    juce::PropertiesFile& preferences;

    juce::Label title, subtitle;
    juce::Label runtimeHeading, runtimeStatus;
    CustomButton runtimeButton;
    juce::Label processingHeading;
#if JUCE_MAC
    juce::ToggleButton gpuToggle { "use the GPU (Metal)" };
#else
    juce::ToggleButton gpuToggle { "use the GPU (Vulkan)" };
#endif
    juce::Label gpuHint;
    juce::Label modelsHeading;
    std::vector<std::unique_ptr<ModelRow>> modelRows;
    CustomButton testButton { "test" };
    CustomButton cancelButton { "cancel" };
    double progressValue = 0.0;
    juce::ProgressBar progressBar { progressValue };
    juce::Label jobStatus;
    juce::Label storageLabel;

    bool renderedRunning = false;   // whether the last refresh() drew a running job
    juce::String lastMessage;
    bool lastMessageIsError = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemsSettings)
};
