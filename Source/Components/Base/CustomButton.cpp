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

void CustomButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto buttonDown = getToggleState() || isDown();
    auto buttonOver = isMouseOver();
    auto buttonEnabled = isEnabled();
    
    // Get colors based on current style
    juce::Colour baseColour, textColour, borderColour;
    
    switch (currentStyle)
    {
        case ButtonStyle::Gary:
            baseColour = Theme::Colors::Gary;
            borderColour = Theme::Colors::Gary;
            textColour = Theme::Colors::TextPrimary;
            break;
            
        case ButtonStyle::Jerry:
            baseColour = Theme::Colors::Jerry;
            borderColour = Theme::Colors::Jerry;
            textColour = Theme::Colors::TextPrimary;
            break;
            
        case ButtonStyle::Terry:
            baseColour = Theme::Colors::Terry;
            borderColour = Theme::Colors::Terry;
            textColour = Theme::Colors::TextPrimary;
            break;

        case ButtonStyle::Darius:
            baseColour = Theme::Colors::Darius;  
            borderColour = Theme::Colors::Darius;
            textColour = Theme::Colors::TextPrimary;
            break;
            
        case ButtonStyle::Inactive:
            baseColour = Theme::Colors::ButtonInactive;
            borderColour = Theme::Colors::TextSecondary;
            textColour = Theme::Colors::TextSecondary;
            break;
            
        case ButtonStyle::Standard:
        default:
            baseColour = Theme::Colors::ButtonInactive;
            borderColour = Theme::Colors::PrimaryRed;
            textColour = Theme::Colors::TextPrimary;
            break;
    }
    
    // Adjust colors based on button state
    if (!buttonEnabled)
    {
        // Creative disabled state - diagonal stripes pattern
        baseColour = Theme::Colors::ButtonInactive.darker(0.3f);
        borderColour = Theme::Colors::TextSecondary.withAlpha(0.6f);
        textColour = Theme::Colors::TextSecondary.withAlpha(0.8f); // More visible than before
    }
    else if (buttonDown)
    {
        // Pressed state - invert colors for that industrial click feel
        baseColour = borderColour;
        borderColour = Theme::Colors::TextPrimary;
    }
    else if (buttonOver)
    {
        // Hover state - "light up" effect with bright background and dark text
        baseColour = borderColour.brighter(0.4f);  // Bright background
        borderColour = borderColour.brighter(0.6f);
        textColour = Theme::Colors::Background;     // Dark text on bright background
    }
    
    // Draw main button background - always sharp rectangles
    g.setColour(baseColour.withAlpha(0.8f));
    g.fillRect(bounds);
    
    // Add creative disabled state visual - diagonal stripe pattern
    if (!buttonEnabled)
    {
        g.setColour(Theme::Colors::TextSecondary.withAlpha(0.2f));
        for (int i = 0; i < bounds.getWidth() + bounds.getHeight(); i += 6)
        {
            g.drawLine(bounds.getX() + i, bounds.getY(), 
                      bounds.getX() + i - bounds.getHeight(), bounds.getBottom(), 1.0f);
        }
    }
    
    // Draw outer border - thick and bold
    g.setColour(borderColour);
    g.drawRect(bounds, 2);
    
    // Add inner highlight for depth and futuristic feel
    if (buttonEnabled && !buttonDown)
    {
        g.setColour(Theme::Colors::TextPrimary.withAlpha(0.1f));
        g.drawRect(bounds.reduced(2), 1);
    }
    
    // Draw icon or text - centered
    if (buttonIcon != nullptr)
    {
        // Draw icon - scale to fit nicely within button bounds
        auto iconBounds = bounds.reduced(8, 8);  // Leave some padding
        
        // Create a copy of the drawable for color modification
        auto iconCopy = buttonIcon->createCopy();
        iconCopy->replaceColour(juce::Colours::white, textColour);
        iconCopy->drawWithin(g, iconBounds.toFloat(), juce::RectanglePlacement::centred, 1.0f);
    }
    else if (getButtonText().isNotEmpty())
    {
        // Draw text - centered and bold
        g.setColour(textColour);
        g.setFont(Theme::Fonts::Body);
        g.drawText(getButtonText(), bounds, juce::Justification::centred, true);
    }
}

void CustomButton::setIcon(std::unique_ptr<juce::Drawable> icon)
{
    buttonIcon = std::move(icon);
    repaint();
}

void CustomButton::clearIcon()
{
    buttonIcon.reset();
    repaint();
}

void CustomButton::applyStyle()
{
    // Style is now handled in paint() method
    repaint();
}