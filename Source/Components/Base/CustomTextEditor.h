#pragma once
#include <JuceHeader.h>
#include "../../Utils/Theme.h"

class CustomTextEditor : public juce::TextEditor
{
public:
    CustomTextEditor();
    ~CustomTextEditor() override;

    // Easy theming interface
    void setThemeColors(juce::Colour background, juce::Colour text, juce::Colour outline);
    void setPlaceholderText(const juce::String& text);

private:
    void applyDefaultStyling();
    juce::String placeholderText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomTextEditor)
};