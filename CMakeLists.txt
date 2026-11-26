cmake_minimum_required(VERSION 3.22)
project(SoulChord VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local checkout if JUCE_DIR is given, otherwise download it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(SoulChord
    COMPANY_NAME "Even"
    PLUGIN_MANUFACTURER_CODE EvNg
    PLUGIN_CODE SChd
    PRODUCT_NAME "SoulChord"
    VERSION 1.0.0
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    FORMATS VST3 Standalone)

target_sources(SoulChord PRIVATE
    Source/Theory.cpp
    Source/Arranger.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SoulChord PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(MSVC)
    target_compile_options(SoulChord PRIVATE /utf-8)
endif()

target_link_libraries(SoulChord
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(SOULCHORD_TESTS "Build headless tests" OFF)
if(SOULCHORD_TESTS)
    add_executable(theory_test tests/theory_test.cpp Source/Theory.cpp Source/Arranger.cpp)
    target_include_directories(theory_test PRIVATE Source)
endif()

if(SOULCHORD_TESTS)
    juce_add_console_app(render_test PRODUCT_NAME "render_test")
    target_sources(render_test PRIVATE
        tests/render_test.cpp
        Source/Theory.cpp
        Source/Arranger.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(render_test PRIVATE
        JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(render_test PRIVATE
        juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats
        juce::juce_recommended_config_flags)
endif()
