# The products as plugins: ObsSynthCT1, ObsSynth4D and ObsDrums. Each one's code is a static library (<name>_impl)
# that gives the CLAP entry's three functions; entry.cpp exports it. The CLAP headers (MIT, headers only) are fetched.
#
# OBS_VST3=ON: the same code also as a VST3, through clap-wrapper (free-audio, MIT), which fetches the VST3 SDK
# (Steinberg, MIT since 3.8). On Windows a VST3 folder: Contents/x86_64-win/<name>.vst3 and Contents/Resources,
# the wavetable banks. clap_render tries a plugin without a DAW.
include(FetchContent)
# OBS_CLAP_SOURCE_DIR: a checkout of the CLAP headers (free-audio/clap, MIT, 1.2.10) to use instead of fetching them (a
# build with no network: a distribution's)
set(OBS_CLAP_SOURCE_DIR "" CACHE PATH "A checkout of free-audio/clap, used instead of fetching it")
if(OBS_CLAP_SOURCE_DIR)
    add_subdirectory(${OBS_CLAP_SOURCE_DIR} ${CMAKE_BINARY_DIR}/clap-headers EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(clap GIT_REPOSITORY https://github.com/free-audio/clap.git GIT_TAG 1.2.10 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(clap)
endif()

option(OBS_VST3 "Also the plugins as VST3 (fetches clap-wrapper and the VST3 SDK)" OFF)
# our float warnings (-Wdouble-promotion, -Wfloat-conversion: the root's) are for our code: clap-wrapper and the
# VST3 SDK build here with -Werror of their own, and do not hold to them. Taken off this directory (and so off what
# it fetches), given back to the plugins' own code below.
set(OBS_FLOAT_WARNINGS "")
if(NOT MSVC)
    set(OBS_FLOAT_WARNINGS -Wdouble-promotion -Wfloat-conversion)
    get_directory_property(obs_dir_options COMPILE_OPTIONS)
    list(REMOVE_ITEM obs_dir_options ${OBS_FLOAT_WARNINGS})
    set_directory_properties(PROPERTIES COMPILE_OPTIONS "${obs_dir_options}")
endif()

if(OBS_VST3)
    set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "" FORCE)
    set(CLAP_WRAPPER_BUILD_AUV2 OFF CACHE BOOL "" FORCE)
    set(CLAP_WRAPPER_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
    # main at a release (its tags stopped in 2024): 806bba0, 2026-10-20
    FetchContent_Declare(clap-wrapper GIT_REPOSITORY https://github.com/free-audio/clap-wrapper.git GIT_TAG 806bba0a5f792e3966b99c1561ab9952d3316d7c)
    FetchContent_MakeAvailable(clap-wrapper)
endif()

# what a plugin carries (VST3: Contents/Resources; CLAP: beside it): pcm/, the wavetables (nothing else of pcm/), and
# its panel, the editor, where it has one
set(OBS_PLUGIN_RESOURCES "${CMAKE_BINARY_DIR}/plugin_resources")
file(GLOB OBS_BANKS "${CMAKE_SOURCE_DIR}/pcm/*.wav")
file(COPY ${OBS_BANKS} DESTINATION "${OBS_PLUGIN_RESOURCES}/pcm")

# name: the targets' (obssynthct1 → obssynthct1_clap, obssynthct1_vst3); product: the files' (ObsSynthCT1.clap)
function(obs_plugin name product)
    add_library(${name}_impl STATIC ${ARGN})
    target_link_libraries(${name}_impl PUBLIC clap synth_core ${CMAKE_DL_LIBS})
    target_include_directories(${name}_impl PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    target_compile_definitions(${name}_impl PRIVATE SYNTH_PCM_DIR="${CMAKE_SOURCE_DIR}/pcm")
    target_compile_options(${name}_impl PRIVATE ${OBS_FLOAT_WARNINGS})
    set_target_properties(${name}_impl PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)

    if(OBS_VST3)
        make_clapfirst_plugins(
            TARGET_NAME ${name}
            IMPL_TARGET ${name}_impl
            OUTPUT_NAME "${product}"
            ENTRY_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/entry.cpp"
            BUNDLE_IDENTIFIER "org.carbonian.${name}"
            BUNDLE_VERSION "0.1.0"
            COPY_AFTER_BUILD FALSE
            WINDOWS_FOLDER_VST3 TRUE
            RESOURCE_DIRECTORY "${OBS_PLUGIN_RESOURCES}"
            ASSET_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
            PLUGIN_FORMATS CLAP VST3)
        # the banks: in the VST3 its Contents/Resources (clap-wrapper does it on macOS only), beside the .clap a pcm/
        add_custom_command(TARGET ${name}_vst3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${OBS_PLUGIN_RESOURCES}" "$<TARGET_FILE_DIR:${name}_vst3>/../Resources")
        add_custom_command(TARGET ${name}_clap POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${OBS_PLUGIN_RESOURCES}" "$<TARGET_FILE_DIR:${name}_clap>")
    else()
        add_library(${name}_clap MODULE entry.cpp)
        target_link_libraries(${name}_clap PRIVATE ${name}_impl)
        set_target_properties(${name}_clap PROPERTIES PREFIX "" OUTPUT_NAME "${product}" SUFFIX ".clap"
                              CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON
                              LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/clap")
    endif()
    if(UNIX AND NOT APPLE) # the host plays the audio: none of synth_core's backends (Pulse, ALSA) linked in
        target_link_options(${name}_clap PRIVATE -Wl,--as-needed)
    endif()
endfunction()

obs_plugin(obssynthct1 ObsSynthCT1 ct1_clap.cpp)
obs_plugin(obssynth4d ObsSynth4D synth4d_clap.cpp ${CMAKE_SOURCE_DIR}/src/app/obssynth4d/presets.cpp)
obs_plugin(obsdrums ObsDrums drums_clap.cpp)
obs_plugin(obssynthone ObsSynthOne one_clap.cpp ${CMAKE_SOURCE_DIR}/src/app/obssynth4d/presets.cpp)
foreach(edited obssynthct1 obssynth4d obsdrums obssynthone) # where their panels are built (a plugin run from the build tree)
    target_compile_definitions(${edited}_impl PRIVATE SYNTH_PANEL_DIR="${CMAKE_BINARY_DIR}/src/app")
endforeach()

# the editors: the product's panel carried with the plugin (VST3: in its Resources; CLAP: beside it)
if(OBS_VST3)
    foreach(edited obssynthct1 obssynth4d obsdrums obssynthone)
        if(TARGET ${edited})
            add_dependencies(${edited}_vst3 ${edited})
            add_dependencies(${edited}_clap ${edited})
            add_custom_command(TARGET ${edited}_vst3 POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${edited}>" "$<TARGET_FILE_DIR:${edited}_vst3>/../Resources/")
            add_custom_command(TARGET ${edited}_clap POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${edited}>" "$<TARGET_FILE_DIR:${edited}_clap>/")
        endif()
    endforeach()
endif()
if(OBS_VST3)
    message(STATUS "Plugins: ObsSynthCT1, ObsSynth4D, ObsDrums, ObsSynthOne as CLAP and VST3 in ${CMAKE_BINARY_DIR}/plugins")
else()
    message(STATUS "Plugins: ObsSynthCT1.clap, ObsSynth4D.clap, ObsDrums.clap, ObsSynthOne.clap in ${CMAKE_BINARY_DIR}/clap (OBS_VST3=ON: also VST3)")
endif()

if(NOT WIN32)
    add_executable(clap_render clap_render.cpp)
    target_link_libraries(clap_render PRIVATE clap ${CMAKE_DL_LIBS})
    set_target_properties(clap_render PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()
