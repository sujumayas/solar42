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
add_library(solar42n_fv1 STATIC
    src/fv1/Fv1Vm.cpp
)
target_link_libraries(solar42n_fv1 PUBLIC solar42n_dsp)

# ---------------------------------------------------------------- engine (JUCE-free)
add_library(solar42n_engine STATIC
    src/engine/Rack.cpp
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
    )
    target_link_libraries(solar42n_tests PRIVATE solar42n_engine Catch2::Catch2WithMain)
    include(Catch)
    catch_discover_tests(solar42n_tests)
endif()

# ---------------------------------------------------------------- tools
if(SOLAR42N_BUILD_TOOLS)
    add_executable(fv1asm tools/fv1asm/Main.cpp)
    target_link_libraries(fv1asm PRIVATE solar42n_fv1)

    add_executable(solar42n_render tools/render/Main.cpp)
    target_link_libraries(solar42n_render PRIVATE solar42n_engine)
endif()
