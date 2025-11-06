# Jerry Dice Button Implementation

## Overview

Add a dice button to JerryUI's prompt editor that generates semi-random prompts based on the smart loop state:
- **Smart Loop OFF or Auto**: Uses combined "all prompts" pool (BeatPrompts + InstrumentPrompts)
- **Smart Loop ON + Drums**: Uses BeatPrompts only
- **Smart Loop ON + Instruments**: Uses InstrumentPrompts only

The dice button will be positioned to the right of the prompt text editor, similar to the DariusUI implementation.

## Reference Implementation

The DariusUI dice button implementation (in `DariusUI.cpp`) serves as the reference pattern. Key elements to replicate:
- Dice button as a `CustomButton` with custom paint callback
- 5-pip dice icon drawing (center + 4 corners)
- Hover and pressed state visual feedback
- Positioned adjacent to text input field

## Visual Design

**Color Scheme** (Jerry theme - red/black instead of magenta):
- **Normal**: `Theme::Colors::Jerry` (red) background with white pips
- **Hover**: Brighter red background (`.brighter(0.3f)`) with white pips  
- **Pressed**: Inverted - red background with white pips (maintain contrast)

---

## Part 1: Add Members to JerryUI.h

### Include Prompt Generators

Add these includes at the top of `JerryUI.h`:

```cpp
#include "BeatPrompts.h"
#include "InstrumentPrompts.h"
```

### Add Private Members

Add these private members to the `JerryUI` class:

```cpp
private:
    // Prompt generators
    BeatPrompts beatPrompts;
    InstrumentPrompts instrumentPrompts;
    
    // Dice button
    std::unique_ptr<CustomButton> promptDiceButton;
    
    // Helper methods
    void drawDiceIcon(juce::Graphics& g, juce::Rectangle<float> bounds, 
                      bool isHovered, bool isPressed);
    juce::String generateConditionalPrompt();
```

---

## Part 2: Modify JerryUI.cpp Constructor

### Initialize Dice Button

Add this code in the `JerryUI::JerryUI()` constructor, right after the `jerryPromptEditor` initialization:

```cpp
    // Dice button for prompt generation
    promptDiceButton = std::make_unique<CustomButton>();
    promptDiceButton->setButtonText("");  // No text, we'll draw the icon
    promptDiceButton->setButtonStyle(CustomButton::ButtonStyle::Jerry);
    promptDiceButton->setTooltip("Generate random prompt");
    
    promptDiceButton->onClick = [this]()
    {
        juce::String prompt = generateConditionalPrompt();
        jerryPromptEditor.setText(prompt, juce::sendNotification);
        setPromptText(prompt);  // Ensure state is updated
    };
    
    // Override paint to draw custom dice icon with hover/pressed states
    promptDiceButton->onPaint = [this](juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        bool isHovered = promptDiceButton->isMouseOver();
        bool isPressed = promptDiceButton->isDown();
        drawDiceIcon(g, bounds.toFloat().reduced(2), isHovered, isPressed);
    };
    
    addAndMakeVisible(*promptDiceButton);
```

---

## Part 3: Add Conditional Prompt Generation Method

Add this method to `JerryUI.cpp`:

```cpp
juce::String JerryUI::generateConditionalPrompt()
{
    // Determine which prompt generator to use based on UI state
    
    // Case 1: Smart Loop OFF or loopTypeIndex == 0 (auto)
    // Use combined "all prompts" pool
    if (!smartLoop || loopTypeIndex == 0)
    {
        // Randomly choose between BeatPrompts and InstrumentPrompts
        bool useBeat = juce::Random::getSystemRandom().nextBool();
        
        if (useBeat)
        {
            // Use BeatPrompts with its weighted logic
            return beatPrompts.getRandomPrompt();
        }
        else
        {
            // Use InstrumentPrompts with its weighted logic
            return instrumentPrompts.getRandomGenrePrompt();
        }
    }
    
    // Case 2: Smart Loop ON + loopTypeIndex == 1 (drums)
    // Use BeatPrompts only
    if (loopTypeIndex == 1)
    {
        return beatPrompts.getRandomPrompt();
    }
    
    // Case 3: Smart Loop ON + loopTypeIndex == 2 (instruments)
    // Use InstrumentPrompts only
    if (loopTypeIndex == 2)
    {
        return instrumentPrompts.getRandomGenrePrompt();
    }
    
    // Fallback (should never reach here)
    return beatPrompts.getRandomPrompt();
}
```

---

## Part 4: Add Dice Icon Drawing Method

Add this method to `JerryUI.cpp`:

```cpp
void JerryUI::drawDiceIcon(juce::Graphics& g, juce::Rectangle<float> bounds, 
                           bool isHovered, bool isPressed)
{
    // Choose colors based on state (Jerry theme: red/black)
    juce::Colour bgColour, pipColour;

    if (isPressed)
    {
        // Pressed: slightly brighter red with white pips
        bgColour = Theme::Colors::Jerry.brighter(0.2f);
        pipColour = juce::Colours::white;
    }
    else if (isHovered)
    {
        // Hover: brighter red background with white pips
        bgColour = Theme::Colors::Jerry.brighter(0.3f);
        pipColour = juce::Colours::white;
    }
    else
    {
        // Normal: Jerry red background with white pips
        bgColour = Theme::Colors::Jerry.withAlpha(0.9f);
        pipColour = juce::Colours::white;
    }

    // Draw dice square with rounded corners
    juce::Path dicePath;
    dicePath.addRoundedRectangle(bounds, 2.0f);

    g.setColour(bgColour);
    g.fillPath(dicePath);

    // Draw 5 pips in classic dice pattern (like a 5-face)
    float pipRadius = bounds.getWidth() * 0.12f;

    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();
    float offset = bounds.getWidth() * 0.25f;

    // Center pip
    g.setColour(pipColour);
    g.fillEllipse(cx - pipRadius, cy - pipRadius, pipRadius * 2, pipRadius * 2);

    // Four corner pips
    auto drawPip = [&](float x, float y)
    {
        g.fillEllipse(x - pipRadius, y - pipRadius, pipRadius * 2, pipRadius * 2);
    };

    drawPip(cx - offset, cy - offset);  // Top-left
    drawPip(cx + offset, cy - offset);  // Top-right
    drawPip(cx - offset, cy + offset);  // Bottom-left
    drawPip(cx + offset, cy + offset);  // Bottom-right
}
```

---

## Part 5: Update Layout in resized()

Modify the prompt editor layout section in `JerryUI::resized()` to include the dice button:

**Find this section:**
```cpp
// Prompt label
auto promptLabelBounds = area.removeFromTop(kPromptLabelHeight);
jerryPromptLabel.setBounds(promptLabelBounds);
area.removeFromTop(kInterRowGap);

// Prompt editor
auto promptBounds = area.removeFromTop(kPromptEditorHeight);
jerryPromptEditor.setBounds(promptBounds);
area.removeFromTop(kInterRowGap);
```

**Replace with:**
```cpp
// Prompt label
auto promptLabelBounds = area.removeFromTop(kPromptLabelHeight);
jerryPromptLabel.setBounds(promptLabelBounds);
area.removeFromTop(kInterRowGap);

// Prompt editor with dice button
auto promptRow = area.removeFromTop(kPromptEditorHeight);
const int diceW = 22;  // Dice button width (square)
auto diceBounds = promptRow.removeFromRight(diceW);
promptRow.removeFromRight(2);  // Small gap between editor and dice
jerryPromptEditor.setBounds(promptRow);

// Make dice button square by centering vertically
auto diceSquare = diceBounds.withHeight(diceW).withY(diceBounds.getY() + (kPromptEditorHeight - diceW) / 2);
promptDiceButton->setBounds(diceSquare);

area.removeFromTop(kInterRowGap);
```

---

## Part 6: Update setVisibleForTab()

Ensure the dice button visibility is managed when switching tabs. In `JerryUI::setVisibleForTab()`:

```cpp
void JerryUI::setVisibleForTab(bool visible)
{
    setVisible(visible);
    setInterceptsMouseClicks(visible, visible);
    
    // Ensure dice button follows visibility
    if (promptDiceButton)
        promptDiceButton->setVisible(visible);
}
```

---

## Testing Checklist

### Prompt Generation Logic
- [ ] Smart Loop OFF → generates from both BeatPrompts and InstrumentPrompts
- [ ] Smart Loop ON + Auto (index 0) → generates from both BeatPrompts and InstrumentPrompts
- [ ] Smart Loop ON + Drums (index 1) → generates only from BeatPrompts
- [ ] Smart Loop ON + Instruments (index 2) → generates only from InstrumentPrompts
- [ ] Each click produces a different prompt (randomization works)
- [ ] Prompts appear in the text editor correctly
- [ ] `onPromptChanged` callback is triggered when dice is clicked

### Visual Behavior
- [ ] Dice button appears to the right of prompt editor
- [ ] Dice button is square (22x22 pixels)
- [ ] Normal state: red background with white pips
- [ ] Hover state: brighter red background with white pips
- [ ] Pressed state: distinct visual feedback
- [ ] 5-pip dice pattern is clearly visible
- [ ] Button scales properly on different screen sizes

### Integration
- [ ] No compilation errors
- [ ] BeatPrompts and InstrumentPrompts are properly included
- [ ] Dice button visible on Jerry tab only
- [ ] Layout doesn't break with different prompt text lengths
- [ ] Tooltip appears on hover: "Generate random prompt"

---

## Implementation Notes

### Conditional Logic Flow

```
User clicks dice button
    ↓
generateConditionalPrompt() checks:
    ↓
    Is smartLoop OFF?  ──YES──→  50/50 choice: BeatPrompts OR InstrumentPrompts
    ↓ NO
    Is loopTypeIndex == 0 (auto)?  ──YES──→  50/50 choice: BeatPrompts OR InstrumentPrompts
    ↓ NO
    Is loopTypeIndex == 1 (drums)?  ──YES──→  BeatPrompts.getRandomPrompt()
    ↓ NO
    Is loopTypeIndex == 2 (instruments)?  ──YES──→  InstrumentPrompts.getRandomGenrePrompt()
    ↓
    Returns prompt string
    ↓
    Prompt applied to jerryPromptEditor
```

### Color Scheme Comparison

| State | DariusUI (Magenta) | JerryUI (Red) |
|-------|-------------------|---------------|
| Normal | `Theme::Colors::Darius` | `Theme::Colors::Jerry` |
| Hover | Darius brighter(0.3f) | Jerry brighter(0.3f) |
| Pressed | Inverted (magenta on white) | Jerry brighter(0.2f) |

### Layout Dimensions

- **Dice button size**: 22x22 pixels (square)
- **Gap between editor and dice**: 2 pixels
- **Prompt editor height**: `kPromptEditorHeight` (24 pixels from constants)
- **Vertical centering**: Dice centered within prompt row

---

## Future Enhancements

- [ ] Add shift+click for "all prompts" mode regardless of smart loop state
- [ ] Add dice button tooltip that shows current mode (e.g., "drums mode")
- [ ] Add prompt history (prevent immediate repeats)
- [ ] Add animation on dice click (brief rotation/shake)
- [ ] Add option to lock current prompt (disable dice temporarily)
- [ ] For finetune models: Load custom prompts from HF repo `prompts.txt`

---

## Expected Behavior Examples

### Scenario 1: User generating drum beats
```
1. User clicks "smart loop" button (OFF → ON)
2. User clicks "drums" button
3. User clicks dice button
   → Result: "compressed drums trap beat" (from BeatPrompts)
4. User clicks dice again
   → Result: "side-chained punchy drums" (from BeatPrompts)
```

### Scenario 2: User wants instrument generation
```
1. User has smart loop ON
2. User clicks "instr" button
3. User clicks dice button
   → Result: "liquid dnb chords" (from InstrumentPrompts)
4. User clicks dice again
   → Result: "neurofunk bass" (from InstrumentPrompts)
```

### Scenario 3: User wants variety (auto mode)
```
1. User has smart loop OFF or "auto" selected
2. User clicks dice button
   → Result: "crispy drums techno beat" (BeatPrompts - randomly chosen)
3. User clicks dice again
   → Result: "chillwave pads" (InstrumentPrompts - randomly chosen)
4. User clicks dice again
   → Result: "trap EDM kick pattern" (BeatPrompts - randomly chosen)
```

---

## Critical Reminders

- Include `BeatPrompts.h` and `InstrumentPrompts.h` at top of `JerryUI.h`
- Initialize `beatPrompts` and `instrumentPrompts` member variables
- Use `Theme::Colors::Jerry` for red color scheme (NOT `Theme::Colors::Darius`)
- Dice button must be square (equal width and height)
- Call `setPromptText()` after setting editor text to maintain state consistency
- Test all three smart loop modes (auto, drums, instruments)
- Ensure dice button follows tab visibility (only shown on Jerry tab)

---

## Checklist Summary

- [ ] Add `#include "BeatPrompts.h"` and `#include "InstrumentPrompts.h"` to JerryUI.h
- [ ] Add member variables: `beatPrompts`, `instrumentPrompts`, `promptDiceButton`
- [ ] Add method declarations: `drawDiceIcon()`, `generateConditionalPrompt()`
- [ ] Initialize dice button in constructor with onClick and onPaint callbacks
- [ ] Implement `generateConditionalPrompt()` with conditional logic
- [ ] Implement `drawDiceIcon()` with Jerry color scheme
- [ ] Update `resized()` to layout dice button next to prompt editor
- [ ] Update `setVisibleForTab()` to manage dice button visibility
- [ ] Test all smart loop modes (OFF, auto, drums, instruments)
- [ ] Verify visual states (normal, hover, pressed)
- [ ] Verify prompts are generated correctly for each mode
