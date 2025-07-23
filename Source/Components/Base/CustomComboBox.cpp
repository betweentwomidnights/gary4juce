#include "CustomComboBox.h"

CustomComboBox::CustomComboBox()
{
    applyDefaultStyling();
}

CustomComboBox::~CustomComboBox()
{
}

void CustomComboBox::setThemeColors(juce::Colour background, juce::Colour text, juce::Colour outline)
{
    setColour(juce::ComboBox::backgroundColourId, background);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, outline);
    setColour(juce::ComboBox::arrowColourId, text);
    
    repaint();
}

void CustomComboBox::applyDefaultStyling()
{
    // Apply theme colors
    setThemeColors(
        Theme::Colors::ButtonInactive,
        Theme::Colors::TextPrimary,
        Theme::Colors::TextSecondary
    );
    
    // Set consistent font
    setFont(Theme::Fonts::Body);
}