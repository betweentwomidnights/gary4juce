# SPDX-License-Identifier: AGPL-3.0-only
"""Fail when gary4juce opens a request anywhere but ServiceRequests.

A pairing token for gary4local's listener reaches a request only if it opens through
ServiceRequests::open, or takes its headers from ServiceRequests::headersFor. A new
endpoint that calls URL::createInputStream directly would work on this computer and
fail with 401 on someone's LAN, so this check stops it at review instead.

Run: python tests/service_requests/check_call_sites.py
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Source"

# Opens that are not gary4local services, each with the reason it may stay direct. The count
# must match exactly, so a new direct open in one of these files still fails.
ALLOWED = {
    "Utils/ServiceRequests.h": (1, "the one place service requests open"),
    "PluginEditor.Storage.cpp": (3, "local files"),
    "Utils/EmbeddedAudioMetadata.h": (2, "local files"),
    "PluginEditor.Updates.cpp": (1, "the GitHub update manifest"),
    "Stems/StemsRuntime.cpp": (2, "a local file and Hugging Face model downloads"),
}

CREATE = re.compile(r"\.createInputStream\s*\(")
WEB = re.compile(r"\bWebInputStream\s*>\s*\(|\bWebInputStream\s+\w+\s*\(")
SOCKET = re.compile(r"\bStreamingSocket\s+\w+\s*;")


def code_lines(text):
    for number, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        yield number, line.split("//", 1)[0]


def main():
    failures = []
    seen = {}
    for path in sorted(SOURCE.rglob("*")):
        if path.suffix not in {".cpp", ".h", ".mm"}:
            continue
        relative = path.relative_to(SOURCE).as_posix()
        lines = list(code_lines(path.read_text(encoding="utf-8", errors="replace")))
        text = [line for _, line in lines]
        for index, (number, line) in enumerate(lines):
            if CREATE.search(line):
                seen[relative] = seen.get(relative, 0) + 1
            nearby = "\n".join(text[index:index + 4])
            if WEB.search(line) and "ServiceRequests::headersFor" not in nearby:
                failures.append(f"{relative}:{number}: a WebInputStream without ServiceRequests::headersFor")
            if SOCKET.search(line):
                block = "\n".join(text[index:index + 40])
                if "ServiceRequests::headersFor" not in block:
                    failures.append(f"{relative}:{number}: a raw socket request without ServiceRequests::headersFor")

    for relative, count in sorted(seen.items()):
        allowed, reason = ALLOWED.get(relative, (0, ""))
        if count != allowed:
            failures.append(
                f"{relative}: {count} direct createInputStream call(s), {allowed} allowed"
                + (f" ({reason})" if reason else "")
                + ". Open service requests with ServiceRequests::open.")
    for relative, (allowed, reason) in sorted(ALLOWED.items()):
        if relative not in seen:
            failures.append(f"{relative}: expected {allowed} direct call(s) ({reason}), found none; update ALLOWED")

    if failures:
        print("\n".join(failures))
        return 1
    print(f"ok: every service request opens through ServiceRequests ({sum(seen.values())} allowed direct opens)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
