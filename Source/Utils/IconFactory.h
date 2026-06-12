// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <JuceHeader.h>
#include <memory>

class IconFactory
{
public:
    // Static factory methods for each icon type
    static std::unique_ptr<juce::Drawable> createCropIcon();
    static std::unique_ptr<juce::Drawable> createCheckConnectionIcon();
    static std::unique_ptr<juce::Drawable> createTrashIcon();
    static std::unique_ptr<juce::Drawable> createPlayIcon();
    static std::unique_ptr<juce::Drawable> createPauseIcon();
    static std::unique_ptr<juce::Drawable> createStopIcon();
    static std::unique_ptr<juce::Drawable> createHelpIcon();
    static std::unique_ptr<juce::Drawable> createDiscordIcon();
    static std::unique_ptr<juce::Drawable> createXIcon();
    static std::unique_ptr<juce::Drawable> createUploadIcon();
    
    // Binary resource loading
    static juce::Image loadLogoImage();
    
private:
    // Helper method for SVG creation
    static std::unique_ptr<juce::Drawable> createFromSvg(const char* svgData);
};