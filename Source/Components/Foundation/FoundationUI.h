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
    FoundationUI();

    ~FoundationUI() override;

    void paint(juce::Graphics&) override;

    void resized() override;

    void setVisibleForTab(bool visible);

    juce::Rectangle<int> getTitleBounds() const { return titleBounds; }

    void setTitleVisible(bool visible);

    // ==================== GETTERS ====================

    juce::String getFamily() const { return familyComboBox.getText(); }
    juce::String getSubfamily() const { return subfamilyComboBox.getText(); }
    juce::String getDescriptorA() const { return descriptorAVisible ? descriptorAKnob.getCurrentLabel() : ""; }
    juce::String getDescriptorB() const { return descriptorBVisible ? descriptorBKnob.getCurrentLabel() : ""; }
    juce::String getDescriptorC() const { return descriptorCVisible ? descriptorCKnob.getCurrentLabel() : ""; }
    juce::StringArray getDescriptorsExtraValues() const { return getDescriptorsExtraInternal(); }

    juce::StringArray getSelectedBehaviorTags() const;

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
    int getSeed() const;
    juce::String getCustomPromptOverride() const { return overrideEditor.getText().trim(); }
    bool getAudio2AudioEnabled() const { return audio2audioToggle.getToggleState(); }
    double getTransformation() const { return transformationSlider.getValue(); }

    juce::String getBuiltPrompt() const { return lastBuiltPrompt; }

    // ==================== SETTERS ====================

    void setBpm(double bpm);

    void setIsStandalone(bool standalone);

    void setFamily(const juce::String& family);

    void setSubfamily(const juce::String& sub);

    void setGenerateButtonEnabled(bool enabled, bool isGenerating);

    void setGenerateButtonText(const juce::String& text);

    void setInfoText(const juce::String& text);

    void setAdvancedOpen(bool open);

    void refreshDurationInfo();

    // ==================== STATE PERSISTENCE ====================

    /** Serialize the entire Foundation UI state to a JSON string. */
    juce::String serializeState() const;

    /** Restore the Foundation UI state from a JSON string. */
    void restoreState(const juce::String& jsonString);

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
    void applyRandomizeResponse(const juce::var& response);

    void setRandomizeEnabled(bool enabled);

private:

    void addToContent(juce::Component& component);

    void clearToggleGroup(juce::OwnedArray<CustomButton>& buttons);

    void setToggleByLabel(juce::OwnedArray<CustomButton>& buttons, const juce::String& label);

    // Flow-layout toggle buttons and advance y past them
    void layoutFlowButtons(juce::OwnedArray<CustomButton>& buttons, int startX, int maxX, int& y);

    // Layout knobs for a pair of toggles that share a row.
    // Each side can be single-knob (knobB=nullptr) or dual-knob (knobB!=nullptr with +/- buttons).
    void layoutKnobsForPair(bool leftOn, SteppedKnob* leftA, SteppedKnob* leftB, bool leftBVis,
                             CustomButton* leftAdd, CustomButton* leftRemove,
                             bool rightOn, SteppedKnob* rightA, SteppedKnob* rightB, bool rightBVis,
                             CustomButton* rightAdd, CustomButton* rightRemove,
                             const std::function<juce::Rectangle<int>(int)>& /*fullRow*/,
                             int& y, int sidePadding, int contentWidth);

    // Setup a toggle button that reveals a single stepped knob (spatial, style, band pattern)
    void setupSingleKnobToggle(CustomButton& toggle, SteppedKnob& knob,
                                const juce::String& label, const juce::StringArray& tags,
                                const juce::String& tooltip);

    // Setup a toggle button that reveals 1 knob with option to add/remove a 2nd (rhythm, contour, density)
    void setupDualKnobToggle(CustomButton& toggle, SteppedKnob& knobA, SteppedKnob& knobB,
                              CustomButton& addBtn, CustomButton& removeBtn, bool& bVisible,
                              const juce::String& label, const juce::StringArray& tags,
                              const juce::String& tooltip);

    void setupComboBox(CustomComboBox& combo, const juce::StringArray& items, const juce::String& tooltip);

    // Create a row of toggle buttons for a tag group.
    // If radio=true, clicking one deselects all others (mutually exclusive).
    void setupToggleGroup(juce::OwnedArray<CustomButton>& buttons,
                          const juce::StringArray& labels, bool radio);

    // ---- Descriptor extra row management (like DariusUI GenStyleRow) ----
    void addDescriptorExtraRow(const juce::String& text);

    void removeDescriptorExtraRow(int index);

    void clearDescriptorExtraRows();

    juce::StringArray getDescriptorsExtraInternal() const;

    juce::File getPresetsFolder() const
    {
        auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        auto folder = docs.getChildFile("gary4juce").getChildFile("foundation-presets");
        if (!folder.isDirectory())
            folder.createDirectory();
        return folder;
    }

    void savePreset();

    void loadPreset();

    void updateSubfamilyOptions();

    void updateBpmDisplay();

    void updateBpmWarning();

    void rebuildPromptPreview();

    void randomizeControls();

    void updateContentLayout();

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
