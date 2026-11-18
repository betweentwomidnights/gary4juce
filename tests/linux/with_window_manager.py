#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Run a desktop check in an existing isolated X server with a real WM."""
import subprocess
import sys
import time

manager = subprocess.Popen(["openbox"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for attempt in range(40):
        probe = subprocess.run(["xprop", "-root", "_NET_SUPPORTING_WM_CHECK"], capture_output=True, text=True)
        if "0x" in probe.stdout:
            break
        if manager.poll() is not None:
            raise RuntimeError("Window manager exited before desktop checks")
        time.sleep(0.1)
    else:
        raise RuntimeError("Window manager did not initialize its X11 properties")
    result = subprocess.run(sys.argv[1:])
finally:
    manager.terminate()
    try:
        manager.wait(timeout=5)
    except subprocess.TimeoutExpired:
        manager.kill()
        manager.wait(timeout=5)
sys.exit(result.returncode)
