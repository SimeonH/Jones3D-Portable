cmake_minimum_required(VERSION 3.15)

project(OpenJones3D VERSION 0.3.1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20) # C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable solution folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(JONES3D_BUILD_PROGRAMS "Build programs" ON)
option(JONES3D_BUILD_GAME "Build the game (Jones3D); OFF builds only the libraries" ON)
option(JONES3D_QOL_IMPROVEMENTS "Enable quality-of-life improvements" ON)

# Rust HLP Viewer for the in-game help (Libs/external/HlpViewer). It's built with cargo for the host,
# so it's only available when the game is built for the host OS.
find_program(JONES3D_CARGO cargo)
if(JONES3D_CARGO AND CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
    set(JONES3D_HLP_VIEWER_DEFAULT ON)
else()
    set(JONES3D_HLP_VIEWER_DEFAULT OFF)
endif()
option(JONES3D_BUILD_HLP_VIEWER "Build the help file viewer (needs cargo)" ${JONES3D_HLP_VIEWER_DEFAULT})

if(JONES3D_QOL_IMPROVEMENTS)
    add_compile_definitions(J3D_QOL_IMPROVEMENTS=1)
endif()

# Renderer backend of std3D/stdDisplay: DirectDraw (original, Windows only) or Vulkan
if(WIN32)
    set(JONES3D_RENDERER "DirectDraw" CACHE STRING "Renderer backend (DirectDraw or Vulkan)")
else()
    set(JONES3D_RENDERER "Vulkan" CACHE STRING "Renderer backend (DirectDraw or Vulkan)")
    if(NOT JONES3D_RENDERER STREQUAL "Vulkan")
        message(FATAL_ERROR "Only the Vulkan renderer is supported on ${CMAKE_SYSTEM_NAME}")
    endif()
endif()
set_property(CACHE JONES3D_RENDERER PROPERTY STRINGS DirectDraw Vulkan)
if(JONES3D_RENDERER STREQUAL "Vulkan")
    add_compile_definitions(J3D_RENDERER_VULKAN=1)
elseif(NOT JONES3D_RENDERER STREQUAL "DirectDraw")
    message(FATAL_ERROR "Unknown JONES3D_RENDERER '${JONES3D_RENDERER}', expected DirectDraw or Vulkan")
endif()
message(STATUS "Jones3D renderer: ${JONES3D_RENDERER}")

# Configure the version header file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Libs/j3dcore/j3d.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/Libs/j3dcore/j3d.h"
)

# Set global property for MSVC runtime library (CMake 3.15+)
set(CMAKE_MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Release>:MultiThreaded>$<$<CONFIG:MinSizeRel>:MultiThreaded>")

if (MSVC)
    add_compile_definitions("__FILE_NAME__=\"%(Filename)%(Extension)\"")
    add_compile_options(
        /utf-8
        /Zc:__STDC__
        /Zc:__cplusplus
        /diagnostics:caret
        /sdl
        #"$<$<CONFIG:DEBUG>:/Gm>" # minimal rebuild TODO: uncomment
        /W4 # warning level 4
        /Zc:preprocessor # This option enables a token-based preprocessor that conforms to C99 and C++11 and later standards.
        "$<$<NOT:$<CXX_COMPILER_ID:Clang>>:/MP>" # TODO: remove when /Gm is uncommented
        #$<$<AND:$<NOT:$<CXX_COMPILER_ID:Clang>>,$<NOT:$<CONFIG:DEBUG>>>:/MP> TODO: uncomment when /Gm is uncommented
        "$<$<CONFIG:DEBUG>:/ZI>"
    )
    add_link_options("/Brepro")
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "Jones3D target: 64-bit")
    else()
        message(STATUS "Jones3D target: 32-bit")
    endif()

    add_compile_definitions(_GNU_SOURCE)
    add_compile_options(
        -Wall
        -Wextra
        -fno-strict-aliasing # game code type-puns freely
        -fwrapv
        -fdiagnostics-color=always
        $<$<COMPILE_LANGUAGE:C>:-Wno-incompatible-pointer-types>
        $<$<COMPILE_LANGUAGE:C>:-Wno-incompatible-function-pointer-types>
    )
else()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

# Add Libs subdirectory
add_subdirectory(Libs)

# Add main project Jones3D
if(JONES3D_BUILD_GAME)
    add_subdirectory(Jones3D)
endif()

if (JONES3D_BUILD_PROGRAMS)
    add_subdirectory(Programs)
endif()
