include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# JUCE — pinned. Update deliberately, never floating.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

if(SOLAR42N_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.8.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
