#pragma once
#include <JuceHeader.h>
#include "../../Utils/Theme.h"

class CustomSlider : public juce::Slider
{
public:
    CustomSlider();
    ~CustomSlider() override;

    void paint(juce::Graphics& g) override;
    
    // Easy theming interface
    void setThemeColors(juce::Colour track, juce::Colour thumb, juce::Colour text);

private:
    juce::Colour trackColour = Theme::Colors::ButtonInactive;
    juce::Colour thumbColour = Theme::Colors::TextPrimary;
    juce::Colour textColour = Theme::Colors::TextSecondary;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomSlider)
};