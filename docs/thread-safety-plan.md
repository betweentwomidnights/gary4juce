# gary4juce Thread-Safety + Backend Lifecycle Plan

## Context

We reproduced the Windows CPU spike without `gary4juce` loaded in Ableton, so the 150% CPU / system degradation issue still points to FlexASIO or Windows audio-driver scheduling rather than the plugin itself.

This branch should still harden the plugin. The important adjustment is scope: the risk is not only in `PluginProcessor.cpp`. For `gary4juce`, the realtime and lifetime surface spans the processor and the editor because the product depends on:

- DAW transport automatically starting capture into the recording buffer
- Saving that captured buffer and sending it to the backend for ML iteration
- Playing generated audio out of the plugin through the host output path
- Remote/local backend switching and health reporting
- Long-running generation and polling flows that may outlive the editor window if we are not careful

This document is the implementation brief for the `feature/thread-safety` branch.

## Product constraints to preserve

1. Pressing play in the DAW must still start recording into the input buffer reliably.
2. The user must still be able to save that buffer and send it to the backend.
3. Generated output must still be playable from inside the plugin through the host audio path.
4. Remote/local backend toggle must remain intact.
5. Localhost mode must keep per-service health visibility for Gary, Terry, Jerry, Carey, and Foundation.
6. Manual connection checks, connection indicators, retry/continue flows, session handling, and drag-to-DAW export must remain intact.

## Primary goals

1. Make background work explicitly owned, cancellable, and restartable.
2. Remove unsafe detached-thread patterns that capture dead processor/editor pointers.
3. Keep the audio thread free of blocking locks.
4. Preserve transport-driven recording reliability and in-plugin output audition behavior.
5. Keep backend health awareness without surprising hosts during scan/load.
6. Make editor close/reopen, project load, device switch, and plugin destruction safe.

## Non-goals

Do not redesign the whole plugin.

Do not remove backend health checks.

Do not remove remote/local switching or service-specific localhost checks.

Do not remove in-plugin output playback.

Do not change generation request/response formats unless required for cancellation or safety.

Do not convert the whole project to `AudioProcessorValueTreeState` in this branch.

## Important findings from the current code

### 1. Backend network work starts during processor construction and state restore

Today the processor calls `checkBackendHealth()` in the constructor, and `setStateInformation()` also triggers a health check.

That means network activity can start during:

- host scanning
- plugin validation
- project loading
- state restore when the editor is not open

For this plugin, backend health still matters, but these are the wrong lifecycle points.

### 2. Closing the editor currently disables future processor health checks

`Gary4juceAudioProcessorEditor::~Gary4juceAudioProcessorEditor()` calls `stopAllBackgroundOperations()`, which calls `audioProcessor.stopHealthChecks()`.

Right now `stopHealthChecks()` permanently sets `shouldStopBackgroundOperations = true`.

That is a bigger issue than the original brief captured: after the editor closes once, future calls to `checkBackendHealth()` will abort unless the processor is recreated.

For `gary4juce`, editor teardown must cancel in-flight work, not permanently disable backend health for the rest of the processor lifetime.

### 3. Processor health checks use a detached thread that captures `this`

`juce::Thread::launch([this] { ... })` is still used in `PluginProcessor.cpp`, and the destructor only sleeps for 100 ms.

That is not strong enough lifetime management for plugin destruction.

### 4. `processBlock()` still calls blocking recording state transitions

The transport edge path currently calls `startRecording()` / `stopRecording()` from `processBlock()`, and both take `juce::ScopedLock lock(bufferLock)`.

That can block the audio thread.

### 5. A pure `tryEnter()` replacement is not enough for this plugin

The earlier generic recommendation to replace those calls with:

- `startRecordingFromAudioThread()`
- `stopRecordingFromAudioThread()`

using only `tryEnter()` is not sufficient here.

Why: if the lock is busy on the exact block where transport changes from stopped to playing, the plugin can miss the recording start edge entirely. For `gary4juce`, that is product-visible because transport start is how users intentionally capture input for backend iteration.

So the plan must preserve the edge, not just avoid blocking.

### 6. Output audition playback is part of the realtime surface

This plugin is unusual because generated audio is mixed back out through the plugin itself.

The playback path in `PluginProcessor.cpp` already uses `playbackBufferLock`, but it still needs explicit review:

- the audio thread checks `outputPlaybackBuffer.getNumSamples()` before taking the lock
- message-thread playback control methods take blocking locks
- `loadOutputAudioForPlayback()` resizes/swaps the buffer used by `processBlock()`

The original processor-only plan did not cover this, but it should.

### 7. Editor-side lifetime risk is spread across multiple files, not one class body

There are still many detached threads, `callAsync([this])`, and `Timer::callAfterDelay([this])` callbacks across:

- `PluginEditor.cpp`
- `PluginEditor.Backend.cpp`
- `PluginEditor.Jerry.cpp`
- `PluginEditor.Terry.cpp`
- `PluginEditor.Carey.cpp`
- `PluginEditor.Foundation.cpp`
- `PluginEditor.Updates.cpp`

Some places already use good patterns such as `SafePointer` or request nonces. This branch should extend those patterns consistently rather than rewriting everything from scratch.

### 8. Shutdown still relies on `Thread::sleep()`

Both processor/editor cleanup paths currently use sleeps as a best-effort shutdown strategy.

That is fragile in plugin code and can freeze the message thread during editor close.

## Recommended implementation strategy

### Phase 1: Fix processor backend health ownership and lifecycle

1. Remove automatic health checks from:
   - `Gary4juceAudioProcessor` constructor
   - `setStateInformation()`
2. Replace the one-way `shouldStopBackgroundOperations` shutdown flag with a restartable health-check owner.
3. Keep manual health checks and backend toggle checks, but route them through the owned worker.
4. Editor close must cancel in-flight health work only. It must not permanently disable future health checks for the processor lifetime.
5. The worker must copy the URL before it starts so it never reads mutable processor strings from the background thread.

Preferred shape:

- `requestBackendHealthCheck()`
- `cancelBackendHealthCheck()`
- an owned `BackendHealthChecker` thread/job/request object

Important: `cancel` must mean "stop or ignore the in-flight request", not "never allow checks again".

### Phase 2: Make transport-driven recording safe without losing the start edge

Do not keep the current blocking `ScopedLock` path inside `processBlock()`.

Also do not switch to a naive `tryEnter()` helper that simply returns on failure.

Instead:

1. Detect transport start/stop on the audio thread.
2. Store that transition in atomic pending flags so the edge is latched until applied.
3. Apply the pending start/stop when the recording state can be updated safely.
4. Keep the actual audio copy path non-blocking from the realtime thread perspective.

Minimum acceptable incremental design:

- `std::atomic<bool> recordingStartPending`
- `std::atomic<bool> recordingStopPending`
- `std::atomic<bool> recordingActive`

Then:

- transport edge sets the pending flag
- the pending flag remains set until it has actually been committed
- `processBlock()` never blocks waiting for `bufferLock`

Important branch decision:

If testing shows that save/load/clear operations still contend often enough to miss the beginning of a recording, this branch should escalate to a safer capture design such as a dedicated lock-free staging buffer or double-buffer snapshot approach. For `gary4juce`, preserving the transport-start capture behavior matters more than mechanically applying a generic "just use tryLock" pattern.

### Phase 3: Make output playback synchronization explicit

Because the plugin can audition generated audio through the host output path, the playback buffer needs the same care as the recording buffer.

Recommendations:

1. Stop peeking at `outputPlaybackBuffer` from the audio thread outside the synchronization boundary.
2. Keep `processBlock()` non-blocking; use `tryEnter()` or an atomic published-state pattern there.
3. When loading or replacing output audio:
   - stop playback first
   - swap/update buffer data under the playback lock
   - publish new duration/position/ready state atomically
4. Keep pause/seek/start/stop available from the editor without risking races against the audio thread.

This is not optional for this plugin because in-plugin playback is a core user workflow.

### Phase 4: Expand the cleanup scope to editor async lifetime management

The processor changes alone are not enough.

For this branch, audit and fix the hot-path editor async work first:

1. generation submit requests
2. poll-status requests
3. retry/continue requests
4. backend toggle and manual connection checks
5. localhost per-service polling
6. Darius health/config/progress requests
7. update checks

Preferred patterns:

- `juce::Component::SafePointer`
- request nonces / generation IDs
- explicit "ignore late response" cancellation
- owned worker objects for repeated operations

Avoid:

- `juce::Thread::launch([this] { ... })`
- `MessageManager::callAsync([this] { ... })` without a lifetime guard
- `Timer::callAfterDelay(..., [this] { ... })` without a lifetime guard
- `Thread::sleep()` during editor destruction

Note: several Carey and update flows already use nonce or safe-pointer style logic. Reuse those patterns instead of inventing a second cancellation system.

### Phase 5: Decouple processor state changes from direct editor access

The earlier review suggestion still stands here.

Move away from:

```cpp
if (auto* editor = getActiveEditor())
    if (auto* myEditor = dynamic_cast<Gary4juceAudioProcessorEditor*>(editor))
        myEditor->updateConnectionStatus(connected);
```

Preferred incremental fix:

- make the processor a `juce::ChangeBroadcaster`
- let the editor listen and pull processor state

That keeps backend connection state updates compatible with:

- editor close/reopen
- async health-check completion
- project load without an open editor

### Phase 6: Remove sleep-based teardown

Replace:

- processor `sleep(100)`
- editor `sleep(150)`

with real cancellation/join/ignore-late-response behavior.

The branch should not rely on arbitrary sleeps to "probably" avoid use-after-free.

## Concrete patch sequence

1. Create and work on `feature/thread-safety`.
2. Replace processor health-check lifecycle:
   - remove constructor/state-restore network checks
   - add owned cancellable/restartable health checker
   - split "cancel in-flight check" from "processor is shutting down forever"
3. Refactor transport-edge recording:
   - remove blocking lock-taking on the audio thread
   - latch start/stop edges so a busy lock cannot permanently drop the recording start
4. Harden output playback synchronization:
   - treat playback buffer swap/start/seek/pause/stop as part of the realtime plan
5. Replace high-risk editor async call sites with safe ownership/cancellation patterns.
6. Decouple processor backend status from direct editor pointer access.
7. Run the host and driver validation matrix below before merging to `main`.

## Acceptance tests

### Host lifecycle tests

1. Scan the plugin in Ableton.
2. Open a project containing the plugin without opening the editor.
3. Confirm no backend request is made during processor construction or state restore.
4. Open and close the editor repeatedly.
5. Reopen the editor and confirm connection checks still work after a prior editor close.

### Transport capture tests

1. Arm the desired track/input path in Ableton.
2. Press play in the DAW and confirm recording starts immediately.
3. Stop and restart transport repeatedly.
4. Trigger save/clear/load-buffer operations near transport transitions and confirm:
   - no crash
   - no audio-thread glitch
   - no permanently missed recording start edge

### Output playback tests

1. Generate output audio.
2. Play it from inside the plugin.
3. Pause, resume, seek, stop, and replay.
4. Clear or replace output audio while playback is active.
5. Confirm no crash, no stuck state, and no corrupted playback position.

### Backend workflow tests

1. Manual connection check in remote mode.
2. Toggle remote/local back and forth.
3. In localhost mode, verify per-service health reporting for Gary, Terry, Jerry, Carey, and Foundation.
4. Start a generation, close the editor, reopen it, and confirm late responses do not crash the plugin.
5. Run generate, continue, retry, Terry transform, Carey requests, and Foundation requests.

### Device and driver tests

1. Ableton + Focusrite native ASIO
2. Ableton + ASIO4ALL
3. Ableton + WASAPI shared or MME/DirectX
4. Ableton + FlexASIO only as a regression check, not as the expected fix

For each:

- load plugin
- start/stop transport
- open/close editor
- switch audio device if possible
- audition generated output
- close Ableton cleanly

### Out-of-scope reminder

If FlexASIO still spikes CPU without `gary4juce` loaded, that remains a driver/backend issue and should not be described as fixed by this branch.

## Suggested commit message

```text
Harden backend lifecycle and realtime thread safety
```

## Suggested PR summary

```markdown
## Summary
- move backend health checks out of processor construction/state restore
- replace detached health-check work with owned cancellable lifecycle management
- remove blocking recording transitions from the audio thread without dropping DAW transport start
- harden output audition playback synchronization
- replace high-risk editor async callbacks with safe lifetime guards
- decouple processor connection state from direct editor pointer updates

## Notes
This branch improves plugin lifecycle and realtime safety. It is not expected to fix FlexASIO itself, since the Windows CPU spike was reproduced without gary4juce loaded.
```
