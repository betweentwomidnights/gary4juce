# DariusUI — Checkpoint Menu & Refresh Wiring (Small, Safe Task)

This task verifies and (if needed) completes the **checkpoint menu** and **refresh config** wiring after the `DariusUI` extraction. It’s intentionally small, UI-only, and reversible.

---

## Context

- `DariusUI` raises UI events; `PluginEditor` owns networking/behavior.
- We must ensure the editor **wires** DariusUI events → existing editor methods **and** keeps the view state in sync so buttons aren’t gated off.

---

## What to wire (4 hooks)

Add/verify these immediately after constructing `dariusUI` in `PluginEditor`:

```cpp
// 1) Refresh config
dariusUI->onRefreshConfigRequested = [this] {
    fetchDariusConfig(); // existing editor method
};

// 2) Cold open: Fetch checkpoints list when user clicks menu but cache is empty
dariusUI->onFetchCheckpointsRequested = [this] {
    fetchDariusCheckpoints(); // existing editor method
};

// 3) User picked a checkpoint (or "latest"/"none")
dariusUI->onCheckpointSelected = [this](const juce::String& selection) {
    // selection: "latest", "none", or step string "4000"
    postDariusSelect(selection); // or existing select/apply path
};

// 4) Keep view gating in sync (call these from editor when state changes)
dariusUI->setConnected(/* true/false from health check */);
dariusUI->setUsingBaseModel(/* true/false from toggle */);
```

On receiving the actual checkpoints (from your network handler), update the view and let DariusUI open the menu if it was waiting:

```cpp
void PluginEditor::handleDariusCheckpointsResponse(const juce::Array<int>& steps) {
    if (dariusUI) {
        dariusUI->setCheckpointSteps(steps); // DariusUI opens the menu if it requested a fetch
    }
}
```

> Note: If base-model is ON or not connected, DariusUI intentionally suppresses the menu. Keep those setters current.

---

## Local smoke tests (no backend needed)

You can validate wiring and UI flow without spinning up servers.

```cpp
// Somewhere safe in editor after construction (temporary test code / dev menu):
dariusUI->setConnected(true);
dariusUI->setUsingBaseModel(false);

// Case A: Steps already known
dariusUI->setCheckpointSteps(juce::Array<int>{ 2000, 4000, 6000 });
// -> Click "checkpoint ▾": menu should show "latest", "none", 2000, 4000, 6000
// -> Selecting an item should call onCheckpointSelected (log/printf in lambda)

// Case B: Cold open
dariusUI->setCheckpointSteps({}); // ensure empty
// -> Click "checkpoint ▾": should fire onFetchCheckpointsRequested
// Simulate a response:
dariusUI->setCheckpointSteps(juce::Array<int>{ 1000, 3000 });
// -> Menu should pop open automatically; selection should fire onCheckpointSelected
```

Also confirm **Refresh Config**:
- Click **Refresh Config** → `onRefreshConfigRequested` should fire and call `fetchDariusConfig()`.
- On your response handler, update DariusUI via setters (`setUsingBaseModel`, repo/status labels, etc.).

---

## Cross-check against `main` (reference implementation)

Codex should compare the new wiring with the original “godfile” on the default branch.

Suggested commands (for Codex or humans):

```bash
# Ensure 'main' is available locally
git fetch origin main

# View old, working wiring in the godfile:
git show origin/main:Source/PluginEditor.cpp | sed -n '1,2000p' | less

# Grep anchors in the old file (examples):
git show origin/main:Source/PluginEditor.cpp |   grep -nE 'fetchDarius(Checkpoints|Config)|postDariusSelect|showDariusCheckpointMenu|handleDariusCheckpointsResponse'

# Compare with the new DariusUI-based editor file:
git diff origin/main -- Source/PluginEditor.cpp
```

**What to verify in the old file:**
- Buttons and menu actions previously triggered:  
  `fetchDariusConfig()`, `fetchDariusCheckpoints()`, `showDariusCheckpointMenu(...)`, `postDariusSelect(...)` (or your exact names).  
- Any guards enabling/disabling checkpoint access based on **connected** and **useBaseModel** states.

**What to verify in the new files:**
- `PluginEditor` sets all four callbacks above.
- Editor calls `setCheckpointSteps(...)` on response (to let the UI display the menu).
- Editor maintains `setConnected(...)` and `setUsingBaseModel(...)` so the menu/button aren’t gated off.

---

## Acceptance criteria

- ✅ Clicking **Refresh Config** triggers `fetchDariusConfig()` and updates the corresponding UI readouts when the response arrives.
- ✅ Clicking **Checkpoint ▾** when there’s **no** cached list triggers `onFetchCheckpointsRequested` → `fetchDariusCheckpoints()`; when the response handler calls `setCheckpointSteps(...)`, the menu **opens automatically**.
- ✅ When a checkpoint is selected (including **"latest"** or **"none"**), `onCheckpointSelected` fires and the editor routes to the existing select/apply path.
- ✅ Menu is disabled/hidden when `setConnected(false)` or `setUsingBaseModel(true)`; enabled when `true/false` respectively.
- ✅ No changes to networking logic or other tabs.

---

## PR notes

- Title: `darius-ui: wire checkpoint menu & config refresh`
- Labels: `ui`, `small`, `regression-proof`
- Risk: **low** (UI-only)
- Rollback: revert the single commit
