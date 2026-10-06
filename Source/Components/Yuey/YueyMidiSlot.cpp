// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

#include "YueyMidiSlot.h"

#include "../../Utils/Theme.h"

namespace
{
const juce::Colour kOutline(0xff555555);
const juce::Colour kFill(0xff161616);
const juce::Colour kError(0xffe6a23c);  // the orange the score window uses for a check that failed

bool isMidiFile(const juce::File& file)
{
    const auto extension = file.getFileExtension().toLowerCase();
    return extension == ".mid" || extension == ".midi" || extension == ".alc";
}

// Where the last file came from, shared by both slots: someone picking a melody and then chords
// from the same folder shouldn't have to find it twice.
juce::File& lastFolder()
{
    static juce::File folder;
    return folder;
}

juce::AttributedString noteText(const juce::String& text, juce::Colour colour)
{
    juce::AttributedString attributed;
    attributed.setJustification(juce::Justification::topLeft);
    attributed.setWordWrap(juce::AttributedString::byWord);
    attributed.append(text, juce::FontOptions(10.5f), colour);
    return attributed;
}
}  // namespace

YueyMidiSlot::YueyMidiSlot(const juce::String& laneName)
    : lane(laneName)
{
    updateTooltip();
}

void YueyMidiSlot::showEmpty()
{
    state = State::Empty;
    file = juce::File();
    note = juce::String();
    updateTooltip();
    repaint();
}

void YueyMidiSlot::showLoaded(const juce::File& loaded, const juce::String& summary)
{
    state = State::Loaded;
    file = loaded;
    note = summary;
    updateTooltip();
    repaint();
}

void YueyMidiSlot::showInvalid(const juce::File& refused, const juce::String& error)
{
    state = State::Invalid;
    file = refused;
    note = error;
    updateTooltip();
    repaint();
}

juce::Rectangle<int> YueyMidiSlot::boxBounds() const
{
    return getLocalBounds().withTrimmedTop(kLabelHeight).withHeight(kBoxHeight);
}

juce::Rectangle<int> YueyMidiSlot::iconBounds() const
{
    return boxBounds().removeFromRight(28);
}

int YueyMidiSlot::heightForWidth(int width) const
{
    int height = kLabelHeight + kBoxHeight;
    if (note.isNotEmpty())
    {
        juce::TextLayout layout;
        layout.createLayout(noteText(note, Theme::Colors::TextSecondary), (float) juce::jmax(60, width));
        height += 3 + (int) std::ceil(layout.getHeight()) + 2;
    }
    return height;
}

void YueyMidiSlot::updateTooltip()
{
    switch (state)
    {
        case State::Empty:
            setTooltip("drop a .mid, .midi or Ableton .alc file here, or click to browse for the " + lane
                       + "\nLive Clips import notes only, one saved region; instruments and effects aren't loaded");
            break;
        case State::Loaded:
            setTooltip(file.getFullPathName() + "\nclick the x to remove it, or drop another file to replace it"
                + (file.hasFileExtension("alc") ? "\nLive Clip: notes only, one saved region; instruments and effects aren't loaded" : ""));
            break;
        case State::Invalid:
            setTooltip(note);
            break;
    }
}

void YueyMidiSlot::paint(juce::Graphics& g)
{
    g.setFont(juce::FontOptions(11.0f));
    g.setColour(Theme::Colors::TextSecondary);
    g.drawText(lane, getLocalBounds().removeFromTop(kLabelHeight), juce::Justification::centredLeft);

    const auto box = boxBounds();
    g.setColour(kFill);
    g.fillRoundedRectangle(box.toFloat(), 3.0f);
    g.setColour(dragOver ? Theme::Colors::Terry : (state == State::Invalid ? kError : kOutline));
    g.drawRoundedRectangle(box.toFloat().reduced(0.5f), 3.0f, dragOver ? 1.5f : 1.0f);

    auto inner = box.reduced(8, 0);
    const auto icon = iconBounds().toFloat().reduced(4.0f);
    inner.removeFromRight(24);

    g.setFont(juce::FontOptions(12.0f));
    if (state == State::Empty)
    {
        g.setColour(Theme::Colors::TextSecondary.withAlpha(0.6f));
        g.drawText("drop MIDI / .alc", inner, juce::Justification::centredLeft);

        // A folder: the picker's handle.
        const auto tint = Theme::Colors::TextSecondary.withAlpha(overIcon ? 1.0f : 0.7f);
        const auto r = icon.withSizeKeepingCentre(16.0f, 12.0f);
        juce::Path folder;
        folder.addRoundedRectangle(r.getX(), r.getY() + 2.5f, r.getWidth(), r.getHeight() - 2.5f, 1.5f);
        folder.addRectangle(r.getX(), r.getY(), r.getWidth() * 0.42f, 3.5f);
        g.setColour(tint);
        g.strokePath(folder, juce::PathStrokeType(1.2f));
    }
    else
    {
        g.setColour(state == State::Invalid ? Theme::Colors::TextSecondary : Theme::Colors::TextPrimary);
        g.drawText(file.getFileName(), inner, juce::Justification::centredLeft, true);

        // The x that takes it back out.
        const auto tint = Theme::Colors::TextSecondary.withAlpha(overIcon ? 1.0f : 0.7f);
        const auto r = icon.withSizeKeepingCentre(9.0f, 9.0f);
        g.setColour(tint);
        g.drawLine(r.getX(), r.getY(), r.getRight(), r.getBottom(), 1.4f);
        g.drawLine(r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.4f);
    }

    if (note.isNotEmpty())
    {
        const auto area = getLocalBounds().withTrimmedTop(kLabelHeight + kBoxHeight + 3).toFloat();
        noteText(note, state == State::Invalid ? kError : Theme::Colors::TextSecondary).draw(g, area);
    }
}

void YueyMidiSlot::mouseMove(const juce::MouseEvent& event)
{
    const bool over = iconBounds().contains(event.getPosition());
    if (over != overIcon)
    {
        overIcon = over;
        repaint();
    }
    const bool clickable = boxBounds().contains(event.getPosition()) && (state == State::Empty || over);
    setMouseCursor(clickable ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void YueyMidiSlot::mouseExit(const juce::MouseEvent&)
{
    overIcon = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void YueyMidiSlot::mouseUp(const juce::MouseEvent& event)
{
    if (!boxBounds().contains(event.getPosition()) || !event.mouseWasClicked())
        return;

    if (state == State::Empty)
    {
        browse();  // anywhere in an empty box, not just on the folder
    }
    else if (iconBounds().contains(event.getPosition()))
    {
        if (onClear)
            onClear();
    }
}

void YueyMidiSlot::browse()
{
    const auto start = lastFolder().isDirectory() ? lastFolder()
                                                  : juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    chooser = std::make_unique<juce::FileChooser>("choose MIDI or a Live Clip for the " + lane, start, "*.mid;*.midi;*.alc");
    juce::Component::SafePointer<YueyMidiSlot> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safe](const juce::FileChooser& finished)
    {
        const auto picked = finished.getResult();
        if (safe == nullptr || !picked.existsAsFile())
            return;
        lastFolder() = picked.getParentDirectory();
        if (safe->onFile)
            safe->onFile(picked);
    });
}

bool YueyMidiSlot::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() == 1 && isMidiFile(juce::File(files[0]));
}

void YueyMidiSlot::fileDragEnter(const juce::StringArray&, int, int)
{
    dragOver = true;
    repaint();
}

void YueyMidiSlot::fileDragExit(const juce::StringArray&)
{
    dragOver = false;
    repaint();
}

void YueyMidiSlot::filesDropped(const juce::StringArray& files, int, int)
{
    dragOver = false;
    repaint();
    if (!isInterestedInFileDrag(files))
        return;
    const juce::File dropped(files[0]);
    lastFolder() = dropped.getParentDirectory();
    if (onFile)
        onFile(dropped);
}
