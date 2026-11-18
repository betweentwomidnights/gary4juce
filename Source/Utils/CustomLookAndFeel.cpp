// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "CustomLookAndFeel.h"

CustomLookAndFeel::CustomLookAndFeel()
{
    // Set default colors for other components if needed
    setColour(juce::ScrollBar::thumbColourId, Theme::Colors::TextPrimary);
    setColour(juce::ScrollBar::trackColourId, Theme::Colors::Background);
}

namespace
{
class PopupLookAndFeel final : public CustomLookAndFeel
{
public:
    PopupLookAndFeel()
    {
        setColourScheme({ Theme::Colors::Background, juce::Colour(0xff151515),
                          juce::Colour(0xff151515), Theme::Colors::AccentRed,
                          Theme::Colors::TextPrimary, Theme::Colors::PrimaryRed,
                          Theme::Colors::TextPrimary, Theme::Colors::AccentRed,
                          Theme::Colors::TextPrimary });
        setColour(juce::AlertWindow::backgroundColourId, Theme::Colors::Background);
        setColour(juce::AlertWindow::outlineColourId, Theme::Colors::AccentRed);
        setColour(juce::AlertWindow::textColourId, Theme::Colors::TextPrimary);
        setColour(juce::ComboBox::outlineColourId, Theme::Colors::AccentRed);
        setColour(juce::TextEditor::focusedOutlineColourId, Theme::Colors::PrimaryRed);
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff252525));
        setColour(juce::TextButton::buttonOnColourId, Theme::Colors::AccentRed);
    }

    juce::AlertWindow* createAlertWindow(const juce::String& title, const juce::String& message,
        const juce::String& button1, const juce::String& button2, const juce::String& button3,
        juce::MessageBoxIconType icon, int numButtons, juce::Component* associated) override
    {
        auto* alert = juce::LookAndFeel_V4::createAlertWindow(title, message, button1, button2,
            button3, icon, numButtons, associated);
        alert->setLookAndFeel(this);
        return alert;
    }

    void drawAlertBox(juce::Graphics& g, juce::AlertWindow& alert,
                      const juce::Rectangle<int>&, juce::TextLayout& text) override
    {
        const auto bounds = alert.getLocalBounds();
        g.fillAll(alert.findColour(juce::AlertWindow::backgroundColourId));
        g.setColour(alert.findColour(juce::AlertWindow::outlineColourId));
        g.drawRect(bounds, 1);
        g.setColour(Theme::Colors::PrimaryRed);
        g.fillRect(bounds.withHeight(3));

        const bool hasIcon = alert.getAlertType() != juce::MessageBoxIconType::NoIcon;
        if (hasIcon)
        {
            const auto badge = juce::Rectangle<float>(22.0f, 30.0f, 38.0f, 38.0f);
            g.setColour(Theme::Colors::AccentRed);
            g.fillEllipse(badge);
            g.setColour(Theme::Colors::TextPrimary);
            g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
            const auto symbol = alert.getAlertType() == juce::MessageBoxIconType::WarningIcon ? "!"
                              : alert.getAlertType() == juce::MessageBoxIconType::QuestionIcon ? "?" : "i";
            g.drawText(symbol, badge, juce::Justification::centred);
        }
        // Keep JUCE's text layout and component spacing, replacing only its chrome and icon.
        text.draw(g, juce::Rectangle<float>(hasIcon ? 81.0f : 1.0f, 30.0f,
                  (float) bounds.getWidth() - 2.0f,
                  (float) bounds.getHeight() - getAlertWindowButtonHeight() - 22.0f));
    }
};
}

juce::LookAndFeel& CustomLookAndFeel::popupLookAndFeel()
{
    static PopupLookAndFeel instance;
    return instance;
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
