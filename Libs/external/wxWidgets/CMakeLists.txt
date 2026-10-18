cmake_minimum_required(VERSION 3.14)
project(wxWidgets_external)

# wxWidgets is used for all game dialogs (Jones3D/Gui/wx).
# It is fetched at configure time and linked statically.
# Set FETCHCONTENT_SOURCE_DIR_WXWIDGETS to use a local source tree instead of downloading.

include(FetchContent)

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
    WEBVIEW STC RICHTEXT AUI PROPGRID RIBBON MEDIACTRL XRC HTML WXHTML_HELP
    OPENGL GLCANVAS LIBTIFF REGEX XML DEBUGREPORT SOCKETS URL PROTOCOL FS_INET
)
    set(wxUSE_${feature} OFF CACHE BOOL "" FORCE)
endforeach()

FetchContent_MakeAvailable(wxWidgets)

# wx exports UNICODE/_UNICODE to its users, which would switch the game's C code to the wide Win32 API.
# Jones3D sets these only on its wx sources (see Jones3D/CMakeLists.txt).
foreach(target wxbase wxcore)
    get_target_property(defs ${target} INTERFACE_COMPILE_DEFINITIONS)
    if(defs)
        list(REMOVE_ITEM defs UNICODE _UNICODE)
        set_target_properties(${target} PROPERTIES INTERFACE_COMPILE_DEFINITIONS "${defs}")
    endif()
endforeach()

foreach(target wxbase wxcore wxnet wxadv wxqa wxzlib wxexpat wxjpeg wxpng wxregex wxnanosvg)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES FOLDER "Libs/external/wxWidgets")

        # wx forces its own MSVC runtime per config, use the same runtime as the rest of the project
        if(MSVC)
            set_target_properties(${target} PROPERTIES MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
        endif()
    endif()
endforeach()
