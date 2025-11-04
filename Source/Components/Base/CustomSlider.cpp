#include "CustomSlider.h"

CustomSlider::CustomSlider()
{
    // Apply default JUCE slider styling that matches current PluginEditor
    setSliderStyle(juce::Slider::LinearHorizontal);
    setTextBoxStyle(juce::Slider::TextBoxRight, false, Theme::Layout::SliderTextBoxWidth, Theme::Layout::SliderTextBoxHeight);
    
    // Set default colors to match current styling
    setColour(juce::Slider::textBoxTextColourId, textColour);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::grey);
}

CustomSlider::~CustomSlider()
{
}

void CustomSlider::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto textBoxWidth = getTextBoxWidth();
    auto textBoxHeight = getTextBoxHeight();
    
    // Calculate slider area (excluding text box)
    auto sliderBounds = bounds;
    if (getTextBoxPosition() == juce::Slider::TextBoxRight)
        sliderBounds.removeFromRight(textBoxWidth + 5);
    else if (getTextBoxPosition() == juce::Slider::TextBoxLeft)
        sliderBounds.removeFromLeft(textBoxWidth + 5);
    
    // Make track thick and rectangular - almost same height as thumb
    auto trackHeight = 12;  // Chunky track height
    auto thumbSize = 16;    // Square thumb size
    
    auto trackBounds = sliderBounds.withSizeKeepingCentre(sliderBounds.getWidth(), trackHeight);
    
    // Calculate thumb position
    auto sliderRange = getMaximum() - getMinimum();
    auto currentValue = getValue() - getMinimum();
    auto normalizedValue = currentValue / sliderRange;
    auto thumbX = trackBounds.getX() + (normalizedValue * (trackBounds.getWidth() - thumbSize));
    auto thumbBounds = juce::Rectangle<int>(thumbX, trackBounds.getCentreY() - thumbSize/2, thumbSize, thumbSize);
    
    // Draw track background (unfilled) - dark grunge
    g.setColour(Theme::Colors::Background.brighter(0.1f));  // Very slightly lighter than black
    g.fillRect(trackBounds);
    
    // Draw thin white border around track to match textbox outlines
    g.setColour(Theme::Colors::TextPrimary);
    g.drawRect(trackBounds, 1);
    
    // Draw filled portion of track - accent color
    auto filledWidth = thumbX + (thumbSize / 2) - trackBounds.getX();
    auto filledBounds = trackBounds.withWidth(juce::jmax(0, (int)filledWidth));

    if (filledWidth > 0)
    {
        g.setColour(accentColour);
        g.fillRect(filledBounds);

        // Add subtle inner glow to filled area
        g.setColour(accentColour.brighter(0.3f));
        g.drawRect(filledBounds.reduced(1), 1);
    }

    // Draw rectangular thumb - sharp edges, high contrast
    g.setColour(thumbColour);
    g.fillRect(thumbBounds);

    // Add accent border to thumb for that grungy edge
    g.setColour(accentColour);
    g.drawRect(thumbBounds, 2);
    
    // Add inner highlight for dimension
    g.setColour(Theme::Colors::TextPrimary.withAlpha(0.8f));
    g.drawRect(thumbBounds.reduced(2), 1);
    
    // Note: Text box is handled separately by JUCE's component system
    // We only need to draw the slider track and thumb here
}

void CustomSlider::setThemeColors(juce::Colour track, juce::Colour accent, juce::Colour thumb, juce::Colour text)
{
    trackColour = track;
    accentColour = accent;
    thumbColour = thumb;
    textColour = text;

    // Update JUCE slider colors
    setColour(juce::Slider::trackColourId, trackColour);
    setColour(juce::Slider::thumbColourId, thumbColour);
    setColour(juce::Slider::textBoxTextColourId, textColour);

    repaint();
}