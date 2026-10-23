cmake_minimum_required(VERSION 3.24)
project(SDL3_external C)

# SDL3 is the platform layer (window, input, timing) on all platforms.
# Linux uses the system library (lib32-sdl3 for the 32-bit build). Windows builds it from source as a static library,
# so the game stays a single executable. Set FETCHCONTENT_SOURCE_DIR_SDL3 to use a local source tree instead of downloading.
if(NOT WIN32)
    find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3-shared GLOBAL)
    message(STATUS "Using SDL3 ${SDL3_VERSION}")
    return()
endif()

include(FetchContent)

FetchContent_Declare(SDL3
    URL      https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
    URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Don't build SDL with the project's warning flags
set_directory_properties(PROPERTIES COMPILE_OPTIONS "")

set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL      OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3)

if(NOT TARGET SDL3::SDL3)
    add_library(SDL3::SDL3 ALIAS SDL3-static)
endif()
set_target_properties(SDL3-static PROPERTIES FOLDER "Libs/external")
