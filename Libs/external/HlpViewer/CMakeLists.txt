cmake_minimum_required(VERSION 3.14)
project(HlpViewer_external NONE)

# Rust HLP Viewer (https://github.com/NullMagic2/Rust-HLP-Viewer, MIT) opens the game's help file (Jones3D.hlp)
# from the in-game help menu, since current Windows versions and Linux don't come with a WinHelp viewer.
# It's a separate program built with cargo for the host and copied next to the game executable,
# where JonesHud_OpenHelp looks for it.
# Set JONES3D_HLP_VIEWER_SOURCE_DIR to use a local source tree instead of downloading.

include(ExternalProject)

set(HLP_VIEWER_COMMIT a685ecc2fe8d040750ada76d42a8bc6f5a508f7d)
set(JONES3D_HLP_VIEWER_SOURCE_DIR "" CACHE PATH "Local Rust HLP Viewer source tree, downloaded when empty")

if(JONES3D_HLP_VIEWER_SOURCE_DIR)
    set(HLP_VIEWER_SOURCE SOURCE_DIR ${JONES3D_HLP_VIEWER_SOURCE_DIR})
    set(HLP_VIEWER_PATCH "")
else()
    set(HLP_VIEWER_SOURCE
        URL      https://github.com/NullMagic2/Rust-HLP-Viewer/archive/${HLP_VIEWER_COMMIT}.tar.gz
        URL_HASH SHA256=e8bb90de8e4ec88d008d3a410108f922082eaea7acad4e3f8aa4f263f1cfc9a6
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    # In this commit the workspace manifest was uploaded as LICENSE and Cargo.toml holds a PowerShell script
    set(HLP_VIEWER_PATCH PATCH_COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/LICENSE <SOURCE_DIR>/Cargo.toml)
endif()

set(HLP_VIEWER_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/cargo)
set(HLP_VIEWER_EXE ${HLP_VIEWER_TARGET_DIR}/release/hlp-viewer${CMAKE_EXECUTABLE_SUFFIX})

ExternalProject_Add(hlp_viewer
    ${HLP_VIEWER_SOURCE}
    ${HLP_VIEWER_PATCH}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ${JONES3D_CARGO} build --release --locked -p hlp-viewer --target-dir ${HLP_VIEWER_TARGET_DIR}
    BUILD_IN_SOURCE   TRUE
    INSTALL_COMMAND   ""
    BUILD_BYPRODUCTS  ${HLP_VIEWER_EXE}
    USES_TERMINAL_BUILD TRUE
)

set_target_properties(hlp_viewer PROPERTIES FOLDER "Libs/external")
set_property(GLOBAL PROPERTY JONES3D_HLP_VIEWER_EXE ${HLP_VIEWER_EXE})
