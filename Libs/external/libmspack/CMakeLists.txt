cmake_minimum_required(VERSION 3.14)
project(libmspack_external C)

# libmspack (LGPL-2.1) reads the CHM help file of the GOG release in the wxWidgets help viewer
# (wxUSE_LIBMSPACK, see Jones3D/Gui/wx/JonesWxHelp.cpp). Only its CHM decompressor is built, as a static library.
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
target_compile_options(mspack PRIVATE -w)
target_compile_definitions(mspack PRIVATE _FILE_OFFSET_BITS=64 LARGEFILE_SUPPORT=1)
target_include_directories(mspack PRIVATE ${MSPACK_DIR})
target_include_directories(mspack SYSTEM PUBLIC ${MSPACK_DIR})

# Found by wxWidgets' FindMSPACK.cmake (find_path/find_library keep these cached values).
# wx needs the library file, not a target, Libs/external/wxWidgets adds the build dependency.
set_target_properties(mspack PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(MSPACK_INCLUDE_DIR ${MSPACK_DIR} CACHE PATH "" FORCE)
set(MSPACK_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}mspack${CMAKE_STATIC_LIBRARY_SUFFIX} CACHE FILEPATH "" FORCE)
