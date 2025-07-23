#include "CustomButton.h"

CustomButton::CustomButton()
{
    applyStyle();
}

CustomButton::CustomButton(const juce::String& buttonText) : juce::TextButton(buttonText)
{
    applyStyle();
}

CustomButton::~CustomButton()
{
}

void CustomButton::setButtonStyle(ButtonStyle style)
{
    currentStyle = style;
    applyStyle();
}

void CustomButton::setCustomColors(juce::Colour buttonColour, juce::Colour textColour)
{
    setColour(juce::TextButton::buttonColourId, buttonColour);
    setColour(juce::TextButton::textColourOffId, textColour);
    setColour(juce::TextButton::textColourOnId, textColour);
}

void CustomButton::applyStyle()
{
    switch (currentStyle)
    {
        case ButtonStyle::Gary:
            setCustomColors(Theme::Colors::Gary, Theme::Colors::TextPrimary);
            break;
            
        case ButtonStyle::Jerry:
            setCustomColors(Theme::Colors::Jerry, Theme::Colors::TextPrimary);
            break;
            
        case ButtonStyle::Terry:
            setCustomColors(Theme::Colors::Terry, Theme::Colors::TextPrimary);
            break;
            
        case ButtonStyle::Inactive:
            setCustomColors(Theme::Colors::ButtonInactive, Theme::Colors::TextSecondary);
            break;
            
        case ButtonStyle::Standard:
        default:
            setCustomColors(Theme::Colors::ButtonInactive, Theme::Colors::TextPrimary);
            break;
    }
    
    // TODO: Apply font styling through LookAndFeel later
   // setFont(Theme::Fonts::Body, false);  // Comment out for now
}