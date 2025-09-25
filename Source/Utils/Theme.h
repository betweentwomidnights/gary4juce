#pragma once
#include <JuceHeader.h>

namespace Theme 
{
    namespace Colors 
    {
        // Core brand colors based on gary4live_logo.png
        const auto Background = juce::Colours::black;
        const auto PrimaryRed = juce::Colour(0xffff0000);    // Bright red from logo
        const auto AccentRed = juce::Colours::darkred;       // Darker red variant
        const auto TextPrimary = juce::Colours::white;
        const auto TextSecondary = juce::Colour(0xffcccccc); // Light grey alternative
        
        // Tab colors (UNIFIED RED - consistent across all tabs)
        const auto Gary = PrimaryRed;
        const auto Jerry = PrimaryRed;   // Same red as Gary for consistency
        const auto Terry = PrimaryRed;   // Same red as Gary for consistency

        static const juce::Colour Darius = juce::Colour(0xffff1493);
        
        // UI elements (red/white/black only)
        const auto ButtonActive = PrimaryRed;
        const auto ButtonInactive = juce::Colour(0xff333333);  // Very dark grey, almost black
        const auto BorderColor = PrimaryRed;
        const auto HighlightColor = juce::Colours::white;
        
        // Waveform colors (brand-aligned)
        const auto WaveformFill = PrimaryRed;          // was lightblue
        const auto WaveformOutline = juce::Colours::white;  // keep white
        const auto PlaybackCursor = juce::Colours::white;   // was yellow
        
        // Status colors (using red variants instead of traffic light colors)
        const auto StatusActive = PrimaryRed;           // was lightgreen  
        const auto StatusError = juce::Colour(0xff800000);    // dark red, was red
        const auto StatusWarning = AccentRed;           // was orange
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