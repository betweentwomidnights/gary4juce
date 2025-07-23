#pragma once
#include <JuceHeader.h>
#include "../../Utils/Theme.h"

class CustomButton : public juce::TextButton
{
public:
    enum class ButtonStyle
    {
        Gary,
        Jerry, 
        Terry,
        Standard,
        Inactive
    };

    CustomButton();
    CustomButton(const juce::String& buttonText);
    ~CustomButton() override;

    // Override paint for custom rendering
    void paint(juce::Graphics& g) override;

    // Easy theming interface
    void setButtonStyle(ButtonStyle style);
    void setCustomColors(juce::Colour buttonColour, juce::Colour textColour);

private:
    ButtonStyle currentStyle = ButtonStyle::Standard;
    
    void applyStyle();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomButton)
};