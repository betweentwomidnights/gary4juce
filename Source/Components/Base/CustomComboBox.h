#pragma once
#include <JuceHeader.h>
#include "../../Utils/Theme.h"

class CustomComboBox : public juce::ComboBox
{
public:
    CustomComboBox();
    ~CustomComboBox() override;

    // Easy theming interface
    void setThemeColors(juce::Colour background, juce::Colour text, juce::Colour outline);

private:
    void applyDefaultStyling();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomComboBox)
};