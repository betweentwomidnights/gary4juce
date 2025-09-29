# AGENTS: Final fixes for Centroids display + Recording/Output source

This task finishes the Darius refactor by fixing two UX issues **without touching backend behavior**:

1) **Centroids/mean conditional UI** should appear immediately after a config refresh or checkpoint switch (no tab dance required).
2) **Recording vs Output source** must select the correct file path when generating (no “always myOutput.wav” fallback).

Scope is intentionally tight and revert‑friendly.

---

## Invariants (do not change)

- Network functions (`sendToGary`, `sendToTerry`, etc.) remain untouched.
- Existing `DariusUI` structure and callbacks stay; we’re only tightening wiring and visibility.
- We do **not** change JSON contract with the backend.
- Keep the current style/Theme and Custom* controls.

---

## Part A — Centroids / Mean conditional display

### Problem
Sometimes only the **mean** slider appears while **centroid sliders** stay hidden until the user flips tabs or switches checkpoints. We want the sliders to appear (or disappear) immediately after config refresh or checkpoint apply/warm completes.

### Ground truth (current flow)
- After parsing model config, we **already** trigger an assets refresh and then set the UI (mean available, centroid count, weights).
- `DariusUI::setSteeringAssets(meanAvailable, centroidCount, weights)` is intended to **(a)** rebuild rows, **(b)** update toggle text, and **(c)** `resized()` to lay things out.

### Fix plan
1. **Force a clean UI state before fetching** so we never show stale sliders while fetching new assets.
2. **Ensure visibility toggles are tied only to `steeringMeanAvailable` and `steeringCentroidCount`** and that `rebuildGenCentroidRows()` plus `resized()` run on every update path.
3. **Trigger asset fetch** on all the places a user can change model state that affects assets (refresh config, successful apply/warm, checkpoint change → on apply/warm success).

### Concrete changes

#### A.1 Clear steering UI just before fetching assets
In the editor code that handles **“refresh config”** and the start of **apply/warm**, immediately clear the steering UI so the user sees a deterministic transition:

```cpp
// BEFORE we call fetchDariusAssetsStatus();
dariusAssetsMeanAvailable = false;
dariusAssetsCentroidCount = 0;
dariusCentroidWeights.clear();
if (dariusUI) {
    dariusUI->setSteeringAssets(false, 0, {});  // clears mean+centroids UI
}
```

Do this in the handler that responds to:
- `onRefreshConfigRequested`
- right before (or at the beginning of) **apply & warm** workflow (so we don’t show stale sliders during warm).

> Note: these calls are idempotent; calling them again after the fetch returns is fine.

#### A.2 Ensure post-config path always fetches assets **and** relays to UI
Confirm you **call** (or add if missing) the assets fetch right after updating model config, and then relay the result to `DariusUI`:

```cpp
// After updating model status fields from config JSON:
fetchDariusAssetsStatus();  // async

// In handleDariusAssetsStatusResponse(...):
if (dariusUI) {
    dariusUI->setSteeringAssets(meanLoaded, centroidCount, centroidWeightsVector);
}
```

#### A.3 Keep visibility logic local to DariusUI
Double‑check (and if needed, adjust) `DariusUI::setSteeringAssets(...)` to always:
- store `steeringMeanAvailable` and `steeringCentroidCount`,
- resize `genCentroidWeights` to match,
- call `rebuildGenCentroidRows()`,
- call `updateGenSteeringToggleText()`,
- call `resized()`.

If any of these calls are missing, add them (they’re all cheap).

#### A.4 No extra tab gymnastics
Do **not** require `setCurrentSubTab()` calls to show centroids; `setSteeringAssets()` must be sufficient.

#### A.5 Optional guard: header visibility
If not already present, ensure the “centroids” header/labels are hidden when `steeringCentroidCount == 0`, and visible otherwise. For example, in your steering layout logic:

```cpp
// Pseudocode inside layoutGenSteeringUI():
const bool haveCentroids = (steeringCentroidCount > 0) && (genCentroidSliders.size() > 0);
genCentroidsHeaderLabel.setVisible(haveCentroids);
for (auto* l : genCentroidLabels)  if (l) l->setVisible(haveCentroids);
for (auto* s : genCentroidSliders) if (s) s->setVisible(haveCentroids);
```

This prevents an empty block when none are available.

---

## Part B — Recording vs Output source (file path)

### Problem
Even when the **Recording** radio is selected, generation sometimes uses the first 10 seconds of **myOutput.wav** (Output) instead of **myBuffer.wav** (Recording). Terry’s section behaves correctly.

### Ground truth (current flow)
- UI source selection bubbles through `DariusUI::onAudioSourceChanged(bool useRecording)`.
- Editor sets `audioProcessor.setTransformRecording(useRecording)`.
- When building a generation request, we choose the file path based on `getTransformRecording()`; recorded path should be **myBuffer.wav**, output path is **myOutput.wav**.

### Fix plan
1. **Guarantee the UI → Processor flag is updated** on every path (explicit button clicks and automatic fallbacks when a source isn’t available).
2. **Log and verify** the chosen path right where we compute it.
3. **Propagate availability** (`setSavedRecordingAvailable` / `setOutputAudioAvailable`) consistently so the fallback logic in the UI is exercised only when appropriate.

### Concrete changes

#### B.1 Wire every pathway to `onAudioSourceChanged`
In `DariusUI`:
- Ensure the **Recording** and **Output** button `onClick` handlers call the local setter (which updates internal state) **and** invoke `onAudioSourceChanged(useRecording)` if provided.
- In the **fallback path** (when a source isn’t available), after you change `genAudioSource`, also call `onAudioSourceChanged(...)` so the processor flag stays in sync.

Example (pattern):
```cpp
genRecordingButton->onClick = [this]() {
    setAudioSourceRecording(true);
    if (onAudioSourceChanged) onAudioSourceChanged(true);
};

genOutputButton->onClick = [this]() {
    setAudioSourceRecording(false);
    if (onAudioSourceChanged) onAudioSourceChanged(false);
};

// In updateGenSourceEnabled(), if you flip genAudioSource due to availability:
if (fallbackHappened && onAudioSourceChanged) {
    onAudioSourceChanged(genAudioSource == GenAudioSource::Recording);
}
```

#### B.2 Update availability from the editor on every tick that matters
From the editor’s timer/update path, keep these in sync whenever saved samples or output audio availability change:

```cpp
if (dariusUI) {
    dariusUI->setSavedRecordingAvailable(savedSamples > 0);
    dariusUI->setOutputAudioAvailable(hasOutputAudio);
    dariusUI->setAudioSourceRecording(audioProcessor.getTransformRecording());
}
```

#### B.3 Add a one‑line debug at selection time and at path resolution time
This helps catch any mismatches during QA:

```cpp
// In the editor’s onAudioSourceChanged:
DBG("Gen source set to " + juce::String(useRecording ? "Recording" : "Output"));

// Where the file path is chosen (e.g. getGenAudioFilePath or request builder):
DBG("Using gen audio path: " + chosenFile.getFullPathName());
```

If you see “Recording” selected but path shows `myOutput.wav`, the processor flag isn’t latched; fix B.1.

---

## Definition of Done (DoD)

- After pressing **Refresh config**, if the backend exposes `cluster_centroids.npy`, the **mean** and all **centroid** sliders appear immediately—no tab switch needed.
- Changing checkpoints and pressing **Apply & warm** results in the correct centroids appearing/hiding right after warm completes.
- Selecting **Recording** uses **myBuffer.wav**; selecting **Output** uses **myOutput.wav** for generation (verified via logs).
- No regressions to Gary/Jerry/Terry tabs.
- No blocking changes to network request code.

---

## Test script

1. Launch the plugin with a finetune **without** centroids → Refresh config → Verify **no** mean/centroids visible.
2. Switch to a checkpoint/finetune **with** centroids → Apply & warm → Verify mean + 5 centroid sliders appear.
3. Toggle **Recording** (with a saved buffer present) → Generate → Check logs show `myBuffer.wav`.
4. Toggle **Output** → Generate → Check logs show `myOutput.wav`.
5. Remove saved recording (clear buffer) → Ensure UI falls back to **Output** and logs confirm the path.
6. Repeat 1–5 without leaving the **Generation** tab.

---

## Git / Branch hygiene

- Branch: keep working in `refactor/darius-ui-extract`.
- Commit message: `fix: centroids display and gen source path wiring in DariusUI`.
- If you need to back out, revert this single commit.

---

## Notes for the agent

- Prefer minimal diffs: add calls and small guard code rather than moving logic around.
- Avoid adding any sleeps/delays; UI should update immediately from the main thread.
- Keep all new logs under `DBG(...)` so they’re compiled out in release builds.
- If you find duplicate “assets handler” code paths, unify to the one that calls `dariusUI->setSteeringAssets(...)` after fetch, then `resized()`. Do **not** duplicate UI rebuild code in the editor.
