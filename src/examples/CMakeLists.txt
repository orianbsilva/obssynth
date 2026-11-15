# Examples CMakeLists.txt

# Create platform utilities library for examples
add_library(example_platform_utils STATIC
    platform_utils.cpp
    platform_utils.hpp
)
target_link_libraries(example_platform_utils PRIVATE ${PLATFORM_LIBS})
target_include_directories(example_platform_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(example_platform_utils PUBLIC ../lib)

# Define platform-specific and cross-platform example executables
set(COMMON_EXAMPLES
    simple_wav_example
    test_a4
    audio_backend_comparison
    test_performance_comparison
    test_voice_crossfade
    test_simd_detection
    test_effects_simd
    test_lockfree_experiment
    test_lock_overhead
    test_adsr_polyphonic
    test_dry_wet_mix
    test_adsr_release
    test_repeated_notes
    test_chord_clicks
    test_all_waveshapes
    test_fm_synthesis
    test_audio_simple
    test_stereo_demo
    test_synth_core
    # test_midi_reverb
    # test_midi_polyphonic
    # test_reverb_demo
    # test_polyphonic_midi
    # test_polyphonic_demo
    # test_adsr_demo
    # test_wave_print
    # test_logger
    # simple_logger_test
    # debug_logger_test
    # synth_with_logger
    # test_threaded_audio
    # test_oscillator_continuity
    # test_keyboard_midi_simple
    # test_channel_aftertouch
    # test_channel_aftertouch_parsing
    # test_polyphonic_aftertouch
    # benchmark_frequency_conversion
    # patch_demo
    # test_launchpad
    # test_launchpadx_device
    # test_launchpadx_session_mode
    # test_launchpadx_midi_output
    # test_launchkey_mk4_device
    test_launchkey_mk4_probing
    ct1_proof
    # test_apc_key25_mk2
    # test_apc_key25_mk1
    # test_apc_key25_mk1_updated
    # simple_midi_monitor
    # test_chorus
    # test_overdrive
    # test_device_factory
)

# Windows-specific examples (vst_host_test requires windows.h)
if(WIN32)
    set(WINDOWS_EXAMPLES
        vst_host_test
        test_directsound
        test_buffer_timing
        # test_asio
    )
    list(APPEND COMMON_EXAMPLES ${WINDOWS_EXAMPLES})
endif()

# Set the final examples list
set(EXAMPLES ${COMMON_EXAMPLES})

# Create executables for each example
foreach(EXAMPLE ${EXAMPLES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE}.cpp")
        add_executable(${EXAMPLE} ${EXAMPLE}.cpp)
        target_link_libraries(${EXAMPLE} synth_core example_platform_utils ${PLATFORM_LIBS})
        target_include_directories(${EXAMPLE} PRIVATE ../lib)

        # Set properties
        set_target_properties(${EXAMPLE} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )

        # Install the executable
        install(TARGETS ${EXAMPLE}
            RUNTIME DESTINATION bin
        )
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE}.c")
        # For any remaining C files, we'll compile them as C++
        add_executable(${EXAMPLE} ${EXAMPLE}.c)
        target_link_libraries(${EXAMPLE} synth_core ${PLATFORM_LIBS})

        # Set properties
        set_target_properties(${EXAMPLE} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )

        # Compile C files as C++
        set_source_files_properties(${EXAMPLE}.c PROPERTIES LANGUAGE CXX)

        # Install the executable
        install(TARGETS ${EXAMPLE}
            RUNTIME DESTINATION bin
        )
    endif()
endforeach()

# Register tests (only if BUILD_TESTING enabled)
if(BUILD_TESTING)
    if(TARGET test_oscillator_continuity)
        add_test(NAME oscillator_continuity COMMAND test_oscillator_continuity)
    endif()
endif()

# VST3 GUI Host (requires VST3 SDK headers)
set(VST3SDK_PATH "C:/Users/orian/dev/VST_SDK/vst3sdk")
if(EXISTS "${VST3SDK_PATH}")
    message(STATUS "Building VST3 GUI Host with SDK from ${VST3SDK_PATH}")
    
    add_executable(vst_gui_host vst_gui_host.cpp)
    
    target_include_directories(vst_gui_host PRIVATE
        ${VST3SDK_PATH}
        ../lib
    )
    
    # Link with synth core and platform libs
    target_link_libraries(vst_gui_host 
        synth_core 
        example_platform_utils 
        ${PLATFORM_LIBS}
    )
    
    target_compile_definitions(vst_gui_host PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        UNICODE
        _UNICODE
    )
    
    set_target_properties(vst_gui_host PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    
    install(TARGETS vst_gui_host RUNTIME DESTINATION bin)
else()
    message(STATUS "VST3 SDK not found - skipping vst_gui_host build")
endif()

# Special handling for any example-specific dependencies
if(TARGET test_midi)
    # MIDI example might need additional libraries
    target_compile_definitions(test_midi PRIVATE UNICODE _UNICODE)
endif()

# Copy any resource files if needed
if(EXISTS "${CMAKE_SOURCE_DIR}/*.wav")
    file(GLOB WAV_FILES "${CMAKE_SOURCE_DIR}/*.wav")
    foreach(WAV_FILE ${WAV_FILES})
        configure_file(${WAV_FILE} ${CMAKE_BINARY_DIR}/bin/ COPYONLY)
    endforeach()
endif()
