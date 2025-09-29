
# AGENTS.md — Gary Tab UI Extraction (JUCE)

## Goal
Extract all **Gary** tab UI elements and immediate UI logic out of `PluginEditor` into a new reusable component `Components/Gary/GaryUI.{h,cpp}` while **not** refactoring any networking or long‑running logic. The editor remains the orchestrator that owns state, threads, and network calls. The new UI delegates raise typed callbacks that the editor wires into existing methods (`sendToGary()`, `continueMusic()`, etc.).

This refactor must **not reduce UI responsiveness** and must **decrease constructor/resized() line counts** inside `PluginEditor`.

---

## Scope & Constraints
- **In-scope:** Extract only the **Gary** tab controls (labels, sliders, combo, and buttons) and their immediate UI behaviors (enablement, tooltips, visibility/layout).
- **Out-of-scope (for this task):**
  - Any **networking** code or background work.
  - Jerry, Terry, and Darius tabs (except minimal edits to tabs visibility to host GaryUI).
  - Audio playback, timers, or file I/O.
  - Any git commands (do not use git).
- **Branch:** Continue working in the existing `refactor/darius-ui-extract` branch.
- **Threading:** UI callbacks must stay on the **message thread**. If an editor method may touch background work, call it **directly** (the editor already owns the threading contract). **Do not** start timers or threads in the new component.

---

## Target Files
- **Create**
  - `Components/Gary/GaryUI.h`
  - `Components/Gary/GaryUI.cpp`
- **Modify**
  - `Source/PluginEditor.h`
  - `Source/PluginEditor.cpp`
- **Reuse**
  - `Components/Base/CustomButton.*`
  - `Components/Base/CustomSlider.*`
  - `Components/Base/CustomComboBox.*`
  - `Utils/Theme.h`

---

## Inventory (Gary controls & state)
Move the following **UI elements** from `PluginEditor` into `GaryUI`:
- `garyLabel`
- `promptDurationSlider`, `promptDurationLabel`
- `modelComboBox`, `modelLabel`
- `sendToGaryButton`
- `continueButton`
- `retryButton`

Preserve **editor-owned state**, but expose setters/getters so the editor remains source of truth:
- `currentPromptDuration : float` (default 6.0)
- `currentModelIndex : int`

Preserve **editor methods** (do not move):  
`sendToGary()`, `continueMusic()`, `retryLastContinuation()`, `updateModelAvailability()`, `updateAllGenerationButtonStates()`, `updateContinueButtonState()`, `updateRetryButtonState()`.

---

## Public API for `GaryUI`
Design `GaryUI` as a “dumb” view with thin state. Provide:
- **Setters (called by editor):**
  - `void setVisibleForTab(bool visible)` – optional helper to toggle visibility when the Gary tab is active.
  - `void setPromptDuration(float seconds)` – updates slider + tooltips but **does not** trigger callbacks.
  - `void setModelItems(const juce::StringArray& items, int selectedIndex)` – rebuild model list and set selection.
  - `void setButtonsEnabled(bool hasAudio, bool isConnected, bool isGenerating, bool retryAvailable, bool continueAvailable)` – minimal batching to avoid per-button churn.
- **Getters:**
  - `float getPromptDuration() const`
  - `int getSelectedModelIndex() const`
- **Signals / Callbacks (std::function)** for the editor to wire up:
  - `onPromptDurationChanged(float seconds)`
  - `onModelChanged(int index)`
  - `onSendToGary()`
  - `onContinue()`
  - `onRetry()`

**No timers. No threads.**

---

## Layout Responsibilities
- `GaryUI::resized()` lays out its own children **horizontally/vertically** similar to current Gary block. Keep spacing constants local to `GaryUI`.
- `PluginEditor::resized()` becomes shorter: it positions the single `GaryUI` component within the Gary tab content area, instead of laying out each child control.
- Do **not** hardcode tab visibility inside `GaryUI`; the editor decides when the Gary tab is shown.

---

## Tooltips / Text Updates
`GaryUI` is responsible for regenerating button tooltips when `promptDuration` changes. The editor can simply call `setPromptDuration(x)` after reading processor state or slider change.

---

## Enablement / State Strategy
- The editor remains the canonical place that knows about connectivity, audio presence, generating state, and retry/continue availability.
- Editor recomputes enable/disable using its existing logic and pushes a snapshot via `GaryUI::setButtonsEnabled(...)`.
- **Focus:** This avoids cross‑thread issues and keeps GaryUI trivial.

---

## Step‑by‑Step Plan

1. **Create skeleton component**
   - Add `Components/Gary/GaryUI.h/.cpp` with member controls and callbacks.
   - Implement `paint()` minimal (or empty), `resized()` with layout.
   - Default fonts/colors follow `Theme.h` usage.

2. **Move UI construction from editor to GaryUI**
   - Instantiate all Gary controls in `GaryUI` constructor.
   - Wire internal `onClick`/`onValueChange` to raise the public callbacks.

3. **Replace inline editor members**
   - Remove Gary-specific `juce::Label`, `CustomSlider`, `CustomComboBox`, and `CustomButton` members from `PluginEditor` and replace them with `std::unique_ptr<GaryUI> garyUI;` (or a direct member).
   - In `PluginEditor` constructor:
     - `garyUI = std::make_unique<GaryUI>();`
     - `addAndMakeVisible(*garyUI);`
     - **Wire callbacks:**
       - `garyUI->onPromptDurationChanged = [this](float s){ currentPromptDuration = s; updateAllGenerationButtonStates(); /* also refresh tooltips if needed */ };`
       - `garyUI->onModelChanged = [this](int i){ currentModelIndex = i; /* optionally call updateModelAvailability(); */ };`
       - `garyUI->onSendToGary = [this]{ sendToGary(); };`
       - `garyUI->onContinue   = [this]{ continueMusic(); };`
       - `garyUI->onRetry      = [this]{ retryLastContinuation(); };`

4. **Sync initial state from editor to GaryUI**
   - After reading persisted processor/editor state in the editor’s constructor (existing code), push:
     - `garyUI->setPromptDuration(currentPromptDuration);`
     - `garyUI->setModelItems(modelNames, currentModelIndex);`
     - `garyUI->setButtonsEnabled(/* compute from existing flags */);`

5. **Update tab switching**
   - In `switchToTab(ModelTab::Gary)`:
     - Hide the legacy Gary controls code path.
     - Show only `garyUI` (`setVisible(true)`) and hide others.
   - Remove per‑control `setVisible()` calls for Gary controls; keep this for Jerry/Terry until they are extracted.

6. **Update `resized()`**
   - Replace Gary section layout with a single `garyUI->setBounds(garyTabArea);`

7. **Remove obsolete tooltip/update paths**
   - `promptDurationSlider.onValueChange` logic moves to `GaryUI` and fires `onPromptDurationChanged` with the new value. The editor no longer touches Gary slider/tooltips directly.

8. **Recompile and fix references**
   - Replace any direct references in the editor (e.g., `sendToGaryButton.setEnabled(...)`) with calls to `garyUI->setButtonsEnabled(...)`.

9. **QA & Performance Check**
   - Verify **no new timers/threads** were added.
   - Validate UI remains responsive while generating.
   - Confirm constructor and `resized()` **line count drops significantly**.

---

## Minimal Class Stubs

### `Components/Gary/GaryUI.h`
```cpp
#pragma once
#include <JuceHeader.h>
#include "../Base/CustomButton.h"
#include "../Base/CustomSlider.h"
#include "../Base/CustomComboBox.h"

class GaryUI : public juce::Component
{
public:
    GaryUI();
    void paint(juce::Graphics&) override;
    void resized() override;

    // Setters
    void setPromptDuration(float seconds);
    void setModelItems(const juce::StringArray& items, int selectedIndex);
    void setButtonsEnabled(bool hasAudio, bool isConnected, bool isGenerating, bool retryAvailable, bool continueAvailable);

    // Getters
    float getPromptDuration() const;
    int getSelectedModelIndex() const;

    // Signals
    std::function<void(float)> onPromptDurationChanged;
    std::function<void(int)>   onModelChanged;
    std::function<void()>      onSendToGary;
    std::function<void()>      onContinue;
    std::function<void()>      onRetry;

private:
    juce::Label garyLabel;
    CustomSlider promptDurationSlider;
    juce::Label promptDurationLabel;
    CustomComboBox modelComboBox;
    juce::Label modelLabel;
    CustomButton sendToGaryButton;
    CustomButton continueButton;
    CustomButton retryButton;

    float promptDuration {6.0f};
    int modelIndex {0};

    void refreshTooltips();
    void applyEnablement(bool hasAudio, bool isConnected, bool isGenerating, bool retryAvailable, bool continueAvailable);
};
```

### `Components/Gary/GaryUI.cpp`
```cpp
#include "GaryUI.h"
#include "../../Utils/Theme.h"

GaryUI::GaryUI()
{
    // Labels
    garyLabel.setText("gary (musicgen)", juce::dontSendNotification);
    garyLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    garyLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    garyLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(garyLabel);

    // Prompt duration
    promptDurationSlider.setRange(1.0, 15.0, 1.0);
    promptDurationSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    promptDurationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    promptDurationSlider.setTextValueSuffix("s");
    promptDurationSlider.onValueChange = [this]() {
        promptDuration = (float)promptDurationSlider.getValue();
        refreshTooltips();
        if (onPromptDurationChanged) onPromptDurationChanged(promptDuration);
    };
    addAndMakeVisible(promptDurationSlider);

    promptDurationLabel.setText("prompt duration", juce::dontSendNotification);
    promptDurationLabel.setFont(juce::FontOptions(12.0f));
    promptDurationLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    promptDurationLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(promptDurationLabel);

    // Model
    modelComboBox.onChange = [this]() {
        modelIndex = modelComboBox.getSelectedId() - 1;
        if (onModelChanged) onModelChanged(modelIndex);
    };
    addAndMakeVisible(modelComboBox);

    modelLabel.setText("model", juce::dontSendNotification);
    modelLabel.setFont(juce::FontOptions(12.0f));
    modelLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    modelLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(modelLabel);

    // Buttons
    sendToGaryButton.setButtonText("send to gary");
    sendToGaryButton.setButtonStyle(CustomButton::ButtonStyle::Gary);
    sendToGaryButton.onClick = [this]() { if (onSendToGary) onSendToGary(); };
    addAndMakeVisible(sendToGaryButton);

    continueButton.setButtonText("continue");
    continueButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    continueButton.onClick = [this]() { if (onContinue) onContinue(); };
    addAndMakeVisible(continueButton);

    retryButton.setButtonText("retry");
    retryButton.setButtonStyle(CustomButton::ButtonStyle::Standard);
    retryButton.onClick = [this]() { if (onRetry) onRetry(); };
    addAndMakeVisible(retryButton);

    setPromptDuration(6.0f);
    refreshTooltips();
}

void GaryUI::paint(juce::Graphics&) {}
void GaryUI::resized()
{
    auto area = getLocalBounds().reduced(10);
    auto line = [&](int h){ auto r = area.removeFromTop(h); r.removeFromRight(10); return r; };

    garyLabel.setBounds(line(24));
    auto row = line(28);
    promptDurationLabel.setBounds(row.removeFromLeft(140));
    promptDurationSlider.setBounds(row);

    row = line(28);
    modelLabel.setBounds(row.removeFromLeft(140));
    modelComboBox.setBounds(row);

    row = line(32);
    sendToGaryButton.setBounds(row.removeFromLeft(150));
    continueButton.setBounds(row.removeFromLeft(120));
    retryButton.setBounds(row.removeFromLeft(100));
}

void GaryUI::setPromptDuration(float s)
{
    promptDuration = s;
    promptDurationSlider.setValue(s, juce::dontSendNotification);
    refreshTooltips();
}

void GaryUI::setModelItems(const juce::StringArray& items, int selectedIndex)
{
    modelComboBox.clear(juce::dontSendNotification);
    for (int i = 0; i < items.size(); ++i) modelComboBox.addItem(items[i], i+1);
    modelIndex = juce::jlimit(0, items.size()-1, selectedIndex);
    modelComboBox.setSelectedId(modelIndex + 1, juce::dontSendNotification);
}

void GaryUI::setButtonsEnabled(bool hasAudio, bool isConnected, bool isGenerating, bool retryAvail, bool continueAvail)
{
    applyEnablement(hasAudio, isConnected, isGenerating, retryAvail, continueAvail);
}

float GaryUI::getPromptDuration() const { return promptDuration; }
int   GaryUI::getSelectedModelIndex() const { return modelIndex; }

void GaryUI::refreshTooltips()
{
    const int secs = (int)promptDuration;
    sendToGaryButton.setTooltip("have gary extend the recorded audio using the first " + juce::String(secs) + " seconds as audio prompt");
    continueButton.setTooltip("have gary extend the output audio using the last " + juce::String(secs) + " seconds as audio prompt");
    retryButton.setTooltip("have gary retry that last continuation using different prompt duration or model if you want, or just have him do it over");
}

void GaryUI::applyEnablement(bool hasAudio, bool isConnected, bool isGenerating, bool retryAvail, bool continueAvail)
{
    const bool canSend = isConnected && hasAudio && !isGenerating;
    sendToGaryButton.setEnabled(canSend);
    continueButton.setEnabled(continueAvail && canSend);
    retryButton.setEnabled(retryAvail && isConnected && !isGenerating);
}
```

---

## Wiring Map (Editor ↔ GaryUI)

- Editor → GaryUI:
  - `setPromptDuration(currentPromptDuration)`
  - `setModelItems(modelNames, currentModelIndex)`
  - `setButtonsEnabled(hasAudio, isConnected, isGenerating, retryAvailable, continueAvailable)`

- GaryUI → Editor:
  - `onPromptDurationChanged(seconds)` → editor updates `currentPromptDuration` and calls `updateAllGenerationButtonStates()`.
  - `onModelChanged(index)` → editor updates `currentModelIndex` and (optionally) `updateModelAvailability()`.
  - `onSendToGary()` → calls existing `sendToGary()`.
  - `onContinue()` → calls existing `continueMusic()`.
  - `onRetry()` → calls existing `retryLastContinuation()`.

---

## Acceptance Criteria
1. `PluginEditor` constructor and `resized()` **shrink significantly** (Gary layout is delegated).
2. No regressions in:
   - Prompt duration behavior (including tooltips).
   - Model selection.
   - Send/Continue/Retry availability and actions.
3. No new threads/timers are introduced by GaryUI.
4. When switching tabs, only the single `GaryUI` component is shown for the Gary tab.
5. Build succeeds and the UI remains responsive during generation.

---

## Testing Checklist
- Initial open: Gary tab visible, controls present, tooltips reflect default 6s.
- Change prompt duration → tooltips update and editor receives callback.
- Switch models → editor receives index; generation still works.
- With/without audio, connected/disconnected, generating/not generating → button enablement matches previous behavior.
- Tab switching: GaryUI hidden when on Jerry/Terry/Darius; visible on Gary.
- Repeat a generation and retry/continue flows to verify enablement updates sent by editor.

---

## Notes
- Keep parity with `DariusUI` style (callbacks from UI → editor) to maintain a consistent component pattern for later extractions (Jerry/Terry).
- After Gary is extracted and validated, repeat the same pattern for Jerry and Terry in subsequent tasks.
