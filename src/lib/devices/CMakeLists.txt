# Devices library - MIDI device abstractions and implementations

# Device source files
set(DEVICE_SOURCES
    midi_device.cpp
    launchpadx.cpp
    launchkey_mk4.cpp
    apc_key25_mk2.cpp
)

# Device header files
set(DEVICE_HEADERS
    midi_device.hpp
    launchpadx.hpp
    launchkey_mk4.hpp
    apc_key25_mk2.hpp
)

# Add devices to the main synth_core library
target_sources(synth_core PRIVATE
    ${DEVICE_SOURCES}
)

# Make headers available to other parts of the project
target_include_directories(synth_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Set source properties for consistency
set_source_files_properties(${DEVICE_SOURCES} PROPERTIES
    COMPILE_FLAGS "${FLOAT_ONLY_FLAGS}"
)

# Add devices subdirectory to parent scope
set(DEVICE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
