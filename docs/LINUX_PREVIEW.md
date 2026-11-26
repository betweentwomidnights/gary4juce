# Linux desktop preview

Linux support lives on `main` with the Windows and macOS source. The experimental
Ubuntu 24.04 build is for testing with
[gary4local v0.7.6](https://github.com/betweentwomidnights/gary-localhost-installer/releases/tag/v0.7.6). Packages are available for x64 Linux PCs and ARM64 systems such
as DGX Spark. GPU drivers,
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

The shared source includes the current models, Carey job API, Yuey score
and MIDI workflows, SA3 LoRA lanes, saved recording-buffer lane, improved
storage, and optional JSON or embedded audio metadata. The full intervening
history is in [CHANGELOG.md](CHANGELOG.md).

Linux CMake builds request keyboard focus so hosts can send typing to prompt
boxes. Windows and macOS keep their existing Projucer keyboard-focus setting.
Linux external audio drags request a copy and retain their file list through the
callback; Windows and macOS keep their existing drag policy. The **copy** button beside output saves an export in
`dragged_audio` and reveals it in the file manager, for dragging through your
DAW's browser or file manager when direct plugin dragging is unavailable.
It uses the selected WAV/FLAC and generation-details settings.

Built-in stem separation downloads the pinned stems.cpp v0.1.7 core and Vulkan
packages for x64 or ARM64 from **gary settings > stem separator**. Models are
separate downloads in the same panel. Separation runs inside gary4juce; it does
not need gary4local or Python. The CPU works without a GPU. Vulkan acceleration
needs the system Vulkan loader (`libvulkan1` on Ubuntu) and a working GPU driver;
without a supported device the runtime falls back to the CPU.

These runtime packages require GLIBC 2.38 and GLIBCXX 3.4.32 or newer, available
on Ubuntu 24.04. Real desktop/DAW behavior and GPU separation still need hardware
validation. Generation backends are provided by gary4local or the remote service
as on the other platforms.

The updater reads `preview-linux.json`, published from `main`. Windows and macOS
stable feeds stay separate. CMake reads the numeric plugin version from
`gary4juce.jucer`; local builds default to `<version>-linux-preview.1`, and release
builds can set `GARY4JUCE_RELEASE_VERSION` to the full preview version. The current
published Linux preview is `5.0.8-linux-preview.1`, paired with gary4local v0.7.7.

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

A clean tagged source can be packaged with (the tag must match the version in
`gary4juce.jucer`):

```sh
python3 packaging/linux/package.py --build-dir build-linux-release \
  --juce-dir build-linux-release/_deps/juce-src --tag v5.0.8-linux-preview.1
```

For the offline stems integration test, put the matching pinned core/Vulkan ZIPs
and `uvr_denoise_lite-4M-v1.0-F32.gguf` in a fixture directory, then configure with
`-DBUILD_TESTING=ON -DGARY4JUCE_STEMS_TEST_FIXTURES=/absolute/path/to/fixtures`.
Build `gary_stems_runtime_test` and run `ctest --test-dir build-linux-release -R stems_runtime --output-on-failure`.
It checks the fixtures against the compiled package/model hashes, rejects a
corrupted ZIP, installs into temporary storage and separates on the CPU through
the real StemsService from outside the runtime folder. It leaves user data alone.

CI checks `main` and pull requests natively on Ubuntu 24.04 x64 and ARM64, verifies
stems installation and CPU separation on both, launches the standalone in Xvfb,
and runs the metadata regression suite on x64. Linux preview tags also
verify archive contents, hashes, dependency resolution and legal files. This does not establish real DAW keyboard routing, Bitwig drag behavior,
audio-device compatibility or GPU generation. Those are the preview's manual
test targets.
