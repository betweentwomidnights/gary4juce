#include "CustomLookAndFeel.h"

CustomLookAndFeel::CustomLookAndFeel()
{
    // Set default colors for other components if needed
    setColour(juce::ScrollBar::thumbColourId, Theme::Colors::TextPrimary);
    setColour(juce::ScrollBar::trackColourId, Theme::Colors::Background);
}

void CustomLookAndFeel::setScrollbarAccentColour(juce::Colour colour)
{
    accentColour = colour;
}

void CustomLookAndFeel::drawScrollbar(juce::Graphics& g,
                                      juce::ScrollBar& scrollbar,
                                      int x, int y, int width, int height,
                                      bool isScrollbarVertical,
                                      int thumbStartPosition,
                                      int thumbSize,
                                      bool isMouseOver,
                                      bool isMouseDown)
{
    juce::ignoreUnused(scrollbar, isMouseOver, isMouseDown);

    auto bounds = juce::Rectangle<int>(x, y, width, height);

    // Track background - dark with subtle border
    g.setColour(Theme::Colors::Background.brighter(0.1f));
    g.fillRect(bounds);

    // Thin white border around track
    g.setColour(Theme::Colors::TextPrimary.withAlpha(0.3f));
    g.drawRect(bounds, 1);

    // Calculate thumb bounds from provided parameters
    juce::Rectangle<int> thumbBounds;

    if (isScrollbarVertical)
    {
        thumbBounds = juce::Rectangle<int>(x, thumbStartPosition, width, thumbSize);

        // Draw filled portion (accent rail behind thumb)
        auto filledHeight = thumbStartPosition - y;
        if (filledHeight > 0)
        {
            auto filledBounds = juce::Rectangle<int>(x, y, width, filledHeight);
            g.setColour(accentColour);
            g.fillRect(filledBounds);
        }
    }
    else
    {
        thumbBounds = juce::Rectangle<int>(thumbStartPosition, y, thumbSize, height);

        // Draw filled portion (accent rail behind thumb)
        auto filledWidth = thumbStartPosition - x;
        if (filledWidth > 0)
        {
            auto filledBounds = juce::Rectangle<int>(x, y, filledWidth, height);
            g.setColour(accentColour);
            g.fillRect(filledBounds);
        }
    }

    // Don't draw thumb if it's too small
    if (thumbBounds.getWidth() < 4 || thumbBounds.getHeight() < 4)
        return;

    // Draw rectangular thumb - white with accent border
    g.setColour(Theme::Colors::TextPrimary);
    g.fillRect(thumbBounds);

    // Accent outline on thumb
    g.setColour(accentColour);
    g.drawRect(thumbBounds, 2);

    // Inner highlight for dimension (only if thumb is large enough)
    if (thumbBounds.getWidth() >= 8 && thumbBounds.getHeight() >= 8)
    {
        g.setColour(Theme::Colors::TextPrimary.withAlpha(0.8f));
        g.drawRect(thumbBounds.reduced(2), 1);
    }
}
