// SPDX-FileCopyrightText: 2025-2026 Kevin Griffing
// SPDX-License-Identifier: AGPL-3.0-only

// One MIDI input slot on the yuey tab, melody or chords: a lane label, an outlined box that takes
// a dropped MIDI or .alc file or opens a file picker, and a line under it that says what was loaded or why
// it wasn't. It holds no MIDI itself. The editor reads the file and tells the slot what to show.

#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>

class YueyMidiSlot final : public juce::Component,
                           public juce::FileDragAndDropTarget,
                           public juce::SettableTooltipClient
{
public:
    explicit YueyMidiSlot(const juce::String& laneName);

    // Empty, or a file that was read, or a file that was refused (kept on show so the error under
    // it has something to point at, and so one click clears it).
    void showEmpty();
    void showLoaded(const juce::File& file, const juce::String& summary);
    void showInvalid(const juce::File& file, const juce::String& error);

    bool isEmpty() const { return state == State::Empty; }

    // How tall the slot wants to be at a width, since an error wraps onto a few lines.
    int heightForWidth(int width) const;

    std::function<void(const juce::File&)> onFile;  // dropped or picked: both end up here
    std::function<void()> onClear;

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    enum class State { Empty, Loaded, Invalid };

    juce::Rectangle<int> boxBounds() const;
    juce::Rectangle<int> iconBounds() const;
    void browse();
    void updateTooltip();

    juce::String lane;
    State state = State::Empty;
    juce::File file;
    juce::String note;  // the summary when loaded, the error when not
    bool dragOver = false;
    bool overIcon = false;
    std::unique_ptr<juce::FileChooser> chooser;

    static constexpr int kLabelHeight = 16;
    static constexpr int kBoxHeight = 30;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YueyMidiSlot)
};
