cmake_minimum_required(VERSION 3.12)
project(Jones3D)

# Game sources, the entry point is added per platform
file(GLOB_RECURSE JONES3D_SOURCES CONFIGURE_DEPENDS *.c Gui/wx/*.cpp)
file(GLOB_RECURSE JONES3D_HEADERS *.h)
list(REMOVE_ITEM JONES3D_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/standalone.c")
list(REMOVE_ITEM JONES3D_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/linuxmain.c")

set(JONES3D_WX_LIBS wx::html wx::core wx::base) # wx::html: help viewer (Gui/wx/JonesWxHelp.cpp)

file(GLOB JONES3D_WX_SOURCES CONFIGURE_DEPENDS Gui/wx/*.cpp)

if(WIN32)
    # OpenJones3D.exe, the original Indy3D.exe is only read for its icon resources
    add_executable(OpenJones3D WIN32 ${JONES3D_SOURCES} ${JONES3D_HEADERS} "${CMAKE_CURRENT_SOURCE_DIR}/standalone.c")
    target_link_libraries(OpenJones3D PRIVATE Comctl32.lib Gdi32.lib User32.lib Shell32.lib Comdlg32.lib Advapi32.lib Ole32.lib)
    set_property(TARGET OpenJones3D PROPERTY VS_DPI_AWARE "PerMonitor")

    # wx requires the wide Win32 API, the rest of the game uses the ANSI API
    set_source_files_properties(${JONES3D_WX_SOURCES} PROPERTIES COMPILE_DEFINITIONS "UNICODE;_UNICODE")
else()
    add_executable(OpenJones3D ${JONES3D_SOURCES} ${JONES3D_HEADERS} "${CMAKE_CURRENT_SOURCE_DIR}/linuxmain.c")
    set_target_properties(OpenJones3D PROPERTIES OUTPUT_NAME jones3d-portable)
endif()

target_include_directories(OpenJones3D PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(OpenJones3D PRIVATE Libs ${JONES3D_WX_LIBS})

# Organize source files in Visual Studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${JONES3D_SOURCES} ${JONES3D_HEADERS})
