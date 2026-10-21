cmake_minimum_required(VERSION 3.12)
project(Jones3D)

# Option to enable or disable the post-build command.
# When set to ON, a post-build step will be added that copies the
# output files (.dll and .exe) to a specified directory.
option(JONES3D_ENABLE_POST_BUILD_COPY "Enable copying of output files (Jones3D.dll, Jones3D.exe) after building" OFF)

# Path to the directory where the output files should be copied.
# This should be set to an absolute path or a path relative to the project root.
# The copied files will be placed in this directory under a 'Debug' subfolder.
set(JONES3D_POST_BUILD_COPY_DIR "" CACHE PATH "Destination folder for the post-build copied files (Jones3D.dll, Jones3D.exe)")

# Collect Jones3D DLL sources
file(GLOB_RECURSE JONES3D_DLL_SOURCES CONFIGURE_DEPENDS *.c Gui/wx/*.cpp)
file(GLOB_RECURSE JONES3D_DLL_HEADERS *.h)
list(REMOVE_ITEM JONES3D_DLL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/sha256.h")
list(REMOVE_ITEM JONES3D_DLL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/standalone.c")
list(REMOVE_ITEM JONES3D_DLL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/linuxmain.c")

set(JONES3D_WX_LIBS wx::core wx::base)

# Copies the help file viewer next to the game executable, where JonesHud_OpenHelp looks for it
function(jones3d_add_hlp_viewer target)
    if(TARGET hlp_viewer)
        get_property(hlpViewerExe GLOBAL PROPERTY JONES3D_HLP_VIEWER_EXE)
        add_dependencies(${target} hlp_viewer)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${hlpViewerExe} $<TARGET_FILE_DIR:${target}>
        )
    endif()
endfunction()

file(GLOB JONES3D_WX_SOURCES CONFIGURE_DEPENDS Gui/wx/*.cpp)

if(NOT WIN32)
    # Native build: a single jones3d-portable executable, there's no Indy3D.exe to inject into
    set(JONES3D_NATIVE_SOURCES ${JONES3D_DLL_SOURCES})
    list(REMOVE_ITEM JONES3D_NATIVE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/dllmain.c")
    list(APPEND JONES3D_NATIVE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/linuxmain.c")

    add_executable(OpenJones3D ${JONES3D_NATIVE_SOURCES} ${JONES3D_DLL_HEADERS})
    target_include_directories(OpenJones3D PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_link_libraries(OpenJones3D PRIVATE Libs ${JONES3D_WX_LIBS})
    set_target_properties(OpenJones3D PROPERTIES OUTPUT_NAME jones3d-portable)
    jones3d_add_hlp_viewer(OpenJones3D)
    return()
endif()

# Standalone OpenJones3D.exe sources: everything from the DLL except its injection entry point
set(JONES3D_STANDALONE_SOURCES ${JONES3D_DLL_SOURCES})
list(REMOVE_ITEM JONES3D_STANDALONE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/dllmain.c")
list(APPEND JONES3D_STANDALONE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/standalone.c")

# Jones3D EXE sources
set(JONES3D_EXE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/exemain.cpp")

# Create the executable and DLL targets
add_executable(Jones3D ${JONES3D_EXE_SOURCES} )
add_library(Jones3D_DLL SHARED ${JONES3D_DLL_SOURCES} ${JONES3D_DLL_HEADERS})

# Make Jones3D.exe depending on Jones3D.dll
add_dependencies(Jones3D Jones3D_DLL)

# exemain.cpp calls MessageBoxW (via the UNICODE-mapped MessageBox macro) directly, with
# nothing else pulling in user32.lib for this target.
target_link_libraries(Jones3D PRIVATE User32.lib)

# Add Unicode character set for Jones3D.exe
# NOTE: UNICODE (no underscore) controls the Win32 API's TCHAR-style A/W macro selection
# (MessageBox, CreateProcess, etc. in winuser.h/processthreadsapi.h); _UNICODE alone only
# affects the CRT's tchar.h. exemain.cpp calls these with wide (L"...") strings, so without
# UNICODE they resolve to the ANSI *A entry points and fail to compile (and would equally
# fail to compile correctly under real MSVC, not just here).
target_compile_definitions(Jones3D PRIVATE _UNICODE UNICODE)

# Set DPI awareness
set_property(TARGET Jones3D PROPERTY VS_DPI_AWARE "PerMonitor")

# Rename the DLL target output
set_target_properties(Jones3D_DLL PROPERTIES OUTPUT_NAME "Jones3D")

# Add include to Jones3D.dll
target_include_directories(Jones3D_DLL PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Link against Libs project
target_link_libraries(Jones3D_DLL PRIVATE Libs)
target_link_libraries(Jones3D_DLL PRIVATE Comctl32.lib)
# JonesDialog.c/JonesHud.c call GDI/USER32/SHELL32 APIs (DeleteObject, SelectObject,
# GetWindowLongA, SendMessageA, ShellExecuteExA, etc.) directly with no #pragma comment(lib)
# anywhere to pull these in implicitly; link them explicitly.
target_link_libraries(Jones3D_DLL PRIVATE Gdi32.lib User32.lib Shell32.lib Comdlg32.lib Advapi32.lib Ole32.lib)

# Game dialogs are implemented with wxWidgets (Gui/wx).
# wx requires the wide Win32 API, the rest of the game uses the ANSI API.
set_source_files_properties(${JONES3D_WX_SOURCES} PROPERTIES COMPILE_DEFINITIONS "UNICODE;_UNICODE")
target_link_libraries(Jones3D_DLL PRIVATE ${JONES3D_WX_LIBS})

# Standalone OpenJones3D.exe, runs the game without Indy3D.exe code (Indy3D.exe is only read for its resources)
option(JONES3D_BUILD_STANDALONE "Build standalone OpenJones3D.exe" ON)
if(JONES3D_BUILD_STANDALONE)
    add_executable(OpenJones3D WIN32 ${JONES3D_STANDALONE_SOURCES} ${JONES3D_DLL_HEADERS})
    target_include_directories(OpenJones3D PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_link_libraries(OpenJones3D PRIVATE Libs)
    target_link_libraries(OpenJones3D PRIVATE Comctl32.lib Gdi32.lib User32.lib Shell32.lib Comdlg32.lib Advapi32.lib Ole32.lib)
    target_link_libraries(OpenJones3D PRIVATE ${JONES3D_WX_LIBS})
    set_property(TARGET OpenJones3D PROPERTY VS_DPI_AWARE "PerMonitor")
    jones3d_add_hlp_viewer(OpenJones3D)
endif()

# Organize source files in Visual Studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${JONES3D_DLL_SOURCES} ${JONES3D_DLL_HEADERS})

# If the post-build command is enabled, and the destination path is provided,
# add the post-build command to copy the output files to the specified directory.
if(JONES3D_ENABLE_POST_BUILD_COPY)
    if(NOT JONES3D_POST_BUILD_COPY_DIR)
        message(FATAL_ERROR "Post-build command is enabled, but no path was specified. Please set JONES3D_POST_BUILD_COPY_DIR.")
    endif()

    # Add post-build copy command for Jones3D.exe
    add_custom_command(
        TARGET Jones3D
        POST_BUILD
        COMMAND xcopy /y /d "$(OutDir)$(TargetName)$(TargetExt)" "\"${JONES3D_POST_BUILD_COPY_DIR}\""
        COMMENT "Copying output file Jones3D.exe to '${JONES3D_POST_BUILD_COPY_DIR}'"
    )

    # Set debugging work dir to copy dir
    set_target_properties(Jones3D PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${JONES3D_POST_BUILD_COPY_DIR}"
    )

    # Add post-build copy command for Jones3D.dll
    add_custom_command(
        TARGET Jones3D_DLL
        POST_BUILD
        COMMAND xcopy /y /d "$(OutDir)$(TargetName)$(TargetExt)" "\"${JONES3D_POST_BUILD_COPY_DIR}\""
        COMMAND xcopy /y /d "$(OutDir)$(TargetName).pdb" "\"${JONES3D_POST_BUILD_COPY_DIR}\""
        COMMENT "Copying output file Jones3D.dll and Jones3D.pdb to '${JONES3D_POST_BUILD_COPY_DIR}'"
    )

endif()

# # Set up post-build event for Debug configuration
# set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/Output")
# file(TO_NATIVE_PATH "${OUTPUT_DIR}" OUTPUT_DIR_NATIVE)

# set_target_properties(Jones3D PROPERTIES
#     VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
#     VS_DEBUGGER_COMMAND "${CMAKE_BINARY_DIR}/$(Configuration)/Jones3D.exe"
# )

#  #xcopy /Y /I "$(TargetPath)" "${OUTPUT_DIR_NATIVE}"
# add_custom_command(TARGET Jones3D POST_BUILD
#     COMMAND if $(Configuration) == Debug (
#         xcopy /y /d  "$(OutDir)$(TargetName)$(TargetExt)" "${OUTPUT_DIR_NATIVE}"
#     )
#     COMMENT "Copying executable to ${OUTPUT_DIR_NATIVE}"
# )

# add_custom_command(TARGET Jones3D_DLL POST_BUILD
#     COMMAND if $(Configuration) == Debug (
#         xcopy /y /d  "$(OutDir)$(TargetName)$(TargetExt)" "${OUTPUT_DIR_NATIVE}"
#         xcopy /y /d  "$(OutDir)$(TargetName).pdb" "${OUTPUT_DIR_NATIVE}"
#     )
#     COMMENT "Copying DLL to ${OUTPUT_DIR_NATIVE}"
# )