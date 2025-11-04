#pragma once

#include <JuceHeader.h>
#include "Theme.h"

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel();

    // Set custom accent color for scrollbars (defaults to red)
    void setScrollbarAccentColour(juce::Colour colour);

    // Override scrollbar drawing
    void drawScrollbar(juce::Graphics& g,
                      juce::ScrollBar& scrollbar,
                      int x, int y, int width, int height,
                      bool isScrollbarVertical,
                      int thumbStartPosition,
                      int thumbSize,
                      bool isMouseOver,
                      bool isMouseDown) override;

private:
    juce::Colour accentColour = Theme::Colors::PrimaryRed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomLookAndFeel)
};
