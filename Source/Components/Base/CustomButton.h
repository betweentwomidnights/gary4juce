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
    
    // Icon support
    void setIcon(std::unique_ptr<juce::Drawable> icon);
    void clearIcon();

private:
    ButtonStyle currentStyle = ButtonStyle::Standard;
    std::unique_ptr<juce::Drawable> buttonIcon;
    
    void applyStyle();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomButton)
};