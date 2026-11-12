@echo off
rem Builds and runs models_test.cpp against the plugin's Debug shared-code library. Build the
rem library first: MSBuild Builds\VisualStudio2022\gary4juce_SharedCode.vcxproj /p:Configuration=Debug /p:Platform=x64
rem Downloads the pinned stems.cpp runtime from GitHub, and any model not in the seed folder from Hugging Face.
rem Usage: build-models-test.cmd test-folder input.wav [seed-models-folder]
setlocal enabledelayedexpansion
call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

set "HERE=%~dp0"
set "ROOT=%HERE%..\.."
set "OUT=%TEMP%\gary-stems-models-test"
if not exist "%OUT%" mkdir "%OUT%"
rem The editor checks only run against an isolated storage folder, never the user's session.
set "GARY4JUCE_STORAGE_TEST_DIRECTORY=%~1"

set "DEFS=/D_DEBUG /DDEBUG /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS"
set "DEFS=%DEFS% /DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 /DJUCE_STRICT_REFCOUNTEDPOINTER=1"
set "DEFS=%DEFS% /DJUCE_STANDALONE_APPLICATION=1 /DJUCE_VST3_CAN_REPLACE_VST2=0"
for %%M in (juce_audio_basics juce_audio_devices juce_audio_formats juce_audio_processors juce_audio_utils juce_core juce_cryptography juce_data_structures juce_events juce_graphics juce_gui_basics juce_gui_extra) do (
    set "DEFS=!DEFS! /DJUCE_MODULE_AVAILABLE_%%M=1"
)

cl /nologo /MDd /std:c++17 /EHsc /Zc:__cplusplus /W3 %DEFS% ^
   /I"%ROOT%\JuceLibraryCode" /I"%ROOT%\..\JUCE\modules" ^
   /Fo"%OUT%\\" /Fe"%OUT%\models_test.exe" "%HERE%models_test.cpp" ^
   "%ROOT%\Builds\VisualStudio2022\x64\Debug\Shared Code\gary4juce.lib" ^
   /link /SUBSYSTEM:CONSOLE kernel32.lib user32.lib gdi32.lib winmm.lib comdlg32.lib advapi32.lib ^
   shell32.lib ole32.lib oleaut32.lib uuid.lib wininet.lib ws2_32.lib version.lib imm32.lib ^
   rpcrt4.lib shlwapi.lib dxgi.lib d2d1.lib dwrite.lib windowscodecs.lib setupapi.lib
if errorlevel 1 exit /b 1

"%OUT%\models_test.exe" "%~1" "%~2" "%~3"
exit /b %errorlevel%
