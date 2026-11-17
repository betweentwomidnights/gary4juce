#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

binary = Path(sys.argv[1]).resolve()
process = subprocess.Popen([str(binary)], start_new_session=True)
try:
    time.sleep(8)
    assert process.poll() is None, f"Standalone exited with {process.returncode}"
    windows = subprocess.check_output(["xwininfo", "-root", "-tree"], text=True)
    assert "gary4juce" in windows.lower(), windows
    print("Standalone remained running and created its desktop window in Xvfb")
finally:
    if process.poll() is None:
        os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait(timeout=5)
