# LMMS Compatibility Notes

Status: not supported yet. This note documents what we tried so far so LMMS
users can help us find a better path.

## Test Environment

Tested on Windows with:

- gary4juce `4.0.0`
- JUCE `8.0.8`
- Visual Studio 2022 Community / MSBuild `17.14.40`
- LMMS `1.3.0-alpha.1.102+g89fc6c960`
- LMMS reported platform: `win32 x86_64, Qt 5.9.8, GCC 7.3-win32 20180312`
- LMMS install path: `C:\Program Files\LMMS`

The experiment work happened on a separate local branch/worktree:

```text
codex/lmms-experiments
C:\dev\gary4juce-lmms
```

## VST3

The current Windows release is a VST3 plugin. We did not find a working LMMS
path for loading that VST3 directly.

LMMS's Windows VST flow appears to be centered on legacy VST2-style `.dll`
plugins. In the tested install, the visible VST support came from:

```text
C:\Program Files\LMMS\plugins\vestige.dll
C:\Program Files\LMMS\plugins\vstbase.dll
C:\Program Files\LMMS\plugins\vsteffect.dll
C:\Program Files\LMMS\plugins\RemoteVstPlugin64.exe
```

## Legacy VST2

Projucer can generate a `VST (Legacy)` target for this project. Enabling that
format generated:

```text
Builds/VisualStudio2022/gary4juce_VST.vcxproj
JuceLibraryCode/include_juce_audio_plugin_client_VST2.cpp
```

The build failed on the expected missing VST2 SDK header:

```text
juce_audio_plugin_client_VST2.cpp(98,10): error C1083:
Cannot open include file: 'pluginterfaces/vst2.x/aeffect.h'
```

That means the project can ask JUCE for a VST2 target, but this machine does
not have the discontinued Steinberg VST2 SDK headers. We checked these public
GitHub source ZIPs on 2026-05-21:

```text
https://github.com/juce-framework/JUCE/archive/refs/tags/5.3.2.zip
https://github.com/steinbergmedia/vst3sdk/archive/refs/tags/vstsdk368_08_11_2017_build_121.zip
```

Neither ZIP contained `aeffect.h` or `aeffectx.h`. We do not want to rely on
random SDK mirrors for an open source build.

## LV2

LV2 was more promising from the gary4juce side. JUCE `8.0.8` includes LV2
support, and the experiment branch built a Windows LV2 bundle successfully.

The experiment branch enabled:

```text
buildVST3
buildAU
buildStandalone
buildLV2
```

It also set an explicit LV2 URI:

```text
https://thecollabagepatch.com/plugins/gary4juce
```

The successful output bundle was:

```text
Builds/VisualStudio2022/x64/Release/LV2 Plugin/gary4juce.lv2/
```

Bundle contents:

```text
dsp.ttl
gary4juce.dll
manifest.ttl
ui.ttl
```

Visual Studio also produced `.lib` and `.exp` linker byproducts in the bundle
directory.

Build commands used:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  .\Builds\VisualStudio2022\gary4juce_SharedCode.vcxproj `
  /p:Configuration=Release /p:Platform=x64 /m

& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  .\Builds\VisualStudio2022\gary4juce_LV2ManifestHelper.vcxproj `
  /p:Configuration=Release /p:Platform=x64 /m

& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  .\Builds\VisualStudio2022\gary4juce_LV2Plugin.vcxproj `
  /p:Configuration=Release /p:Platform=x64 /m
```

Use the amd64 MSBuild binary. The non-amd64 MSBuild binary failed JUCE's LV2
host check with:

```text
"x64" LV2 cross-compilation is not available on "x86" hosts.
```

## LMMS LV2 Test

We copied the whole LV2 bundle to:

```text
%APPDATA%\LV2\gary4juce.lv2
```

On the test machine:

```text
C:\Users\thegr\AppData\Roaming\LV2\gary4juce.lv2
```

Then we launched LMMS with an explicit LV2 search path:

```powershell
$env:LV2_PATH="$env:APPDATA\LV2;$env:COMMONPROGRAMFILES\LV2"
& "C:\Program Files\LMMS\lmms.exe"
```

LMMS did not show `gary4juce`.

This Windows alpha package did not appear to include native LMMS LV2 hosting:

- no obvious `lilv`, `lv2`, `suil`, `serd`, `sord`, or `sratom` DLLs in the
  LMMS install tree
- no LV2 browser/plugin module in `C:\Program Files\LMMS\plugins`
- `lmms.exe` did not contain expected LMMS LV2 debug strings such as
  `LMMS_LV2_DEBUG` or `Lv2 plugin SUMMARY`

## Carla Test

The LMMS install included Carla-related plugin DLLs:

```text
C:\Program Files\LMMS\plugins\carlarack.dll
C:\Program Files\LMMS\plugins\carlapatchbay.dll
C:\Program Files\LMMS\plugins\carlabase.dll
```

At first Carla Rack was not visible. `carlabase.dll` depends on `carla.dll`,
but the installer placed `carla.dll` under:

```text
C:\Program Files\LMMS\plugins\optional\
```

Launching LMMS with that optional directory on `PATH` made Carla Rack visible:

```powershell
$env:PATH="C:\Program Files\LMMS\plugins\optional;C:\Program Files\LMMS\plugins;$env:PATH"
$env:LV2_PATH="$env:APPDATA\LV2;$env:COMMONPROGRAMFILES\LV2"
& "C:\Program Files\LMMS\lmms.exe"
```

However, dragging Carla Rack into the LMMS project crashed before we could try
loading `gary4juce.lv2`. Windows reported:

```text
Faulting application name: lmms.exe
Faulting module name: carlabase.dll
Exception code: 0xc0000005
Fault offset: 0x0000000000001e41
```

This looks like an LMMS/Carla integration issue in that Windows alpha package,
not a gary4juce-specific LV2 failure.

## Current Read

So far:

- VST3 did not appear to be loadable in LMMS.
- Legacy VST2 is blocked by the discontinued VST2 SDK headers.
- gary4juce can build an LV2 bundle with JUCE `8.0.8`.
- The tested LMMS Windows alpha did not provide a working way to host that LV2
  bundle.

Useful next tests:

- Try `gary4juce.lv2` in an LMMS build that definitely has native LV2 support.
- Try on Linux, where LV2 support is more likely to be present and well-tested.
- Try a newer LMMS nightly/master Windows build if one includes LV2/lilv/suil.
- Try the generated LV2 bundle in standalone Carla outside LMMS.

If you have an LMMS setup where LV2 plugins are already working, please share
the LMMS version, OS, plugin install path, and any relevant `LV2_PATH` or Carla
setup details.
