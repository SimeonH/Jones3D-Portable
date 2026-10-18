# CMake toolchain for cross-compiling OpenJones3D (32-bit MSVC-ABI target)
# using clang-cl + lld-link on Linux, with headers/import libs supplied by
# `xwin` (https://github.com/Jake-Shadle/xwin) instead of a real Windows SDK install.
#
# Usage:
#   cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=toolchain-clang-cl-win32.cmake ..

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

set(XWIN_SDK_DIR "$ENV{HOME}/Projects/xwin-sdk" CACHE PATH "Path to xwin splat output")

set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER lld-link)
set(CMAKE_RC_COMPILER llvm-rc)

set(CMAKE_C_COMPILER_TARGET i686-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET i686-pc-windows-msvc)

# Force-include a small shim (see dx_shim.h) so MinGW-w64's legacy DirectX 6-era headers
# (ddraw.h/dsound.h/dinput.h/dplay.h/d3dtypes.h), which are only used for the handful of
# DirectX types/declarations this project needs, can layer on top of the REAL Windows
# SDK's windows.h (from xwin) rather than MinGW's own -- whose winnt.h hits a clang
# i686-pc-windows-msvc intrinsic-header bug on this system's clang build.
set(CMAKE_C_FLAGS_INIT "/FI\"${CMAKE_CURRENT_LIST_DIR}/dx_shim.h\"")
set(CMAKE_CXX_FLAGS_INIT "/FI\"${CMAKE_CURRENT_LIST_DIR}/dx_shim.h\"")

# MSVC is loose about passing concrete COM interface pointers (LPDIRECTDRAWSURFACE4* etc.)
# where QueryInterface-style APIs take void**, and about implicit int<->long on Win32
# (where both are 32-bit); recent clang treats both as hard errors by default. Downgrade to
# warnings rather than hand-patching decompiled source during initial bring-up.
string(APPEND CMAKE_C_FLAGS_INIT " -Wno-error=incompatible-pointer-types -Wno-error=incompatible-function-pointer-types")
string(APPEND CMAKE_CXX_FLAGS_INIT " -Wno-error=incompatible-pointer-types -Wno-error=incompatible-function-pointer-types")

set(_xwin_includes
    # Real Windows SDK (xwin) first, so unqualified `#include <windows.h>` inside MinGW's
    # DirectX headers below resolves to this (working) one rather than MinGW's own.
    "${XWIN_SDK_DIR}/crt/include"
    "${XWIN_SDK_DIR}/sdk/include/ucrt"
    "${XWIN_SDK_DIR}/sdk/include/um"
    "${XWIN_SDK_DIR}/sdk/include/shared"
    "${XWIN_SDK_DIR}/sdk/include/winrt"
    # MinGW-w64's legacy DirectX 6-era headers themselves (ddraw.h/dsound.h/dinput.h/dplay.h).
    "/usr/i686-w64-mingw32/include"
)
set(_xwin_libs
    "${XWIN_SDK_DIR}/crt/lib/x86"
    "${XWIN_SDK_DIR}/sdk/lib/um/x86"
    "${XWIN_SDK_DIR}/sdk/lib/ucrt/x86"
)

foreach(_inc ${_xwin_includes})
    string(APPEND CMAKE_C_FLAGS_INIT " /imsvc\"${_inc}\"")
    string(APPEND CMAKE_CXX_FLAGS_INIT " /imsvc\"${_inc}\"")
endforeach()

foreach(_lib ${_xwin_libs})
    string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " /libpath:\"${_lib}\"")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " /libpath:\"${_lib}\"")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS_INIT " /libpath:\"${_lib}\"")
endforeach()

# clang-cl's /winsysroot flag is simpler than per-include /imsvc, but requires xwin's
# layout to match exactly; the explicit /imsvc + /libpath approach above is more robust
# across xwin versions. Uncomment to try /winsysroot instead:
# string(APPEND CMAKE_C_FLAGS_INIT " /winsysroot\"${XWIN_SDK_DIR}\"")
# string(APPEND CMAKE_CXX_FLAGS_INIT " /winsysroot\"${XWIN_SDK_DIR}\"")

set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)

set(CMAKE_FIND_ROOT_PATH "${XWIN_SDK_DIR}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
