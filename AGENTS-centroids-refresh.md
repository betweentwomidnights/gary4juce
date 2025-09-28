# Task: Make centroid sliders appear immediately after config refresh or checkpoint change

**Branch:** `refactor/darius-ui-extract`  
**Scope:** Wire the assets-status fetch into the config/selection flow and ensure DariusUI is updated immediately. No redesigns.

## Summary
Users only see the 5 centroid sliders after doing a sequence of actions (change checkpoint → press “refresh config” → visit the Generation tab). This points to the `/model/assets/status` snapshot not being refreshed when config is refreshed, leaving `DariusUI` without up-to-date `mean/centroids` availability until some later side-effect calls it.

## What to fix
1. **Always refresh assets status right after `/model/config` completes.**  
   After `handleDariusConfigResponse(...)` parses and stashes the config, trigger `fetchDariusAssetsStatus()` (optionally guard by `loaded==true`).
2. Ensure the select/apply path **already** refreshes assets (it does) and keep that behavior.
3. No UI logic changes to `DariusUI` are needed: `setSteeringAssets()` already rebuilds the centroid rows and resizes the layout.

## Acceptance criteria
- Pressing **“refresh config”** (Model tab) immediately shows mean/centroid controls on the **Generation** tab (if available) without needing any extra navigation or checkpoint switching.
- Changing checkpoints (and applying) still updates centroid controls as before.
- No regressions to Gary/Jerry/Terry functionality. No additional network calls beyond one assets-status fetch per config refresh.

---

## Implementation details

### 1) Call `fetchDariusAssetsStatus()` after config refresh
In `PluginEditor.cpp`, after successful parse and UI update in `handleDariusConfigResponse(...)`, add:

```cpp
// After:
//   lastDariusConfig = parsed;
//   updateDariusModelConfigUI();
// and logging

// Option A: Always fetch (simple and robust)
fetchDariusAssetsStatus();

// Option B: Only when model is actually loaded (slightly fewer calls)
if (auto* obj = parsed.getDynamicObject())
{
    const bool loaded = (bool)obj->getProperty("loaded");
    if (loaded)
        fetchDariusAssetsStatus();
}
```

> Keep the call near the end of `handleDariusConfigResponse(...)`, after we’ve updated labels and internal state. This mirrors the apply/select path which already does:
```cpp
fetchDariusConfig();
fetchDariusAssetsStatus();
```
…right after a successful `/model/select` response.

### 2) (Optional) Also fetch on initial **health → config** success
If you want centroid controls to appear immediately on first connect (when the backend is warmed and a finetune is already selected server-side), you can also add a single `fetchDariusAssetsStatus()` right after the first `fetchDariusConfig()` returns successfully on app start. This is optional; the primary fix is the call inside `handleDariusConfigResponse(...)`.

### 3) No changes needed in `DariusUI`
`DariusUI::setSteeringAssets(meanAvailable, centroidCount, weights)` already:
- Sets `genMeanAvailable` and `genCentroidCount`
- Copies `genCentroidWeights`
- Calls `rebuildGenCentroidRows()` and `resized()` so the UI is rebuilt immediately

So as long as `handleDariusAssetsStatusResponse(...)` calls `dariusUI->setSteeringAssets(...)`, the controls will appear without requiring a tab switch.

---

## Test plan

1. **Refresh-only path**
   - Start the app and connect to a backend where `cluster_centroids.npy` exists for the active model.
   - Go to **Model** tab → click **Refresh config**.
   - Go to **Generation** tab.
   - **Expected:** Mean slider and 5 centroid sliders are visible immediately.

2. **Checkpoint change path**
   - Open **Checkpoint** menu → pick a different step → **Apply & Prewarm**.
   - After warm completes, go to **Generation** tab.
   - **Expected:** Centroid controls reflect the newly selected checkpoint (no extra refresh needed).

3. **No-assets path**
   - Switch to a checkpoint with **no** centroid assets.
   - **Expected:** Mean/centroid controls are hidden and the rest of the generation UI still works.

4. **Regression scan**
   - Verify Gary/Jerry/Terry flows are unaffected.
   - Verify the centroid weight labels display values (no “wat” suffix).

---

## Notes for reviewers
- Keep the change **small**: just the single call into `fetchDariusAssetsStatus()` after config parsing (and optionally one on initial ready).  
- Avoid adding timers or tab-change hooks; this should be purely data-driven.
- If you need to gate the call, use the `loaded` flag from `/model/config`.
