# Source CMakeLists.txt - Organized project structure

# Add subdirectories for different project components
add_subdirectory(app)
add_subdirectory(examples)

# the products as CLAP plugins (src/plugins/clap): OBS_CLAP, on by default
option(OBS_CLAP "Build the products as CLAP plugins" ON)
if(OBS_CLAP)
    add_subdirectory(plugins/clap)
endif()

# Add VST plugin (if VST SDK is available)
set(VST3SDK_PATH "C:/Users/orian/dev/VST_SDK/vst3sdk")
if(EXISTS "${VST3SDK_PATH}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vst/CMakeLists.txt")
    message(STATUS "VST3 SDK found - building VST plugin")
    add_subdirectory(vst)
else()
    message(STATUS "VST3 SDK not found at ${VST3SDK_PATH} - skipping VST plugin build")
endif()
