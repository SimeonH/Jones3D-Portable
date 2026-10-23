cmake_minimum_required(VERSION 3.14)
project(libmspack_external C)

# libmspack (LGPL-2.1) unpacks the CHM help file of the GOG release for the wxWidgets help viewer
# (see Jones3D/Gui/wx/JonesWxChm.cpp). Only its CHM decompressor is built, as a static library.
# It's built from source because 32-bit packages aren't commonly available.
# Set FETCHCONTENT_SOURCE_DIR_LIBMSPACK to use a local source tree instead of downloading.

include(FetchContent)

FetchContent_Declare(libmspack
    URL      https://github.com/kyz/libmspack/archive/refs/tags/v1.11.tar.gz
    URL_HASH SHA256=284e0e51fee991c40e8cbbc0f5e635f99722796a3642a14397989fc00b862f72
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _none # built below, upstream uses autotools
)
FetchContent_MakeAvailable(libmspack)

set(MSPACK_DIR ${libmspack_SOURCE_DIR}/libmspack/mspack)
add_library(mspack STATIC
    ${MSPACK_DIR}/system.c
    ${MSPACK_DIR}/chmd.c
    ${MSPACK_DIR}/lzxd.c
)

# Don't build libmspack with the project's warning flags
set_target_properties(mspack PROPERTIES COMPILE_OPTIONS "" POSITION_INDEPENDENT_CODE ON FOLDER "Libs/external")
if(MSVC)
    target_compile_options(mspack PRIVATE /w)
else()
    target_compile_options(mspack PRIVATE -w)
endif()
target_compile_definitions(mspack PRIVATE _FILE_OFFSET_BITS=64 LARGEFILE_SUPPORT=1)
target_include_directories(mspack PRIVATE ${MSPACK_DIR})
target_include_directories(mspack SYSTEM PUBLIC ${MSPACK_DIR})
