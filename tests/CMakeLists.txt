# ============================================================================
# Synth Unit Tests - using doctest framework
# ============================================================================
# 
# Usage:
#   cmake --build build --target run_tests      # Build and run all tests
#   ctest --test-dir build -V                    # Run via CTest with verbose
#   ctest --test-dir build -R "BiquadFilter"     # Run tests matching pattern
#
# ============================================================================

cmake_minimum_required(VERSION 3.20)

# Fetch doctest automatically (header-only)
# Using master branch for CMake 4.x compatibility
include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(doctest)

# ============================================================================
# Test executable: synth_core_tests
# ============================================================================
add_executable(synth_core_tests
    test_main.cpp
    test_synth_core.cpp
    test_oscillator.cpp
    test_voice.cpp
    test_presets.cpp
    test_drums.cpp
    test_cc_overlay.cpp
    test_ct1.cpp
    test_obs_one.cpp
)

target_link_libraries(synth_core_tests PRIVATE
    synth_core
    doctest::doctest
)

target_include_directories(synth_core_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib
)

# the wavetable banks, for the presets that read them
target_compile_definitions(synth_core_tests PRIVATE SYNTH_PCM_DIR="${CMAKE_SOURCE_DIR}/pcm")

# MSVC: disable specific warnings for doctest
if(MSVC)
    target_compile_options(synth_core_tests PRIVATE
        /wd4996  # deprecated functions
        /wd4267  # size_t conversion
    )
endif()

# ============================================================================
# CTest Integration
# ============================================================================
include(CTest)

# Register test suites with CTest
# Each TEST_CASE becomes a separate CTest entry when using doctest's test discovery

# Method 1: Single test executable (simpler, faster)
add_test(NAME synth_core_tests 
    COMMAND synth_core_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Method 2: Individual test cases (optional, more granular)
# Requires doctest test discovery - enable if needed:
# include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
# doctest_discover_tests(synth_core_tests)

# ============================================================================
# Convenience target: run_tests
# ============================================================================
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>
    DEPENDS synth_core_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all unit tests..."
)

# ============================================================================
# Output location
# ============================================================================
set_target_properties(synth_core_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
