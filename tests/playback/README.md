# Playback source regression

The test drives the real editor and processor, with explicit audio callbacks and
isolated temporary storage. It starts output playback without a callback, then
switches to stem preview, saved-buffer playback or seeking. A callback for the
new source must not activate the output play button or move its cursor. Normal
output playback must still confirm when the host renders it.

Windows: build the Debug Shared Code target after a Projucer resave, then run:

```bat
tests\playback\build-playback-test.cmd C:\temp\gary-playback-test
```

Linux: configure a Debug CMake build with `BUILD_TESTING=ON`, build
`gary_playback_state_test`, then run `xvfb-run -a ctest --test-dir BUILD_DIR -R
playback_state --output-on-failure`. Linux CI runs this alongside metadata checks.

For the MacBook check, rebuild PR #16 and test the output, saved-buffer and stem
players. If the host is withholding callbacks, request output playback, then
preview a stem or play/seek the saved buffer before output starts. When the host
resumes processing, only the selected source should show as playing, and the
output cursor should stay stopped. Also check returning to output, pause/resume,
and GarageBand master-track playback. No change to the infinite tail is involved.
