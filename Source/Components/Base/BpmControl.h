// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <JuceHeader.h>
#include "../../Utils/Theme.h"

// Compact tempo editor for standalone mode. Drag vertically, use the mouse
// wheel, or double-click the value to type an exact BPM.
class BpmControl final : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::Label::Listener
{
public:
    BpmControl()
    {
        valueLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setEditable(false, true, false);
        valueLabel.setColour(juce::Label::textColourId, Theme::Colors::TextPrimary);
        valueLabel.setColour(juce::Label::textWhenEditingColourId, Theme::Colors::TextPrimary);
        valueLabel.setColour(juce::Label::backgroundWhenEditingColourId, Theme::Colors::ButtonInactive);
        valueLabel.setColour(juce::Label::outlineWhenEditingColourId, accentColour);
        valueLabel.addListener(this);
        valueLabel.addMouseListener(this, false);
        valueLabel.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        addAndMakeVisible(valueLabel);

        setTooltip("drag up/down or use the wheel; double-click to type bpm");
        updateText();
    }

    ~BpmControl() override
    {
        valueLabel.removeMouseListener(this);
        valueLabel.removeListener(this);
    }

    void setRange(double newMinimum, double newMaximum, double newInterval = 1.0)
    {
        minimum = juce::jmin(newMinimum, newMaximum);
        maximum = juce::jmax(newMinimum, newMaximum);
        interval = juce::jmax(0.0001, newInterval);
        setValue(value, false);
    }

    void setValue(double newValue, bool sendChange = true)
    {
        const double snapped = juce::jlimit(minimum, maximum,
            minimum + std::round((newValue - minimum) / interval) * interval);
        if (juce::approximatelyEqual(value, snapped))
        {
            updateText();
            return;
        }

        value = snapped;
        updateText();
        repaint();
        if (sendChange && onValueChange)
            onValueChange(value);
    }

    double getValue() const noexcept { return value; }

    void setTooltip(const juce::String& tooltip)
    {
        juce::SettableTooltipClient::setTooltip(tooltip);
        valueLabel.setTooltip(tooltip);
    }

    void setAccentColour(juce::Colour colour)
    {
        accentColour = colour;
        valueLabel.setColour(juce::Label::outlineWhenEditingColourId, accentColour);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        g.setColour(Theme::Colors::ButtonInactive.withAlpha(0.85f));
        g.fillRect(bounds);
        g.setColour(isMouseOverOrDragging() ? accentColour.brighter(0.35f) : accentColour);
        g.drawRect(bounds, 1);
    }

    void resized() override
    {
        valueLabel.setBounds(getLocalBounds().reduced(2, 1));
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragStartY = event.getEventRelativeTo(this).position.y;
        dragStartValue = value;
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        const float currentY = event.getEventRelativeTo(this).position.y;
        const double steps = std::round((dragStartY - currentY) / 2.0f);
        setValue(dragStartValue + steps * interval);
    }

    void mouseWheelMove(const juce::MouseEvent&,
                        const juce::MouseWheelDetails& wheel) override
    {
        const int direction = wheel.deltaY > 0.0f ? 1 : (wheel.deltaY < 0.0f ? -1 : 0);
        if (direction != 0)
            setValue(value + direction * interval);
    }

    std::function<void(double)> onValueChange;

private:
    void labelTextChanged(juce::Label* changedLabel) override
    {
        if (changedLabel != &valueLabel)
            return;

        const auto numericText = valueLabel.getText().retainCharacters("0123456789.");
        if (numericText.isEmpty())
        {
            updateText();
            return;
        }

        setValue(numericText.getDoubleValue());
    }

    void updateText()
    {
        const int decimals = interval < 1.0 ? 1 : 0;
        valueLabel.setText(juce::String(value, decimals) + " bpm", juce::dontSendNotification);
    }

    juce::Label valueLabel;
    juce::Colour accentColour { Theme::Colors::PrimaryRed };
    double minimum = 40.0;
    double maximum = 300.0;
    double interval = 1.0;
    double value = 120.0;
    float dragStartY = 0.0f;
    double dragStartValue = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BpmControl)
};
