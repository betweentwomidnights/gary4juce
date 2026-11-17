#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Stage Linux preview ZIPs from a clean, tagged source and a Release build."""
import argparse
import hashlib
import json
import platform
import shutil
import subprocess
import zipfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--build-dir", type=Path, required=True)
parser.add_argument("--juce-dir", type=Path, required=True)
parser.add_argument("--tag", required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]


def git(*arguments):
    return subprocess.check_output(["git", *arguments], cwd=root, text=True).strip()


commit = git("rev-parse", "HEAD")
assert git("rev-parse", args.tag + "^{commit}") == commit
assert not git("status", "--porcelain"), "Package only clean tagged source"
arch = {"x86_64": "x64", "aarch64": "arm64"}[platform.machine()]
release = root / ".release"
stage = release / ("package-" + args.tag + "-" + arch)
stage.mkdir(parents=True, exist_ok=False)
artefacts = args.build_dir.resolve() / "gary4juce_artefacts/Release"
plugin = stage / "gary4juce.vst3"
shutil.copytree(artefacts / "VST3/gary4juce.vst3", plugin)
app = stage / "gary4juce-standalone"
app.mkdir()
shutil.copy2(artefacts / "Standalone/gary4juce", app)
juce = args.juce_dir.resolve()
license_paths = {
    "JUCE-LICENSE.md": "LICENSE.md",
    "FLAC-LICENSE.txt": "modules/juce_audio_formats/codecs/flac/Flac Licence.txt",
    "OGG-VORBIS-LICENSE.txt": "modules/juce_audio_formats/codecs/oggvorbis/Ogg Vorbis Licence.txt",
    "JPEG-LICENSE.txt": "modules/juce_graphics/image_formats/jpglib/README",
    "PNG-LICENSE.txt": "modules/juce_graphics/image_formats/pnglib/LICENSE",
    "ZLIB-LICENSE.txt": "modules/juce_core/zip/zlib/README",
    "HARFBUZZ-LICENSE.txt": "modules/juce_graphics/fonts/harfbuzz/COPYING",
    "SHEENBIDI-LICENSE.txt": "modules/juce_graphics/unicode/sheenbidi/LICENSE",
    "VST3-SDK-LICENSE.txt": "modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt",
    "VST3-BASE-BSD-LICENSE.txt": "modules/juce_audio_processors/format_types/VST3_SDK/base/LICENSE.txt",
    "VST3-PUBLIC-SDK-BSD-LICENSE.txt": "modules/juce_audio_processors/format_types/VST3_SDK/public.sdk/LICENSE.txt",
}
source = f"""# Corresponding Source

gary4juce {args.tag}, numeric plugin version 5.0.5, source commit {commit}.

- Source: https://github.com/betweentwomidnights/gary4juce/tree/{args.tag}
- Archive: https://github.com/betweentwomidnights/gary4juce/archive/refs/tags/{args.tag}.zip
- Build: https://github.com/betweentwomidnights/gary4juce/blob/{args.tag}/docs/LINUX_PREVIEW.md
- JUCE 8.0.8: https://github.com/juce-framework/JUCE/tree/d6181bde38d858c283c3b7bf699ce6340c050b5d
- JUCE license: https://github.com/juce-framework/JUCE/blob/8.0.8/LICENSE.md

Source is available at no charge. See THIRD_PARTY_NOTICES.md and licenses/.
Models, GPU drivers and gary4local runtimes are separate downloads.
"""
for legal in (plugin / "Contents/Resources/Legal", app / "Legal"):
    licenses = legal / "licenses"
    licenses.mkdir(parents=True, exist_ok=True)
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.md"):
        shutil.copy2(root / name, legal / name)
    (legal / "SOURCE.md").write_text(source)
    for name, path in license_paths.items():
        shutil.copy2(juce / path, licenses / name)
    for path in (root / "packaging/linux/licenses").iterdir():
        shutil.copy2(path, licenses / path.name)
    assert len(list(licenses.iterdir())) == 14
    assert all(p.stat().st_size > 0 for p in legal.rglob("*") if p.is_file())
shutil.copy2(root / "docs/LINUX_PREVIEW.md", app / "README.md")
module = plugin / "Contents/Resources/moduleinfo.json"
# JUCE's moduleinfo JSON permits trailing commas.
import re
info = json.loads(re.sub(r",(\s*[}\]])", r"\1", module.read_text()))
assert info["Version"] == "5.0.5"
binary = plugin / f"Contents/{platform.machine()}-linux/gary4juce.so"
for executable in (binary, app / "gary4juce"):
    assert executable.stat().st_mode & 0o111
    dependencies = subprocess.check_output(["ldd", str(executable)], text=True)
    assert "not found" not in dependencies, dependencies

hashes = {}
for kind, bundle in (("vst3", plugin), ("standalone", app)):
    archive = release / f"gary4juce-{args.tag}-{arch}-{kind}.zip"
    with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED) as output:
        for path in sorted(bundle.rglob("*")):
            if path.is_file():
                output.write(path, path.relative_to(stage))
    verify = release / f"verify-{args.tag}-{arch}-{kind}"
    with zipfile.ZipFile(archive) as output:
        assert all(p.startswith(bundle.name + "/") for p in output.namelist())
        output.extractall(verify)
        for entry in output.infolist():
            extracted = verify / entry.filename
            extracted.chmod((entry.external_attr >> 16) & 0o777)
    for path in bundle.rglob("*"):
        if path.is_file():
            assert path.read_bytes() == (verify / path.relative_to(stage)).read_bytes()
    hashes[archive.name] = hashlib.sha256(archive.read_bytes()).hexdigest()
subprocess.run(["xvfb-run", "-a", "python3", str(root / "tests/linux/with_window_manager.py"),
                "python3", str(root / "tests/linux/standalone_smoke.py"),
                str(release / f"verify-{args.tag}-{arch}-standalone/gary4juce-standalone/gary4juce")], check=True)
checksums = release / f"SHA256SUMS-linux-{arch}.txt"
checksums.write_text("".join(f"{value}  {name}\n" for name, value in hashes.items()))
build_info = {"tag": args.tag, "source_commit": commit, "architecture": arch,
              "target": "Ubuntu 24.04", "sha256": hashes,
              "compiler": subprocess.check_output(["c++", "--version"], text=True).splitlines()[0]}
(release / f"BUILD-INFO-linux-{arch}.json").write_text(json.dumps(build_info, indent=2) + "\n")
print(json.dumps(build_info, indent=2))
