# Linux desktop preview

This is an experimental Ubuntu 24.04 build of gary4juce 5.0.5, for testing
with the separately supplied gary4local Linux desktop preview. Packages are
available for x64 Linux PCs and ARM64 systems such as DGX Spark. GPU drivers,
models and backend runtimes are separate downloads.

Use the archive matching your CPU architecture. The VST3 archive contains a
single `gary4juce.vst3` directory; after closing your DAW, extract and copy that
directory into `~/.vst3/`, then rescan plugins. Keep any previous Linux plugin
bundle as a backup. For the standalone archive, extract the whole
`gary4juce-standalone` directory and run `./gary4juce` inside it.

The archives preserve executable permissions. If your extraction utility drops
them, run `chmod +x gary4juce` for the standalone executable, or
`chmod +x gary4juce.vst3/Contents/*-linux/gary4juce.so` for the plugin module.

The Linux desktop needs ALSA, X11/XWayland, FreeType/fontconfig and libcurl.
On Ubuntu 24.04, the relevant runtime packages are `libasound2t64`, `libx11-6`,
`libxext6`, `libxinerama1`, `libxrandr2`, `libxcursor1`, `libfreetype6`,
`libfontconfig1` and `libcurl4t64`. Native Wayland and DAW-specific drag behavior
need testing on a real desktop.

## What changed since v2.0.0-linux

The Linux branch now includes main's current models, Carey job API, Yuey score
and MIDI workflows, SA3 LoRA lanes, saved recording-buffer lane, improved
storage, and optional JSON or embedded audio metadata. The full intervening
history is in [CHANGELOG.md](CHANGELOG.md).

The Linux keyboard-focus flag is retained so hosts can send typing to prompt
boxes. External audio drags request a copy and retain their file list through
the callback. The **copy** button beside output saves an export in
`dragged_audio` and reveals it in the file manager, for dragging through your
DAW's browser or file manager when direct plugin dragging is unavailable.
It uses the selected WAV/FLAC and generation-details settings.

Built-in stem separation has no pinned Linux runtime yet, so its installation
and separation controls remain unavailable on Linux. Generation backends are
provided by gary4local or the remote service as on the other platforms.

The updater reads `preview-linux.json`, published from `main`, and identifies
this build as `5.0.5-linux-preview.1`. Windows and macOS stable feeds are separate.
The native plugin version remains 5.0.5; the tag identifies the preview build.

## Build

Install build dependencies on Ubuntu 24.04:

```sh
sudo apt-get install build-essential cmake ninja-build pkg-config libasound2-dev \
  libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev \
  libfreetype-dev libfontconfig1-dev libcurl4-openssl-dev libgl1-mesa-dev \
  xvfb xauth x11-utils openbox
cmake -S . -B build-linux-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-linux-release --parallel 2
```

CMake downloads the pinned JUCE 8.0.8 archive and verifies its SHA-256. An
existing JUCE 8.0.8 source tree can be supplied with `-DJUCE_SOURCE_DIR=/path/to/JUCE`.
The build has no post-build copy into your installed plugin directories.

For regression checks, configure a separate `build-linux-debug` with
`-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`, build it, then run
`xvfb-run -a ctest --test-dir build-linux-debug --output-on-failure`.
Use a fresh build directory for each metadata regression run; its generated
fixtures and isolated preferences are kept inside that directory.

A clean tagged source can be packaged with:

```sh
python3 packaging/linux/package.py --build-dir build-linux-release \
  --juce-dir build-linux-release/_deps/juce-src --tag v5.0.5-linux-preview.1
```

CI builds natively on Ubuntu 24.04 x64 and ARM64, launches the standalone in
Xvfb, and verifies archive contents, hashes, dependency resolution and legal
files. This does not establish real DAW keyboard routing, Bitwig drag behavior,
audio-device compatibility or GPU generation. Those are the preview's manual
test targets.
