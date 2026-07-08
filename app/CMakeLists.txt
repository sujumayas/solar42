cmake_minimum_required(VERSION 3.25)

project(Solar42N VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

option(SOLAR42N_BUILD_TESTS "Build unit tests" ON)
option(SOLAR42N_BUILD_TOOLS "Build fv1asm / render CLI tools" ON)

include(cmake/Dependencies.cmake)

# ---------------------------------------------------------------- dsp (JUCE-free)
add_library(solar42n_dsp STATIC
    src/dsp/Dsp.cpp
)
target_include_directories(solar42n_dsp PUBLIC src)
target_compile_features(solar42n_dsp PUBLIC cxx_std_20)

# ---------------------------------------------------------------- fv1 (JUCE-free)
file(GLOB_RECURSE S42_SPN_SOURCES CONFIGURE_DEPENDS programs/*.spn)
set(S42_PROGRAMS_INC ${CMAKE_CURRENT_BINARY_DIR}/generated/CartridgePrograms.inc)
add_custom_command(
    OUTPUT ${S42_PROGRAMS_INC}
    COMMAND ${CMAKE_COMMAND}
        -DPROGRAM_DIR=${CMAKE_CURRENT_SOURCE_DIR}/programs
        -DOUT=${S42_PROGRAMS_INC}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedPrograms.cmake
    DEPENDS ${S42_SPN_SOURCES} cmake/EmbedPrograms.cmake
    COMMENT "Embedding FV-1 cartridge programs"
)

add_library(solar42n_fv1 STATIC
    src/fv1/Fv1Vm.cpp
    src/fv1/SpinAsm.cpp
    src/fv1/CartridgeLibrary.cpp
    ${S42_PROGRAMS_INC}
)
target_include_directories(solar42n_fv1 PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(solar42n_fv1 PUBLIC solar42n_dsp)

# ---------------------------------------------------------------- engine (JUCE-free)
add_library(solar42n_engine STATIC
    src/engine/Rack.cpp
    src/engine/keyboard/KbMenu.cpp
    src/engine/keyboard/TouchKeyboard.cpp
    src/engine/modules/ClassicDroneVoice.cpp
    src/engine/modules/PapaSrapaVoice.cpp
    src/engine/modules/VcoVoice.cpp
)
target_link_libraries(solar42n_engine PUBLIC solar42n_dsp solar42n_fv1)

# ---------------------------------------------------------------- plugin
juce_add_plugin(Solar42N
    PRODUCT_NAME "Solar42N"
    COMPANY_NAME "Solar42 Project"
    BUNDLE_ID com.solar42project.solar42n
    PLUGIN_MANUFACTURER_CODE S42p
    PLUGIN_CODE S42n
    FORMATS Standalone AU VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Solar 42N uses the audio input for its preamp / envelope follower."
    # The standalone's MIDI-device scan touches Bluetooth; without this usage
    # string macOS TCC kills the app (SIGABRT, seen 2026-07-04).
    BLUETOOTH_PERMISSION_ENABLED TRUE
    BLUETOOTH_PERMISSION_TEXT "Solar 42N scans for Bluetooth MIDI controllers."
)

target_sources(Solar42N PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
)

target_compile_definitions(Solar42N PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Solar42N
    PRIVATE
        solar42n_engine
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------- tests
if(SOLAR42N_BUILD_TESTS)
    enable_testing()
    add_executable(solar42n_tests
        tests/TestSanity.cpp
        tests/TestDsp.cpp
        tests/TestDroneVoice.cpp
        tests/TestRouting.cpp
        tests/TestM3Voices.cpp
        tests/TestPolivoks.cpp
        tests/TestFv1.cpp
        tests/TestM5Panel.cpp
        tests/TestKeyboard.cpp
        tests/TestMidiPerformer.cpp
        tests/TestMidiClock.cpp
        tests/TestRtSafety.cpp
    )
    target_link_libraries(solar42n_tests PRIVATE solar42n_engine Catch2::Catch2WithMain)
    target_compile_definitions(solar42n_tests PRIVATE
        S42_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    include(Catch)
    catch_discover_tests(solar42n_tests)
endif()

# ---------------------------------------------------------------- tools
if(SOLAR42N_BUILD_TOOLS)
    add_executable(fv1asm tools/fv1asm/Main.cpp)
    target_link_libraries(fv1asm PRIVATE solar42n_fv1)

    add_executable(solar42n_render tools/render/Main.cpp)
    target_link_libraries(solar42n_render PRIVATE solar42n_engine)

    # M7 verify harness: full-state round trip / factory presets / click-free
    # switch against the REAL processor. Also renders preset audition WAVs.
    juce_add_console_app(solar42n_statecheck PRODUCT_NAME "solar42n_statecheck")
    target_sources(solar42n_statecheck PRIVATE
        tools/statecheck/Main.cpp
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
    )
    target_include_directories(solar42n_statecheck PRIVATE src)
    target_compile_definitions(solar42n_statecheck PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
    )
    target_link_libraries(solar42n_statecheck PRIVATE
        solar42n_engine
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
    )
    if(SOLAR42N_BUILD_TESTS)
        add_test(NAME statecheck COMMAND solar42n_statecheck)
    endif()

    # M8 calibration kit: renders one focused audition WAV per tunable domain
    # of TuningConstants.h through the real processor (09-calibration-protocol.md).
    juce_add_console_app(solar42n_calib PRODUCT_NAME "solar42n_calib")
    target_sources(solar42n_calib PRIVATE
        tools/calib/Main.cpp
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
    )
    target_include_directories(solar42n_calib PRIVATE src)
    target_compile_definitions(solar42n_calib PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
    )
    target_link_libraries(solar42n_calib PRIVATE
        solar42n_engine
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
    )
endif()
