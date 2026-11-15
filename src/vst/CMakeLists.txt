cmake_minimum_required(VERSION 3.14.0)

option(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES "Enable VST 3 Plug-in Examples" OFF)
option(SMTG_ENABLE_VST3_HOSTING_EXAMPLES "Enable VST 3 Hosting Examples" OFF)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "")

set(vst3sdk_SOURCE_DIR "C:/Users/orian/dev/VST_SDK/vst3sdk")
if(NOT vst3sdk_SOURCE_DIR)
    message(FATAL_ERROR "Path to VST3 SDK is empty!")
endif()

project(ObsSynth
    # This is your plug-in version number. Change it here only.
    # Version number symbols usable in C++ can be found in
    # source/version.h and ${PROJECT_BINARY_DIR}/projectversion.h.
    VERSION 1.0.0.0 
    DESCRIPTION "ObsSynth VST 3 Plug-in"
)

set(SMTG_VSTGUI_ROOT "${vst3sdk_SOURCE_DIR}")

add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

smtg_add_vst3plugin(ObsSynth
    source/version.h
    source/cids.h
    source/processor.h
    source/processor.cpp
    source/controller.h
    source/controller.cpp
    source/entry.cpp
)

# Add include directory for synth core library
target_include_directories(ObsSynth
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/lib
)

#- VSTGUI Wanted ----
if(SMTG_ENABLE_VSTGUI_SUPPORT)
    target_sources(ObsSynth
        PRIVATE
            resource/editor.uidesc
    )
    target_link_libraries(ObsSynth
        PRIVATE
            vstgui_support
    )
    smtg_target_add_plugin_resources(ObsSynth
        RESOURCES
            "resource/editor.uidesc"
    )
    
    # Add VSTGUI inline editor resources for Debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR VSTGUI_LIVE_EDITING)
        set(VSTGUI_EDITOR_RESOURCES_DIR "${vst3sdk_SOURCE_DIR}/vstgui4/vstgui/uidescription/editing")
        smtg_target_add_plugin_resources(ObsSynth
            RESOURCES
                "${VSTGUI_EDITOR_RESOURCES_DIR}/uidescriptioneditor.uidesc"
                "${VSTGUI_EDITOR_RESOURCES_DIR}/uidescriptioneditor_res_dark.uidesc"
                "${VSTGUI_EDITOR_RESOURCES_DIR}/uidescriptioneditor_res_light.uidesc"
        )
        # Copy editor bitmap resources
        file(GLOB VSTGUI_EDITOR_BITMAPS "${VSTGUI_EDITOR_RESOURCES_DIR}/resources/*.png")
        smtg_target_add_plugin_resources(ObsSynth RESOURCES ${VSTGUI_EDITOR_BITMAPS})
    endif()
endif(SMTG_ENABLE_VSTGUI_SUPPORT)
# -------------------

smtg_target_add_plugin_snapshots (ObsSynth
    RESOURCES
        resource/DA88C81B43DE5533A76359DD8C0FA6ED_snapshot.png
        resource/DA88C81B43DE5533A76359DD8C0FA6ED_snapshot_2.0x.png
)

target_link_libraries(ObsSynth
    PRIVATE
        sdk
        synth_core
)

# VSTGUI Live Editing DISABLED - causes stack overflow crashes
# To enable inline UI editor, uncomment and fix the VSTGUI editing subsystem integration
# target_compile_definitions(ObsSynth PRIVATE
#     $<$<CONFIG:Debug>:VSTGUI_LIVE_EDITING=1>
# )

smtg_target_configure_version_file(ObsSynth)

if(SMTG_MAC)
    smtg_target_set_bundle(ObsSynth
        BUNDLE_IDENTIFIER com.modularsistemas.obsynth
        COMPANY_NAME "My Plug-in Company"
    )
    smtg_target_set_debug_executable(ObsSynth
        "/Applications/VST3PluginTestHost.app"
        "--pluginfolder;$(BUILT_PRODUCTS_DIR)"
    )
elseif(SMTG_WIN)
    target_sources(ObsSynth PRIVATE 
        resource/win32resource.rc
    )
    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ObsSynth)

        smtg_target_set_debug_executable(ObsSynth
            "$(ProgramW6432)/Steinberg/VST3PluginTestHost/VST3PluginTestHost.exe"
            "--pluginfolder \"$(OutDir)/\""
        )
    endif()
endif(SMTG_MAC)
