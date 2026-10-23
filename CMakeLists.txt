cmake_minimum_required(VERSION 3.15)

project(OpenJones3D VERSION 1.0.0)

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

if(JONES3D_QOL_IMPROVEMENTS)
    add_compile_definitions(J3D_QOL_IMPROVEMENTS=1)
endif()

# Renderers of std3D/stdDisplay (Libs/std/Gpu). Each one is a display device, selectable at runtime in the display settings.
# Vulkan is always built, OpenGL ES 2.0 and Direct3D 11 (Windows only) are optional.
option(JONES3D_RENDERER_GLES2 "Build the OpenGL ES 2.0 renderer" ON)
if(WIN32)
    option(JONES3D_RENDERER_D3D11 "Build the Direct3D 11 renderer" ON)
else()
    set(JONES3D_RENDERER_D3D11 OFF)
endif()

set(JONES3D_RENDERERS Vulkan)
add_compile_definitions(J3D_RENDERER_VULKAN=1)
if(JONES3D_RENDERER_GLES2)
    add_compile_definitions(J3D_RENDERER_GLES2=1)
    list(APPEND JONES3D_RENDERERS GLES2)
endif()
if(JONES3D_RENDERER_D3D11)
    add_compile_definitions(J3D_RENDERER_D3D11=1)
    list(APPEND JONES3D_RENDERERS D3D11)
endif()
message(STATUS "Jones3D renderers: ${JONES3D_RENDERERS}")

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
