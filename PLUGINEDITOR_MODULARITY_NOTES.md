# PluginEditor Modularity Pass

This branch keeps `Gary4juceAudioProcessorEditor` as a single editor class while splitting its method definitions across multiple translation units.

## Goals

- Reduce the size and cognitive load of `Source/PluginEditor.cpp`
- Preserve runtime behavior and avoid adding a new manager/abstraction layer in the generation hot path
- Keep future merges manageable for both Windows and Mac project files

## What Changed

Feature-specific editor logic was moved out of `Source/PluginEditor.cpp` into dedicated files:

- `Source/PluginEditor.Carey.cpp`
  Carey request submission, Carey polling, Carey progress handling, and Carey mode-specific flows including extract/complete/cover
- `Source/PluginEditor.Jerry.cpp`
  Jerry sub-tabs, model/checkpoint/prompt loading, custom model switching, and Jerry generation
- `Source/PluginEditor.Terry.cpp`
  Terry transform and Terry undo flows
- `Source/PluginEditor.Backend.cpp`
  Backend reachability checks, local backend status handling, and backend-related dialogs/messages
- `Source/PluginEditor.Updates.cpp`
  Update checks, release prompt flow, and update preference handling
- `Source/PluginEditor.Foundation.cpp`
  Foundation request flow and Foundation audio-to-audio handling

Small feature-local helper headers were also added:

- `Source/PluginEditorCareyHelpers.h`
  Carey failure parsing and Carey progress parsing/resolution helpers
- `Source/PluginEditorJerryHelpers.h`
  Jerry loop-type conversion helpers
- `Source/PluginEditorTerryHelpers.h`
  Terry variation list helper
- `Source/PluginEditorTextHelpers.h`
  Shared ANSI/control-character stripping used by `showStatusMessage` and Carey queue sanitization

The Carey UI component was also normalized to match the existing split-implementation pattern used by other model UIs:

- `Source/Components/Carey/CareyUI.h`
  Carey UI declarations, public accessors, and member state
- `Source/Components/Carey/CareyUI.cpp`
  Carey UI construction, layout, drawing, prompt-bank helpers, and lyrics dialog implementation

## What Still Lives In PluginEditor.cpp

`Source/PluginEditor.cpp` remains the home for shared editor behavior and the still-unsplit domains:

- Core editor construction/setup/painting/layout flow
- Shared status and waveform behavior
- Shared polling/completion behavior outside feature-local flows
- Gary generation logic
- Darius logic
- Audio import, drag/drop, playback, and crop handling

## Project Wiring

The new translation units and helper headers were added to:

- `gary4juce.jucer`

This keeps the source-of-truth project definition aligned so Visual Studio and Xcode project files can be regenerated from the Projucer configuration when needed.

## Result

- `Source/PluginEditor.cpp` was reduced from roughly `11,732` physical lines to `6,514`
- The class model is unchanged: this is still one `PluginEditor`, just split across multiple `.cpp` files
- `Debug|x64` builds passed in Visual Studio 2022 after the split
- Manual plugin smoke testing in Ableton showed no intended behavior changes during this pass

## Guidance For The Next Pass

The safest next extractions are utility-sized and mechanical:

- Audio import helpers inside `loadAudioFileIntoBuffer`
  Candidate split: buffer resampling helper and recording-buffer persistence helper
- Pure filesystem path getters
  Candidate split: `Documents/gary4juce`, `myBuffer.wav`, and `myOutput.wav` lookup helpers

The riskiest area to centralize too early is directory creation/error handling. That behavior currently varies slightly by feature flow and should be normalized carefully, especially for fresh-install behavior and Mac parity.

Avoid reintroducing a generalized network manager for this refactor. The current approach deliberately favors feature-local method moves over new runtime indirection.
