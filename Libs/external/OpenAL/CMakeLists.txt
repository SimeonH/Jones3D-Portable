cmake_minimum_required(VERSION 3.14)
project(OpenAL_external C CXX)

# OpenAL Soft is the sound backend (Libs/sound/Driver) of the non-Windows builds, replacing DirectSound3D.
# It's built from source because 32-bit packages aren't commonly available. Audio system backends
# (PipeWire, PulseAudio, ALSA) are loaded at runtime.
# Set FETCHCONTENT_SOURCE_DIR_OPENALSOFT to use a local source tree instead of downloading.

include(FetchContent)

FetchContent_Declare(OpenALSoft
    URL      https://github.com/kcat/openal-soft/archive/refs/tags/1.24.3.tar.gz
    URL_HASH SHA256=7e1fecdeb45e7f78722b776c5cf30bd33934b961d7fd2a11e0494e064cc631ce
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Don't build OpenAL with the project's warning flags
set_directory_properties(PROPERTIES COMPILE_OPTIONS "")

set(LIBTYPE                STATIC CACHE STRING "" FORCE)
set(ALSOFT_UTILS           OFF    CACHE BOOL "" FORCE)
set(ALSOFT_EXAMPLES        OFF    CACHE BOOL "" FORCE)
set(ALSOFT_TESTS           OFF    CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL         OFF    CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_CONFIG  OFF    CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_HRTF_DATA OFF  CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "" FORCE)
set(ALSOFT_NO_CONFIG_UTIL  ON     CACHE BOOL "" FORCE)
set(ALSOFT_UPDATE_BUILD_VERSION OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(OpenALSoft)

if(TARGET OpenAL)
    set_target_properties(OpenAL PROPERTIES FOLDER "Libs/external")
endif()
