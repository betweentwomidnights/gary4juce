cmake_minimum_required(VERSION 3.22)
# Projucer remains the version source for every platform.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/gary4juce.jucer")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/gary4juce.jucer" gary_project)
string(REGEX MATCH "<JUCERPROJECT[^>]*version=\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" gary_version_match "${gary_project}")
if(NOT gary_version_match)
    message(FATAL_ERROR "Cannot read the numeric plugin version from gary4juce.jucer")
endif()
project(gary4juce VERSION "${CMAKE_MATCH_1}" LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "This CMake entry point builds the Linux preview; use Projucer for Windows/macOS.")
endif()
set(GARY4JUCE_RELEASE_VERSION "" CACHE STRING "Linux preview updater version (defaults to the project version)")
if(NOT GARY4JUCE_RELEASE_VERSION)
    set(GARY4JUCE_RELEASE_VERSION "${PROJECT_VERSION}-linux-preview.1")
endif()
string(FIND "${GARY4JUCE_RELEASE_VERSION}" "${PROJECT_VERSION}-linux-preview." gary_release_prefix)
if(NOT gary_release_prefix EQUAL 0)
    message(FATAL_ERROR "Linux preview version must match project version ${PROJECT_VERSION}")
endif()
set(JUCE_SOURCE_DIR "" CACHE PATH "JUCE 8.0.8 source directory (optional)")
if(JUCE_SOURCE_DIR)
    add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        URL https://codeload.github.com/juce-framework/JUCE/tar.gz/d6181bde38d858c283c3b7bf699ce6340c050b5d
        URL_HASH SHA256=ab02a58f4cc0e42102a22e863e8bbe3aaa1c3e72899fd567154a5a1aa7898219)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(gary4juce
    COMPANY_NAME "the collabage patch"
    COMPANY_WEBSITE "https://thecollabagepatch.com"
    BUNDLE_ID com.thecollabagepatch.gary4juce
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Xmho
    PRODUCT_NAME gary4juce
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_CURL TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    FORMATS VST3 Standalone)
juce_add_binary_data(gary4juce_assets SOURCES
    gary4live_logo.png
)
juce_generate_juce_header(gary4juce)
target_sources(gary4juce PRIVATE
    Source/Components/AudioSelectionDialog.cpp
    Source/Components/Terry/TerryUI.cpp
    Source/Components/Base/CustomButton.cpp
    Source/Components/Base/CustomComboBox.cpp
    Source/Components/Base/CustomSlider.cpp
    Source/Components/Base/CustomTextEditor.cpp
    Source/Components/Darius/DariusUI.cpp
    Source/Components/Darius/MagentaPrompts.cpp
    Source/Components/Gary/GaryUI.cpp
    Source/Components/Carey/CareyUI.cpp
    Source/Components/Yuey/YueyUI.cpp
    Source/Components/Yuey/YueyMidiSlot.cpp
    Source/Components/Foundation/FoundationUI.cpp
    Source/Components/Stems/StemsDialog.cpp
    Source/Components/Stems/StemsSettings.cpp
    Source/Components/Jerry/BeatPrompts.cpp
    Source/Components/Jerry/InstrumentPrompts.cpp
    Source/Components/Jerry/SA3UI.cpp
    Source/Components/Jerry/SA3Lanes.cpp
    Source/Components/Jerry/SA3LaneEditor.cpp
    Source/Components/Jerry/JerryUI.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PluginEditor.Carey.cpp
    Source/PluginEditor.Yuey.cpp
    Source/PluginEditor.Jerry.cpp
    Source/PluginEditor.Terry.cpp
    Source/PluginEditor.Backend.cpp
    Source/PluginEditor.Updates.cpp
    Source/PluginEditor.Storage.cpp
    Source/PluginEditor.Foundation.cpp
    Source/PluginEditor.Stems.cpp
    Source/Stems/StemsRuntime.cpp
    Source/Yuey/YueyMidiScore.cpp
    Source/Yuey/YueyMidiImport.cpp
    Source/Utils/CustomLookAndFeel.cpp
    Source/Utils/IconFactory.cpp
)
target_compile_features(gary4juce PUBLIC cxx_std_17)
target_compile_definitions(gary4juce PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1 JUCE_VST3_CAN_REPLACE_VST2=0
    GARY4JUCE_UPDATE_VERSION="${GARY4JUCE_RELEASE_VERSION}")
target_link_libraries(gary4juce PRIVATE
    juce::juce_audio_utils juce::juce_cryptography gary4juce_assets
    PUBLIC juce::juce_recommended_config_flags)

include(CTest)
if(BUILD_TESTING)
    # The real stems installer/inference path, without building or launching the editor.
    juce_add_console_app(gary_stems_runtime_test PRODUCT_NAME "gary stems runtime test" NEEDS_CURL TRUE)
    juce_generate_juce_header(gary_stems_runtime_test)
    target_sources(gary_stems_runtime_test PRIVATE tests/linux/stems_runtime_test.cpp
        Source/Stems/StemsRuntime.cpp)
    target_compile_features(gary_stems_runtime_test PRIVATE cxx_std_17)
    target_compile_definitions(gary_stems_runtime_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=1)
    target_link_libraries(gary_stems_runtime_test PRIVATE juce::juce_audio_formats
        juce::juce_cryptography juce::juce_recommended_config_flags)
    set(GARY4JUCE_STEMS_TEST_FIXTURES "" CACHE PATH "Verified stems ZIPs and denoise-lite GGUF for the offline runtime test")
    if(GARY4JUCE_STEMS_TEST_FIXTURES)
        add_test(NAME stems_runtime COMMAND gary_stems_runtime_test "${GARY4JUCE_STEMS_TEST_FIXTURES}")
        set_tests_properties(stems_runtime PROPERTIES TIMEOUT 180)
    endif()

    add_executable(gary_output_metadata_test tests/output_metadata/metadata_test.cpp)
    target_link_libraries(gary_output_metadata_test PRIVATE gary4juce
        juce::juce_audio_utils juce::juce_cryptography gary4juce_assets)
    target_include_directories(gary_output_metadata_test PRIVATE
        "${CMAKE_BINARY_DIR}/gary4juce_artefacts/JuceLibraryCode")
    # The storage isolation override is deliberately available only in Debug.
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_test(NAME output_metadata COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/linux/with_window_manager.py"
            "$<TARGET_FILE:gary_output_metadata_test>"
            "${CMAKE_BINARY_DIR}/metadata-test-data")
        set_tests_properties(output_metadata PROPERTIES ENVIRONMENT
            "GARY4JUCE_STORAGE_TEST_DIRECTORY=${CMAKE_BINARY_DIR}/metadata-test-data")
    endif()
endif()
