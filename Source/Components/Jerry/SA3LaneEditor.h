// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <JuceHeader.h>

#include "SA3Lanes.h"

#include <functional>
#include <vector>

// Stacked per-LoRA curves over a shared time axis, where a curve's height is the LoRA's strength.
// A JUCE port of sa3-iplug2-demo's SA3LaneEditorControl, with the same interactions: click to add
// a point, drag it, double- or right-click to remove it, shift-drag to draw freehand, and ctrl
// (cmd on macOS) to step off or onto the grid. Below kWideWidth each lane's controls sit above its
// curve instead of beside it. The editor owns the lanes and decides when the panel shows.
class SA3LaneEditor final : public juce::Component
{
public:
    struct Host
    {
        std::function<void(size_t laneIndex)> rollPrompt;          // fills in the lane's prompt later
        std::function<bool(const gary::SA3Lane&)> isMissing;       // its LoRA is off or gone
        std::function<float(const gary::SA3Lane&)> resetStrength;  // where reset flattens a curve
        std::function<void(const juce::String&)> notify;
        std::function<void()> changed;
    };

    SA3LaneEditor(sa3lanes::State& state, Host host);
    ~SA3LaneEditor() override;

    static juce::Colour laneColour(int index);

    void setTimeline(double seconds, double bpm, double beatsPerBar);
    // Where playback is, in seconds from the start of the curves. Anything outside them hides it.
    void setPlayhead(double seconds);
    // Min/max pairs across the range, drawn behind every curve in the strip above the lanes.
    void setBackdrop(std::vector<float> peaks);
    void lanesReplaced();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    using Rect = juce::Rectangle<float>;

    static constexpr float kWideWidth = 620.f;
    static constexpr float kPad = 14.f;   // leaves the strength labels clear of a point at the very start
    static constexpr float kCurvePad = 5.f;

    enum class Drag { None, Point, Stroke, Scroll };

    struct Row
    {
        Rect head;
        Rect canvas;
        Rect name;
        Rect remove;
        Rect prompt;
        Rect dice;
    };

    void layout();
    float xAt(const Rect& canvas, float position) const;
    float yAt(const Rect& canvas, float amount) const;
    float positionAt(const Rect& canvas, float x) const;
    float amountAt(const Rect& canvas, float y) const;
    void snap(float& position, float& amount, bool stepModifier, bool freehand) const;
    int hitPoint(const Row& row, const gary::SA3Lane& lane, juce::Point<float> at) const;
    void canvasDown(const juce::MouseEvent& e, const Row& row, gary::SA3Lane& lane, int laneIndex);
    void applyStroke(gary::SA3Lane& lane);
    void removePoint(gary::SA3Lane& lane, int index);
    void resetCurve(gary::SA3Lane& lane);
    void scrollTo(float y);
    size_t scrollFirstAt(float y) const;
    void openSnapMenu();
    void openPromptEntry(size_t laneIndex, const Row& row);
    void commitPromptEntry();
    void closePromptEntry();
    void changed();
    bool missing(const gary::SA3Lane& lane) const;
    static bool stepModifier(const juce::ModifierKeys& mods);

    void drawTopBar(juce::Graphics& g);
    void drawGrid(juce::Graphics& g, const Rect& canvas, bool labels);
    juce::Path curvePath(const Rect& canvas, const std::vector<gary::SA3LanePoint>& points) const;
    void drawTake(juce::Graphics& g);
    void drawDice(juce::Graphics& g, const Rect& rect);
    void drawReset(juce::Graphics& g, const Rect& rect);
    void drawLane(juce::Graphics& g, const Row& row, const gary::SA3Lane& lane, int index);
    void drawScrollbar(juce::Graphics& g);
    void drawPlayhead(juce::Graphics& g);
    void drawReadout(juce::Graphics& g);

    sa3lanes::State& state;
    Host host;

    double seconds = 0.0;
    double bpm = 0.0;
    double beatsPerBar = 4.0;
    std::vector<float> backdrop;
    float playhead = -1.f;

    Rect titleRect, blendOneRect, blendDistinctRect, snapRect, rangeRect, takeHead, takeCanvas, scrollTrack;
    std::vector<Row> rows;
    size_t firstLane = 0;

    Drag drag = Drag::None;
    int dragLane = -1;
    int dragPoint = -1;
    std::vector<gary::SA3LanePoint> stroke;

    int hoverLane = -1;
    int hoverPoint = -1;
    juce::Point<float> hoverAt;
    float readoutPosition = 0.f;
    float readoutAmount = 0.f;

    std::unique_ptr<juce::TextEditor> promptEntry;
    int promptEntryLane = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SA3LaneEditor)
};
