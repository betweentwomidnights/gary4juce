#include "CustomTextEditor.h"

CustomTextEditor::CustomTextEditor()
{
    applyDefaultStyling();
}

CustomTextEditor::~CustomTextEditor()
{
}

void CustomTextEditor::setThemeColors(juce::Colour background, juce::Colour text, juce::Colour outline)
{
    setColour(juce::TextEditor::backgroundColourId, background);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::outlineColourId, outline);
    setColour(juce::TextEditor::focusedOutlineColourId, text.brighter(0.5f));
    
    repaint();
}

void CustomTextEditor::setPlaceholderText(const juce::String& text)
{
    placeholderText = text;
    // JUCE TextEditor doesn't have built-in placeholder, but we preserve the interface for future enhancement
}

void CustomTextEditor::applyDefaultStyling()
{
    // Apply theme colors
    setThemeColors(
        juce::Colours::darkgrey.darker(),
        Theme::Colors::TextPrimary,
        Theme::Colors::TextSecondary
    );
    
    // Set consistent font
    setFont(Theme::Fonts::Body);
    
    // Apply multi-line support (common for prompts)
    setMultiLine(true);
    setReturnKeyStartsNewLine(true);
    setScrollbarsShown(true);
}