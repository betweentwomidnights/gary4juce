// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Stems/StemsRuntime.h"
#include "Components/Stems/StemsSettings.h"

stems::StemsService& Gary4juceAudioProcessorEditor::getStemsService()
{
    if (stemsService == nullptr)
        stemsService = std::make_shared<stems::StemsService>(activeGaryDataDirectory);
    return *stemsService;
}

void Gary4juceAudioProcessorEditor::showStemsSettings()
{
    if (!ensureGaryDataDirectoryAvailable(true))
        return;

    getStemsService();
    auto* panel = new StemsSettings(stemsService, getUpdatePreferences());

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(panel);
    options.dialogTitle = "stem separator";
    options.dialogBackgroundColour = juce::Colour(0x1e, 0x1e, 0x1e);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    trackEditorModalWindow(options.launchAsync());
}
