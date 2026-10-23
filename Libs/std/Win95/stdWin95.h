#ifndef STD_STDWIN95_H
#define STD_STDWIN95_H
#include <j3dcore/j3d.h>
#include <std/types.h>
#include <std/RTI/addresses.h>

J3D_EXTERN_C_START

void J3DAPI stdWin95_SetWindow(HWND hwnd);
HWND J3DAPI stdWin95_GetWindow();
void J3DAPI stdWin95_SetInstance(HINSTANCE hinstance);
HINSTANCE stdWin95_GetInstance(void);
void J3DAPI stdWin95_SetGuid(const GUID* pGuid);
const GUID* J3DAPI stdWin95_GetGuid(void); // Added

// Added: The game window is an SDL window on all platforms, the HWND of the game is its SDL_Window* (see wkernelSDL.c)

// Graphics API the SDL game window is created for, SDL can't use another API on a window (Added)
typedef enum eStdWindowApi
{
    STDWINDOW_API_VULKAN = 0,
    STDWINDOW_API_OPENGL = 1,
    STDWINDOW_API_NATIVE = 2, // Plain window, e.g. for a DXGI swap chain
} StdWindowApi;

// Recreates the game window for api if it was created for another one, returns the game window
typedef HWND (*StdWin95SetWindowApiFunc)(StdWindowApi api);

void stdWin95_SetPreferredWindowApi(StdWindowApi api); // API of the game window created at startup
StdWindowApi stdWin95_GetPreferredWindowApi(void);
void stdWin95_SetWindowApiFunc(StdWin95SetWindowApiFunc pfFunc);
HWND stdWin95_SetWindowApi(StdWindowApi api);

#ifdef _WIN32
// Win32 window handle of the game window (Added). It changes when the game window is recreated for another API.
HWND stdWin95_GetNativeWindow(void);
#endif

// Helper hooking functions
void stdWin95_InstallHooks(void);

J3D_EXTERN_C_END
#endif // STD_STDWIN95_H
