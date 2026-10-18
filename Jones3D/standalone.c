// Standalone OpenJones3D entry point.
// Runs the reimplemented game directly, without injecting Jones3D.dll into Indy3D.exe.
// The original Indy3D.exe is still required, but only as a data file for its dialog and icon resources.
#include "types.h" // Should be included first due to Windows.h / CommCtrl.h conflict
#include "Indy3D.h"

#include <wkernel/wkernel.h>

#include <stdio.h>
#include <string.h>

#define INDY3D_RESOURCE_MODULE "Indy3D.exe"
#define INDY3D_RESOURCE_PROBE  114 // Display settings dialog

static HMODULE Standalone_LoadResourceModuleFrom(const char* pPath)
{
    HMODULE hModule = LoadLibraryExA(pPath, NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if ( !hModule ) {
        return NULL;
    }

    // Make sure the module carries the game dialogs
    if ( !FindResourceA(hModule, MAKEINTRESOURCEA(INDY3D_RESOURCE_PROBE), (LPCSTR)RT_DIALOG) )
    {
        FreeLibrary(hModule);
        return NULL;
    }

    return hModule;
}

static HMODULE Standalone_LoadResourceModule(void)
{
    // First look next to this exe, then in the working directory
    char aPath[MAX_PATH] = { 0 };
    DWORD len = GetModuleFileNameA(NULL, aPath, MAX_PATH);
    if ( len > 0 && len < MAX_PATH )
    {
        char* pSep = strrchr(aPath, '\\');
        if ( pSep && (size_t)(pSep + 1 - aPath) + sizeof(INDY3D_RESOURCE_MODULE) <= MAX_PATH )
        {
            strcpy_s(pSep + 1, MAX_PATH - (pSep + 1 - aPath), INDY3D_RESOURCE_MODULE);
            HMODULE hModule = Standalone_LoadResourceModuleFrom(aPath);
            if ( hModule ) {
                return hModule;
            }
        }
    }

    return Standalone_LoadResourceModuleFrom(INDY3D_RESOURCE_MODULE);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    HMODULE hResModule = Standalone_LoadResourceModule();
    if ( !hResModule )
    {
        MessageBoxA(NULL,
            "Can't find " INDY3D_RESOURCE_MODULE "!\n\nPlace this program in the game's Resource folder next to " INDY3D_RESOURCE_MODULE ".",
            "OpenJones3D Error", MB_ICONERROR | MB_OK
        );
        return -1;
    }

    wkernel_SetResourceInstance(hResModule);
    Indy3D_InitProcess();

    int result = Indy3D_WinMain(hInstance, hPrevInstance, lpCmdLine, nShowCmd);

    wkernel_SetResourceInstance(NULL);
    FreeLibrary(hResModule);
    return result;
}
