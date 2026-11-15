# ============================================================================
# Synth Benchmarks - Performance testing for cross-platform comparison
# ============================================================================
#
# These benchmarks are designed to measure performance across different
# platforms (Windows, Linux, macOS) and hardware configurations.
#
# Run benchmarks:
#   cmake --build build --target run_benchmarks
#   ctest --test-dir build -V -L benchmark
#
# ============================================================================

cmake_minimum_required(VERSION 3.20)

# ============================================================================
# Benchmark Sources
# ============================================================================

set(BENCHMARK_SOURCES
    bench_effects_simd.cpp
    bench_oscillator_performance.cpp
    bench_lock_overhead.cpp
)

# ============================================================================
# Create benchmark executables
# ============================================================================

foreach(BENCH_SOURCE ${BENCHMARK_SOURCES})
    get_filename_component(BENCH_NAME ${BENCH_SOURCE} NAME_WE)
    
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${BENCH_SOURCE}")
        add_executable(${BENCH_NAME} ${BENCH_SOURCE})
        
        target_link_libraries(${BENCH_NAME} PRIVATE
            synth_core
            ${PLATFORM_LIBS}
        )
        
        target_include_directories(${BENCH_NAME} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/lib
            ${CMAKE_SOURCE_DIR}/src/examples
        )
        
        # MSVC warnings
        if(MSVC)
            target_compile_options(${BENCH_NAME} PRIVATE
                /wd4996
                /wd4267
            )
        endif()
        
        set_target_properties(${BENCH_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
        )
        
        # Register as CTest with "benchmark" label
        add_test(NAME ${BENCH_NAME} COMMAND ${BENCH_NAME})
        set_tests_properties(${BENCH_NAME} PROPERTIES LABELS "benchmark")
    endif()
endforeach()

# ============================================================================
# Convenience target to run all benchmarks
# ============================================================================

add_custom_target(run_benchmarks
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L benchmark -C $<CONFIG>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all benchmarks..."
)

# Add dependencies to ensure benchmarks are built
foreach(BENCH_SOURCE ${BENCHMARK_SOURCES})
    get_filename_component(BENCH_NAME ${BENCH_SOURCE} NAME_WE)
    if(TARGET ${BENCH_NAME})
        add_dependencies(run_benchmarks ${BENCH_NAME})
    endif()
endforeach()
