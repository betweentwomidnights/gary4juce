#pragma once
#include <JuceHeader.h>

namespace Theme 
{
    namespace Colors 
    {
        // Tab-specific colors (extracted from current PluginEditor)
        const auto Gary = juce::Colours::darkred;
        const auto Jerry = juce::Colours::darkgreen; 
        const auto Terry = juce::Colours::darkblue;
        
        // UI element colors
        const auto Background = juce::Colours::black;
        const auto ButtonInactive = juce::Colours::darkgrey;
        const auto TextPrimary = juce::Colours::white;
        const auto TextSecondary = juce::Colours::lightgrey;
        
        // Waveform colors
        const auto WaveformFill = juce::Colours::lightblue;
        const auto WaveformOutline = juce::Colours::white;
        const auto PlaybackCursor = juce::Colours::yellow;
        
        // Status colors
        const auto StatusActive = juce::Colours::lightgreen;
        const auto StatusError = juce::Colours::red;
        const auto StatusWarning = juce::Colours::orange;
    }
    
    namespace Fonts
    {
        const auto HeaderLarge = juce::FontOptions(16.0f, juce::Font::bold);
        const auto Header = juce::FontOptions(14.0f, juce::Font::bold);
        const auto Body = juce::FontOptions(12.0f);
        const auto Small = juce::FontOptions(10.0f);
    }
    
    namespace Layout
    {
        // Button dimensions
        const int StandardButtonHeight = 40;
        const int SmallButtonHeight = 30;
        const int TabButtonHeight = 35;
        
        // Margins and spacing
        const int StandardMargin = 5;
        const int LargeMargin = 10;
        const int SmallMargin = 3;
        
        // Waveform display
        const int WaveformMinHeight = 80;
        const int WaveformMaxHeight = 180;
        
        // Tab areas
        const int TabAreaHeight = 40;
        const int ModelControlsMinHeight = 350;
        
        // Slider dimensions
        const int SliderTextBoxWidth = 60;
        const int SliderTextBoxHeight = 20;
    }
}