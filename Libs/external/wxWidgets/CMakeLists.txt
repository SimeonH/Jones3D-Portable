cmake_minimum_required(VERSION 3.14)
project(wxWidgets_external)

# wxWidgets is used for all game dialogs (Jones3D/Gui/wx).
# It is fetched at configure time and linked statically.
# Set FETCHCONTENT_SOURCE_DIR_WXWIDGETS to use a local source tree instead of downloading.

include(FetchContent)

if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 gtk+-3.0)
    if(NOT GTK3_FOUND)
        message(FATAL_ERROR "wxWidgets needs GTK 3 development files for the target architecture (32-bit: lib32-gtk3)")
    endif()
endif()

FetchContent_Declare(wxWidgets
    URL      https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.8/wxWidgets-3.2.8.tar.bz2
    URL_HASH SHA256=c74784904109d7229e6894c85cfa068f1106a4a07c144afd78af41f373ee0fe6
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Don't build wx with the project's strict warning flags (/W4, /sdl, ...)
set_directory_properties(PROPERTIES COMPILE_OPTIONS "")

set(wxBUILD_SHARED         OFF CACHE BOOL "" FORCE)
set(wxBUILD_MONOLITHIC     OFF CACHE BOOL "" FORCE)
set(wxBUILD_SAMPLES        OFF CACHE STRING "" FORCE)
set(wxBUILD_TESTS          OFF CACHE STRING "" FORCE)
set(wxBUILD_DEMOS          OFF CACHE BOOL "" FORCE)
set(wxBUILD_INSTALL        OFF CACHE BOOL "" FORCE)
set(wxBUILD_PRECOMP        OFF CACHE STRING "" FORCE)

# Only the core GUI is needed, disable everything else
foreach(feature
    WEBVIEW STC RICHTEXT AUI PROPGRID RIBBON MEDIACTRL XRC
    OPENGL GLCANVAS LIBTIFF REGEX XML DEBUGREPORT SOCKETS URL PROTOCOL FS_INET
    LIBNOTIFY # GTK: not needed, and fails to build with some libnotify versions
    LIBSDL    # Unix wxSound backend, not used (and would pull in the host SDL2)
)
    set(wxUSE_${feature} OFF CACHE BOOL "" FORCE)
endforeach()

# The game's help is shown in the wxHTML help viewer (Jones3D/Gui/wx/JonesWxHelp.cpp): the WinHelp file converted to HTML,
# or the CHM file of the GOG release, unpacked with libmspack (Libs/external/libmspack) by the game on all platforms.
set(wxUSE_HTML        ON CACHE BOOL "" FORCE)
set(wxUSE_WXHTML_HELP ON CACHE BOOL "" FORCE)
set(wxUSE_LIBMSPACK   OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(wxWidgets)

# wx exports UNICODE/_UNICODE to its users, which would switch the game's C code to the wide Win32 API.
# Jones3D sets these only on its wx sources (see Jones3D/CMakeLists.txt).
foreach(target wxbase wxcore wxhtml)
    if(NOT TARGET ${target})
        continue()
    endif()

    get_target_property(defs ${target} INTERFACE_COMPILE_DEFINITIONS)
    if(defs)
        list(REMOVE_ITEM defs UNICODE _UNICODE)
        set_target_properties(${target} PROPERTIES INTERFACE_COMPILE_DEFINITIONS "${defs}")
    endif()
endforeach()

foreach(target wxbase wxcore wxhtml wxnet wxadv wxqa wxzlib wxexpat wxjpeg wxpng wxregex wxnanosvg)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES FOLDER "Libs/external/wxWidgets")

        # wx forces its own MSVC runtime per config, use the same runtime as the rest of the project
        if(MSVC)
            set_target_properties(${target} PROPERTIES MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
        endif()
    endif()
endforeach()
