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
    // Use default JUCE slider rendering for now to preserve exact current appearance
    juce::Slider::paint(g);
}

void CustomSlider::setThemeColors(juce::Colour track, juce::Colour thumb, juce::Colour text)
{
    trackColour = track;
    thumbColour = thumb;
    textColour = text;
    
    // Update JUCE slider colors
    setColour(juce::Slider::trackColourId, trackColour);
    setColour(juce::Slider::thumbColourId, thumbColour);
    setColour(juce::Slider::textBoxTextColourId, textColour);
    
    repaint();
}