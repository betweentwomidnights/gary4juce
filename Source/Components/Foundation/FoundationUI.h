#pragma once

#include <JuceHeader.h>

#include "../Base/CustomButton.h"
#include "../Base/CustomComboBox.h"
#include "../Base/CustomSlider.h"
#include "../Base/CustomTextEditor.h"
#include "../../Utils/CustomLookAndFeel.h"
#include "../../Utils/Theme.h"

#include <functional>
#include <random>
#include <numeric>
#include <algorithm>

// ============================================================================
// SteppedKnob - Ableton-style rotary knob that snaps to discrete positions
// ============================================================================

class SteppedKnob : public juce::Component, public juce::SettableTooltipClient
{
public:
    SteppedKnob() { setRepaintsOnMouseActivity(true); }

    void setSteps(const juce::StringArray& stepLabels)
    {
        labels = stepLabels;
        numSteps = labels.size();
        if (numSteps > 0 && currentStep >= numSteps)
            currentStep = 0;
        repaint();
    }

    void setAccentColour(juce::Colour c) { accentColour = c; repaint(); }

    int getCurrentStep() const { return currentStep; }
    juce::String getCurrentLabel() const { return (currentStep >= 0 && currentStep < labels.size()) ? labels[currentStep] : ""; }

    void setCurrentStep(int step, bool notify = true)
    {
        step = juce::jlimit(0, juce::jmax(0, numSteps - 1), step);
        if (step != currentStep)
        {
            currentStep = step;
            repaint();
            if (notify && onChange) onChange(currentStep);
        }
    }

    std::function<void(int)> onChange;

    void paint(juce::Graphics& g) override
    {
        if (numSteps < 1) return;

        auto bounds = getLocalBounds().toFloat();
        // Reserve bottom 14px for label text
        auto labelArea = bounds.removeFromBottom(14.0f);
        auto knobArea = bounds.reduced(2.0f);

        float size = juce::jmin(knobArea.getWidth(), knobArea.getHeight());
        auto centre = knobArea.getCentre();
        float radius = size * 0.5f;

        // Draw background track arc (from ~7 o'clock to ~5 o'clock = 220 degrees)
        const float startAngle = juce::MathConstants<float>::pi * 0.75f;   // 135 deg = ~7 o'clock
        const float endAngle   = juce::MathConstants<float>::pi * 2.25f;   // 405 deg = ~5 o'clock
        const float arcRange   = endAngle - startAngle;

        juce::Path trackArc;
        trackArc.addCentredArc(centre.x, centre.y, radius - 3.0f, radius - 3.0f,
                               0.0f, startAngle, endAngle, true);
        g.setColour(juce::Colour(0xff444444));
        g.strokePath(trackArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

        // Draw filled arc to current position
        float normalised = (numSteps > 1) ? (float)currentStep / (float)(numSteps - 1) : 0.0f;
        float currentAngle = startAngle + normalised * arcRange;

        juce::Path filledArc;
        filledArc.addCentredArc(centre.x, centre.y, radius - 3.0f, radius - 3.0f,
                                0.0f, startAngle, currentAngle, true);
        g.setColour(accentColour);
        g.strokePath(filledArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

        // Draw detent dots — skip when too many steps to avoid visual clutter
        if (numSteps <= 20)
        {
            for (int i = 0; i < numSteps; ++i)
            {
                float stepNorm = (numSteps > 1) ? (float)i / (float)(numSteps - 1) : 0.0f;
                float angle = startAngle + stepNorm * arcRange;
                float dotRadius = (i == currentStep) ? 3.5f : 2.0f;
                float dotDist = radius - 3.0f;
                float dx = centre.x + dotDist * std::cos(angle);
                float dy = centre.y + dotDist * std::sin(angle);

                g.setColour(i == currentStep ? accentColour : juce::Colour(0xff666666));
                g.fillEllipse(dx - dotRadius, dy - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
            }
        }
        else
        {
            // Many steps: just draw the current position dot + start/end markers
            auto drawDot = [&](int i, float dotRadius)
            {
                float stepNorm = (numSteps > 1) ? (float)i / (float)(numSteps - 1) : 0.0f;
                float angle = startAngle + stepNorm * arcRange;
                float dotDist = radius - 3.0f;
                float dx = centre.x + dotDist * std::cos(angle);
                float dy = centre.y + dotDist * std::sin(angle);
                g.fillEllipse(dx - dotRadius, dy - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
            };
            g.setColour(juce::Colour(0xff666666));
            drawDot(0, 2.0f);
            drawDot(numSteps - 1, 2.0f);
            g.setColour(accentColour);
            drawDot(currentStep, 3.5f);
        }

        // Draw pointer line
        {
            float pointerInner = radius * 0.3f;
            float pointerOuter = radius - 7.0f;
            float px1 = centre.x + pointerInner * std::cos(currentAngle);
            float py1 = centre.y + pointerInner * std::sin(currentAngle);
            float px2 = centre.x + pointerOuter * std::cos(currentAngle);
            float py2 = centre.y + pointerOuter * std::sin(currentAngle);
            g.setColour(juce::Colours::white);
            g.drawLine(px1, py1, px2, py2, 2.0f);
        }

        // Draw centre circle (knob body)
        float knobRadius = radius * 0.35f;
        bool hover = isMouseOver();
        g.setColour(hover ? juce::Colour(0xff3a3a3a) : juce::Colour(0xff2a2a2a));
        g.fillEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
        g.setColour(juce::Colour(0xff555555));
        g.drawEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

        // Draw label text below knob — auto-shrink font to fit without ellipsis
        {
            g.setColour(juce::Colours::white);
            auto label = getCurrentLabel();
            float maxW = labelArea.getWidth();
            float fontSize = 10.0f;
            while (fontSize > 6.5f)
            {
                auto testFont = juce::Font(juce::FontOptions(fontSize));
                if (testFont.getStringWidthFloat(label) <= maxW)
                    break;
                fontSize -= 0.5f;
            }
            g.setFont(juce::FontOptions(fontSize));
            g.drawText(label, labelArea, juce::Justification::centred, false);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown() || e.mods.isCtrlDown())
        {
            // Right click: show popup menu of all options
            juce::PopupMenu menu;
            for (int i = 0; i < numSteps; ++i)
                menu.addItem(i + 1, labels[i], true, i == currentStep);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                [this](int result) { if (result > 0) setCurrentStep(result - 1); });
            return;
        }
        dragStartY = e.position.y;
        dragStartStep = currentStep;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (numSteps < 2) return;
        float delta = dragStartY - e.position.y; // up = positive
        float totalDrag = juce::jmax(100.0f, (float)(numSteps - 1) * 3.0f); // at least 3px per step
        float sensitivity = totalDrag / (float)(numSteps - 1); // pixels per step
        int stepDelta = juce::roundToInt(delta / sensitivity);
        setCurrentStep(juce::jlimit(0, numSteps - 1, dragStartStep + stepDelta));
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        setCurrentStep(0); // reset to first position
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (numSteps < 2) return;
        int delta = (wheel.deltaY > 0) ? 1 : (wheel.deltaY < 0 ? -1 : 0);
        setCurrentStep(juce::jlimit(0, numSteps - 1, currentStep + delta));
    }

private:
    juce::StringArray labels;
    int numSteps = 0;
    int currentStep = 0;
    juce::Colour accentColour = juce::Colour(0xffff8c00); // Jerry orange default
    float dragStartY = 0.0f;
    int dragStartStep = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SteppedKnob)
};

// ============================================================================
// IconButton - small button with custom-painted outline icon
// ============================================================================

class IconButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Icon { Save, Load, Dice };

    IconButton(Icon type) : iconType(type) { setRepaintsOnMouseActivity(true); }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(3.0f);
        float cx = b.getCentreX(), cy = b.getCentreY();
        float s = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;

        bool hover = isMouseOver();
        auto colour = hover ? juce::Colour(0xffff8c00) : juce::Colour(0xffbbbbbb);
        g.setColour(colour);

        if (iconType == Icon::Save)
        {
            // Floppy disk outline
            float l = cx - s, r = cx + s, t = cy - s, bt = cy + s;
            float notchW = s * 0.5f;

            juce::Path disk;
            disk.startNewSubPath(l, t);
            disk.lineTo(r - notchW, t);
            disk.lineTo(r, t + notchW);    // chamfered corner
            disk.lineTo(r, bt);
            disk.lineTo(l, bt);
            disk.closeSubPath();
            g.strokePath(disk, juce::PathStrokeType(1.4f));

            // Label slot (bottom rectangle)
            float labelT = cy + s * 0.15f;
            g.drawRect(l + s * 0.3f, labelT, s * 1.4f, bt - labelT - 1.0f, 1.2f);

            // Shutter slot (top centre)
            g.drawRect(cx - s * 0.35f, t + 1.0f, s * 0.7f, s * 0.55f, 1.0f);
        }
        else if (iconType == Icon::Load)
        {
            // Folder body outline
            float l = cx - s, r = cx + s, t = cy - s * 0.6f, bt = cy + s;
            float tabW = s * 0.7f, tabH = s * 0.35f;

            juce::Path folder;
            folder.startNewSubPath(l, t + tabH);
            folder.lineTo(l, t);
            folder.lineTo(l + tabW, t);
            folder.lineTo(l + tabW + tabH * 0.5f, t + tabH); // tab angle
            folder.lineTo(r, t + tabH);
            folder.lineTo(r, bt);
            folder.lineTo(l, bt);
            folder.closeSubPath();
            g.strokePath(folder, juce::PathStrokeType(1.4f));

            // "Open" flap line
            g.drawLine(l + 1.0f, t + tabH + s * 0.35f, r - 1.0f, t + tabH + s * 0.15f, 1.0f);
        }
        else // Dice
        {
            // Rounded rectangle outline
            float l = cx - s, t = cy - s;
            g.drawRoundedRectangle(l, t, s * 2.0f, s * 2.0f, s * 0.25f, 1.4f);

            // 5 pips (like a 5-face)
            float pipR = s * 0.12f;
            float off = s * 0.5f;
            auto pip = [&](float px, float py) {
                g.fillEllipse(px - pipR, py - pipR, pipR * 2.0f, pipR * 2.0f);
            };
            pip(cx, cy);                     // centre
            pip(cx - off, cy - off);         // top-left
            pip(cx + off, cy - off);         // top-right
            pip(cx - off, cy + off);         // bottom-left
            pip(cx + off, cy + off);         // bottom-right
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && onClick)
            onClick();
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

    std::function<void()> onClick;

private:
    Icon iconType;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IconButton)
};

// ============================================================================
// Tag vocabularies - curated v1 sets matching backend /tags endpoint
// ============================================================================

namespace FoundationTags
{
    inline const juce::StringArray& families()
    {
        static const juce::StringArray f = {
            "Synth", "Keys", "Bass", "Bowed Strings", "Mallet",
            "Wind", "Guitar", "Brass", "Vocal", "Plucked Strings"
        };
        return f;
    }

    // Subfamilies grouped by family for hierarchical filtering
    struct SubfamilyEntry { juce::String family; juce::String name; };

    inline const std::vector<SubfamilyEntry>& subfamilyEntries()
    {
        static const std::vector<SubfamilyEntry> entries = {
            { "Synth", "Synth Lead" }, { "Synth", "Synth Bass" }, { "Synth", "FM Synth" },
            { "Synth", "Supersaw" }, { "Synth", "Wavetable Synth" }, { "Synth", "Wavetable Bass" },
            { "Synth", "Pad" }, { "Synth", "Atmosphere" },
            { "Keys", "Grand Piano" }, { "Keys", "Rhodes Piano" }, { "Keys", "Digital Piano" },
            { "Keys", "Hammond Organ" }, { "Keys", "Celesta" },
            { "Bass", "Reese Bass" }, { "Bass", "Sub Bass" }, { "Bass", "Synth Bass" },
            { "Bass", "Wavetable Bass" },
            { "Bowed Strings", "Violin" }, { "Bowed Strings", "Cello" },
            { "Mallet", "Bell" }, { "Mallet", "Vibraphone" }, { "Mallet", "Glockenspiel" },
            { "Wind", "Flute" }, { "Wind", "Clarinet" }, { "Wind", "Oboe" },
            { "Wind", "Piccolo" },
            { "Guitar", "Acoustic Guitar" }, { "Guitar", "Electric Guitar" },
            { "Brass", "Trumpet" }, { "Brass", "French Horn" }, { "Brass", "Tuba" },
            { "Vocal", "Choir" }, { "Vocal", "Texture" },
            { "Plucked Strings", "Harp" },
        };
        return entries;
    }

    inline juce::StringArray subfamiliesForFamily(const juce::String& family)
    {
        juce::StringArray result;
        for (auto& e : subfamilyEntries())
            if (e.family == family)
                result.add(e.name);
        return result;
    }

    inline const juce::StringArray& allSubfamilies()
    {
        static juce::StringArray all;
        if (all.isEmpty())
            for (auto& e : subfamilyEntries())
                all.addIfNotAlreadyThere(e.name);
        return all;
    }

    // Full TIMBRE_TAGS vocabulary from backend master_prompt_map.py
    // All 3 descriptor knobs draw from this same pool
    inline const juce::StringArray& timbreTags()
    {
        static const juce::StringArray d = {
            "warm", "bright", "tight", "thick", "airy", "rich", "clean", "gritty",
            "crisp", "focused", "metallic", "dark", "shiny", "present", "silky",
            "sparkly", "smooth", "cold", "buzzy", "round", "fat", "punchy", "thin",
            "soft", "woody", "hollow", "nasal", "biting", "overdriven", "subdued",
            "breathy", "glassy", "pizzicato", "staccato", "snappy", "full", "harsh",
            "knock", "muddy", "steel", "veiled", "rubbery", "rumble", "noisy",
            "boomy", "crispy", "dreamy", "heavy", "tiny", "spiccato"
        };
        return d;
    }

    // ---- Structure (mutually exclusive) ----
    inline const juce::StringArray& structureTags()
    {
        static const juce::StringArray s = {
            "melody", "arp", "chord progression", "dance chord progression", "bassline"
        };
        return s;
    }

    // ---- Speed (mutually exclusive, or none) ----
    inline const juce::StringArray& speedTags()
    {
        static const juce::StringArray s = { "slow speed", "medium speed", "fast speed" };
        return s;
    }

    // ---- Rhythm (multi-select) ----
    inline const juce::StringArray& rhythmTags()
    {
        static const juce::StringArray r = { "off beat", "strummed", "alternating", "triplets" };
        return r;
    }

    // ---- Contour (multi-select) ----
    inline const juce::StringArray& contourTags()
    {
        static const juce::StringArray c = { "bounce", "rising", "falling", "top", "rolling", "sustained", "choppy" };
        return c;
    }

    // ---- Density (multi-select) ----
    inline const juce::StringArray& densityTags()
    {
        static const juce::StringArray d = { "simple", "complex", "epic", "catchy", "sustained", "repeating" };
        return d;
    }

    // ---- Spatial (toggle + knob — single selection to avoid contradictions like near+distant) ----
    inline const juce::StringArray& spatialTags()
    {
        static const juce::StringArray s = {
            "wide", "mono", "near", "far", "spacey", "ambient",
            "distant", "intimate", "small", "big", "deep"
        };
        return s;
    }

    // ---- Style (multi-select) ----
    inline const juce::StringArray& styleTags()
    {
        static const juce::StringArray s = {
            "chiptune", "laser", "vintage", "retro", "dubstep",
            "formant vocal", "siren", "acid", "303", "fx", "growl"
        };
        return s;
    }

    // ---- Band / frequency (multi-select) ----
    inline const juce::StringArray& bandTags()
    {
        static const juce::StringArray b = {
            "sub", "sub bass", "bass", "low mids", "mids", "upper mids", "highs", "air"
        };
        return b;
    }

    // ---- Wave/tech (multi-select) ----
    inline const juce::StringArray& waveTechTags()
    {
        static const juce::StringArray w = {
            "saw", "square", "sine", "triangle", "pulse",
            "analog", "digital", "fm", "supersaw", "reese",
            "pitch bend", "white noise", "filter"
        };
        return w;
    }

    inline const juce::StringArray& reverbAmounts()
    {
        static const juce::StringArray r = { "Low Reverb", "Medium Reverb", "High Reverb", "Plate Reverb" };
        return r;
    }

    inline const juce::StringArray& delayTypes()
    {
        static const juce::StringArray d = {
            "Low Delay", "Medium Delay", "High Delay",
            "Ping Pong Delay", "Stereo Delay", "Cross Delay", "Delay", "Mono Delay"
        };
        return d;
    }

    inline const juce::StringArray& distortionAmounts()
    {
        static const juce::StringArray d = { "Low Distortion", "Medium Distortion", "High Distortion", "Distortion" };
        return d;
    }

    inline const juce::StringArray& phaserAmounts()
    {
        static const juce::StringArray p = { "Phaser", "Low Phaser", "Medium Phaser", "High Phaser" };
        return p;
    }

    inline const juce::StringArray& bitcrushAmounts()
    {
        static const juce::StringArray b = { "Bitcrush", "High Bitcrush" };
        return b;
    }

    inline const juce::Array<int>& supportedBpms()
    {
        static const juce::Array<int> b = { 100, 110, 120, 128, 130, 140, 150 };
        return b;
    }

    inline int nearestFoundationBpm(double hostBpm)
    {
        int best = 120;
        double bestDist = 9999.0;
        for (int b : supportedBpms())
        {
            double d = std::abs(b - hostBpm);
            if (d < bestDist) { bestDist = d; best = b; }
        }
        return best;
    }

    inline const juce::StringArray& keyRoots()
    {
        static const juce::StringArray k = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return k;
    }
}

// ============================================================================
// FoundationUI
// ============================================================================

class FoundationUI : public juce::Component
{
public:
    FoundationUI()
    {
        // Title
        titleLabel.setText("foundation-1", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);

        // Scrollable content
        contentComponent = std::make_unique<juce::Component>();
        contentViewport = std::make_unique<juce::Viewport>();
        contentViewport->setViewedComponent(contentComponent.get(), false);
        contentViewport->setScrollBarsShown(true, false);
        customLookAndFeel.setScrollbarAccentColour(Theme::Colors::Jerry); // use Jerry orange for foundation
        contentViewport->getVerticalScrollBar().setLookAndFeel(&customLookAndFeel);
        addAndMakeVisible(contentViewport.get());

        // ==================== INSTRUMENT SECTION ====================
        auto sectionLabel = [this](juce::Label& label, const juce::String& text)
        {
            label.setText(text, juce::dontSendNotification);
            label.setFont(juce::FontOptions(11.0f));
            label.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
            label.setJustificationType(juce::Justification::centredLeft);
            addToContent(label);
        };

        sectionLabel(familyLabel, "family");
        setupComboBox(familyComboBox, FoundationTags::families(), "instrument family (synth, keys, bass, etc.)");
        familyComboBox.onChange = [this]()
        {
            updateSubfamilyOptions();
            rebuildPromptPreview();
            if (onFamilyChanged)
                onFamilyChanged(familyComboBox.getText());
        };

        sectionLabel(subfamilyLabel, "type");
        updateSubfamilyOptions(); // populate initially
        subfamilyComboBox.setTooltip("specific instrument type within the selected family");
        subfamilyComboBox.onChange = [this]()
        {
            rebuildPromptPreview();
            if (onSubfamilyChanged)
                onSubfamilyChanged(subfamilyComboBox.getText());
        };
        addToContent(subfamilyComboBox);

        // ==================== DESCRIPTOR KNOBS ====================
        sectionLabel(descriptorSectionLabel, "character");

        auto setupDescriptorKnob = [this](SteppedKnob& knob, const juce::String& tooltip)
        {
            knob.setSteps(FoundationTags::timbreTags());
            knob.setAccentColour(juce::Colour(0xffff8c00));
            knob.setTooltip(tooltip);
            knob.onChange = [this](int) { rebuildPromptPreview(); };
            addToContent(knob);
        };

        setupDescriptorKnob(descriptorAKnob, "timbre character A (right-click for full list)");
        setupDescriptorKnob(descriptorBKnob, "timbre character B (right-click for full list)");
        setupDescriptorKnob(descriptorCKnob, "timbre character C (right-click for full list)");

        // Spread initial defaults so they don't all start on "warm"
        descriptorAKnob.setCurrentStep(0, false);   // warm
        descriptorBKnob.setCurrentStep(5, false);   // rich
        descriptorCKnob.setCurrentStep(6, false);   // clean

        // "-" buttons for each descriptor knob
        auto setupDescRemove = [this](CustomButton& btn, bool& vis, SteppedKnob& knob)
        {
            btn.setButtonText(juce::String::fromUTF8("\xe2\x80\x93")); // en-dash as minus
            btn.setButtonStyle(CustomButton::ButtonStyle::Inactive);
            btn.setTooltip("remove this descriptor");
            btn.onClick = [this, &vis, &knob]()
            {
                vis = false;
                knob.setVisible(false);
                rebuildPromptPreview();
                updateContentLayout();
            };
            addToContent(btn);
        };
        setupDescRemove(descriptorRemoveA, descriptorAVisible, descriptorAKnob);
        setupDescRemove(descriptorRemoveB, descriptorBVisible, descriptorBKnob);
        setupDescRemove(descriptorRemoveC, descriptorCVisible, descriptorCKnob);

        // "+" button to add descriptor knobs back
        descriptorAddButton.setButtonText("+");
        descriptorAddButton.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        descriptorAddButton.setTooltip("add a descriptor knob");
        descriptorAddButton.onClick = [this]()
        {
            // Re-enable first hidden knob (A, then B, then C)
            if (!descriptorAVisible)      { descriptorAVisible = true; descriptorAKnob.setVisible(true); }
            else if (!descriptorBVisible)  { descriptorBVisible = true; descriptorBKnob.setVisible(true); }
            else if (!descriptorCVisible)  { descriptorCVisible = true; descriptorCKnob.setVisible(true); }
            rebuildPromptPreview();
            updateContentLayout();
        };
        addToContent(descriptorAddButton);

        // ==================== DESCRIPTORS EXTRA (dynamic text rows) ====================
        sectionLabel(descriptorExtraLabel, "extra descriptors");
        descriptorExtraAddButton.setButtonText("+");
        descriptorExtraAddButton.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        descriptorExtraAddButton.setTooltip("add an extra descriptor text tag");
        descriptorExtraAddButton.onClick = [this]()
        {
            addDescriptorExtraRow("");
            updateContentLayout();
        };
        addToContent(descriptorExtraAddButton);

        // ==================== STRUCTURE (mutually exclusive radio) ====================
        sectionLabel(structureSectionLabel, "structure");
        setupToggleGroup(structureButtons, FoundationTags::structureTags(), true /*radio*/);

        // ==================== SPEED (toggle + knob, like FX) ====================
        speedKnob.setSteps(FoundationTags::speedTags());
        speedKnob.setAccentColour(juce::Colour(0xffff8c00));
        speedKnob.setTooltip("generation speed - drag up/down or scroll");
        speedKnob.onChange = [this](int) { rebuildPromptPreview(); };
        addToContent(speedKnob);
        speedKnob.setVisible(false);

        speedToggle.setButtonText("speed");
        speedToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        speedToggle.setClickingTogglesState(true);
        speedToggle.onClick = [this]()
        {
            speedToggle.setButtonStyle(speedToggle.getToggleState()
                ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            speedKnob.setVisible(speedToggle.getToggleState());
            rebuildPromptPreview();
            updateContentLayout();
        };
        addToContent(speedToggle);

        // ==================== RHYTHM (toggle + knob with optional 2nd) ====================
        setupDualKnobToggle(rhythmToggle, rhythmKnobA, rhythmKnobB, rhythmAddButton, rhythmRemoveB,
                            rhythmBVisible, "rhythm", FoundationTags::rhythmTags(),
                            "rhythm pattern - drag up/down or scroll");

        // ==================== CONTOUR (toggle + knob with optional 2nd) ====================
        setupDualKnobToggle(contourToggle, contourKnobA, contourKnobB, contourAddButton, contourRemoveB,
                            contourBVisible, "contour", FoundationTags::contourTags(),
                            "melodic contour - drag up/down or scroll");

        // ==================== DENSITY (toggle + knob with optional 2nd) ====================
        setupDualKnobToggle(densityToggle, densityKnobA, densityKnobB, densityAddButton, densityRemoveB,
                            densityBVisible, "density", FoundationTags::densityTags(),
                            "pattern density - drag up/down or scroll");

        // ==================== SPATIAL (toggle + knob — single value, avoids contradictions) ====================
        setupSingleKnobToggle(spatialToggle, spatialKnob, "spatial",
                              FoundationTags::spatialTags(), "spatial character - drag up/down or scroll");

        // ==================== STYLE (toggle + knob — single value) ====================
        setupSingleKnobToggle(styleToggle, styleKnob, "style",
                              FoundationTags::styleTags(), "style character - drag up/down or scroll");

        // ==================== BAND / FREQUENCY (toggle + knob — single value) ====================
        setupSingleKnobToggle(bandToggle, bandKnob, "frequency band",
                              FoundationTags::bandTags(), "frequency band - drag up/down or scroll");

        // ==================== WAVE / TECH (toggle + knob, like FX) ====================
        waveTechKnob.setSteps(FoundationTags::waveTechTags());
        waveTechKnob.setAccentColour(juce::Colour(0xffff8c00));
        waveTechKnob.setTooltip("synthesis type - drag up/down or scroll");
        waveTechKnob.onChange = [this](int) { rebuildPromptPreview(); };
        addToContent(waveTechKnob);
        waveTechKnob.setVisible(false);

        waveTechToggle.setButtonText("synthesis");
        waveTechToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        waveTechToggle.setClickingTogglesState(true);
        waveTechToggle.onClick = [this]()
        {
            waveTechToggle.setButtonStyle(waveTechToggle.getToggleState()
                ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            waveTechKnob.setVisible(waveTechToggle.getToggleState());
            rebuildPromptPreview();
            updateContentLayout();
        };
        addToContent(waveTechToggle);

        // ==================== FX SECTION ====================
        sectionLabel(fxSectionLabel, "effects");

        // Setup stepped knobs
        reverbKnob.setSteps(FoundationTags::reverbAmounts());
        reverbKnob.setAccentColour(juce::Colour(0xffff8c00)); // Jerry orange
        reverbKnob.setTooltip("reverb intensity - drag up/down or scroll to change");
        reverbKnob.onChange = [this](int) { rebuildPromptPreview(); };
        addToContent(reverbKnob);
        reverbKnob.setVisible(false);

        delayKnob.setSteps(FoundationTags::delayTypes());
        delayKnob.setAccentColour(juce::Colour(0xffff8c00));
        delayKnob.setTooltip("delay type - drag up/down or scroll to change");
        delayKnob.onChange = [this](int) { rebuildPromptPreview(); };
        addToContent(delayKnob);
        delayKnob.setVisible(false);

        distortionKnob.setSteps(FoundationTags::distortionAmounts());
        distortionKnob.setAccentColour(juce::Colour(0xffff8c00));
        distortionKnob.setTooltip("distortion amount - drag up/down or scroll to change");
        distortionKnob.onChange = [this](int) { rebuildPromptPreview(); };
        addToContent(distortionKnob);
        distortionKnob.setVisible(false);

        // Reverb toggle
        reverbToggle.setButtonText("reverb");
        reverbToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        reverbToggle.setClickingTogglesState(true);
        reverbToggle.onClick = [this]()
        {
            reverbToggle.setButtonStyle(reverbToggle.getToggleState()
                ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            reverbKnob.setVisible(reverbToggle.getToggleState());
            rebuildPromptPreview();
            updateContentLayout();
        };
        addToContent(reverbToggle);

        // Delay toggle
        delayToggle.setButtonText("delay");
        delayToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        delayToggle.setClickingTogglesState(true);
        delayToggle.onClick = [this]()
        {
            delayToggle.setButtonStyle(delayToggle.getToggleState()
                ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            delayKnob.setVisible(delayToggle.getToggleState());
            rebuildPromptPreview();
            updateContentLayout();
        };
        addToContent(delayToggle);

        // Distortion toggle
        distortionToggle.setButtonText("distortion");
        distortionToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        distortionToggle.setClickingTogglesState(true);
        distortionToggle.onClick = [this]()
        {
            distortionToggle.setButtonStyle(distortionToggle.getToggleState()
                ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            distortionKnob.setVisible(distortionToggle.getToggleState());
            rebuildPromptPreview();
            updateContentLayout();
        };
        addToContent(distortionToggle);

        // Phaser knob + toggle
        phaserKnob.setSteps(FoundationTags::phaserAmounts());
        phaserKnob.setAccentColour(juce::Colour(0xffff8c00));
        phaserKnob.setTooltip("phaser amount - drag up/down or scroll to change");
        phaserKnob.onChange = [this](int) { rebuildPromptPreview(); };
        addToContent(phaserKnob);
        phaserKnob.setVisible(false);

        phaserToggle.setButtonText("phaser");
        phaserToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        phaserToggle.setClickingTogglesState(true);
        phaserToggle.onClick = [this]()
        {
            phaserToggle.setButtonStyle(phaserToggle.getToggleState()
                ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            phaserKnob.setVisible(phaserToggle.getToggleState());
            rebuildPromptPreview();
            updateContentLayout();
        };
        addToContent(phaserToggle);

        // Bitcrush knob + toggle
        bitcrushKnob.setSteps(FoundationTags::bitcrushAmounts());
        bitcrushKnob.setAccentColour(juce::Colour(0xffff8c00));
        bitcrushKnob.setTooltip("bitcrush amount - drag up/down or scroll to change");
        bitcrushKnob.onChange = [this](int) { rebuildPromptPreview(); };
        addToContent(bitcrushKnob);
        bitcrushKnob.setVisible(false);

        bitcrushToggle.setButtonText("bitcrush");
        bitcrushToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        bitcrushToggle.setClickingTogglesState(true);
        bitcrushToggle.onClick = [this]()
        {
            bitcrushToggle.setButtonStyle(bitcrushToggle.getToggleState()
                ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            bitcrushKnob.setVisible(bitcrushToggle.getToggleState());
            rebuildPromptPreview();
            updateContentLayout();
        };
        addToContent(bitcrushToggle);

        // ==================== MUSICAL STRUCTURE (persistent header — above viewport) ====================

        barsComboBox.addItem("4 bars", 1);
        barsComboBox.addItem("8 bars", 2);
        barsComboBox.setSelectedId(1, juce::dontSendNotification);
        barsComboBox.setTooltip("number of bars to generate (4 or 8)");
        barsComboBox.onChange = [this]()
        {
            rebuildPromptPreview();
            updateBpmWarning();
            if (onBarsChanged)
                onBarsChanged(getSelectedBars());
        };
        addAndMakeVisible(barsComboBox);

        bpmValueLabel.setText("120 bpm", juce::dontSendNotification);
        bpmValueLabel.setFont(juce::FontOptions(11.0f));
        bpmValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        bpmValueLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(bpmValueLabel);

        // BPM warning row (slim row below header, shown only when time-stretching)
        bpmWarningLabel.setText("", juce::dontSendNotification);
        bpmWarningLabel.setFont(juce::FontOptions(10.0f));
        bpmWarningLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffcc00)); // yellow
        bpmWarningLabel.setJustificationType(juce::Justification::centred);
        bpmWarningLabel.setVisible(false);
        addAndMakeVisible(bpmWarningLabel);

        // Standalone BPM slider
        standaloneBpmSlider.setRange(40.0, 300.0, 1.0);
        standaloneBpmSlider.setValue(120.0, juce::dontSendNotification);
        standaloneBpmSlider.setTooltip("BPM for generation (standalone mode only - DAW mode syncs automatically)");
        standaloneBpmSlider.onValueChange = [this]()
        {
            hostBpm = standaloneBpmSlider.getValue();
            updateBpmDisplay();
            rebuildPromptPreview();
            if (onBpmChanged)
                onBpmChanged(hostBpm);
        };
        addAndMakeVisible(standaloneBpmSlider);

        keyRootComboBox.addItem("C", 1); keyRootComboBox.addItem("C#", 2);
        keyRootComboBox.addItem("D", 3); keyRootComboBox.addItem("D#", 4);
        keyRootComboBox.addItem("E", 5); keyRootComboBox.addItem("F", 6);
        keyRootComboBox.addItem("F#", 7); keyRootComboBox.addItem("G", 8);
        keyRootComboBox.addItem("G#", 9); keyRootComboBox.addItem("A", 10);
        keyRootComboBox.addItem("A#", 11); keyRootComboBox.addItem("B", 12);
        keyRootComboBox.setSelectedId(1, juce::dontSendNotification);
        keyRootComboBox.setTooltip("key root note");
        keyRootComboBox.onChange = [this]() { rebuildPromptPreview(); };
        addAndMakeVisible(keyRootComboBox);

        keyModeComboBox.addItem("major", 1);
        keyModeComboBox.addItem("minor", 2);
        keyModeComboBox.setSelectedId(2, juce::dontSendNotification); // default minor
        keyModeComboBox.setTooltip("major or minor");
        keyModeComboBox.onChange = [this]() { rebuildPromptPreview(); };
        addAndMakeVisible(keyModeComboBox);

        // ==================== ADVANCED SECTION ====================
        advancedToggle.setButtonText(juce::String::fromUTF8("advanced \xe2\x96\xb6"));
        advancedToggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        advancedToggle.onClick = [this]()
        {
            advancedOpen = !advancedOpen;
            advancedToggle.setButtonText(advancedOpen
                ? juce::String::fromUTF8("advanced \xe2\x96\xbc")
                : juce::String::fromUTF8("advanced \xe2\x96\xb6"));
            updateContentLayout();
        };
        addToContent(advancedToggle);

        // Steps
        sectionLabel(stepsLabel, "steps");
        stepsSlider.setRange(50, 200, 10);
        stepsSlider.setValue(100, juce::dontSendNotification);
        stepsSlider.setTooltip("diffusion steps. more = refined but slower. recommended: 100");
        stepsSlider.onValueChange = [this]()
        {
            rebuildPromptPreview();
            if (onStepsChanged) onStepsChanged(getSteps());
        };
        addToContent(stepsSlider);

        // Guidance scale
        sectionLabel(guidanceLabel, "guidance");
        guidanceSlider.setRange(1.0, 15.0, 0.5);
        guidanceSlider.setValue(7.0, juce::dontSendNotification);
        guidanceSlider.setTooltip("guidance scale - higher values follow the prompt more closely");
        guidanceSlider.onValueChange = [this]()
        {
            rebuildPromptPreview();
            if (onGuidanceChanged) onGuidanceChanged(getGuidance());
        };
        addToContent(guidanceSlider);

        // Seed (toggle + text field — disabled by default, populated by randomize)
        seedToggle.setButtonText("use seed");
        seedToggle.setToggleState(false, juce::dontSendNotification);
        seedToggle.setTooltip("when enabled, uses the seed value for reproducible generation");
        seedToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcccccc));
        seedToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffff8c00));
        seedToggle.onClick = [this]()
        {
            seedEditor.setEnabled(seedToggle.getToggleState());
            seedEditor.setAlpha(seedToggle.getToggleState() ? 1.0f : 0.4f);
        };
        addToContent(seedToggle);

        seedEditor.setMultiLine(false);
        seedEditor.setTextToShowWhenEmpty("random", juce::Colour(0xff666666));
        seedEditor.setTooltip("seed for reproducibility. populated by randomize");
        seedEditor.setInputRestrictions(10, "0123456789");
        seedEditor.setEnabled(false);
        seedEditor.setAlpha(0.4f);
        addToContent(seedEditor);

        // Audio2Audio — "use input audio" toggle + transformation slider
        audio2audioToggle.setButtonText("use input audio");
        audio2audioToggle.setToggleState(false, juce::dontSendNotification);
        audio2audioToggle.setTooltip("when enabled, re-voices the recording buffer using the prompt as a timbre target");
        audio2audioToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffcccccc));
        audio2audioToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffff8c00));
        audio2audioToggle.onClick = [this]()
        {
            bool on = audio2audioToggle.getToggleState();
            transformationSlider.setVisible(on);
            transformationLabel.setVisible(on);
            updateContentLayout();
        };
        addToContent(audio2audioToggle);

        sectionLabel(transformationLabel, "transformation");
        transformationSlider.setRange(0.1, 0.95, 0.05);
        transformationSlider.setValue(0.6, juce::dontSendNotification);
        transformationSlider.setTooltip("how much to transform the input. low = subtle timbral shift, high = near-full regeneration");
        transformationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 35, 20);
        addToContent(transformationSlider);
        transformationSlider.setVisible(false);
        transformationLabel.setVisible(false);

        // Text override
        sectionLabel(overrideLabel, "prompt override");
        overrideEditor.setMultiLine(false);
        overrideEditor.setTextToShowWhenEmpty("leave empty to use controls above", juce::Colour(0xff666666));
        overrideEditor.setTooltip("type a custom prompt to override the structured controls. bars/BPM/key will still be appended");
        overrideEditor.onTextChange = [this]()
        {
            rebuildPromptPreview();
        };
        addToContent(overrideEditor);

        // ==================== PROMPT PREVIEW ====================
        sectionLabel(previewLabel, "prompt preview");
        previewText.setMultiLine(true, true);
        previewText.setReadOnly(true);
        previewText.setScrollbarsShown(true);
        previewText.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1a1a));
        previewText.setColour(juce::TextEditor::textColourId, juce::Colour(0xffbbbbbb));
        previewText.setFont(juce::FontOptions(11.0f));
        addToContent(previewText);

        // Duration info
        durationInfoLabel.setText("", juce::dontSendNotification);
        durationInfoLabel.setFont(juce::FontOptions(10.0f));
        durationInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        durationInfoLabel.setJustificationType(juce::Justification::centredLeft);
        addToContent(durationInfoLabel);

        // ==================== GENERATE + RANDOMIZE + PRESET (pinned footer — outside scrollable area) ====================
        savePresetButton.setTooltip("save current settings as a preset");
        savePresetButton.onClick = [this]() { savePreset(); };
        addAndMakeVisible(savePresetButton);

        loadPresetButton.setTooltip("load a saved preset");
        loadPresetButton.onClick = [this]() { loadPreset(); };
        addAndMakeVisible(loadPresetButton);

        randomizeButton.setButtonText("randomize");
        randomizeButton.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        randomizeButton.setTooltip("randomize instrument, descriptors, and behavior tags via backend");
        randomizeButton.onClick = [this]()
        {
            if (onRandomize)
                onRandomize();
            else
                randomizeControls(); // fallback to local randomization
        };
        randomizeButton.onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
        {
            bool over = randomizeButton.isMouseOver();
            bool down = randomizeButton.isDown();

            // Background + border (matches Inactive style)
            auto bg = Theme::Colors::ButtonInactive;
            auto border = Theme::Colors::TextSecondary;
            auto text = Theme::Colors::TextSecondary;
            if (down)       { bg = border; border = Theme::Colors::TextPrimary; text = Theme::Colors::TextPrimary; }
            else if (over)  { bg = border.brighter(0.4f); border = border.brighter(0.6f); text = Theme::Colors::Background; }

            g.setColour(bg.withAlpha(0.8f));
            g.fillRect(bounds);
            g.setColour(border);
            g.drawRect(bounds, 2);

            // Dice icon on the left
            constexpr int diceSize = 14;
            int dicePad = (bounds.getHeight() - diceSize) / 2;
            auto diceArea = juce::Rectangle<float>((float)(bounds.getX() + 6), (float)(bounds.getY() + dicePad),
                                                    (float)diceSize, (float)diceSize);

            float dcx = diceArea.getCentreX(), dcy = diceArea.getCentreY();
            float ds = diceSize * 0.5f;
            g.setColour(text);
            g.drawRoundedRectangle(diceArea, ds * 0.25f, 1.3f);

            float pipR = ds * 0.14f;
            float off = ds * 0.48f;
            auto pip = [&](float px, float py) {
                g.fillEllipse(px - pipR, py - pipR, pipR * 2.0f, pipR * 2.0f);
            };
            pip(dcx, dcy);
            pip(dcx - off, dcy - off);
            pip(dcx + off, dcy - off);
            pip(dcx - off, dcy + off);
            pip(dcx + off, dcy + off);

            // Text to the right of the dice
            g.setFont(Theme::Fonts::Body);
            auto textArea = bounds.withLeft(bounds.getX() + 6 + diceSize + 3);
            g.drawText("randomize", textArea, juce::Justification::centred, true);
        };
        addAndMakeVisible(randomizeButton);

        generateButton.setButtonText("generate");
        generateButton.setButtonStyle(CustomButton::ButtonStyle::Jerry);
        generateButton.onClick = [this]()
        {
            if (onGenerate) onGenerate();
        };
        addAndMakeVisible(generateButton);

        // Info label (below action buttons, also pinned)
        infoLabel.setText("", juce::dontSendNotification);
        infoLabel.setFont(juce::FontOptions(10.0f));
        infoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        infoLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(infoLabel);

        // Initial state
        updateBpmDisplay();
        rebuildPromptPreview();
    }

    ~FoundationUI() override
    {
        if (contentViewport != nullptr)
            contentViewport->getVerticalScrollBar().setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics&) override {}

    void resized() override
    {
        constexpr int marginH = 6;
        auto area = getLocalBounds();
        area.removeFromLeft(marginH);
        area.removeFromRight(marginH);
        area.removeFromTop(2);

        titleBounds = area.removeFromTop(titleHidden ? 0 : 22);
        titleLabel.setBounds(titleBounds);

        if (!titleHidden) area.removeFromTop(2);

        // ---- Persistent header row: bars | BPM | key (above viewport, like CareyUI) ----
        // No separate text labels — saves ~60px for the combo boxes themselves
        {
            auto headerRow = area.removeFromTop(22);

            // Calculate total width needed, then center the block
            int bpmW = isStandaloneMode ? 90 : 56;
            int totalNeeded = 78 + 4 + bpmW + 4 + 58 + 3 + 78;
            int sidePad = juce::jmax(0, (headerRow.getWidth() - totalNeeded) / 2);
            headerRow.removeFromLeft(sidePad);

            barsComboBox.setBounds(headerRow.removeFromLeft(78));
            headerRow.removeFromLeft(4);

            if (isStandaloneMode)
            {
                standaloneBpmSlider.setBounds(headerRow.removeFromLeft(90));
                bpmValueLabel.setVisible(false);
                standaloneBpmSlider.setVisible(true);
            }
            else
            {
                bpmValueLabel.setBounds(headerRow.removeFromLeft(56));
                bpmValueLabel.setVisible(true);
                standaloneBpmSlider.setVisible(false);
            }
            headerRow.removeFromLeft(4);

            keyRootComboBox.setBounds(headerRow.removeFromLeft(58));
            headerRow.removeFromLeft(3);
            keyModeComboBox.setBounds(headerRow.removeFromLeft(78));
        }

        // ---- BPM warning row (slim, only when time-stretching) ----
        if (bpmWarningLabel.isVisible())
        {
            area.removeFromTop(1);
            bpmWarningLabel.setBounds(area.removeFromTop(14));
        }

        // ---- Pinned footer: [save][load] [randomize] [generate] ----
        area.removeFromBottom(1);
        auto footerRow = area.removeFromBottom(24);
        {
            auto actionArea = footerRow.reduced(2, 0);
            constexpr int iconBtnW = 26;
            savePresetButton.setBounds(actionArea.removeFromLeft(iconBtnW));
            actionArea.removeFromLeft(2);
            loadPresetButton.setBounds(actionArea.removeFromLeft(iconBtnW));
            actionArea.removeFromLeft(4);
            int randW = juce::jmin(96, actionArea.getWidth() / 3);
            randomizeButton.setBounds(actionArea.removeFromLeft(randW));
            actionArea.removeFromLeft(4);
            generateButton.setBounds(actionArea);
        }
        // Info label just above buttons (for "generating..." status)
        auto footerInfoRow = area.removeFromBottom(12);
        infoLabel.setBounds(footerInfoRow);
        area.removeFromBottom(2); // slim gap between viewport and footer

        area.removeFromTop(4);
        if (contentViewport != nullptr)
            contentViewport->setBounds(area);

        updateContentLayout();
    }

    void setVisibleForTab(bool visible)
    {
        setVisible(visible);
    }

    juce::Rectangle<int> getTitleBounds() const { return titleBounds; }

    void setTitleVisible(bool visible)
    {
        titleLabel.setVisible(visible);
        titleHidden = !visible;
    }

    // ==================== GETTERS ====================

    juce::String getFamily() const { return familyComboBox.getText(); }
    juce::String getSubfamily() const { return subfamilyComboBox.getText(); }
    juce::String getDescriptorA() const { return descriptorAVisible ? descriptorAKnob.getCurrentLabel() : ""; }
    juce::String getDescriptorB() const { return descriptorBVisible ? descriptorBKnob.getCurrentLabel() : ""; }
    juce::String getDescriptorC() const { return descriptorCVisible ? descriptorCKnob.getCurrentLabel() : ""; }
    juce::StringArray getDescriptorsExtraValues() const { return getDescriptorsExtraInternal(); }

    juce::StringArray getSelectedBehaviorTags() const
    {
        // Collect from all categorized tag groups
        juce::StringArray tags;
        // Structure (radio — at most one)
        for (auto* btn : structureButtons)
            if (btn->getToggleState()) tags.add(btn->getButtonText());
        // Speed (toggle + knob)
        if (speedToggle.getToggleState())
            tags.add(speedKnob.getCurrentLabel());
        // Dual-knob groups (rhythm, contour, density)
        auto collectDual = [&tags](bool on, const SteppedKnob& a, const SteppedKnob& b, bool bVis)
        {
            if (!on) return;
            tags.add(a.getCurrentLabel());
            if (bVis) tags.add(b.getCurrentLabel());
        };
        collectDual(rhythmToggle.getToggleState(), rhythmKnobA, rhythmKnobB, rhythmBVisible);
        collectDual(contourToggle.getToggleState(), contourKnobA, contourKnobB, contourBVisible);
        collectDual(densityToggle.getToggleState(), densityKnobA, densityKnobB, densityBVisible);
        // Single-knob toggles (spatial, style, band)
        if (spatialToggle.getToggleState())
            tags.add(spatialKnob.getCurrentLabel());
        if (styleToggle.getToggleState())
            tags.add(styleKnob.getCurrentLabel());
        if (bandToggle.getToggleState())
            tags.add(bandKnob.getCurrentLabel());
        // Wave/tech (toggle + knob)
        if (waveTechToggle.getToggleState())
            tags.add(waveTechKnob.getCurrentLabel());
        return tags;
    }

    bool getReverbEnabled() const { return reverbToggle.getToggleState(); }
    juce::String getReverbAmount() const { return reverbKnob.getCurrentLabel(); }
    bool getDelayEnabled() const { return delayToggle.getToggleState(); }
    juce::String getDelayType() const { return delayKnob.getCurrentLabel(); }
    bool getDistortionEnabled() const { return distortionToggle.getToggleState(); }
    juce::String getDistortionAmount() const { return distortionKnob.getCurrentLabel(); }
    bool getPhaserEnabled() const { return phaserToggle.getToggleState(); }
    juce::String getPhaserAmount() const { return phaserKnob.getCurrentLabel(); }
    bool getBitcrushEnabled() const { return bitcrushToggle.getToggleState(); }
    juce::String getBitcrushAmount() const { return bitcrushKnob.getCurrentLabel(); }

    int getSelectedBars() const { return barsComboBox.getSelectedId() == 2 ? 8 : 4; }
    double getHostBpm() const { return hostBpm; }
    int getFoundationBpm() const { return FoundationTags::nearestFoundationBpm(hostBpm); }
    juce::String getKeyRoot() const { return keyRootComboBox.getText(); }
    juce::String getKeyMode() const { return keyModeComboBox.getText(); }

    int getSteps() const { return juce::roundToInt(stepsSlider.getValue()); }
    double getGuidance() const { return guidanceSlider.getValue(); }
    int getSeed() const
    {
        if (!seedToggle.getToggleState()) return -1; // disabled = always random
        auto text = seedEditor.getText().trim();
        return text.isEmpty() ? -1 : text.getIntValue();
    }
    juce::String getCustomPromptOverride() const { return overrideEditor.getText().trim(); }
    bool getAudio2AudioEnabled() const { return audio2audioToggle.getToggleState(); }
    double getTransformation() const { return transformationSlider.getValue(); }

    juce::String getBuiltPrompt() const { return lastBuiltPrompt; }

    // ==================== SETTERS ====================

    void setBpm(double bpm)
    {
        hostBpm = bpm;
        standaloneBpmSlider.setValue(bpm, juce::dontSendNotification);
        updateBpmDisplay();
        rebuildPromptPreview();
    }

    void setIsStandalone(bool standalone)
    {
        isStandaloneMode = standalone;
        updateContentLayout();
    }

    void setFamily(const juce::String& family)
    {
        for (int i = 0; i < familyComboBox.getNumItems(); ++i)
            if (familyComboBox.getItemText(i) == family)
            {
                familyComboBox.setSelectedItemIndex(i, juce::dontSendNotification);
                updateSubfamilyOptions();
                break;
            }
    }

    void setSubfamily(const juce::String& sub)
    {
        for (int i = 0; i < subfamilyComboBox.getNumItems(); ++i)
            if (subfamilyComboBox.getItemText(i) == sub)
            {
                subfamilyComboBox.setSelectedItemIndex(i, juce::dontSendNotification);
                break;
            }
    }

    void setGenerateButtonEnabled(bool enabled, bool isGenerating)
    {
        generateButton.setEnabled(enabled && !isGenerating);
        generateButton.setButtonText(isGenerating ? "generating..." : "generate");
    }

    void setGenerateButtonText(const juce::String& text)
    {
        generateButton.setButtonText(text);
    }

    void setInfoText(const juce::String& text)
    {
        infoLabel.setText(text, juce::dontSendNotification);
    }

    void setAdvancedOpen(bool open)
    {
        if (advancedOpen != open)
        {
            advancedOpen = open;
            advancedToggle.setButtonText(open
                ? juce::String::fromUTF8("advanced \xe2\x96\xbc")
                : juce::String::fromUTF8("advanced \xe2\x96\xb6"));
            updateContentLayout();
        }
    }

    void refreshDurationInfo()
    {
        updateBpmDisplay();
        // Reset info label to show duration info instead of "generating..." message
        infoLabel.setText("", juce::dontSendNotification);
    }

    // ==================== STATE PERSISTENCE ====================

    /** Serialize the entire Foundation UI state to a JSON string. */
    juce::String serializeState() const
    {
        auto state = std::make_unique<juce::DynamicObject>();

        // Instrument
        state->setProperty("family", familyComboBox.getText());
        state->setProperty("subfamily", subfamilyComboBox.getText());

        // Descriptor knobs
        state->setProperty("descriptorAStep", descriptorAKnob.getCurrentStep());
        state->setProperty("descriptorBStep", descriptorBKnob.getCurrentStep());
        state->setProperty("descriptorCStep", descriptorCKnob.getCurrentStep());
        state->setProperty("descriptorAVisible", descriptorAVisible);
        state->setProperty("descriptorBVisible", descriptorBVisible);
        state->setProperty("descriptorCVisible", descriptorCVisible);

        // Descriptor extras
        {
            juce::var extras;
            for (auto& row : descriptorExtraRows)
                if (row.text)
                    extras.append(row.text->getText().trim());
            state->setProperty("descriptorExtras", extras);
        }

        // Structure (which radio button, if any)
        {
            juce::String structVal;
            for (auto* btn : structureButtons)
                if (btn->getToggleState()) { structVal = btn->getButtonText(); break; }
            state->setProperty("structure", structVal);
        }

        // Single-knob toggles helper
        auto saveSingleKnob = [&](const juce::String& prefix, const CustomButton& toggle, const SteppedKnob& knob)
        {
            state->setProperty(prefix + "On", toggle.getToggleState());
            state->setProperty(prefix + "Step", knob.getCurrentStep());
        };

        saveSingleKnob("speed", speedToggle, speedKnob);
        saveSingleKnob("spatial", spatialToggle, spatialKnob);
        saveSingleKnob("style", styleToggle, styleKnob);
        saveSingleKnob("band", bandToggle, bandKnob);
        saveSingleKnob("waveTech", waveTechToggle, waveTechKnob);

        // Dual-knob toggles helper
        auto saveDualKnob = [&](const juce::String& prefix, const CustomButton& toggle,
                                 const SteppedKnob& knobA, const SteppedKnob& knobB, bool bVis)
        {
            state->setProperty(prefix + "On", toggle.getToggleState());
            state->setProperty(prefix + "AStep", knobA.getCurrentStep());
            state->setProperty(prefix + "BStep", knobB.getCurrentStep());
            state->setProperty(prefix + "BVisible", bVis);
        };

        saveDualKnob("rhythm", rhythmToggle, rhythmKnobA, rhythmKnobB, rhythmBVisible);
        saveDualKnob("contour", contourToggle, contourKnobA, contourKnobB, contourBVisible);
        saveDualKnob("density", densityToggle, densityKnobA, densityKnobB, densityBVisible);

        // FX toggles + knobs
        auto saveFx = [&](const juce::String& prefix, const CustomButton& toggle, const SteppedKnob& knob)
        {
            state->setProperty(prefix + "On", toggle.getToggleState());
            state->setProperty(prefix + "Step", knob.getCurrentStep());
        };

        saveFx("reverb", reverbToggle, reverbKnob);
        saveFx("delay", delayToggle, delayKnob);
        saveFx("distortion", distortionToggle, distortionKnob);
        saveFx("phaser", phaserToggle, phaserKnob);
        saveFx("bitcrush", bitcrushToggle, bitcrushKnob);

        // Header controls
        state->setProperty("bars", barsComboBox.getSelectedId());
        state->setProperty("keyRoot", keyRootComboBox.getSelectedId());
        state->setProperty("keyMode", keyModeComboBox.getSelectedId());

        // Advanced
        state->setProperty("steps", juce::roundToInt(stepsSlider.getValue()));
        state->setProperty("guidance", guidanceSlider.getValue());
        state->setProperty("seed", seedEditor.getText().trim());
        state->setProperty("seedEnabled", seedToggle.getToggleState());
        state->setProperty("audio2audioEnabled", audio2audioToggle.getToggleState());
        state->setProperty("transformation", transformationSlider.getValue());
        state->setProperty("promptOverride", overrideEditor.getText().trim());

        return juce::JSON::toString(juce::var(state.release()), true);
    }

    /** Restore the Foundation UI state from a JSON string. */
    void restoreState(const juce::String& jsonString)
    {
        if (jsonString.isEmpty()) return;

        auto parsed = juce::JSON::parse(jsonString);
        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr) return;

        // Instrument
        juce::String family = obj->getProperty("family").toString();
        if (family.isNotEmpty())
        {
            setFamily(family);
            juce::String subfamily = obj->getProperty("subfamily").toString();
            if (subfamily.isNotEmpty())
                setSubfamily(subfamily);
        }

        // Descriptor knobs
        descriptorAVisible = (bool)obj->getProperty("descriptorAVisible");
        descriptorBVisible = (bool)obj->getProperty("descriptorBVisible");
        descriptorCVisible = (bool)obj->getProperty("descriptorCVisible");
        descriptorAKnob.setCurrentStep((int)obj->getProperty("descriptorAStep"), false);
        descriptorBKnob.setCurrentStep((int)obj->getProperty("descriptorBStep"), false);
        descriptorCKnob.setCurrentStep((int)obj->getProperty("descriptorCStep"), false);
        descriptorAKnob.setVisible(descriptorAVisible);
        descriptorBKnob.setVisible(descriptorBVisible);
        descriptorCKnob.setVisible(descriptorCVisible);

        // Descriptor extras
        clearDescriptorExtraRows();
        if (auto* extras = obj->getProperty("descriptorExtras").getArray())
            for (const auto& e : *extras)
            {
                juce::String t = e.toString().trim();
                if (t.isNotEmpty()) addDescriptorExtraRow(t);
            }

        // Structure
        clearToggleGroup(structureButtons);
        {
            juce::String s = obj->getProperty("structure").toString();
            if (s.isNotEmpty()) setToggleByLabel(structureButtons, s);
        }

        // Single-knob toggles helper
        auto restoreSingleKnob = [&](const juce::String& prefix, CustomButton& toggle, SteppedKnob& knob)
        {
            bool on = (bool)obj->getProperty(prefix + "On");
            toggle.setToggleState(on, juce::dontSendNotification);
            toggle.setButtonStyle(on ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            knob.setVisible(on);
            knob.setCurrentStep((int)obj->getProperty(prefix + "Step"), false);
        };

        restoreSingleKnob("speed", speedToggle, speedKnob);
        restoreSingleKnob("spatial", spatialToggle, spatialKnob);
        restoreSingleKnob("style", styleToggle, styleKnob);
        restoreSingleKnob("band", bandToggle, bandKnob);
        restoreSingleKnob("waveTech", waveTechToggle, waveTechKnob);

        // Dual-knob toggles helper
        auto restoreDualKnob = [&](const juce::String& prefix, CustomButton& toggle,
                                    SteppedKnob& knobA, SteppedKnob& knobB,
                                    CustomButton& addBtn, CustomButton& removeBtn, bool& bVis)
        {
            bool on = (bool)obj->getProperty(prefix + "On");
            bVis = (bool)obj->getProperty(prefix + "BVisible");
            toggle.setToggleState(on, juce::dontSendNotification);
            toggle.setButtonStyle(on ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            knobA.setVisible(on);
            knobA.setCurrentStep((int)obj->getProperty(prefix + "AStep"), false);
            knobB.setVisible(on && bVis);
            knobB.setCurrentStep((int)obj->getProperty(prefix + "BStep"), false);
            addBtn.setVisible(on && !bVis);
            removeBtn.setVisible(on && bVis);
        };

        restoreDualKnob("rhythm", rhythmToggle, rhythmKnobA, rhythmKnobB,
                         rhythmAddButton, rhythmRemoveB, rhythmBVisible);
        restoreDualKnob("contour", contourToggle, contourKnobA, contourKnobB,
                         contourAddButton, contourRemoveB, contourBVisible);
        restoreDualKnob("density", densityToggle, densityKnobA, densityKnobB,
                         densityAddButton, densityRemoveB, densityBVisible);

        // FX
        auto restoreFx = [&](const juce::String& prefix, CustomButton& toggle, SteppedKnob& knob)
        {
            bool on = (bool)obj->getProperty(prefix + "On");
            toggle.setToggleState(on, juce::dontSendNotification);
            toggle.setButtonStyle(on ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            knob.setVisible(on);
            knob.setCurrentStep((int)obj->getProperty(prefix + "Step"), false);
        };

        restoreFx("reverb", reverbToggle, reverbKnob);
        restoreFx("delay", delayToggle, delayKnob);
        restoreFx("distortion", distortionToggle, distortionKnob);
        restoreFx("phaser", phaserToggle, phaserKnob);
        restoreFx("bitcrush", bitcrushToggle, bitcrushKnob);

        // Header controls
        int barsId = (int)obj->getProperty("bars");
        if (barsId > 0) barsComboBox.setSelectedId(barsId, juce::dontSendNotification);
        int keyRootId = (int)obj->getProperty("keyRoot");
        if (keyRootId > 0) keyRootComboBox.setSelectedId(keyRootId, juce::dontSendNotification);
        int keyModeId = (int)obj->getProperty("keyMode");
        if (keyModeId > 0) keyModeComboBox.setSelectedId(keyModeId, juce::dontSendNotification);

        // Advanced
        stepsSlider.setValue((double)(int)obj->getProperty("steps"), juce::dontSendNotification);
        guidanceSlider.setValue((double)obj->getProperty("guidance"), juce::dontSendNotification);
        seedEditor.setText(obj->getProperty("seed").toString(), juce::dontSendNotification);
        bool seedOn = (bool)obj->getProperty("seedEnabled");
        seedToggle.setToggleState(seedOn, juce::dontSendNotification);
        seedEditor.setEnabled(seedOn);
        seedEditor.setAlpha(seedOn ? 1.0f : 0.4f);
        // Audio2Audio
        bool a2aOn = (bool)obj->getProperty("audio2audioEnabled");
        audio2audioToggle.setToggleState(a2aOn, juce::dontSendNotification);
        transformationSlider.setVisible(a2aOn);
        transformationLabel.setVisible(a2aOn);
        double transVal = (double)obj->getProperty("transformation");
        if (transVal >= 0.1 && transVal <= 0.95)
            transformationSlider.setValue(transVal, juce::dontSendNotification);

        overrideEditor.setText(obj->getProperty("promptOverride").toString(), juce::dontSendNotification);

        rebuildPromptPreview();
        updateBpmDisplay();
        updateContentLayout();
    }

    // ==================== CALLBACKS ====================

    std::function<void(const juce::String&)> onFamilyChanged;
    std::function<void(const juce::String&)> onSubfamilyChanged;
    std::function<void(int)> onBarsChanged;
    std::function<void(double)> onBpmChanged;
    std::function<void(int)> onStepsChanged;
    std::function<void(double)> onGuidanceChanged;
    std::function<void()> onGenerate;
    std::function<void()> onRandomize;

    // Apply results from backend /randomize endpoint to all UI controls
    void applyRandomizeResponse(const juce::var& response)
    {
        auto* obj = response.getDynamicObject();
        if (obj == nullptr) return;

        // Family + subfamily
        juce::String family = obj->getProperty("family").toString();
        if (family.isNotEmpty())
        {
            setFamily(family);
            juce::String subfamily = obj->getProperty("subfamily").toString();
            if (subfamily.isNotEmpty())
                setSubfamily(subfamily);
        }

        // Descriptor knobs A/B/C — match against timbre tags (case-insensitive)
        auto applyDescriptor = [](SteppedKnob& knob, bool& vis, const juce::String& value)
        {
            if (value.isEmpty()) { vis = false; knob.setVisible(false); return; }
            vis = true;
            knob.setVisible(true);
            auto& tags = FoundationTags::timbreTags();
            for (int i = 0; i < tags.size(); ++i)
                if (tags[i].equalsIgnoreCase(value))
                {
                    knob.setCurrentStep(i, false);
                    return;
                }
        };
        applyDescriptor(descriptorAKnob, descriptorAVisible, obj->getProperty("descriptor_knob_a").toString());
        applyDescriptor(descriptorBKnob, descriptorBVisible, obj->getProperty("descriptor_knob_b").toString());
        applyDescriptor(descriptorCKnob, descriptorCVisible, obj->getProperty("descriptor_knob_c").toString());

        // Descriptors extra — populate from backend array
        clearDescriptorExtraRows();
        if (auto* extraArr = obj->getProperty("descriptors_extra").getArray())
        {
            for (const auto& tag : *extraArr)
            {
                juce::String t = tag.toString().trim();
                if (t.isNotEmpty())
                    addDescriptorExtraRow(t);
            }
        }

        // ---- Categorized tags from backend ----

        // Structure (radio — pick from "structure" field)
        clearToggleGroup(structureButtons);
        {
            juce::String structure = obj->getProperty("structure").toString();
            if (structure.isNotEmpty())
                setToggleByLabel(structureButtons, structure);
        }

        // Speed (toggle + knob)
        {
            juce::String speed = obj->getProperty("speed").toString();
            bool speedOn = speed.isNotEmpty();
            speedToggle.setToggleState(speedOn, juce::dontSendNotification);
            speedToggle.setButtonStyle(speedOn ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            speedKnob.setVisible(speedOn);
            if (speedOn)
            {
                auto& speeds = FoundationTags::speedTags();
                for (int i = 0; i < speeds.size(); ++i)
                    if (speeds[i].equalsIgnoreCase(speed))
                        { speedKnob.setCurrentStep(i, false); break; }
            }
        }

        // Dual-knob groups (rhythm, contour, density) — from response arrays
        auto applyDualKnob = [&](CustomButton& toggle, SteppedKnob& knobA, SteppedKnob& knobB,
                                  CustomButton& addBtn, CustomButton& removeBtn, bool& bVis,
                                  const juce::String& key, const juce::StringArray& tagList)
        {
            bool on = false;
            bVis = false;
            if (auto* arr = obj->getProperty(key).getArray())
            {
                if (arr->size() > 0)
                {
                    on = true;
                    juce::String first = (*arr)[0].toString();
                    for (int i = 0; i < tagList.size(); ++i)
                        if (tagList[i].equalsIgnoreCase(first))
                            { knobA.setCurrentStep(i, false); break; }
                }
                if (arr->size() > 1)
                {
                    bVis = true;
                    juce::String second = (*arr)[1].toString();
                    for (int i = 0; i < tagList.size(); ++i)
                        if (tagList[i].equalsIgnoreCase(second))
                            { knobB.setCurrentStep(i, false); break; }
                }
            }
            toggle.setToggleState(on, juce::dontSendNotification);
            toggle.setButtonStyle(on ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            knobA.setVisible(on);
            knobB.setVisible(on && bVis);
            addBtn.setVisible(on && !bVis);
            removeBtn.setVisible(on && bVis);
        };

        applyDualKnob(rhythmToggle, rhythmKnobA, rhythmKnobB, rhythmAddButton, rhythmRemoveB,
                       rhythmBVisible, "rhythm", FoundationTags::rhythmTags());
        applyDualKnob(contourToggle, contourKnobA, contourKnobB, contourAddButton, contourRemoveB,
                       contourBVisible, "contour", FoundationTags::contourTags());
        applyDualKnob(densityToggle, densityKnobA, densityKnobB, densityAddButton, densityRemoveB,
                       densityBVisible, "density", FoundationTags::densityTags());

        // Single-knob toggles (spatial, style, band)
        auto applySingleKnob = [&](CustomButton& toggle, SteppedKnob& knob,
                                    const juce::String& key, const juce::StringArray& tagList)
        {
            bool on = false;
            if (auto* arr = obj->getProperty(key).getArray())
            {
                if (arr->size() > 0)
                {
                    on = true;
                    juce::String firstTag = (*arr)[0].toString();
                    for (int i = 0; i < tagList.size(); ++i)
                        if (tagList[i].equalsIgnoreCase(firstTag))
                            { knob.setCurrentStep(i, false); break; }
                }
            }
            toggle.setToggleState(on, juce::dontSendNotification);
            toggle.setButtonStyle(on ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            knob.setVisible(on);
        };

        applySingleKnob(spatialToggle, spatialKnob, "spatial_tags", FoundationTags::spatialTags());
        applySingleKnob(styleToggle, styleKnob, "style_tags", FoundationTags::styleTags());
        applySingleKnob(bandToggle, bandKnob, "band_tags", FoundationTags::bandTags());
        // Wave/tech (toggle + knob — use first wave_tech_tag if any)
        {
            bool waveTechOn = false;
            if (auto* arr = obj->getProperty("wave_tech_tags").getArray())
            {
                if (arr->size() > 0)
                {
                    waveTechOn = true;
                    juce::String firstTag = (*arr)[0].toString();
                    auto& tags = FoundationTags::waveTechTags();
                    for (int i = 0; i < tags.size(); ++i)
                        if (tags[i].equalsIgnoreCase(firstTag))
                            { waveTechKnob.setCurrentStep(i, false); break; }
                }
            }
            waveTechToggle.setToggleState(waveTechOn, juce::dontSendNotification);
            waveTechToggle.setButtonStyle(waveTechOn ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            waveTechKnob.setVisible(waveTechOn);
        }

        // FX matching helper — handles shortened labels (e.g. "Med Reverb" matches "Medium Reverb")
        auto matchFxStep = [](const juce::StringArray& tags, const juce::String& backendVal) -> int
        {
            // Exact match first
            for (int i = 0; i < tags.size(); ++i)
                if (tags[i].equalsIgnoreCase(backendVal))
                    return i;
            // Fuzzy: backend value contains our label or vice versa (handles Med/Medium, Ping Pong/Ping Pong Delay)
            auto backendLower = backendVal.toLowerCase();
            for (int i = 0; i < tags.size(); ++i)
            {
                auto tagLower = tags[i].toLowerCase();
                if (backendLower.contains(tagLower) || tagLower.contains(backendLower))
                    return i;
            }
            return -1;
        };

        // FX — reverb
        bool reverbOn = (bool)obj->getProperty("reverb_enabled");
        reverbToggle.setToggleState(reverbOn, juce::dontSendNotification);
        reverbToggle.setButtonStyle(reverbOn ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
        reverbKnob.setVisible(reverbOn);
        if (reverbOn)
        {
            int idx = matchFxStep(FoundationTags::reverbAmounts(), obj->getProperty("reverb_amount").toString());
            if (idx >= 0) reverbKnob.setCurrentStep(idx, false);
        }

        // FX — delay
        bool delayOn = (bool)obj->getProperty("delay_enabled");
        delayToggle.setToggleState(delayOn, juce::dontSendNotification);
        delayToggle.setButtonStyle(delayOn ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
        delayKnob.setVisible(delayOn);
        if (delayOn)
        {
            int idx = matchFxStep(FoundationTags::delayTypes(), obj->getProperty("delay_type").toString());
            if (idx >= 0) delayKnob.setCurrentStep(idx, false);
        }

        // FX — distortion
        bool distortionOn = (bool)obj->getProperty("distortion_enabled");
        distortionToggle.setToggleState(distortionOn, juce::dontSendNotification);
        distortionToggle.setButtonStyle(distortionOn ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
        distortionKnob.setVisible(distortionOn);
        if (distortionOn)
        {
            int idx = matchFxStep(FoundationTags::distortionAmounts(), obj->getProperty("distortion_amount").toString());
            if (idx >= 0) distortionKnob.setCurrentStep(idx, false);
        }

        // FX — phaser
        bool phaserOn = (bool)obj->getProperty("phaser_enabled");
        phaserToggle.setToggleState(phaserOn, juce::dontSendNotification);
        phaserToggle.setButtonStyle(phaserOn ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
        phaserKnob.setVisible(phaserOn);
        if (phaserOn)
        {
            int idx = matchFxStep(FoundationTags::phaserAmounts(), obj->getProperty("phaser_amount").toString());
            if (idx >= 0) phaserKnob.setCurrentStep(idx, false);
        }

        // FX — bitcrush
        bool bitcrushOn = (bool)obj->getProperty("bitcrush_enabled");
        bitcrushToggle.setToggleState(bitcrushOn, juce::dontSendNotification);
        bitcrushToggle.setButtonStyle(bitcrushOn ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
        bitcrushKnob.setVisible(bitcrushOn);
        if (bitcrushOn)
        {
            int idx = matchFxStep(FoundationTags::bitcrushAmounts(), obj->getProperty("bitcrush_amount").toString());
            if (idx >= 0) bitcrushKnob.setCurrentStep(idx, false);
        }

        // Store the seed if provided (for reproducibility)
        int seed = (int)obj->getProperty("seed");
        if (seed > 0)
            seedEditor.setText(juce::String(seed), juce::dontSendNotification);
        else
            seedEditor.setText("", juce::dontSendNotification);

        // Clear any previous prompt override — let the UI controls drive the prompt
        overrideEditor.setText("", juce::dontSendNotification);

        rebuildPromptPreview();
        updateBpmDisplay();
        updateContentLayout();
    }

    void setRandomizeEnabled(bool enabled)
    {
        randomizeButton.setEnabled(enabled);
    }

private:

    void addToContent(juce::Component& component)
    {
        if (contentComponent != nullptr)
            contentComponent->addAndMakeVisible(component);
    }

    void clearToggleGroup(juce::OwnedArray<CustomButton>& buttons)
    {
        for (auto* btn : buttons)
        {
            btn->setToggleState(false, juce::dontSendNotification);
            btn->setButtonStyle(CustomButton::ButtonStyle::Inactive);
        }
    }

    void setToggleByLabel(juce::OwnedArray<CustomButton>& buttons, const juce::String& label)
    {
        for (auto* btn : buttons)
            if (btn->getButtonText().equalsIgnoreCase(label))
            {
                btn->setToggleState(true, juce::dontSendNotification);
                btn->setButtonStyle(CustomButton::ButtonStyle::Jerry);
                return;
            }
    }

    // Flow-layout toggle buttons and advance y past them
    void layoutFlowButtons(juce::OwnedArray<CustomButton>& buttons, int startX, int maxX, int& y)
    {
        constexpr int btnHeight = 24;
        constexpr int btnSpacing = 4;
        int x = startX;
        juce::Font btnFont(juce::FontOptions(11.0f));

        for (auto* btn : buttons)
        {
            int btnWidth = juce::jmax(50, (int)btnFont.getStringWidthFloat(btn->getButtonText()) + 16);
            if (x + btnWidth > maxX && x > startX)
            {
                x = startX;
                y += btnHeight + btnSpacing;
            }
            btn->setBounds(x, y, btnWidth, btnHeight);
            x += btnWidth + btnSpacing;
        }
        y += btnHeight + 8;
    }

    // Layout knobs for a pair of toggles that share a row.
    // Each side can be single-knob (knobB=nullptr) or dual-knob (knobB!=nullptr with +/- buttons).
    void layoutKnobsForPair(bool leftOn, SteppedKnob* leftA, SteppedKnob* leftB, bool leftBVis,
                             CustomButton* leftAdd, CustomButton* leftRemove,
                             bool rightOn, SteppedKnob* rightA, SteppedKnob* rightB, bool rightBVis,
                             CustomButton* rightAdd, CustomButton* rightRemove,
                             const std::function<juce::Rectangle<int>(int)>& /*fullRow*/,
                             int& y, int sidePadding, int contentWidth)
    {
        constexpr int knobSize = 64;
        constexpr int knobGap = 6;
        constexpr int smallBtnH = 14;
        constexpr int smallBtnW = 20;
        int usableW = contentWidth - sidePadding * 2;
        int halfW = (usableW - 4) / 2;

        // Count how many knobs each side needs
        int leftCount = leftOn ? (leftBVis && leftB ? 2 : 1) : 0;
        int rightCount = rightOn ? (rightBVis && rightB ? 2 : 1) : 0;
        bool hasDualControls = (leftOn && leftB != nullptr) || (rightOn && rightB != nullptr);

        // Place knobs — each side gets its half of the width
        auto placeKnobsInHalf = [&](int halfStartX, int halfWidth, int count,
                                     SteppedKnob* kA, SteppedKnob* kB)
        {
            if (count == 0) return;
            int totalW = count * knobSize + (count - 1) * knobGap;
            int startX = halfStartX + (halfWidth - totalW) / 2;
            if (kA) kA->setBounds(startX, y, knobSize, knobSize);
            if (count > 1 && kB)
                kB->setBounds(startX + knobSize + knobGap, y, knobSize, knobSize);
        };

        if (leftOn || rightOn)
        {
            if (leftOn && rightOn)
            {
                // Both sides active — each gets half width
                placeKnobsInHalf(sidePadding, halfW, leftCount, leftA, leftB);
                placeKnobsInHalf(sidePadding + halfW + 4, halfW, rightCount, rightA, rightB);
            }
            else if (leftOn)
            {
                // Only left — center in full width
                int totalW = leftCount * knobSize + (leftCount - 1) * knobGap;
                int startX = sidePadding + (usableW - totalW) / 2;
                if (leftA) leftA->setBounds(startX, y, knobSize, knobSize);
                if (leftCount > 1 && leftB)
                    leftB->setBounds(startX + knobSize + knobGap, y, knobSize, knobSize);
            }
            else
            {
                // Only right — center in full width
                int totalW = rightCount * knobSize + (rightCount - 1) * knobGap;
                int startX = sidePadding + (usableW - totalW) / 2;
                if (rightA) rightA->setBounds(startX, y, knobSize, knobSize);
                if (rightCount > 1 && rightB)
                    rightB->setBounds(startX + knobSize + knobGap, y, knobSize, knobSize);
            }
            y += knobSize + 2;

            // Place +/- buttons for dual-knob sides
            auto placeDualBtns = [&](SteppedKnob* kA, SteppedKnob* kB, bool bVis,
                                     CustomButton* addBtn, CustomButton* removeBtn)
            {
                if (!addBtn || !removeBtn) return;
                if (bVis && kB)
                {
                    // "–" below knob B
                    int cx = kB->getBounds().getCentreX();
                    removeBtn->setBounds(cx - smallBtnW / 2, y, smallBtnW, smallBtnH);
                    removeBtn->setVisible(true);
                    addBtn->setVisible(false);
                }
                else if (kA)
                {
                    // "+" below knob A
                    int cx = kA->getBounds().getCentreX();
                    addBtn->setBounds(cx - smallBtnW / 2, y, smallBtnW, smallBtnH);
                    addBtn->setVisible(true);
                    removeBtn->setVisible(false);
                }
            };

            if (hasDualControls)
            {
                if (leftOn && leftB) placeDualBtns(leftA, leftB, leftBVis, leftAdd, leftRemove);
                if (rightOn && rightB) placeDualBtns(rightA, rightB, rightBVis, rightAdd, rightRemove);
                y += smallBtnH + 2;
            }
            y += 2;
        }

        // Hide +/- for inactive sides
        if (!leftOn)  { if (leftAdd) leftAdd->setVisible(false); if (leftRemove) leftRemove->setVisible(false); }
        if (!rightOn) { if (rightAdd) rightAdd->setVisible(false); if (rightRemove) rightRemove->setVisible(false); }
    }

    // Setup a toggle button that reveals a single stepped knob (spatial, style, band pattern)
    void setupSingleKnobToggle(CustomButton& toggle, SteppedKnob& knob,
                                const juce::String& label, const juce::StringArray& tags,
                                const juce::String& tooltip)
    {
        knob.setSteps(tags);
        knob.setAccentColour(juce::Colour(0xffff8c00));
        knob.setTooltip(tooltip);
        knob.onChange = [this](int) { rebuildPromptPreview(); };
        addToContent(knob);
        knob.setVisible(false);

        toggle.setButtonText(label);
        toggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        toggle.setClickingTogglesState(true);
        toggle.onClick = [this, &toggle, &knob]()
        {
            toggle.setButtonStyle(toggle.getToggleState()
                ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            knob.setVisible(toggle.getToggleState());
            rebuildPromptPreview();
            updateContentLayout();
        };
        addToContent(toggle);
    }

    // Setup a toggle button that reveals 1 knob with option to add/remove a 2nd (rhythm, contour, density)
    void setupDualKnobToggle(CustomButton& toggle, SteppedKnob& knobA, SteppedKnob& knobB,
                              CustomButton& addBtn, CustomButton& removeBtn, bool& bVisible,
                              const juce::String& label, const juce::StringArray& tags,
                              const juce::String& tooltip)
    {
        knobA.setSteps(tags);
        knobA.setAccentColour(juce::Colour(0xffff8c00));
        knobA.setTooltip(tooltip);
        knobA.onChange = [this](int) { rebuildPromptPreview(); };
        addToContent(knobA);
        knobA.setVisible(false);

        knobB.setSteps(tags);
        knobB.setAccentColour(juce::Colour(0xffff8c00));
        knobB.setTooltip(tooltip);
        knobB.onChange = [this](int) { rebuildPromptPreview(); };
        addToContent(knobB);
        knobB.setVisible(false);

        addBtn.setButtonText("+");
        addBtn.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        addBtn.setTooltip("add second " + label + " knob");
        addBtn.onClick = [this, &knobB, &addBtn, &removeBtn, &bVisible]()
        {
            bVisible = true;
            knobB.setVisible(true);
            addBtn.setVisible(false);
            removeBtn.setVisible(true);
            rebuildPromptPreview();
            updateContentLayout();
        };
        addToContent(addBtn);
        addBtn.setVisible(false);

        removeBtn.setButtonText(juce::String::fromUTF8("\xe2\x80\x93")); // en-dash "–"
        removeBtn.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        removeBtn.setTooltip("remove second " + label + " knob");
        removeBtn.onClick = [this, &knobB, &addBtn, &removeBtn, &bVisible]()
        {
            bVisible = false;
            knobB.setVisible(false);
            removeBtn.setVisible(false);
            addBtn.setVisible(true);
            rebuildPromptPreview();
            updateContentLayout();
        };
        addToContent(removeBtn);
        removeBtn.setVisible(false);

        bVisible = false;

        toggle.setButtonText(label);
        toggle.setButtonStyle(CustomButton::ButtonStyle::Inactive);
        toggle.setClickingTogglesState(true);
        toggle.onClick = [this, &toggle, &knobA, &knobB, &addBtn, &removeBtn, &bVisible]()
        {
            bool on = toggle.getToggleState();
            toggle.setButtonStyle(on ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            knobA.setVisible(on);
            knobB.setVisible(on && bVisible);
            addBtn.setVisible(on && !bVisible);
            removeBtn.setVisible(on && bVisible);
            rebuildPromptPreview();
            updateContentLayout();
        };
        addToContent(toggle);
    }

    void setupComboBox(CustomComboBox& combo, const juce::StringArray& items, const juce::String& tooltip)
    {
        combo.clear();
        for (int i = 0; i < items.size(); ++i)
            combo.addItem(items[i], i + 1);
        if (items.size() > 0)
            combo.setSelectedId(1, juce::dontSendNotification);
        combo.setTooltip(tooltip);
        combo.onChange = [this]() { rebuildPromptPreview(); };
        addToContent(combo);
    }

    // Create a row of toggle buttons for a tag group.
    // If radio=true, clicking one deselects all others (mutually exclusive).
    void setupToggleGroup(juce::OwnedArray<CustomButton>& buttons,
                          const juce::StringArray& labels, bool radio)
    {
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* btn = new CustomButton();
            btn->setButtonText(labels[i]);
            btn->setButtonStyle(CustomButton::ButtonStyle::Inactive);
            btn->setClickingTogglesState(!radio); // for radio, we handle toggle manually
            if (radio)
            {
                btn->onClick = [this, btn, &buttons]()
                {
                    bool wasOn = btn->getToggleState();
                    for (auto* b : buttons)
                    {
                        b->setToggleState(false, juce::dontSendNotification);
                        b->setButtonStyle(CustomButton::ButtonStyle::Inactive);
                    }
                    if (!wasOn)
                    {
                        btn->setToggleState(true, juce::dontSendNotification);
                        btn->setButtonStyle(CustomButton::ButtonStyle::Jerry);
                    }
                    rebuildPromptPreview();
                };
            }
            else
            {
                btn->onClick = [this, btn]()
                {
                    btn->setButtonStyle(btn->getToggleState()
                        ? CustomButton::ButtonStyle::Jerry
                        : CustomButton::ButtonStyle::Inactive);
                    rebuildPromptPreview();
                };
            }
            addToContent(*btn);
            buttons.add(btn);
        }
    }

    // ---- Descriptor extra row management (like DariusUI GenStyleRow) ----
    void addDescriptorExtraRow(const juce::String& text)
    {
        if ((int)descriptorExtraRows.size() >= descriptorExtraMax) return;

        DescriptorExtraRow row;
        row.text = std::make_unique<CustomTextEditor>();
        row.text->setMultiLine(false);
        row.text->setReturnKeyStartsNewLine(false);
        row.text->setText(text, juce::dontSendNotification);
        row.text->setTextToShowWhenEmpty("type a descriptor...", juce::Colour(0xff666666));
        row.text->setTooltip("extra descriptor tag (e.g. lo-fi, 808, plucky)");
        row.text->onTextChange = [this]() { rebuildPromptPreview(); };

        row.remove = std::make_unique<CustomButton>();
        row.remove->setButtonText(juce::String::fromUTF8("\xe2\x80\x93")); // en-dash
        row.remove->setButtonStyle(CustomButton::ButtonStyle::Inactive);
        row.remove->setTooltip("remove this extra descriptor");
        auto* removePtr = row.remove.get();
        row.remove->onClick = [this, removePtr]()
        {
            juce::MessageManager::callAsync([this, removePtr]()
            {
                int idx = -1;
                for (size_t i = 0; i < descriptorExtraRows.size(); ++i)
                    if (descriptorExtraRows[i].remove.get() == removePtr)
                        { idx = (int)i; break; }
                if (idx >= 0)
                    removeDescriptorExtraRow(idx);
            });
        };

        contentComponent->addAndMakeVisible(*row.text);
        contentComponent->addAndMakeVisible(*row.remove);
        descriptorExtraRows.push_back(std::move(row));
        rebuildPromptPreview();
    }

    void removeDescriptorExtraRow(int index)
    {
        if (index < 0 || index >= (int)descriptorExtraRows.size()) return;

        auto& row = descriptorExtraRows[(size_t)index];
        if (row.text) { row.text->setVisible(false); contentComponent->removeChildComponent(row.text.get()); }
        if (row.remove) { row.remove->setVisible(false); contentComponent->removeChildComponent(row.remove.get()); }

        descriptorExtraRows.erase(descriptorExtraRows.begin() + index);
        rebuildPromptPreview();
        updateContentLayout();
    }

    void clearDescriptorExtraRows()
    {
        for (auto& row : descriptorExtraRows)
        {
            if (row.text) { row.text->setVisible(false); contentComponent->removeChildComponent(row.text.get()); }
            if (row.remove) { row.remove->setVisible(false); contentComponent->removeChildComponent(row.remove.get()); }
        }
        descriptorExtraRows.clear();
    }

    juce::StringArray getDescriptorsExtraInternal() const
    {
        juce::StringArray result;
        for (auto& row : descriptorExtraRows)
            if (row.text)
            {
                auto t = row.text->getText().trim();
                if (t.isNotEmpty())
                    result.add(t);
            }
        return result;
    }

    juce::File getPresetsFolder() const
    {
        auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        auto folder = docs.getChildFile("gary4juce").getChildFile("foundation-presets");
        if (!folder.isDirectory())
            folder.createDirectory();
        return folder;
    }

    void savePreset()
    {
        auto folder = getPresetsFolder();

        // Build a default name from family + subfamily
        juce::String defaultName = familyComboBox.getText() + " - " + subfamilyComboBox.getText();
        defaultName = defaultName.replaceCharacters("\\/:*?\"<>|", "_________");

        auto chooser = std::make_shared<juce::FileChooser>(
            "Save Foundation Preset",
            folder.getChildFile(defaultName + ".f1preset"),
            "*.f1preset");

        chooser->launchAsync(juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File()) return;

                auto filePath = file.getFullPathName();
                if (!filePath.endsWithIgnoreCase(".f1preset"))
                    file = juce::File(filePath + ".f1preset");

                juce::String json = serializeState();
                file.replaceWithText(json);
                infoLabel.setText("Preset saved: " + file.getFileNameWithoutExtension(),
                                  juce::dontSendNotification);
            });
    }

    void loadPreset()
    {
        auto folder = getPresetsFolder();

        auto chooser = std::make_shared<juce::FileChooser>(
            "Load Foundation Preset",
            folder,
            "*.f1preset");

        chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File() || !file.existsAsFile()) return;

                juce::String json = file.loadFileAsString();
                if (json.isNotEmpty())
                {
                    restoreState(json);
                    infoLabel.setText("Loaded: " + file.getFileNameWithoutExtension(),
                                      juce::dontSendNotification);
                }
            });
    }

    void updateSubfamilyOptions()
    {
        juce::String currentFamily = familyComboBox.getText();
        juce::String currentSub = subfamilyComboBox.getText();

        auto subs = FoundationTags::subfamiliesForFamily(currentFamily);
        subfamilyComboBox.clear(juce::dontSendNotification);
        for (int i = 0; i < subs.size(); ++i)
            subfamilyComboBox.addItem(subs[i], i + 1);

        // Try to keep selection
        int idx = subs.indexOf(currentSub);
        subfamilyComboBox.setSelectedId(idx >= 0 ? idx + 1 : 1, juce::dontSendNotification);
    }

    void updateBpmDisplay()
    {
        int foundationBpm = getFoundationBpm();
        bpmValueLabel.setText(juce::String(juce::roundToInt(hostBpm)) + " bpm", juce::dontSendNotification);
        updateBpmWarning();

        // Update duration info
        int bars = getSelectedBars();
        double genDuration = bars * 4.0 * 60.0 / foundationBpm;
        juce::String info = juce::String(bars) + " bars at " + juce::String(foundationBpm)
            + " BPM = " + juce::String(genDuration, 1) + "s";
        if (std::abs(hostBpm - foundationBpm) > 0.5)
        {
            double stretchedDuration = bars * 4.0 * 60.0 / hostBpm;
            info += " (stretched to " + juce::String(stretchedDuration, 1) + "s at "
                + juce::String(juce::roundToInt(hostBpm)) + " BPM)";
        }
        durationInfoLabel.setText(info, juce::dontSendNotification);
    }

    void updateBpmWarning()
    {
        int foundationBpm = getFoundationBpm();
        bool stretching = std::abs(hostBpm - foundationBpm) > 0.5;
        bool visChanged = (bpmWarningLabel.isVisible() != stretching);
        bpmWarningLabel.setVisible(stretching);
        if (stretching)
        {
            juce::String warning = juce::String::fromUTF8("\xe2\x9a\xa0 ") + "generates at "
                + juce::String(foundationBpm) + " BPM, stretches to "
                + juce::String(juce::roundToInt(hostBpm));
            bpmWarningLabel.setText(warning, juce::dontSendNotification);
        }
        // Re-layout since warning row affects viewport position (guard against recursion)
        if (visChanged && getWidth() > 0 && !inLayout)
        {
            inLayout = true;
            resized();
            inLayout = false;
        }
    }

    void rebuildPromptPreview()
    {
        juce::String customOverride = overrideEditor.getText().trim();
        int foundationBpm = getFoundationBpm();
        int bars = getSelectedBars();

        if (customOverride.isNotEmpty())
        {
            juce::String prompt = customOverride;
            if (!prompt.containsIgnoreCase("Bars"))
                prompt += ", " + juce::String(bars) + " Bars";
            if (!prompt.containsIgnoreCase("BPM"))
                prompt += ", " + juce::String(foundationBpm) + " BPM";
            if (!prompt.containsIgnoreCase(keyRootComboBox.getText()))
                prompt += ", " + keyRootComboBox.getText() + " " + keyModeComboBox.getText();
            lastBuiltPrompt = prompt;
        }
        else
        {
            juce::StringArray parts;
            parts.add(familyComboBox.getText());
            if (subfamilyComboBox.getNumItems() > 0)
                parts.add(subfamilyComboBox.getText());
            if (descriptorAVisible) parts.add(descriptorAKnob.getCurrentLabel());
            if (descriptorBVisible) parts.add(descriptorBKnob.getCurrentLabel());
            if (descriptorCVisible) parts.add(descriptorCKnob.getCurrentLabel());
            for (auto& extra : getDescriptorsExtraInternal())
                parts.add(extra);

            // All categorized tag groups
            for (auto& tag : getSelectedBehaviorTags())
                parts.add(tag);

            if (reverbToggle.getToggleState())
                parts.add(reverbKnob.getCurrentLabel());
            if (delayToggle.getToggleState())
                parts.add(delayKnob.getCurrentLabel());
            if (distortionToggle.getToggleState())
                parts.add(distortionKnob.getCurrentLabel());
            if (phaserToggle.getToggleState())
                parts.add(phaserKnob.getCurrentLabel());
            if (bitcrushToggle.getToggleState())
                parts.add(bitcrushKnob.getCurrentLabel());

            parts.add(juce::String(bars) + " Bars");
            parts.add(juce::String(foundationBpm) + " BPM");
            parts.add(keyRootComboBox.getText() + " " + keyModeComboBox.getText());

            // Dedupe
            juce::StringArray deduped;
            for (auto& p : parts)
                if (p.isNotEmpty() && !deduped.contains(p))
                    deduped.add(p);

            lastBuiltPrompt = deduped.joinIntoString(", ");
        }

        previewText.setText(lastBuiltPrompt, juce::dontSendNotification);
        updateBpmDisplay();
    }

    void randomizeControls()
    {
        std::random_device rd;
        std::mt19937 gen(rd());

        // Random family
        auto& families = FoundationTags::families();
        std::uniform_int_distribution<int> famDist(0, families.size() - 1);
        familyComboBox.setSelectedId(famDist(gen) + 1, juce::dontSendNotification);
        updateSubfamilyOptions();

        // Random subfamily
        if (subfamilyComboBox.getNumItems() > 0)
        {
            std::uniform_int_distribution<int> subDist(0, subfamilyComboBox.getNumItems() - 1);
            subfamilyComboBox.setSelectedId(subDist(gen) + 1, juce::dontSendNotification);
        }

        // Random descriptors (always show all 3 on local randomize)
        {
            descriptorAVisible = true; descriptorBVisible = true; descriptorCVisible = true;
            descriptorAKnob.setVisible(true); descriptorBKnob.setVisible(true); descriptorCKnob.setVisible(true);
            auto& tags = FoundationTags::timbreTags();
            std::uniform_int_distribution<int> d(0, tags.size() - 1);
            descriptorAKnob.setCurrentStep(d(gen), false);
            descriptorBKnob.setCurrentStep(d(gen), false);
            descriptorCKnob.setCurrentStep(d(gen), false);
        }

        // Clear extra descriptors on local randomize
        clearDescriptorExtraRows();

        // Random structure (pick one)
        {
            clearToggleGroup(structureButtons);
            std::uniform_int_distribution<int> d(0, structureButtons.size() - 1);
            auto* btn = structureButtons[d(gen)];
            btn->setToggleState(true, juce::dontSendNotification);
            btn->setButtonStyle(CustomButton::ButtonStyle::Jerry);
        }

        // Random speed (toggle + knob)
        {
            std::uniform_int_distribution<int> chance(0, 2); // ~33% chance of speed
            bool speedOn = chance(gen) == 0;
            speedToggle.setToggleState(speedOn, juce::dontSendNotification);
            speedToggle.setButtonStyle(speedOn ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            speedKnob.setVisible(speedOn);
            if (speedOn)
            {
                std::uniform_int_distribution<int> d(0, FoundationTags::speedTags().size() - 1);
                speedKnob.setCurrentStep(d(gen), false);
            }
        }

        // Dual-knob toggles (rhythm, contour, density)
        auto randomizeDual = [&](CustomButton& toggle, SteppedKnob& knobA, SteppedKnob& knobB,
                                  CustomButton& addBtn, CustomButton& removeBtn, bool& bVis,
                                  int numTags)
        {
            std::uniform_int_distribution<int> chance(0, 2);
            bool on = chance(gen) == 0;
            toggle.setToggleState(on, juce::dontSendNotification);
            toggle.setButtonStyle(on ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            knobA.setVisible(on);
            if (on)
            {
                std::uniform_int_distribution<int> d(0, numTags - 1);
                knobA.setCurrentStep(d(gen), false);
                // 33% chance of 2nd knob
                bool dual = chance(gen) == 0;
                bVis = dual;
                knobB.setVisible(on && dual);
                addBtn.setVisible(on && !dual);
                removeBtn.setVisible(on && dual);
                if (dual) knobB.setCurrentStep(d(gen), false);
            }
            else
            {
                bVis = false;
                knobB.setVisible(false);
                addBtn.setVisible(false);
                removeBtn.setVisible(false);
            }
        };
        randomizeDual(rhythmToggle, rhythmKnobA, rhythmKnobB, rhythmAddButton, rhythmRemoveB,
                       rhythmBVisible, FoundationTags::rhythmTags().size());
        randomizeDual(contourToggle, contourKnobA, contourKnobB, contourAddButton, contourRemoveB,
                       contourBVisible, FoundationTags::contourTags().size());
        randomizeDual(densityToggle, densityKnobA, densityKnobB, densityAddButton, densityRemoveB,
                       densityBVisible, FoundationTags::densityTags().size());

        // Single-knob toggles (spatial, style, band)
        auto randomizeSingle = [&](CustomButton& toggle, SteppedKnob& knob, int numTags)
        {
            std::uniform_int_distribution<int> chance(0, 2);
            bool on = chance(gen) == 0;
            toggle.setToggleState(on, juce::dontSendNotification);
            toggle.setButtonStyle(on ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            knob.setVisible(on);
            if (on)
            {
                std::uniform_int_distribution<int> d(0, numTags - 1);
                knob.setCurrentStep(d(gen), false);
            }
        };
        randomizeSingle(spatialToggle, spatialKnob, FoundationTags::spatialTags().size());
        randomizeSingle(styleToggle, styleKnob, FoundationTags::styleTags().size());
        randomizeSingle(bandToggle, bandKnob, FoundationTags::bandTags().size());

        // Random wave/tech (toggle + knob)
        {
            std::uniform_int_distribution<int> chance(0, 3); // 25% chance
            bool waveTechOn = chance(gen) == 0;
            waveTechToggle.setToggleState(waveTechOn, juce::dontSendNotification);
            waveTechToggle.setButtonStyle(waveTechOn ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            waveTechKnob.setVisible(waveTechOn);
            if (waveTechOn)
            {
                std::uniform_int_distribution<int> d(0, FoundationTags::waveTechTags().size() - 1);
                waveTechKnob.setCurrentStep(d(gen), false);
            }
        }

        // Random FX (low probability)
        std::uniform_int_distribution<int> fxChance(0, 3); // 25% chance each
        auto maybeToggleFxKnob = [&](CustomButton& toggle, SteppedKnob& knob, int numSteps) {
            bool enable = fxChance(gen) == 0;
            toggle.setToggleState(enable, juce::dontSendNotification);
            toggle.setButtonStyle(enable ? CustomButton::ButtonStyle::Jerry : CustomButton::ButtonStyle::Inactive);
            knob.setVisible(enable);
            if (enable && numSteps > 0)
            {
                std::uniform_int_distribution<int> d(0, numSteps - 1);
                knob.setCurrentStep(d(gen), false);
            }
        };
        maybeToggleFxKnob(reverbToggle, reverbKnob, FoundationTags::reverbAmounts().size());
        maybeToggleFxKnob(delayToggle, delayKnob, FoundationTags::delayTypes().size());
        maybeToggleFxKnob(distortionToggle, distortionKnob, FoundationTags::distortionAmounts().size());
        maybeToggleFxKnob(phaserToggle, phaserKnob, FoundationTags::phaserAmounts().size());
        maybeToggleFxKnob(bitcrushToggle, bitcrushKnob, FoundationTags::bitcrushAmounts().size());

        // Random key
        std::uniform_int_distribution<int> keyDist(0, 11);
        keyRootComboBox.setSelectedId(keyDist(gen) + 1, juce::dontSendNotification);
        std::uniform_int_distribution<int> modeDist(0, 1);
        keyModeComboBox.setSelectedId(modeDist(gen) + 1, juce::dontSendNotification);

        rebuildPromptPreview();
        updateContentLayout();
    }

    void updateContentLayout()
    {
        if (contentComponent == nullptr || contentViewport == nullptr)
            return;

        const int viewportWidth = juce::jmax(220, contentViewport->getWidth());
        const int scrollbarWidth = contentViewport->getVerticalScrollBar().isVisible()
            ? contentViewport->getVerticalScrollBar().getWidth() : 0;
        const int contentWidth = juce::jmax(220, viewportWidth - scrollbarWidth - 4);

        constexpr int sidePadding = 8;
        int y = 8;
        const auto fullRow = [&](int height) {
            return juce::Rectangle<int>(sidePadding, y, contentWidth - (sidePadding * 2), height);
        };

        // ---- INSTRUMENT (family + type side by side) ----
        {
            auto instrRow = fullRow(16);
            int halfW = (instrRow.getWidth() - 4) / 2;
            familyLabel.setBounds(instrRow.getX(), y, halfW, 16);
            subfamilyLabel.setBounds(instrRow.getX() + halfW + 4, y, halfW, 16);
        }
        y += 16;
        {
            auto instrRow = fullRow(26);
            int halfW = (instrRow.getWidth() - 4) / 2;
            familyComboBox.setBounds(instrRow.getX(), y, halfW, 26);
            subfamilyComboBox.setBounds(instrRow.getX() + halfW + 4, y, halfW, 26);
        }
        y += 32;

        // ---- DESCRIPTORS (up to 3 stepped knobs with +/- controls) ----
        {
            // Section label + "+" button on the same row
            auto labelRow = fullRow(16);
            descriptorSectionLabel.setBounds(labelRow.removeFromLeft(labelRow.getWidth() - 26));

            // Show "+" only if at least one knob is hidden
            bool allVisible = descriptorAVisible && descriptorBVisible && descriptorCVisible;
            descriptorAddButton.setVisible(!allVisible);
            descriptorAddButton.setBounds(labelRow.removeFromRight(22).withHeight(16));
        }
        y += 18;

        // Layout visible knobs and their "-" buttons
        {
            int visCount = (descriptorAVisible ? 1 : 0) + (descriptorBVisible ? 1 : 0) + (descriptorCVisible ? 1 : 0);
            descriptorAKnob.setVisible(descriptorAVisible);
            descriptorBKnob.setVisible(descriptorBVisible);
            descriptorCKnob.setVisible(descriptorCVisible);

            if (visCount > 0)
            {
                const int knobH = 64;
                const int removeH = 14;
                auto row = fullRow(knobH);
                int colW = (row.getWidth() - (visCount - 1) * 4) / visCount;
                int removeW = juce::jmin(22, colW);

                auto placeKnobAndRemove = [&](SteppedKnob& knob, CustomButton& removeBtn, bool vis)
                {
                    if (!vis) { removeBtn.setVisible(false); return; }
                    auto col = row.removeFromLeft(colW);
                    if (row.getWidth() > 4) row.removeFromLeft(4);
                    knob.setBounds(col);
                    // "-" button centered below the knob
                    removeBtn.setVisible(true);
                    removeBtn.setBounds(col.getCentreX() - removeW / 2, col.getBottom() + 1, removeW, removeH);
                };

                placeKnobAndRemove(descriptorAKnob, descriptorRemoveA, descriptorAVisible);
                placeKnobAndRemove(descriptorBKnob, descriptorRemoveB, descriptorBVisible);
                placeKnobAndRemove(descriptorCKnob, descriptorRemoveC, descriptorCVisible);

                y += knobH + removeH + 6;
            }
            else
            {
                descriptorRemoveA.setVisible(false);
                descriptorRemoveB.setVisible(false);
                descriptorRemoveC.setVisible(false);
                y += 4;
            }
        }

        // ---- DESCRIPTORS EXTRA (dynamic text rows) ----
        {
            auto extraLabelRow = fullRow(16);
            bool canAdd = (int)descriptorExtraRows.size() < descriptorExtraMax;
            descriptorExtraLabel.setVisible(true);
            descriptorExtraLabel.setBounds(extraLabelRow.removeFromLeft(extraLabelRow.getWidth() - 26));
            descriptorExtraAddButton.setVisible(canAdd);
            descriptorExtraAddButton.setBounds(extraLabelRow.removeFromRight(22).withHeight(16));
        }
        y += 18;

        // Layout extra rows
        for (auto& row : descriptorExtraRows)
        {
            if (!row.text || !row.remove) continue;
            auto slice = fullRow(24);
            auto removeBounds = slice.removeFromRight(22);
            row.text->setBounds(slice);
            row.remove->setBounds(removeBounds);
            y += 28;
        }
        if (descriptorExtraRows.empty())
            y += 0; // no extra space needed

        // ---- STRUCTURE (radio — centered, primary row larger) ----
        structureSectionLabel.setBounds(fullRow(16)); y += 18;
        {
            // Primary row: melody, arp, chord progression (larger)
            constexpr int primaryH = 28;
            constexpr int secondaryH = 22;
            constexpr int gap = 4;
            juce::Font btnFont(juce::FontOptions(11.0f));

            // Measure primary buttons
            juce::Array<CustomButton*> primaryBtns, secondaryBtns;
            for (auto* btn : structureButtons)
            {
                auto text = btn->getButtonText().toLowerCase();
                if (text == "melody" || text == "arp" || text == "chord progression")
                    primaryBtns.add(btn);
                else
                    secondaryBtns.add(btn);
            }

            // Center primary row
            int totalPrimaryW = 0;
            for (auto* btn : primaryBtns)
                totalPrimaryW += juce::jmax(50, (int)btnFont.getStringWidthFloat(btn->getButtonText()) + 20);
            totalPrimaryW += (primaryBtns.size() - 1) * gap;
            int px = sidePadding + (contentWidth - sidePadding * 2 - totalPrimaryW) / 2;
            for (auto* btn : primaryBtns)
            {
                int bw = juce::jmax(50, (int)btnFont.getStringWidthFloat(btn->getButtonText()) + 20);
                btn->setBounds(px, y, bw, primaryH);
                px += bw + gap;
            }
            y += primaryH + gap;

            // Center secondary row (smaller)
            if (!secondaryBtns.isEmpty())
            {
                int totalSecW = 0;
                for (auto* btn : secondaryBtns)
                    totalSecW += juce::jmax(40, (int)btnFont.getStringWidthFloat(btn->getButtonText()) + 14);
                totalSecW += (secondaryBtns.size() - 1) * gap;
                int sx = sidePadding + (contentWidth - sidePadding * 2 - totalSecW) / 2;
                for (auto* btn : secondaryBtns)
                {
                    int bw = juce::jmax(40, (int)btnFont.getStringWidthFloat(btn->getButtonText()) + 14);
                    btn->setBounds(sx, y, bw, secondaryH);
                    sx += bw + gap;
                }
                y += secondaryH + gap;
            }
            y += 4;
        }

        // ---- BEHAVIOR TOGGLES (two per row to save vertical space) ----
        {
            constexpr int toggleH = 26;
            constexpr int toggleGap = 4;
            auto usableW = contentWidth - sidePadding * 2;
            int halfW = (usableW - toggleGap) / 2;

            // Helper: lay out a pair of toggles on one row
            auto layoutTogglePair = [&](CustomButton& left, CustomButton& right)
            {
                left.setBounds(sidePadding, y, halfW, toggleH);
                right.setBounds(sidePadding + halfW + toggleGap, y, halfW, toggleH);
                y += toggleH + toggleGap;
            };

            // Row 1: speed | rhythm
            layoutTogglePair(speedToggle, rhythmToggle);

            // Knobs for speed + rhythm (side by side if both open)
            {
                bool speedOn = speedToggle.getToggleState();
                bool rhythmOn = rhythmToggle.getToggleState();
                if (speedOn || rhythmOn)
                {
                    layoutKnobsForPair(speedOn, &speedKnob, nullptr, false,
                                        nullptr, nullptr,
                                        rhythmOn, &rhythmKnobA, &rhythmKnobB, rhythmBVisible,
                                        &rhythmAddButton, &rhythmRemoveB,
                                        fullRow, y, sidePadding, contentWidth);
                }
                else
                {
                    rhythmAddButton.setVisible(false);
                    rhythmRemoveB.setVisible(false);
                }
            }

            // Row 2: contour | density
            layoutTogglePair(contourToggle, densityToggle);

            // Knobs for contour + density
            {
                bool contourOn = contourToggle.getToggleState();
                bool densityOn = densityToggle.getToggleState();
                if (contourOn || densityOn)
                {
                    layoutKnobsForPair(contourOn, &contourKnobA, &contourKnobB, contourBVisible,
                                        &contourAddButton, &contourRemoveB,
                                        densityOn, &densityKnobA, &densityKnobB, densityBVisible,
                                        &densityAddButton, &densityRemoveB,
                                        fullRow, y, sidePadding, contentWidth);
                }
                else
                {
                    contourAddButton.setVisible(false);
                    contourRemoveB.setVisible(false);
                    densityAddButton.setVisible(false);
                    densityRemoveB.setVisible(false);
                }
            }

            // Row 3: spatial | style
            layoutTogglePair(spatialToggle, styleToggle);

            // Knobs for spatial + style (both single-knob)
            {
                bool spatialOn = spatialToggle.getToggleState();
                bool styleOn = styleToggle.getToggleState();
                if (spatialOn || styleOn)
                {
                    layoutKnobsForPair(spatialOn, &spatialKnob, nullptr, false,
                                        nullptr, nullptr,
                                        styleOn, &styleKnob, nullptr, false,
                                        nullptr, nullptr,
                                        fullRow, y, sidePadding, contentWidth);
                }
            }

            // Row 4: band | synthesis
            layoutTogglePair(bandToggle, waveTechToggle);

            // Knobs for band + synthesis (both single-knob)
            {
                bool bandOn = bandToggle.getToggleState();
                bool waveTechOn = waveTechToggle.getToggleState();
                if (bandOn || waveTechOn)
                {
                    layoutKnobsForPair(bandOn, &bandKnob, nullptr, false,
                                        nullptr, nullptr,
                                        waveTechOn, &waveTechKnob, nullptr, false,
                                        nullptr, nullptr,
                                        fullRow, y, sidePadding, contentWidth);
                }
            }
        }

        // ---- FX (guitar-pedal style: toggle reveals stepped knob) ----
        fxSectionLabel.setBounds(fullRow(16)); y += 18;
        {
            // Row 1: reverb, delay, distortion
            auto fxRow1 = fullRow(26);
            int toggleW = (fxRow1.getWidth() - 8) / 3;
            reverbToggle.setBounds(fxRow1.removeFromLeft(toggleW));
            fxRow1.removeFromLeft(4);
            delayToggle.setBounds(fxRow1.removeFromLeft(toggleW));
            fxRow1.removeFromLeft(4);
            distortionToggle.setBounds(fxRow1);
        }
        y += 30;
        {
            // Row 2: phaser, bitcrush
            auto fxRow2 = fullRow(26);
            int toggleW2 = (fxRow2.getWidth() - 4) / 2;
            phaserToggle.setBounds(fxRow2.removeFromLeft(toggleW2));
            fxRow2.removeFromLeft(4);
            bitcrushToggle.setBounds(fxRow2);
        }
        y += 30;

        // Show knobs side-by-side for active FX — shrink to fit all on one row
        {
            int activeCount = (reverbToggle.getToggleState() ? 1 : 0)
                            + (delayToggle.getToggleState() ? 1 : 0)
                            + (distortionToggle.getToggleState() ? 1 : 0)
                            + (phaserToggle.getToggleState() ? 1 : 0)
                            + (bitcrushToggle.getToggleState() ? 1 : 0);
            if (activeCount > 0)
            {
                constexpr int knobGap = 4;
                int usableW = contentWidth - sidePadding * 2;
                // Scale knob size to always fit in one row
                int knobSize = juce::jmin(64, (usableW - (activeCount - 1) * knobGap) / activeCount);
                knobSize = juce::jmax(40, knobSize); // floor at 40px

                int totalW = activeCount * knobSize + (activeCount - 1) * knobGap;
                int kx = sidePadding + (usableW - totalW) / 2;

                auto maybePlace = [&](CustomButton& toggle, SteppedKnob& knob)
                {
                    if (!toggle.getToggleState()) return;
                    knob.setBounds(kx, y, knobSize, knobSize);
                    kx += knobSize + knobGap;
                };

                maybePlace(reverbToggle, reverbKnob);
                maybePlace(delayToggle, delayKnob);
                maybePlace(distortionToggle, distortionKnob);
                maybePlace(phaserToggle, phaserKnob);
                maybePlace(bitcrushToggle, bitcrushKnob);
                y += knobSize + 6;
            }
        }

        // ---- ADVANCED ----
        advancedToggle.setBounds(fullRow(26)); y += 32;

        if (advancedOpen)
        {
            auto stepsRow = fullRow(28);
            stepsLabel.setBounds(stepsRow.removeFromLeft(60));
            stepsSlider.setBounds(stepsRow);
            y += 34;

            auto guidanceRow = fullRow(28);
            guidanceLabel.setBounds(guidanceRow.removeFromLeft(60));
            guidanceSlider.setBounds(guidanceRow);
            y += 34;

            {
                auto seedRow = fullRow(22);
                seedToggle.setBounds(seedRow.removeFromLeft(90));
                seedRow.removeFromLeft(4);
                seedEditor.setBounds(seedRow.removeFromLeft(100));
            }
            y += 26;

            // Audio2Audio
            audio2audioToggle.setBounds(fullRow(22)); y += 24;
            if (audio2audioToggle.getToggleState())
            {
                auto transRow = fullRow(28);
                transformationLabel.setBounds(transRow.removeFromLeft(90));
                transformationSlider.setBounds(transRow);
                y += 32;
            }

            overrideLabel.setBounds(fullRow(16)); y += 18;
            overrideEditor.setBounds(fullRow(28)); y += 34;
        }

        // Gate advanced component visibility
        stepsLabel.setVisible(advancedOpen);
        stepsSlider.setVisible(advancedOpen);
        guidanceLabel.setVisible(advancedOpen);
        guidanceSlider.setVisible(advancedOpen);
        seedToggle.setVisible(advancedOpen);
        seedEditor.setVisible(advancedOpen);
        audio2audioToggle.setVisible(advancedOpen);
        transformationLabel.setVisible(advancedOpen && audio2audioToggle.getToggleState());
        transformationSlider.setVisible(advancedOpen && audio2audioToggle.getToggleState());
        overrideLabel.setVisible(advancedOpen);
        overrideEditor.setVisible(advancedOpen);

        // ---- PROMPT PREVIEW ----
        previewLabel.setBounds(fullRow(16)); y += 18;
        previewText.setBounds(fullRow(48)); y += 54;
        durationInfoLabel.setBounds(fullRow(14)); y += 20;

        y += 4; // bottom padding

        // Set content size
        contentComponent->setSize(contentWidth, y);
    }

    // ==================== MEMBERS ====================

    juce::Label titleLabel;
    juce::Rectangle<int> titleBounds;

    std::unique_ptr<juce::Component> contentComponent;
    std::unique_ptr<juce::Viewport> contentViewport;
    CustomLookAndFeel customLookAndFeel;

    // Instrument
    juce::Label familyLabel;
    CustomComboBox familyComboBox;
    juce::Label subfamilyLabel;
    CustomComboBox subfamilyComboBox;

    // Descriptors (stepped knobs — full TIMBRE_TAGS vocabulary, removable)
    juce::Label descriptorSectionLabel;
    SteppedKnob descriptorAKnob;
    SteppedKnob descriptorBKnob;
    SteppedKnob descriptorCKnob;
    CustomButton descriptorRemoveA, descriptorRemoveB, descriptorRemoveC;
    CustomButton descriptorAddButton;
    bool descriptorAVisible = true, descriptorBVisible = true, descriptorCVisible = true;

    // Descriptors extra (dynamic text rows with + / - like DariusUI styles)
    struct DescriptorExtraRow
    {
        std::unique_ptr<CustomTextEditor> text;
        std::unique_ptr<CustomButton> remove;
    };
    juce::Label descriptorExtraLabel;
    CustomButton descriptorExtraAddButton;
    std::vector<DescriptorExtraRow> descriptorExtraRows;
    static constexpr int descriptorExtraMax = 5;

    // Structure (mutually exclusive radio)
    juce::Label structureSectionLabel;
    juce::OwnedArray<CustomButton> structureButtons;

    // Speed (toggle + knob)
    CustomButton speedToggle;
    SteppedKnob speedKnob;

    // Rhythm (toggle + dual knob)
    CustomButton rhythmToggle;
    SteppedKnob rhythmKnobA, rhythmKnobB;
    CustomButton rhythmAddButton, rhythmRemoveB;
    bool rhythmBVisible = false;

    // Contour (toggle + dual knob)
    CustomButton contourToggle;
    SteppedKnob contourKnobA, contourKnobB;
    CustomButton contourAddButton, contourRemoveB;
    bool contourBVisible = false;

    // Density (toggle + dual knob)
    CustomButton densityToggle;
    SteppedKnob densityKnobA, densityKnobB;
    CustomButton densityAddButton, densityRemoveB;
    bool densityBVisible = false;

    // Spatial (toggle + knob)
    CustomButton spatialToggle;
    SteppedKnob spatialKnob;

    // Style (toggle + knob)
    CustomButton styleToggle;
    SteppedKnob styleKnob;

    // Band / frequency (toggle + knob)
    CustomButton bandToggle;
    SteppedKnob bandKnob;

    // Wave / tech (toggle + knob)
    CustomButton waveTechToggle;
    SteppedKnob waveTechKnob;

    // FX
    juce::Label fxSectionLabel;
    CustomButton reverbToggle;
    SteppedKnob reverbKnob;
    CustomButton delayToggle;
    SteppedKnob delayKnob;
    CustomButton distortionToggle;
    SteppedKnob distortionKnob;
    CustomButton phaserToggle;
    SteppedKnob phaserKnob;
    CustomButton bitcrushToggle;
    SteppedKnob bitcrushKnob;

    // Structure (persistent header — not in scrollable content)
    CustomComboBox barsComboBox;
    juce::Label bpmValueLabel;
    juce::Label bpmWarningLabel;
    CustomSlider standaloneBpmSlider;
    CustomComboBox keyRootComboBox;
    CustomComboBox keyModeComboBox;

    // Advanced
    CustomButton advancedToggle;
    bool advancedOpen = false;
    juce::Label stepsLabel;
    CustomSlider stepsSlider;
    juce::Label guidanceLabel;
    CustomSlider guidanceSlider;
    juce::ToggleButton seedToggle;
    CustomTextEditor seedEditor;
    // Audio2Audio
    juce::ToggleButton audio2audioToggle;
    juce::Label transformationLabel;
    CustomSlider transformationSlider;

    juce::Label overrideLabel;
    CustomTextEditor overrideEditor;

    // Preview
    juce::Label previewLabel;
    juce::TextEditor previewText;
    juce::Label durationInfoLabel;

    // Actions
    IconButton savePresetButton { IconButton::Icon::Save };
    IconButton loadPresetButton { IconButton::Icon::Load };
    CustomButton randomizeButton;
    CustomButton generateButton;
    juce::Label infoLabel;

    // State
    double hostBpm = 120.0;
    bool isStandaloneMode = false;
    bool titleHidden = false;
    bool inLayout = false;
    juce::String lastBuiltPrompt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FoundationUI)
};
