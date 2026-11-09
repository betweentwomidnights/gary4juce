// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "SA3LaneEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
    // The demo's palette, so the two plugins' lanes look alike.
    const juce::Colour kPanelDark(8, 8, 8);
    const juce::Colour kFrame(68, 68, 68);
    const juce::Colour kFrameSoft(38, 38, 38);
    const juce::Colour kRed(230, 32, 32);
    const juce::Colour kTextDim(165, 165, 165);
    const juce::Colour kTextFaint(105, 105, 105);
    const juce::Colour kButtonFill(16, 16, 16);
    const juce::Colour kCanvas(12, 12, 12);
    constexpr float kCorner = 4.f;

    float scrollThumbHeight(const juce::Rectangle<float>& track, size_t visible, size_t total)
    {
        return std::max(18.f, track.getHeight() * (float)visible / (float)std::max<size_t>(1, total));
    }

    void drawText(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float size,
                  juce::Colour colour, juce::Justification justification)
    {
        g.setColour(colour);
        g.setFont(juce::FontOptions(size));
        g.drawText(text, area, justification, true);
    }
}

SA3LaneEditor::SA3LaneEditor(sa3lanes::State& laneState, Host laneHost)
    : state(laneState), host(std::move(laneHost))
{
    setOpaque(false);
    setWantsKeyboardFocus(false);
}

SA3LaneEditor::~SA3LaneEditor() = default;

juce::Colour SA3LaneEditor::laneColour(int index)
{
    static const juce::Colour colours[] = {
        juce::Colour(226, 160, 59), juce::Colour(79, 179, 217), juce::Colour(176, 123, 224), juce::Colour(108, 197, 139),
        juce::Colour(229, 103, 127), juce::Colour(214, 214, 90), juce::Colour(95, 143, 240), juce::Colour(224, 138, 92) };
    return colours[(size_t)std::max(0, index) % (sizeof(colours) / sizeof(colours[0]))];
}

void SA3LaneEditor::setTimeline(double newSeconds, double newBpm, double newBeatsPerBar)
{
    if (newSeconds == seconds && newBpm == bpm && newBeatsPerBar == beatsPerBar)
        return;
    seconds = newSeconds;
    bpm = newBpm;
    beatsPerBar = newBeatsPerBar;
    repaint();
}

void SA3LaneEditor::setPlayhead(double playheadSeconds)
{
    const float position = seconds > 0.0 && playheadSeconds >= 0.0 && playheadSeconds <= seconds
        ? (float)(playheadSeconds / seconds) : -1.f;
    if (position == playhead)
        return;
    playhead = position;
    repaint();
}

void SA3LaneEditor::setBackdrop(std::vector<float> peaks)
{
    backdrop = std::move(peaks);
    repaint();
}

void SA3LaneEditor::lanesReplaced()
{
    closePromptEntry();
    firstLane = 0;
    drag = Drag::None;
    hoverLane = -1;
    hoverPoint = -1;
    repaint();
}

void SA3LaneEditor::resized()
{
    closePromptEntry();
    layout();
}

//==============================================================================
void SA3LaneEditor::layout()
{
    const auto bounds = getLocalBounds().toFloat();
    const bool wide = bounds.getWidth() >= kWideWidth;
    const auto inner = bounds.reduced(6.f);
    const Rect bar(inner.getX(), inner.getY(), inner.getWidth(), 22.f);
    titleRect = { bar.getX(), bar.getY(), 74.f, bar.getHeight() };
    blendOneRect = { titleRect.getRight() + 4.f, bar.getY(), 66.f, bar.getHeight() };
    blendDistinctRect = { blendOneRect.getRight(), bar.getY(), 60.f, bar.getHeight() };
    snapRect = { blendDistinctRect.getRight() + 6.f, bar.getY(), 70.f, bar.getHeight() };
    rangeRect = Rect::leftTopRightBottom(snapRect.getRight() + 8.f, bar.getY(), bar.getRight(), bar.getBottom());

    const float headWidth = wide ? 196.f : 0.f;
    const float canvasLeft = inner.getX() + headWidth;
    const float space = inner.getBottom() - bar.getBottom() - 4.f;
    const float takeHeight = juce::jlimit(36.f, 70.f, space * 0.2f);
    takeCanvas = Rect::leftTopRightBottom(canvasLeft, bar.getBottom() + 4.f, inner.getRight(), bar.getBottom() + 4.f + takeHeight);
    takeHead = Rect::leftTopRightBottom(inner.getX(), takeCanvas.getY(), canvasLeft - 6.f, takeCanvas.getBottom());

    rows.clear();
    scrollTrack = {};
    const auto& lanes = state.lanes;
    const float top = takeCanvas.getBottom() + 4.f;
    const float available = std::max(0.f, inner.getBottom() - top);
    const float minimum = wide ? 56.f : 60.f;
    const size_t fit = std::max<size_t>(1, (size_t)(available / minimum));
    const size_t visible = std::min(fit, lanes.size());
    if (visible == 0)
    {
        firstLane = 0;
        return;
    }
    firstLane = std::min(firstLane, lanes.size() - visible);
    // Lanes stop growing at a comfortable height. What's left widens the gaps between them, and the
    // rest sits evenly above and below so a few lanes don't hang from the top of the column.
    const float count = (float)visible;
    const float tallest = wide ? 140.f : 200.f;
    const float gap = visible > 1 ? juce::jlimit(3.f, 24.f, (available - tallest * count) / (count - 1.f)) : 3.f;
    const float rowHeight = std::min((available - gap * (count - 1.f)) / count, tallest);
    const float stackHeight = rowHeight * count + gap * (count - 1.f);
    const float stackTop = top + std::floor(std::max(0.f, available - stackHeight) * 0.5f);
    const float right = visible < lanes.size() ? inner.getRight() - 9.f : inner.getRight();
    if (visible < lanes.size())
        scrollTrack = Rect::leftTopRightBottom(inner.getRight() - 5.f, stackTop, inner.getRight(), stackTop + stackHeight);
    // The take spans what the lanes do, so its waveform and cursor sit over the same times.
    takeCanvas.setRight(right);
    // Two lines of controls take 44 of a row, which is too much of a short one.
    const bool oneLine = !wide && rowHeight - 44.f < 70.f;
    for (size_t i = 0; i < visible; ++i)
    {
        const float y = stackTop + (rowHeight + gap) * (float)i;
        const auto rowRect = Rect::leftTopRightBottom(inner.getX(), y, right, y + rowHeight);
        Row row;
        row.head = wide ? Rect::leftTopRightBottom(rowRect.getX(), rowRect.getY(), canvasLeft - 6.f, rowRect.getBottom())
                        : Rect::leftTopRightBottom(rowRect.getX(), rowRect.getY(), rowRect.getRight(), rowRect.getY() + (oneLine ? 20.f : 42.f));
        row.canvas = wide ? Rect::leftTopRightBottom(canvasLeft, rowRect.getY(), rowRect.getRight(), rowRect.getBottom())
                          : Rect::leftTopRightBottom(rowRect.getX(), row.head.getBottom() + 2.f, rowRect.getRight(), rowRect.getBottom());
        const Rect first(row.head.getX(), row.head.getY(), row.head.getWidth(), 20.f);
        row.remove = Rect::leftTopRightBottom(first.getRight() - 18.f, first.getY() + 1.f, first.getRight(), first.getBottom() - 1.f);
        if (oneLine)
        {
            row.dice = Rect::leftTopRightBottom(row.remove.getX() - 26.f, first.getY(), row.remove.getX() - 4.f, first.getBottom());
            row.name = Rect::leftTopRightBottom(first.getX(), first.getY() + 1.f, first.getX() + 104.f, first.getBottom() - 1.f);
            row.prompt = Rect::leftTopRightBottom(row.name.getRight() + 4.f, first.getY(), row.dice.getX() - 4.f, first.getBottom());
        }
        else
        {
            row.name = Rect::leftTopRightBottom(first.getX(), first.getY() + 1.f, row.remove.getX() - 4.f, first.getBottom() - 1.f);
            const Rect second(row.head.getX(), first.getBottom() + 1.f, row.head.getWidth(), 20.f);
            row.dice = Rect::leftTopRightBottom(second.getRight() - 22.f, second.getY(), second.getRight(), second.getBottom());
            row.prompt = Rect::leftTopRightBottom(second.getX(), second.getY(), row.dice.getX() - 4.f, second.getBottom());
        }
        rows.push_back(row);
    }
}

float SA3LaneEditor::xAt(const Rect& canvas, float position) const
{
    return canvas.getX() + kPad + position * (canvas.getWidth() - 2.f * kPad);
}

float SA3LaneEditor::yAt(const Rect& canvas, float amount) const
{
    return canvas.getBottom() - kCurvePad - amount / gary::kSA3MaxLaneAmount * (canvas.getHeight() - 2.f * kCurvePad);
}

float SA3LaneEditor::positionAt(const Rect& canvas, float x) const
{
    return juce::jlimit(0.f, 1.f, (x - canvas.getX() - kPad) / std::max(1.f, canvas.getWidth() - 2.f * kPad));
}

float SA3LaneEditor::amountAt(const Rect& canvas, float y) const
{
    return juce::jlimit(0.f, 1.f, (canvas.getBottom() - kCurvePad - y) / std::max(1.f, canvas.getHeight() - 2.f * kCurvePad))
        * gary::kSA3MaxLaneAmount;
}

bool SA3LaneEditor::stepModifier(const juce::ModifierKeys& mods)
{
    return mods.isCommandDown();   // ctrl on Windows, cmd on macOS
}

// Points land on the chosen division, and freehand strokes stay free. The step modifier goes the
// other way: free of the division while one is chosen, and onto the beat while none is.
void SA3LaneEditor::snap(float& position, float& amount, bool stepping, bool freehand) const
{
    const double division = gary::LaneSnapSeconds(state.snap, bpm, beatsPerBar);
    double step = 0.0;
    if (stepping)
    {
        const double beat = bpm > 0.0 ? 60.0 / bpm : 0.0;
        if (division <= 0.0)
            step = (seconds > 0.0 && beat > 0.0) ? beat / seconds : 1.0 / 16.0;
        amount = std::round(amount / 0.05f) * 0.05f;
    }
    else if (!freehand && seconds > 0.0)
        step = division / seconds;
    if (step <= 0.0)
        return;
    const double snapped = std::min(1.0, std::round(position / step) * step);
    // The range rarely ends on the grid, but its end stays reachable.
    position = (float)(1.0 - position < std::abs(position - snapped) ? 1.0 : snapped);
}

int SA3LaneEditor::hitPoint(const Row& row, const gary::SA3Lane& lane, juce::Point<float> at) const
{
    for (size_t i = 0; i < lane.points.size(); ++i)
    {
        const float dx = xAt(row.canvas, lane.points[i].position) - at.x;
        const float dy = yAt(row.canvas, lane.points[i].amount) - at.y;
        if (dx * dx + dy * dy < 64.f)
            return (int)i;
    }
    return -1;
}

bool SA3LaneEditor::missing(const gary::SA3Lane& lane) const
{
    return host.isMissing && host.isMissing(lane);
}

void SA3LaneEditor::changed()
{
    repaint();
    if (host.changed)
        host.changed();
}

//==============================================================================
void SA3LaneEditor::mouseDown(const juce::MouseEvent& e)
{
    commitPromptEntry();
    layout();
    const auto at = e.position;
    if (blendOneRect.contains(at) || blendDistinctRect.contains(at))
    {
        const auto blend = blendOneRect.contains(at) ? gary::SA3LaneBlend::OneSong : gary::SA3LaneBlend::Distinct;
        if (blend != state.blend)
        {
            state.blend = blend;
            changed();
        }
        return;
    }
    if (snapRect.contains(at))
    {
        openSnapMenu();
        return;
    }
    if (scrollTrack.contains(at))
    {
        drag = Drag::Scroll;
        scrollTo(at.y);
        return;
    }

    for (size_t i = 0; i < rows.size(); ++i)
    {
        const Row row = rows[i];
        const size_t index = firstLane + i;
        auto& lane = state.lanes[index];
        // Every active LoRA has a lane, so only one whose LoRA is off or gone can be removed from
        // here. The others have a reset button in that spot.
        if (row.remove.contains(at))
        {
            if (missing(lane))
                state.lanes.erase(state.lanes.begin() + (std::ptrdiff_t)index);
            else
                resetCurve(lane);
            hoverLane = -1;
            hoverPoint = -1;
            changed();
            return;
        }
        if (row.dice.contains(at))
        {
            if (host.rollPrompt)
                host.rollPrompt(index);
            return;
        }
        if (row.prompt.contains(at))
        {
            openPromptEntry(index, row);
            return;
        }
        if (row.canvas.contains(at))
        {
            canvasDown(e, row, lane, (int)index);
            return;
        }
    }
}

void SA3LaneEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    layout();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (!rows[i].canvas.contains(e.position))
            continue;
        auto& lane = state.lanes[firstLane + i];
        const int hit = hitPoint(rows[i], lane, e.position);
        if (hit >= 0)
        {
            removePoint(lane, hit);
            drag = Drag::None;
            return;
        }
    }
}

void SA3LaneEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (drag == Drag::Scroll)
    {
        scrollTo(e.position.y);
        return;
    }
    if (drag == Drag::None || dragLane < 0 || dragLane >= (int)state.lanes.size())
        return;
    layout();
    auto& lane = state.lanes[(size_t)dragLane];
    const int rowIndex = dragLane - (int)firstLane;
    if (rowIndex < 0 || rowIndex >= (int)rows.size())
        return;
    const Row& row = rows[(size_t)rowIndex];

    float position = positionAt(row.canvas, e.position.x);
    float amount = amountAt(row.canvas, e.position.y);
    snap(position, amount, stepModifier(e.mods), drag == Drag::Stroke);
    hoverLane = dragLane;
    hoverAt = e.position;
    readoutPosition = position;
    readoutAmount = amount;

    if (drag == Drag::Point)
    {
        if (dragPoint < 0 || dragPoint >= (int)lane.points.size())
            return;
        const size_t i = (size_t)dragPoint;
        const float low = i > 0 ? lane.points[i - 1].position : 0.f;
        const float high = i + 1 < lane.points.size() ? lane.points[i + 1].position : 1.f;
        position = juce::jlimit(low, high, position);
        readoutPosition = position;
        lane.points[i] = { position, amount };
        hoverPoint = dragPoint;
    }
    else if (drag == Drag::Stroke)
    {
        stroke.push_back({ position, amount });
        applyStroke(lane);
    }
    repaint();
}

void SA3LaneEditor::mouseUp(const juce::MouseEvent&)
{
    if (drag == Drag::None)
        return;
    const bool edited = drag != Drag::Scroll;
    drag = Drag::None;
    stroke.clear();
    if (edited)
        changed();
}

void SA3LaneEditor::mouseMove(const juce::MouseEvent& e)
{
    layout();
    const auto at = e.position;
    int lane = -1;
    int point = -1;
    bool overCanvas = false;
    bool overControl = blendOneRect.contains(at) || blendDistinctRect.contains(at) || snapRect.contains(at)
        || scrollTrack.contains(at);
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const Row& row = rows[i];
        overControl = overControl || row.remove.contains(at) || row.dice.contains(at) || row.prompt.contains(at);
        if (row.canvas.contains(at))
        {
            overCanvas = true;
            lane = (int)(firstLane + i);
            const auto& points = state.lanes[(size_t)lane].points;
            point = hitPoint(row, state.lanes[(size_t)lane], at);
            readoutPosition = point >= 0 ? points[(size_t)point].position : positionAt(row.canvas, at.x);
            readoutAmount = point >= 0 ? points[(size_t)point].amount : amountAt(row.canvas, at.y);
            if (point < 0)   // says where a click would land
                snap(readoutPosition, readoutAmount, stepModifier(e.mods), e.mods.isShiftDown());
        }
    }
    setMouseCursor(point >= 0 || overControl ? juce::MouseCursor::PointingHandCursor
                   : overCanvas ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor);
    const bool dirty = lane != hoverLane || point != hoverPoint || overCanvas;
    hoverLane = lane;
    hoverPoint = point;
    hoverAt = at;
    if (dirty)
        repaint();
}

void SA3LaneEditor::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
    hoverLane = -1;
    hoverPoint = -1;
    repaint();
}

void SA3LaneEditor::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    layout();
    if (state.lanes.size() <= rows.size())
        return;
    const size_t last = state.lanes.size() - rows.size();
    if (wheel.deltaY < 0.f && firstLane < last)
        ++firstLane;
    else if (wheel.deltaY > 0.f && firstLane > 0)
        --firstLane;
    repaint();
}

//==============================================================================
void SA3LaneEditor::canvasDown(const juce::MouseEvent& e, const Row& row, gary::SA3Lane& lane, int laneIndex)
{
    const int hit = hitPoint(row, lane, e.position);
    if (e.mods.isPopupMenu())
    {
        if (hit >= 0)
            removePoint(lane, hit);
        return;
    }

    dragLane = laneIndex;
    if (hit >= 0)
    {
        drag = Drag::Point;
        dragPoint = hit;
        return;
    }

    float position = positionAt(row.canvas, e.position.x);
    float amount = amountAt(row.canvas, e.position.y);
    snap(position, amount, stepModifier(e.mods), e.mods.isShiftDown());
    if (e.mods.isShiftDown())
    {
        drag = Drag::Stroke;
        stroke.assign(1, { position, amount });
        return;
    }
    if (lane.points.size() >= gary::kSA3MaxLanePoints)
    {
        if (host.notify)
            host.notify("this lane is at its point limit");
        return;
    }
    const auto insertAt = std::upper_bound(lane.points.begin(), lane.points.end(), position,
                                           [](float p, const gary::SA3LanePoint& point) { return p < point.position; });
    dragPoint = (int)(lane.points.insert(insertAt, { position, amount }) - lane.points.begin());
    drag = Drag::Point;
    hoverLane = laneIndex;
    hoverPoint = dragPoint;
    repaint();
}

// The stroke replaces whatever was under it, keeping a point about every tenth of a second.
void SA3LaneEditor::applyStroke(gary::SA3Lane& lane)
{
    auto byPosition = stroke;
    gary::SortLanePoints(byPosition);
    const float from = byPosition.front().position;
    const float to = byPosition.back().position;
    std::vector<gary::SA3LanePoint> merged;
    for (const auto& point : lane.points)
        if (point.position < from - 1e-6f || point.position > to + 1e-6f)
            merged.push_back(point);
    const float spacing = seconds > 0.0 ? (float)(0.1 / seconds) : 0.005f;
    float lastKept = -1.f;
    for (const auto& point : byPosition)
    {
        if (lastKept < 0.f || point.position - lastKept >= spacing)
        {
            merged.push_back(point);
            lastKept = point.position;
        }
    }
    if (lastKept < to)
        merged.push_back(byPosition.back());
    gary::SortLanePoints(merged);
    lane.points = gary::SimplifyLanePoints(merged);
}

void SA3LaneEditor::removePoint(gary::SA3Lane& lane, int index)
{
    lane.points.erase(lane.points.begin() + index);
    hoverPoint = -1;
    changed();
}

void SA3LaneEditor::resetCurve(gary::SA3Lane& lane)
{
    const float strength = host.resetStrength ? host.resetStrength(lane) : 0.f;
    lane.points = { { 0.f, strength }, { 1.f, strength } };
}

size_t SA3LaneEditor::scrollFirstAt(float y) const
{
    const size_t visible = rows.size();
    const size_t total = state.lanes.size();
    if (total <= visible)
        return 0;
    const float thumb = scrollThumbHeight(scrollTrack, visible, total);
    const float fraction = (y - scrollTrack.getY() - thumb * 0.5f) / std::max(1.f, scrollTrack.getHeight() - thumb);
    return (size_t)(juce::jlimit(0.f, 1.f, fraction) * (float)(total - visible) + 0.5f);
}

void SA3LaneEditor::scrollTo(float y)
{
    firstLane = scrollFirstAt(y);
    repaint();
}

void SA3LaneEditor::openSnapMenu()
{
    juce::PopupMenu menu;
    const char* labels[] = { "off", "1/4", "1/2", "1 bar" };
    for (int i = 0; i < 4; ++i)
        menu.addItem(i + 1, labels[i], true, (int)state.snap == i);
    juce::Component::SafePointer<SA3LaneEditor> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                           .withTargetScreenArea(localAreaToGlobal(snapRect.toNearestInt())),
                       [safeThis](int result)
                       {
                           if (safeThis == nullptr || result <= 0)
                               return;
                           safeThis->state.snap = gary::LaneSnapFromIndex(result - 1);
                           safeThis->changed();
                       });
}

// Wider than the box, which can be narrow, and over whatever is beside it until the edit ends.
void SA3LaneEditor::openPromptEntry(size_t laneIndex, const Row& row)
{
    closePromptEntry();
    promptEntryLane = (int)laneIndex;
    promptEntry = std::make_unique<juce::TextEditor>();
    auto& entry = *promptEntry;
    entry.setMultiLine(true, true);
    entry.setReturnKeyStartsNewLine(false);
    entry.setFont(juce::FontOptions(12.f));
    entry.setColour(juce::TextEditor::backgroundColourId, kPanelDark);
    entry.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    entry.setColour(juce::TextEditor::outlineColourId, kFrame);
    entry.setColour(juce::TextEditor::focusedOutlineColourId, kRed);
    entry.setTextToShowWhenEmpty("uses the base prompt", kTextFaint);
    entry.setText(juce::String(state.lanes[laneIndex].prompt), false);
    const auto area = Rect::leftTopRightBottom(row.head.getX(), row.prompt.getY(), row.canvas.getRight(), row.prompt.getY() + 66.f);
    entry.setBounds(area.toNearestInt());
    entry.onReturnKey = [this]() { commitPromptEntry(); };
    entry.onEscapeKey = [this]() { closePromptEntry(); };
    entry.onFocusLost = [this]() { commitPromptEntry(); };
    addAndMakeVisible(entry);
    entry.grabKeyboardFocus();
    entry.selectAll();
}

void SA3LaneEditor::commitPromptEntry()
{
    if (promptEntry == nullptr)
        return;
    const int lane = promptEntryLane;
    const auto text = promptEntry->getText().trim();
    closePromptEntry();
    if (lane < 0 || lane >= (int)state.lanes.size())
        return;
    state.lanes[(size_t)lane].prompt = text.toStdString();
    changed();
}

void SA3LaneEditor::closePromptEntry()
{
    if (promptEntry == nullptr)
        return;
    // Deleted after its callbacks return: this can run from inside one of them.
    auto entry = std::move(promptEntry);
    promptEntryLane = -1;
    entry->onFocusLost = nullptr;
    entry->onReturnKey = nullptr;
    entry->onEscapeKey = nullptr;
    removeChildComponent(entry.get());
    juce::MessageManager::callAsync([doomed = std::shared_ptr<juce::TextEditor>(entry.release())]() {});
    repaint();
}

//==============================================================================
void SA3LaneEditor::paint(juce::Graphics& g)
{
    layout();
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(kPanelDark);
    g.fillRoundedRectangle(bounds, kCorner);
    g.setColour(kFrameSoft);
    g.drawRoundedRectangle(bounds.reduced(0.5f), kCorner, 1.f);
    drawTopBar(g);
    drawTake(g);
    if (state.lanes.empty())
    {
        const auto empty = Rect::leftTopRightBottom(bounds.getX(), takeCanvas.getBottom(), bounds.getRight(), bounds.getBottom());
        drawText(g, "turn on a LoRA to draw its strength over the range", empty, 12.f, kTextDim, juce::Justification::centred);
        return;
    }
    for (size_t i = 0; i < rows.size(); ++i)
        drawLane(g, rows[i], state.lanes[firstLane + i], (int)(firstLane + i));
    drawScrollbar(g);
    drawPlayhead(g);
    drawReadout(g);
}

void SA3LaneEditor::drawTopBar(juce::Graphics& g)
{
    drawText(g, "automation", titleRect, 13.f, juce::Colours::white, juce::Justification::centredLeft);
    const auto segment = [&](const Rect& rect, const char* label, bool active)
    {
        g.setColour(active ? kRed : kButtonFill);
        g.fillRect(rect);
        g.setColour(active ? kRed : kFrame);
        g.drawRect(rect);
        drawText(g, label, rect, 11.f, active ? juce::Colours::black : kTextDim, juce::Justification::centred);
    };
    segment(blendOneRect, "one song", state.blend == gary::SA3LaneBlend::OneSong);
    segment(blendDistinctRect, "distinct", state.blend == gary::SA3LaneBlend::Distinct);

    const char* labels[] = { "snap off", "snap 1/4", "snap 1/2", "snap 1 bar" };
    const bool on = state.snap != gary::SA3LaneSnap::Off;
    g.setColour(kButtonFill);
    g.fillRect(snapRect);
    g.setColour(on ? kRed : kFrame);
    g.drawRect(snapRect);
    drawText(g, labels[(int)state.snap], snapRect, 10.f, on ? juce::Colours::white : kTextDim, juce::Justification::centred);

    const auto range = seconds > 0.0 ? juce::String(seconds, 2) + " s" : juce::String("set a range");
    drawText(g, range, rangeRect, 11.f, kTextDim, juce::Justification::centredRight);
}

void SA3LaneEditor::drawGrid(juce::Graphics& g, const Rect& canvas, bool labels)
{
    if (seconds <= 0.0)
        return;
    const float width = canvas.getWidth() - 2.f * kPad;
    const double beat = bpm > 0.0 ? 60.0 / bpm : 0.0;
    const double barLength = beat * std::max(1.0, beatsPerBar);
    if (barLength <= 0.0)
        return;
    const float barWidth = (float)(barLength / seconds) * width;
    const float beatWidth = (float)(beat / seconds) * width;
    if (beatWidth >= 14.f)
    {
        g.setColour(juce::Colour(24, 24, 24));
        for (double t = beat; t < seconds; t += beat)
            g.drawVerticalLine(juce::roundToInt(xAt(canvas, (float)(t / seconds))), canvas.getY(), canvas.getBottom());
    }
    const int every = std::max(1, (int)std::ceil(28.f / std::max(1.f, barWidth)));
    int barIndex = 0;
    for (double t = 0.0; t < seconds; t += barLength, ++barIndex)
    {
        if (barIndex % every != 0)
            continue;
        const float x = xAt(canvas, (float)(t / seconds));
        g.setColour(kFrameSoft);
        g.drawVerticalLine(juce::roundToInt(x), canvas.getY(), canvas.getBottom());
        if (labels)
            drawText(g, juce::String(barIndex + 1), { x + 3.f, canvas.getY() + 1.f, 31.f, 12.f }, 10.f, kTextFaint,
                     juce::Justification::topLeft);
    }
}

juce::Path SA3LaneEditor::curvePath(const Rect& canvas, const std::vector<gary::SA3LanePoint>& points) const
{
    juce::Path path;
    path.startNewSubPath(canvas.getX() + kPad, yAt(canvas, points.front().amount));
    for (const auto& point : points)
        path.lineTo(xAt(canvas, point.position), yAt(canvas, point.amount));
    path.lineTo(canvas.getRight() - kPad, yAt(canvas, points.back().amount));
    return path;
}

void SA3LaneEditor::drawTake(juce::Graphics& g)
{
    if (takeHead.getWidth() > 10.f)
        drawText(g, backdrop.empty() ? "no render yet" : "last render", takeHead, 11.f, kTextDim, juce::Justification::topLeft);
    g.setColour(kCanvas);
    g.fillRect(takeCanvas);
    drawGrid(g, takeCanvas, true);

    if (backdrop.size() >= 2)
    {
        const size_t columns = backdrop.size() / 2;
        const float width = takeCanvas.getWidth() - 2.f * kPad;
        const int steps = std::min((int)columns, (int)width);
        const float middle = takeCanvas.getCentreY();
        const float scale = takeCanvas.getHeight() * 0.46f;
        g.setColour(juce::Colour(80, 80, 80));
        for (int i = 0; i < steps; ++i)
        {
            const size_t column = (size_t)i * columns / (size_t)std::max(1, steps);
            const float x = takeCanvas.getX() + kPad + width * (float)i / (float)std::max(1, steps);
            g.drawVerticalLine(juce::roundToInt(x), middle - backdrop[column * 2 + 1] * scale,
                               middle - backdrop[column * 2] * scale + 0.5f);
        }
    }

    for (size_t i = 0; i < state.lanes.size(); ++i)
    {
        if (state.lanes[i].points.empty() || missing(state.lanes[i]))
            continue;
        g.setColour(laneColour((int)i));
        g.strokePath(curvePath(takeCanvas, state.lanes[i].points), juce::PathStrokeType(1.5f));
    }
    g.setColour(kFrameSoft);
    g.drawRect(takeCanvas);
}

void SA3LaneEditor::drawDice(juce::Graphics& g, const Rect& rect)
{
    g.setColour(kButtonFill);
    g.fillRoundedRectangle(rect, kCorner);
    g.setColour(kFrame);
    g.drawRoundedRectangle(rect, kCorner, 1.f);
    const float left = rect.getX() + rect.getWidth() * 0.31f;
    const float right = rect.getRight() - rect.getWidth() * 0.31f;
    const float top = rect.getY() + rect.getHeight() * 0.31f;
    const float bottom = rect.getBottom() - rect.getHeight() * 0.31f;
    g.setColour(juce::Colours::white);
    for (const auto pip : { juce::Point<float>(left, top), juce::Point<float>(right, top), rect.getCentre(),
                            juce::Point<float>(left, bottom), juce::Point<float>(right, bottom) })
        g.fillEllipse(pip.x - 1.4f, pip.y - 1.4f, 2.8f, 2.8f);
}

void SA3LaneEditor::drawReset(juce::Graphics& g, const Rect& rect)
{
    g.setColour(kButtonFill);
    g.fillRoundedRectangle(rect, kCorner);
    g.setColour(kFrame);
    g.drawRoundedRectangle(rect, kCorner, 1.f);
    const auto centre = rect.getCentre();
    const float radius = std::min(rect.getWidth(), rect.getHeight()) * 0.27f;
    juce::Path arc;
    arc.addCentredArc(centre.x, centre.y, radius, radius, 0.f,
                      juce::degreesToRadians(70.f), juce::degreesToRadians(360.f), true);
    g.setColour(juce::Colours::white);
    g.strokePath(arc, juce::PathStrokeType(1.3f));
    const float tipY = centre.y - radius;
    juce::Path tip;
    tip.addTriangle(centre.x + 3.2f, tipY, centre.x - 0.4f, tipY - 2.6f, centre.x - 0.4f, tipY + 2.6f);
    g.fillPath(tip);
}

void SA3LaneEditor::drawLane(juce::Graphics& g, const Row& row, const gary::SA3Lane& lane, int index)
{
    const bool gone = missing(lane);
    const auto colour = gone ? kTextFaint : laneColour(index);
    g.setColour(kButtonFill);
    g.fillRoundedRectangle(row.name, kCorner);
    g.setColour(colour);
    g.drawRoundedRectangle(row.name, kCorner, 1.f);
    const auto label = juce::String(lane.name) + (gone ? " (off)" : "");
    drawText(g, label, row.name.reduced(5.f, 0.f), 11.f, gone ? kTextDim : juce::Colours::white,
             juce::Justification::centredLeft);
    if (gone)
    {
        g.setColour(kButtonFill);
        g.fillRoundedRectangle(row.remove, kCorner);
        g.setColour(kFrame);
        g.drawRoundedRectangle(row.remove, kCorner, 1.f);
        drawText(g, "x", row.remove, 11.f, juce::Colours::white, juce::Justification::centred);
    }
    else
    {
        drawReset(g, row.remove);
    }

    g.setColour(juce::Colour(14, 14, 14));
    g.fillRoundedRectangle(row.prompt, kCorner);
    g.setColour(kFrameSoft);
    g.drawRoundedRectangle(row.prompt, kCorner, 1.f);
    const auto promptText = row.prompt.reduced(5.f, 0.f);
    if (lane.prompt.empty())
        drawText(g, "uses the base prompt", promptText, 11.f, kTextFaint, juce::Justification::centredLeft);
    else
        drawText(g, juce::String(lane.prompt), promptText, 11.f, gone ? kTextDim : juce::Colours::white,
                 juce::Justification::centredLeft);
    drawDice(g, row.dice);

    g.setColour(kCanvas);
    g.fillRect(row.canvas);
    drawGrid(g, row.canvas, false);
    const float unity = yAt(row.canvas, 1.f);
    g.setColour(kFrame);
    g.drawHorizontalLine(juce::roundToInt(unity), row.canvas.getX(), row.canvas.getRight());
    const auto scaleX = row.canvas.getX() + 3.f;
    drawText(g, "2", { scaleX, row.canvas.getY() + 1.f, kPad - 3.f, 11.f }, 9.f, kTextFaint, juce::Justification::centredLeft);
    drawText(g, "1", { scaleX, unity - 11.f, kPad - 3.f, 11.f }, 9.f, kTextFaint, juce::Justification::centredLeft);
    drawText(g, "0", { scaleX, row.canvas.getBottom() - 12.f, kPad - 3.f, 11.f }, 9.f, kTextFaint, juce::Justification::centredLeft);
    if (!lane.points.empty())
    {
        auto fill = curvePath(row.canvas, lane.points);
        fill.lineTo(row.canvas.getRight() - kPad, row.canvas.getBottom() - kCurvePad);
        fill.lineTo(row.canvas.getX() + kPad, row.canvas.getBottom() - kCurvePad);
        fill.closeSubPath();
        g.setColour(colour.withAlpha((juce::uint8)44));
        g.fillPath(fill);
        g.setColour(colour);
        g.strokePath(curvePath(row.canvas, lane.points), juce::PathStrokeType(2.f));
        for (size_t i = 0; i < lane.points.size(); ++i)
        {
            const bool hovered = index == hoverLane && (int)i == hoverPoint;
            const float x = xAt(row.canvas, lane.points[i].position);
            const float y = yAt(row.canvas, lane.points[i].amount);
            const float radius = hovered ? 5.f : 3.5f;
            g.setColour(hovered ? juce::Colours::white : colour);
            g.fillEllipse(x - radius, y - radius, radius * 2.f, radius * 2.f);
        }
    }
    g.setColour(kFrameSoft);
    g.drawRect(row.canvas);
}

void SA3LaneEditor::drawScrollbar(juce::Graphics& g)
{
    const size_t visible = rows.size();
    const size_t total = state.lanes.size();
    if (total <= visible || scrollTrack.isEmpty())
        return;
    g.setColour(kFrameSoft);
    g.fillRoundedRectangle(scrollTrack, 2.f);
    const float thumb = scrollThumbHeight(scrollTrack, visible, total);
    const float top = scrollTrack.getY() + (scrollTrack.getHeight() - thumb) * (float)firstLane / (float)(total - visible);
    g.setColour(kTextDim);
    g.fillRoundedRectangle({ scrollTrack.getX(), top, scrollTrack.getWidth(), thumb }, 2.f);
}

void SA3LaneEditor::drawPlayhead(juce::Graphics& g)
{
    if (playhead < 0.f)
        return;
    g.setColour(juce::Colours::white);
    const auto line = [&](const Rect& canvas)
    {
        const float x = xAt(canvas, playhead);
        g.drawLine(x, canvas.getY(), x, canvas.getBottom(), 1.5f);
    };
    line(takeCanvas);
    for (const auto& row : rows)
        line(row.canvas);
}

void SA3LaneEditor::drawReadout(juce::Graphics& g)
{
    if (hoverLane < 0)
        return;
    const int rowIndex = hoverLane - (int)firstLane;
    if (rowIndex < 0 || rowIndex >= (int)rows.size())
        return;
    const auto& canvas = rows[(size_t)rowIndex].canvas;
    const auto text = seconds > 0.0
        ? juce::String(readoutPosition * seconds, 2) + " s  " + juce::String::fromUTF8("\xc2\xb7") + "  " + juce::String(readoutAmount, 2)
        : juce::String(juce::roundToInt(readoutPosition * 100.f)) + "%  " + juce::String::fromUTF8("\xc2\xb7") + "  " + juce::String(readoutAmount, 2);
    const float left = std::min(hoverAt.x + 10.f, canvas.getRight() - 86.f);
    const float top = std::max(canvas.getY() + 1.f, hoverAt.y - 20.f);
    const Rect tip(left, top, 84.f, 16.f);
    g.setColour(juce::Colours::black.withAlpha((juce::uint8)230));
    g.fillRoundedRectangle(tip, 2.f);
    drawText(g, text, tip, 10.f, juce::Colours::white, juce::Justification::centred);
}
