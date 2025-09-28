# Refactor Step 1 — Extract **Darius UI** from `PluginEditor`

**Goal (limited scope):** Move *UI-only* code for the **Darius** tab (labels, buttons, text editors, sub-tab containers, and layout logic) out of `PluginEditor` into a new pair of files:
- `Source/Components/Darius/DariusUI.h`
- `Source/Components/Darius/DariusUI.cpp`

This is a purely **presentation-layer** extraction. No network/IO logic moves in this step.

---

## Why this is safe and foundational

- The Darius tab is visually/self-contained and already uses our base components and theme (e.g., `CustomButton`, `Theme`) which are independent of networking.  
- Several Darius symbols and layout blocks are concentrated in `PluginEditor.h/.cpp`, which makes them straightforward to relocate without touching Gary/Jerry/Terry flows.  
- Network-facing methods (`checkDariusHealth`, `fetchDariusConfig`, `fetchDariusCheckpoints`, `postDariusSelect`, `beginDariusApplyAndWarm`, etc.) **stay in `PluginEditor`** and are invoked via lightweight callbacks exposed by `DariusUI`.

> Rationale: keep behavior (network, timers, polling) where it already works to avoid latency regressions; only move widgets and their layout.

---

## In-Scope (move to `DariusUI`)

### Components / state to relocate
Move the following members from `PluginEditor.h` into `DariusUI` (as private members unless noted).

- **Tab label + basic controls**
  - `juce::Label dariusLabel`
  - `juce::TextEditor dariusUrlEditor`
  - `juce::Label dariusUrlLabel`
  - `CustomButton dariusHealthCheckButton`
  - `juce::Label dariusStatusLabel`

- **Sub-tab system**
  - `enum class DariusSubTab { Backend, Model, Generation }`
  - `DariusSubTab currentDariusSubTab = DariusSubTab::Backend;`
  - `CustomButton dariusBackendTabButton, dariusModelTabButton, dariusGenerationTabButton`

- **Model sub-tab UI containers**
  - `std::unique_ptr<juce::Viewport> dariusModelViewport`
  - `std::unique_ptr<juce::Component> dariusModelContent`

- **Generation sub-tab UI containers**
  - `std::unique_ptr<juce::Viewport> dariusGenerationViewport`
  - `std::unique_ptr<juce::Component> dariusGenerationContent`

- **Model readout + controls (UI elements only)**
  - `juce::Label dariusModelHeaderLabel, dariusModelGuardLabel`
  - `CustomButton dariusRefreshConfigButton`
  - `juce::Label dariusActiveSizeLabel, dariusRepoLabel, dariusStepLabel, dariusLoadedLabel, dariusWarmupLabel`
  - `juce::ToggleButton dariusUseBaseModelToggle`  
  - `juce::Label dariusRepoFieldLabel`
  - `juce::TextEditor dariusRepoField`
  - `CustomButton dariusCheckpointButton`
  - `CustomButton dariusApplyWarmButton`
  - `juce::Label dariusWarmStatusLabel`

- **Generation controls (UI elements only)**
  - Styles row widgets and helpers (pure UI): `genStylesHeaderLabel`, `genAddStyleButton`, `GenStyleRow` struct, `genStyleRows` vector, `addGenStyleRow`, `removeGenStyleRow`, `rebuildGenStylesUI`, `layoutGenStylesUI`, `genStylesCSV`, `genStyleWeightsCSV`
  - Loop Influence UI: `genLoopLabel`, `genLoopSlider`, `updateGenLoopLabel`, `genLoopInfluence` (retain as UI state)
  - Advanced section UI: `genAdvancedToggle`, `genAdvancedOpen`, `genTempLabel/genTempSlider/genTemperature`, `genTopKLabel/genTopKSlider/genTopK`, `genGuidanceLabel/genGuidanceSlider/genGuidance`
  - Bars/BPM readout UI: `genBarsLabel`, `genBars4Button`, `genBars8Button`, `genBars16Button`, `genBpmLabel`, `genBpmValueLabel`, `updateGenBarsButtons` (UI state for selected button)
  - Source select UI: `genSourceLabel`, `genRecordingButton`, `genOutputButton`, `genSourceGuardLabel`, `updateGenSourceButtons`, `updateGenSourceEnabled`
  - Generate button UI: `genGenerateButton`
  - Steering dropdown UI: `genSteeringToggle`, `genSteeringOpen`, `updateGenSteeringToggleText`, `genMeanLabel`, `genMeanSlider`, `genMean`

> Keep non-UI data that is shared with processor or networking (e.g., backend connectivity booleans, request/response handling) inside `PluginEditor` for now. UI state that only drives control visuals can live in `DariusUI`.

### Layout code to relocate
Move the **Darius-only** blocks from `PluginEditor::resized()` into `DariusUI::resized()`:
- The main Darius `FlexBox` (title/subtab row/content area)
- Per-subtab layout (`Backend`, `Model`, `Generation`) including the sub-containers and all their child placements
- Any helper methods that exist *solely* to compute or update Darius UI layout/labels (e.g., `updateGenAdvancedToggleText`, `updateDariusCheckpointButtonLabel`, etc.)

> After move, `PluginEditor::resized()` should only reserve a rectangle for the Darius component (`dariusUI.setBounds(...)`).

### Wiring events (no behavior moved)
Replace direct `onClick` lambdas that call PluginEditor methods with **callbacks** exposed by `DariusUI`:
- `std::function<void(const juce::String&)> onUrlChanged;`
- `std::function<void()> onHealthCheckRequested;`
- `std::function<void()> onRefreshConfigRequested;`
- `std::function<void()> onOpenCheckpointMenuRequested;`
- `std::function<void()> onFetchCheckpointsRequested;`
- `std::function<void()> onApplyWarmRequested;`
- `std::function<void()> onGenerateRequested;`
- Additional callbacks as needed for sub-controls (e.g., toggles, bars selection, source selection).

**`DariusUI` does not perform requests**; it simply raises these events. `PluginEditor` connects them to its existing methods (e.g., `checkDariusHealth()`, `fetchDariusConfig()`, `fetchDariusCheckpoints(...)`, `beginDariusApplyAndWarm()`, etc.).

---

## Out-of-Scope (keep in `PluginEditor`)

- All Darius **networking and stateful behaviors**:
  - `checkDariusHealth`, `handleDariusHealthResponse`
  - `fetchDariusConfig`, `handleDariusConfigResponse`, `updateDariusModelConfigUI`
  - `fetchDariusCheckpoints`, `handleDariusCheckpointsResponse`, `showDariusCheckpointMenu`
  - `postDariusSelect`, `handleDariusSelectResponse`, `makeDariusSelectApplyRequest`, `beginDariusApplyAndWarm`, `onDariusApplyFinished`, `startDariusWarmPolling`, warm “dots” animation tickers
  - Any JSON parsing, polling, timers, or processor coordination
- The global tab system and tab button ownership (`gary/jerry/terry/darius` root buttons) — `PluginEditor` remains the owner that shows/hides `DariusUI` based on the active tab.
- Gary/Jerry/Terry UI and logic remain untouched.

---

## Public API for `DariusUI` (minimal)

```cpp
// DariusUI.h (public surface)
class DariusUI : public juce::Component {
public:
  DariusUI();

  // data -> view setters
  void setBackendUrl(juce::String url);
  void setConnectionStatusText(juce::String text);
  void setConnected(bool connected);
  void setBpm(double bpm);                  // updates small readout only
  void setSavedRecordingAvailable(bool ok); // enables/disables "recording" source option
  void setUsingBaseModel(bool on);
  void setCurrentSubTab(DariusSubTab tab);  // optional, defaults internal state

  // read view -> data getters (if needed by editor)
  juce::String getBackendUrl() const;
  bool getUsingBaseModel() const;

  // events (wired by PluginEditor)
  std::function<void(const juce::String&)> onUrlChanged;
  std::function<void()> onHealthCheckRequested;
  std::function<void()> onRefreshConfigRequested;
  std::function<void()> onFetchCheckpointsRequested;
  std::function<void()> onOpenCheckpointMenuRequested;
  std::function<void()> onApplyWarmRequested;
  std::function<void()> onGenerateRequested;
  // ...add others as needed for buttons/switches

  // juce::Component
  void paint(juce::Graphics&) override;
  void resized() override;
};
```

Notes:
- Internally `DariusUI` owns all Darius widgets and updates their enabled/disabled state.  
- It **never** calls network code; it only triggers callbacks.

---

## Code Changes

1. **Create files**
   - `Source/Components/Darius/DariusUI.h`
   - `Source/Components/Darius/DariusUI.cpp`

2. **Move declarations/definitions**
   - Cut/paste the **Darius UI members** listed above from `PluginEditor.h` into `DariusUI.h` (private by default).
   - Cut/paste the **Darius setup code** (constructor sections adding/initializing these widgets) from `PluginEditor.cpp` into `DariusUI` constructor.
   - Cut/paste the **Darius layout code** from `PluginEditor::resized()` into `DariusUI::resized()`.
   - Replace `onClick` lambdas to invoke the corresponding `std::function` callback if set.

3. **Instantiate in PluginEditor**
   - Add member: `std::unique_ptr<DariusUI> dariusUI;`
   - In `PluginEditor` constructor:
     - `dariusUI = std::make_unique<DariusUI>();`
     - `addAndMakeVisible(*dariusUI);`
     - Wire callbacks, e.g.:
       ```cpp
       dariusUI->onHealthCheckRequested = [this]{ checkDariusHealth(); };
       dariusUI->onRefreshConfigRequested = [this]{ fetchDariusConfig(); };
       // etc.
       ```
     - Initialize data->view:
       ```cpp
       dariusUI->setBackendUrl(dariusBackendUrl);
       dariusUI->setConnectionStatusText("not checked");
       dariusUI->setBpm(audioProcessor.getCurrentBPM());
       dariusUI->setSavedRecordingAvailable(audioProcessor.getSavedSamples() > 0);
       ```

4. **Bounds + visibility**
   - In `PluginEditor::resized()`, reserve a rectangle for Darius (replacing the old layout code):
     ```cpp
     if (currentTab == ModelTab::Darius) {
       dariusUI->setVisible(true);
       dariusUI->setBounds(modelControlsArea); // whatever rect we used before
     } else {
       dariusUI->setVisible(false);
     }
     ```

5. **Remove now-duplicated members** from `PluginEditor.h`
   - All Darius UI widgets and the UI-only helpers you moved now live in `DariusUI`.

6. **Build system**
   - Add the two new files to the project (Projucer/CMake as appropriate).

---

## Acceptance Criteria

- ✅ Project builds with no warnings or missing symbols.
- ✅ Switching to the **Darius** tab displays identical UI as before (Backend / Model / Generation subtabs, controls, labels).
- ✅ All **buttons still function** because callbacks are wired to existing `PluginEditor` methods:
  - *Check connection*, *Refresh config*, *Checkpoint menu/fetch*, *Apply & warm*, *Generate*.
- ✅ No network or background/timer code relocated in this step.
- ✅ No behavior or latency regression observed in Gary/Jerry/Terry tabs.
- ✅ `PluginEditor::resized()` no longer contains any Darius layout code except a simple `setBounds` call.
- ✅ `PluginEditor` no longer declares Darius widgets; ownership moved to `DariusUI`.

---

## Nice-to-haves (optional this step)

- Expose a thin **ViewModel struct** to batch-update readouts (e.g., model status fields) to minimize redraw churn when editor reacts to network responses.
- Gate **BPM** and **Saved Recording** state via setters (`setBpm`, `setSavedRecordingAvailable`) and call them from the same places `PluginEditor` previously updated labels/enables.

---

## Notes for the reviewer

- We deliberately keep `dariusBackendUrl` *state source of truth* in `PluginEditor` to avoid surprising side effects; `DariusUI` mirrors it via `setBackendUrl` and emits changes through `onUrlChanged`.
- `Theme.h` + `CustomButton` styling must remain unchanged and should be included by `DariusUI` (same imports as in `PluginEditor`).

---

## How to test manually

1. Build and launch in a host.
2. Switch across tabs; confirm Gary/Jerry/Terry unchanged.
3. Go to **Darius → Backend**: edit URL, click **check connection** (should call existing `checkDariusHealth`).
4. **Model**: hit **refresh config**; if backend is offline, guard text still shows.
5. **Model**: open **checkpoint** menu; verify menu still appears after fetch if needed.
6. **Generation**: change advanced sliders/toggles; press **generate**; confirm same call path.
7. Resize the plugin window; verify Darius layouts behave as before (FlexBox-based).

---

## Follow-up steps (future PRs, *not* part of this task)

- Move Darius networking logic into a dedicated `DariusService` (or `DariusController`) and connect to `DariusUI` via a ViewModel/Presenter pattern.
- Extract common FlexBox layout helpers into `Utils/Layout.h` if duplication appears across tabs.
- Consider splitting Darius sub-tabs into smaller child components once the top-level `DariusUI` is stable.
