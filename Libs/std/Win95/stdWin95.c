
#include "stdWin95.h"

#include <j3dcore/j3dhook.h>
#include <std/General/std.h>
#include <std/RTI/symbols.h>

#include <SDL3/SDL_video.h>

static GUID stdWin95_guid;
static HWND stdWin95_hwnd = NULL;
static HINSTANCE stdWin95_hInstance = NULL;

void stdWin95_InstallHooks(void)
{
    J3D_HOOKFUNC(stdWin95_SetWindow);
    J3D_HOOKFUNC(stdWin95_GetWindow);
    J3D_HOOKFUNC(stdWin95_SetInstance);
    J3D_HOOKFUNC(stdWin95_GetInstance);
    J3D_HOOKFUNC(stdWin95_SetGuid);
}


void J3DAPI stdWin95_SetWindow(HWND hwnd)
{
    stdWin95_hwnd = hwnd;
}

HWND J3DAPI stdWin95_GetWindow()
{
    return stdWin95_hwnd;
}

void J3DAPI stdWin95_SetInstance(HINSTANCE hinstance)
{
    stdWin95_hInstance = hinstance;
}

HINSTANCE stdWin95_GetInstance(void)
{
    return stdWin95_hInstance;
}

void J3DAPI stdWin95_SetGuid(const GUID* pGuid)
{
    STD_ASSERTREL(pGuid);
    stdWin95_guid = *pGuid;
}

const GUID* J3DAPI stdWin95_GetGuid(void)
{
    return &stdWin95_guid;
}

static StdWindowApi stdWin95_preferredWindowApi       = STDWINDOW_API_VULKAN;
static StdWin95SetWindowApiFunc stdWin95_pfSetWindowApi = NULL;

void stdWin95_SetPreferredWindowApi(StdWindowApi api)
{
    stdWin95_preferredWindowApi = api;
}

StdWindowApi stdWin95_GetPreferredWindowApi(void)
{
    return stdWin95_preferredWindowApi;
}

void stdWin95_SetWindowApiFunc(StdWin95SetWindowApiFunc pfFunc)
{
    stdWin95_pfSetWindowApi = pfFunc;
}

HWND stdWin95_SetWindowApi(StdWindowApi api)
{
    stdWin95_preferredWindowApi = api;
    if ( stdWin95_pfSetWindowApi ) {
        stdWin95_hwnd = stdWin95_pfSetWindowApi(api);
    }

    return stdWin95_hwnd;
}

#ifdef _WIN32
HWND stdWin95_GetNativeWindow(void)
{
    if ( !stdWin95_hwnd ) {
        return NULL;
    }

    return (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties((SDL_Window*)stdWin95_hwnd), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
}
#endif
