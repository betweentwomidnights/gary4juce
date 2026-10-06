# Third-Party Notices

gary4juce is built with JUCE 8.0.8.

JUCE is Copyright (c) Raw Material Software Limited and is dual-licensed
under the GNU Affero General Public License version 3 (AGPLv3) and the
commercial JUCE license. This project uses JUCE under the AGPLv3 option.

- JUCE 8.0.8 source: https://github.com/juce-framework/JUCE/tree/8.0.8
- JUCE license and dependency notices:
  https://github.com/juce-framework/JUCE/blob/8.0.8/LICENSE.md
- GNU AGPLv3: https://www.gnu.org/licenses/agpl-3.0.en.html

JUCE modules include third-party components under their own licenses. The
authoritative notices and license paths for the version used by this project
are listed in the JUCE 8.0.8 license document linked above.

Binary release packages include the JUCE license and the applicable dependency
license texts in their `licenses` directory.

## YuE score compiler

The native MIDI score converter adapts the event compiler in YuE's
`skills/yue2-music/instrumental/scripts/compile_score.py` and its ABC helpers,
published by the Multimodal Art Projection project under Apache License 2.0.
gary4juce's C++ adaptation reads MIDI ticks instead of event lists and adds
validation for exact note/chord timing and clip lengths. No Python runtime is
bundled.

- Upstream source: https://github.com/multimodal-art-projection/YuE/tree/72272f907522dcca2e97c848d6c8f0d343999183/skills/yue2-music/instrumental/scripts
- License: https://github.com/multimodal-art-projection/YuE/blob/72272f907522dcca2e97c848d6c8f0d343999183/LICENSE

The Apache 2.0 license text is included as `YuE-Apache-2.0.txt` in binary
release packages.

## stems.cpp ABI header

`Source/Stems/libstems_v1.h` is vendored from stems.cpp v0.1.2 under the MIT
License, Copyright (c) 2026 ath (tinycrops). The upstream license also credits
portions Copyright (c) 2026 betweentwomidnights.

- Source: https://github.com/betweentwomidnights/stems.cpp/tree/v0.1.2
- License: https://github.com/betweentwomidnights/stems.cpp/blob/v0.1.2/LICENSE

The MIT license text is included as `stems.cpp-MIT.txt` in binary release
packages. The stems.cpp runtime and model weights download separately and are
not bundled in these archives.

The gary4local companion applications and installers, AI models and weights,
and backend or hosted services used with gary4juce are separate works and are
not licensed by this repository's AGPLv3 grant. Consult each separate project
or provider for its applicable terms. Model and service references are listed
in the gary4juce README.
