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

#ifndef _WIN32
// Graphics API the SDL game window is created for, SDL can't use another API on a window (Added)
typedef enum eStdWindowApi
{
    STDWINDOW_API_VULKAN = 0,
    STDWINDOW_API_OPENGL = 1,
} StdWindowApi;

// Recreates the game window for api if it was created for another one, returns the game window
typedef HWND (*StdWin95SetWindowApiFunc)(StdWindowApi api);

void stdWin95_SetPreferredWindowApi(StdWindowApi api); // API of the game window created at startup
StdWindowApi stdWin95_GetPreferredWindowApi(void);
void stdWin95_SetWindowApiFunc(StdWin95SetWindowApiFunc pfFunc);
HWND stdWin95_SetWindowApi(StdWindowApi api);
#endif

// Helper hooking functions
void stdWin95_InstallHooks(void);

J3D_EXTERN_C_END
#endif // STD_STDWIN95_H
