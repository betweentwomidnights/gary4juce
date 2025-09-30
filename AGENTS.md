# AGENTS.md — Keep Progress Text Stable Across Tabs (Gary/Terry/Jerry)

**Owner:** Plugin UI Team  
**Last updated:** 2025‑09‑30  
**Scope:** `PluginEditor.h/.cpp`, child UIs (`GaryUI`, `TerryUI`, `JerryUI`)

---

## Goal

When a generation starts (e.g., “send to gary”), the progress/status text must stay consistent with **the operation that started it** — even if the user navigates between tabs. Today we derive the text from the **current tab**, which causes mismatches (e.g., “cooking 0%” → switch to Terry → suddenly shows “transforming 3%” for the same Gary job).

We’ll introduce a single source of truth: **`ActiveOp`**, an enum that tracks the currently running operation. All status/progress text and button labels will consult `ActiveOp` instead of `currentTab`.

This is a surgical change that doesn’t alter networking or polling logic. It only centralizes how we **label** the in‑flight work.

---

## Target UX

- Start **Gary** job → text always reads **“cooking …%”** until completion, regardless of tab switches.
- Start **Gary Continue** or **Retry** → also **“cooking …%”**.
- Start **Terry Transform** → text always reads **“transforming …%”**, regardless of tab switches.
- **Jerry** is fast; we don’t poll the same way. We can either:
  - **Omit** progress entirely (current behavior), or
  - If we ever add polling, map Jerry to a neutral label (e.g., “cooking …%” or “generating …%”).

On completion/failure/cancel, `ActiveOp` resets to `None`.

---

## Design

### 1) Add `ActiveOp` enum & state

**File:** `PluginEditor.h`

```cpp
// Tracks which operation (if any) is currently in-flight, independent of the visible tab.
enum class ActiveOp {
    None = 0,
    GaryGenerate,
    GaryContinue,
    GaryRetry,
    TerryTransform,
    JerryGenerate // (optional; only if you decide to poll Jerry later)
};

ActiveOp activeOp = ActiveOp::None;

void setActiveOp(ActiveOp op) { activeOp = op; }
ActiveOp getActiveOp() const { return activeOp; }
```

> Keep it private next to other editor state. This mirrors how we already store `isGenerating`, `continueInProgress`, etc.

### 2) Set `activeOp` at the **entry points**

**File:** `PluginEditor.cpp` — at the start of each action:

```cpp
void Gary4juceAudioProcessorEditor::sendToGary() {
    setActiveOp(ActiveOp::GaryGenerate);
    continueInProgress = false; // clarity
    // existing body…
}

void Gary4juceAudioProcessorEditor::continueMusic() {
    setActiveOp(ActiveOp::GaryContinue);
    continueInProgress = true;
    // existing body…
}

void Gary4juceAudioProcessorEditor::retryLastContinuation() {
    setActiveOp(ActiveOp::GaryRetry);
    continueInProgress = true; // retry still relates to previous continue chain
    // existing body…
}

void Gary4juceAudioProcessorEditor::sendToTerry() {
    setActiveOp(ActiveOp::TerryTransform);
    // existing body…
}

// If you ever add polling for Jerry:
void Gary4juceAudioProcessorEditor::sendToJerry() {
    setActiveOp(ActiveOp::JerryGenerate);
    // existing body…
}
```

### 3) Clear `activeOp` when the op ends

Clear it in **all terminal paths** (success, failure, or when explicitly stopping).

Where to do it:
- On **successful completion** (inside `handlePollingResponse` after we handle `"completed"`):
  ```cpp
  isGenerating = false;
  setActiveOp(ActiveOp::None);
  ```
- On **failure paths** (`handleGenerationFailure`, backend disconnection handlers) add:
  ```cpp
  isGenerating = false;
  setActiveOp(ActiveOp::None);
  ```
- In **`stopAllBackgroundOperations()`** after flags are reset:
  ```cpp
  setActiveOp(ActiveOp::None);
  ```

> Don’t clear the session ID if you rely on it for Retry/Undo; this change only touches the active op label.

### 4) Route **all progress labels** through `activeOp`

Replace places that key on `currentTab` during polling. Example changes in `handlePollingResponse`:

```diff
- if (currentTab == ModelTab::Terry) {
-     showStatusMessage("transforming: " + juce::String(serverProgress) + "%", 5000);
- } else {
-     showStatusMessage("cooking: " + juce::String(serverProgress) + "%", 5000);
- }
+ switch (getActiveOp()) {
+     case ActiveOp::TerryTransform:
+         showStatusMessage("transforming: " + juce::String(serverProgress) + "%", 5000);
+         break;
+     case ActiveOp::GaryGenerate:
+     case ActiveOp::GaryContinue:
+     case ActiveOp::GaryRetry:
+     case ActiveOp::JerryGenerate: // if/when applicable
+         showStatusMessage("cooking: " + juce::String(serverProgress) + "%", 5000);
+         break;
+     default:
+         // Fallback if we lost context but are still generating
+         showStatusMessage("processing...", 5000);
+         break;
+ }
```

And for the **fallback** (no queue info but in progress):

```diff
- if (currentTab == ModelTab::Terry) {
-     showStatusMessage("processing transform.", 5000);
- } else {
-     showStatusMessage("processing audio.", 5000);
- }
+ if (getActiveOp() == ActiveOp::TerryTransform)
+     showStatusMessage("processing transform.", 5000);
+ else
+     showStatusMessage("processing audio.", 5000);
```

> This removes the race where switching tabs mid‑job flips the text.

### 5) Output waveform overlay (if it uses text)

If your waveform overlay also embeds the string (e.g., “cooking 42%”), make the same mapping there. Create a tiny helper so both the overlay and status area stay in sync:

```cpp
juce::String currentOperationVerb() const {
    switch (getActiveOp()) {
        case ActiveOp::TerryTransform: return "transforming";
        case ActiveOp::GaryGenerate:
        case ActiveOp::GaryContinue:
        case ActiveOp::GaryRetry:
        case ActiveOp::JerryGenerate:  return "cooking";
        default:                       return "processing";
    }
}
```

Then render `currentOperationVerb() + ": " + juce::String(generationProgress) + "%"`.


---

## Minimal Patch Summary

- **`PluginEditor.h`**
  - Add `enum class ActiveOp { … }`, a private `ActiveOp activeOp`, and trivial setter/getter.
- **`PluginEditor.cpp`**
  - Set `activeOp` at the start of `sendToGary`, `continueMusic`, `retryLastContinuation`, `sendToTerry` (and optionally `sendToJerry`).
  - In `handlePollingResponse` replace `currentTab`-based decisions with `activeOp` switch (both progress and fallback messages).
  - Clear `activeOp` in completion & error/stop paths.

No API surfaces change. No child UI types change.


---

## Acceptance Tests

1) **Gary → switch tabs**
   - Click “send to gary” → see “cooking 0%”.
   - Switch to Terry/Darius/Jerry tabs while polling → text remains “cooking …%”.
   - Completion → “audio generation complete!” and `activeOp == None`.

2) **Terry → switch tabs**
   - Click “transform with terry” → see “transforming 0%”.
   - Switch to Gary/Jerry/Darius tabs → text remains “transforming …%”.
   - Completion → “transform complete!” and `activeOp == None`; Undo becomes available.

3) **Gary Continue / Retry**
   - Start a continue → “cooking …%”. Switch tabs → unchanged. Completion enables Retry. `activeOp == None`.

4) **Stop / Failure**
   - Trigger a backend failure or call `stopAllBackgroundOperations()` → progress stops; `activeOp == None`.

5) **Waveform overlay (if applicable)**
   - Overlay verb matches status text for both Gary and Terry while navigating tabs.

---

## Notes & Edge Cases

- If a second operation is triggered while one is in‑flight, decide whether to **ignore**, **queue**, or **replace** the active op. Today we effectively replace UI state when a new request starts; keeping that behavior is fine — the entry point setter will simply overwrite `activeOp`.
- Jerry currently doesn’t use long polling. If you later add it, mapping to “cooking …%” keeps UX consistent.
- If you want the simplest possible fix, you can hardcode both paths to **“cooking …%”**, but the `ActiveOp` approach is future‑proof and matches the user’s mental model.

---

## Quick Diffs (apply by hand)

### PluginEditor.h

```diff
 class Gary4juceAudioProcessorEditor : public juce::AudioProcessorEditor, … {
 private:
+    enum class ActiveOp { None=0, GaryGenerate, GaryContinue, GaryRetry, TerryTransform, JerryGenerate };
+    ActiveOp activeOp = ActiveOp::None;
+    void setActiveOp(ActiveOp op) { activeOp = op; }
+    ActiveOp getActiveOp() const { return activeOp; }
```

### PluginEditor.cpp — entry points

```diff
 void Gary4juceAudioProcessorEditor::sendToGary() {
+    setActiveOp(ActiveOp::GaryGenerate);
     // existing…
 }

 void Gary4juceAudioProcessorEditor::continueMusic() {
+    setActiveOp(ActiveOp::GaryContinue);
     // existing…
 }

 void Gary4juceAudioProcessorEditor::retryLastContinuation() {
+    setActiveOp(ActiveOp::GaryRetry);
     // existing…
 }

 void Gary4juceAudioProcessorEditor::sendToTerry() {
+    setActiveOp(ActiveOp::TerryTransform);
     // existing…
 }
```

### PluginEditor.cpp — polling text

```diff
- if (currentTab == ModelTab::Terry) {
-     showStatusMessage("transforming: " + juce::String(serverProgress) + "%", 5000);
- } else {
-     showStatusMessage("cooking: " + juce::String(serverProgress) + "%", 5000);
- }
+ switch (getActiveOp()) {
+     case ActiveOp::TerryTransform:
+         showStatusMessage("transforming: " + juce::String(serverProgress) + "%", 5000);
+         break;
+     case ActiveOp::GaryGenerate:
+     case ActiveOp::GaryContinue:
+     case ActiveOp::GaryRetry:
+     case ActiveOp::JerryGenerate:
+         showStatusMessage("cooking: " + juce::String(serverProgress) + "%", 5000);
+         break;
+     default:
+         showStatusMessage("processing...", 5000);
+         break;
+ }
```

```diff
- if (currentTab == ModelTab::Terry) {
-     showStatusMessage("processing transform.", 5000);
- } else {
-     showStatusMessage("processing audio.", 5000);
- }
+ if (getActiveOp() == ActiveOp::TerryTransform)
+     showStatusMessage("processing transform.", 5000);
+ else
+     showStatusMessage("processing audio.", 5000);
```

### PluginEditor.cpp — completion / failure / stop

```diff
 // On successful completion (both generation and transform branches):
- isGenerating = false;
+ isGenerating = false;
+ setActiveOp(ActiveOp::None);

 // In handleGenerationFailure(...):
+ setActiveOp(ActiveOp::None);

 // In handleBackendDisconnection(...):
+ setActiveOp(ActiveOp::None);

 // In stopAllBackgroundOperations():
+ setActiveOp(ActiveOp::None);
```

---

## Rollback

If anything feels brittle, comment out the `ActiveOp` switch blocks and force the neutral label:

```cpp
showStatusMessage("cooking: " + juce::String(serverProgress) + "%", 5000);
```

This keeps the UI consistent (though less descriptive) while you iterate.

---

## Why this works (reference)

- We currently choose labels inside `handlePollingResponse` by checking `currentTab`, e.g.:
  - `if (currentTab == ModelTab::Terry) showStatusMessage("transforming: …%") else "cooking: …%"`
- That makes the label reflect the **visible tab**, not the **running operation**.
- Moving to `ActiveOp` removes this coupling and keeps labels correct while navigating.
