add_executable(synth_cli_v2 synth_cli_v2.cpp)
target_link_libraries(synth_cli_v2 PRIVATE synth_core)

# Windows-specific settings
if(WIN32)
    target_link_libraries(synth_cli_v2 PRIVATE winmm)
    target_compile_definitions(synth_cli_v2 PRIVATE _WIN32_WINNT=0x0601)
endif()

# Linux-specific settings
if(UNIX AND NOT APPLE)
    find_package(ALSA QUIET)
    find_package(PkgConfig QUIET)
    
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(PULSE libpulse-simple)
    endif()
    
    if(ALSA_FOUND)
        target_link_libraries(synth_cli_v2 PRIVATE ${ALSA_LIBRARIES})
    endif()
    
    if(PULSE_FOUND)
        target_include_directories(synth_cli_v2 PRIVATE ${PULSE_INCLUDE_DIRS})
        target_link_libraries(synth_cli_v2 PRIVATE ${PULSE_LIBRARIES})
        target_compile_definitions(synth_cli_v2 PRIVATE HAVE_PULSEAUDIO)
    endif()
endif()

# Output to bin directory
set_target_properties(synth_cli_v2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
