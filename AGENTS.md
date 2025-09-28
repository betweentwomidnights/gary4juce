# Task: Restore Darius “styles & weights” UI (+/− rows, weight display) and tidy layout

This task is **intentionally small and reversible**. It fixes two regressions introduced during the DariusUI extraction and restores parity with the previous `PluginEditor.cpp` behavior.

---

## Why we’re doing this (diff-based summary)

1) **“+” button vanished**
   - In `DariusUI`, we **add** `genStylesHeaderLabel` and `genAddStyleButton`, but we never **position** them in `resized()`. The Generation branch lays out source/bpm rows and then calls `layoutGenStylesUI(area)` directly, skipping a header row entirely. Result: the “+” control has zero-sized bounds and appears missing.
   - New code: header controls are added (see `DariusUI.cpp` around construction), but no `setBounds` in `resized()`.
   - Old code placed all Generation controls, including a visible add button.

2) **Weight slider reads like `1.00 wat / wgt` and not just a number**
   - Old behavior: **No text box** for the style weight slider; simple horizontal slider, range **0.0–1.0**.
   - New behavior: slider uses `TextBoxRight` with a `setTextValueSuffix(" wgt")` and a broader range **0.0–2.0**. The suffix is unwanted (“wat/wgt”), and the broader range diverges from the prior UI and may confuse server expectations.

3) **“−” remove button callback is index-captured (can drift after edits)**
   - Old code located the **current** row index at click time using a pointer to the clicked button inside a `MessageManager::callAsync(...)` safe callback. This avoids stale indices and deleting while inside the callback.
   - New code captures `index` by value when the row is created. After deletions, that index can point at the wrong row.

References in repo:
- Old slider + remove pattern in `PluginEditor.cpp`: linear slider **NoTextBox**, range **0–1**, “-” remove button with pointer lookup + async.  
- New code in `DariusUI.cpp`: `TextBoxRight` + `" wgt"` suffix, range **0–2**, remove callback capturing the original `index`.

---

## Acceptance criteria

- Generation *Styles & weights* **header row** (label left, “+” button right) is **visible** and laid out at the top of the styles section in the Generation subtab.
- The style **weight slider** shows **just the numeric value** (no suffix). Use the same interaction model as before (either NoTextBox like the old UI, or TextBoxRight **without any suffix**). Keep **two decimals** if a text box is used.
- Slider **range is 0.0–1.0** (step 0.01), matching previous behavior.
- The “−” remove button works reliably even after insertions/removals. (Do not remove the first row.)
- The “+” button is **enabled only if** `genStyleRows.size() < genStylesMax` (unchanged behavior).

---

## Concrete edits

> Keep changes minimal and scoped to **DariusUI** files.

### 1) Lay out the header row (label + “+” button)

In `DariusUI::resized()` → *Generation* branch, before `layoutGenStylesUI(area)`, add a header row.

```cpp
// Header: "styles & weights" (left) + [+] (right)
{
    auto headerRow = area.removeFromTop(20);
    // Leave a small gap under header
    auto headerLeft = headerRow.removeFromLeft(headerRow.getWidth() - 28);
    genStylesHeaderLabel.setBounds(headerLeft);
    genAddStyleButton.setBounds(headerRow.removeFromRight(24));
    area.removeFromTop(6);
}
```

This mirrors how other compact rows are placed in the Generation branch and guarantees the (+) button is visible.

### 2) Restore weight slider display + range

In `DariusUI::addGenStyleRowInternal(...)`, change the style weight slider to match old semantics:

```cpp
// Old behavior: no text box; range 0..1; 0.01 step
row.weight = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
row.weight->setRange(0.0, 1.0, 0.01);
row.weight->setValue(juce::jlimit(0.0, 1.0, weight), juce::dontSendNotification);
// If we prefer to keep a text box, then use TextBoxRight but **no suffix**:
// row.weight->setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
// row.weight->setNumDecimalPlacesToDisplay(2);
// row.weight->setTextValueSuffix("");
```

Also **remove** the suffix line if present:

```cpp
// REMOVE this in the new code path:
// row.weight->setTextValueSuffix(" wgt");
```

### 3) Make the remove button robust (no stale index)

Replace the index-captured callback with the safe pointer-lookup pattern:

```cpp
row.remove = std::make_unique<CustomButton>();
row.remove->setButtonText("-");
row.remove->setButtonStyle(CustomButton::ButtonStyle::Standard);

auto* removeBtnPtr = row.remove.get();
row.remove->onClick = [this, removeBtnPtr]()
{
    juce::MessageManager::callAsync([this, removeBtnPtr]()
    {
        int idx = -1;
        for (size_t i = 0; i < genStyleRows.size(); ++i)
        {
            if (genStyleRows[i].remove.get() == removeBtnPtr)
            {
                idx = (int)i;
                break;
            }
        }
        if (idx <= 0) return; // never remove first row
        handleRemoveStyleRow(idx);   // or the local remove implementation
        rebuildGenStylesUI();
        resized();
    });
};
```

> If you prefer to centralize deletion logic, keep `handleRemoveStyleRow(int)` and call it from the async block after computing `idx`.

Ensure `rebuildGenStylesUI()` still:
- Enables/disables the (+) button based on row count.
- Hides the remove button for the first row (and when only 1 row total).

No other logic (CSV building, etc.) needs to change for this task.

---

## Scope guardrails

- **Do not** touch networking (e.g., `sendToGary`, `sendToTerry`) or any non-Darius tabs.
- **Do not** alter CSV formatting or downstream request-building aside from the slider range/display noted above.
- **Do not** change visual themes or shared components.

---

## Test checklist

- Switch to the **Darius → Generation** subtab.
- Confirm header shows **“styles & weights”** left and **“+”** right.
- Click **“+”** repeatedly: rows increase up to `genStylesMax` (default 4). On the 5th click, (+) disables.
- Row 0 has **no “−”**. Rows 1..N have a **“−”** that removes the correct row after any sequence of add/remove.
- Each row’s weight slider operates **0.00–1.00** and shows **no “wgt/wat” suffix**.
- Layout remains stable with viewport scrolling.

---

## PR notes

- Title: `darius-ui: restore styles header/plus and weight slider semantics`
- Labels: `ui`, `regression`, `small`
- Risk: low (UI-only, scoped to Darius Generation section)
- Rollback: revert this commit
