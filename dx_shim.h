/* Force-included shim so MinGW-w64's legacy DirectX 6-era headers (ddraw.h/dsound.h/
 * dinput.h/dplay.h/d3dtypes.h) can be used on top of the real Windows SDK's windows.h
 * (from xwin) instead of MinGW's own windows.h, whose winnt.h hits a clang i686-msvc
 * intrinsic-header bug on this system. WINBOOL is a MinGW-only alias for BOOL/int that
 * the real SDK never defines; _mingw_dxhelper.h supplies the WINELIB_NAME_AW-style
 * macros ddraw.h/dinput.h expect and is otherwise self-contained (no windows.h pulled in). */
#ifndef DX_SHIM_H
#define DX_SHIM_H
#ifndef WINBOOL
typedef int WINBOOL;
#endif
#include <_mingw_dxhelper.h>
#include <_mingw_mac.h>        /* __MSABI_LONG(), used by d3dtypes.h-era constants */
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0500 /* DX6.1-era DirectInput; dinput.h defaults to 0x0800 (DI8) otherwise */
#endif
#endif
