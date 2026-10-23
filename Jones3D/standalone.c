// Windows entry point of OpenJones3D.exe.
// The original Indy3D.exe is still required, but only as a data file for its icon resources.
// When the game data isn't found, the data installer copies it next to this exe (see Indy3D_EnsureGameData).
#include "types.h" // Should be included first due to Windows.h / CommCtrl.h conflict
#include "Indy3D.h"
#include <Jones3D/Gui/wx/JonesWxDialogs.h>

#include <Jones3D/Main/JonesMain.h>
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

static HMODULE Standalone_LoadResourceModule(const char* pInstallPath)
{
    // First look in the Resource folder of the install dir, then next to this exe, then in the working directory
    if ( pInstallPath && pInstallPath[0] )
    {
        char aResPath[MAX_PATH];
        if ( sprintf_s(aResPath, sizeof(aResPath), "%s\\Resource\\%s", pInstallPath, INDY3D_RESOURCE_MODULE) > 0 )
        {
            HMODULE hModule = Standalone_LoadResourceModuleFrom(aResPath);
            if ( hModule ) {
                return hModule;
            }
        }
    }

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
    Indy3D_InitProcess();

    // -help "<file>" shows the help file, started by the game's Help (JonesHud_OpenHelp)
    if ( __argc == 3 && strcmp(__argv[1], "-help") == 0 ) {
        return JonesWx_ShowHelp(__argv[2]);
    }

    // -installdata opens the game data installer even when the data is already installed
    char aCmdLine[1024] = { 0 };
    strcpy_s(aCmdLine, sizeof(aCmdLine), lpCmdLine ? lpCmdLine : "");
    bool bInstallData = false;
    char* pArg = strstr(aCmdLine, "-installdata");
    if ( pArg )
    {
        bInstallData = true;
        const char* pRest = pArg + strlen("-installdata");
        memmove(pArg, pRest, strlen(pRest) + 1);
        while ( aCmdLine[0] == ' ' ) {
            memmove(aCmdLine, aCmdLine + 1, strlen(aCmdLine));
        }
    }

    char aInstallPath[128] = { 0 };
    if ( !Indy3D_EnsureGameData(bInstallData, aInstallPath, sizeof(aInstallPath)) )
    {
        JonesWx_Shutdown();
        return 0;
    }

    HMODULE hResModule = Standalone_LoadResourceModule(aInstallPath);
    if ( !hResModule )
    {
        JonesWx_ShowMessageBox("OpenJones3D Error",
            "Can't find " INDY3D_RESOURCE_MODULE "!\n\nRun this program with -installdata to install the game data, "
            "or place it in the game's Resource folder next to " INDY3D_RESOURCE_MODULE ".",
            /*bError=*/1
        );
        JonesWx_Shutdown();
        return -1;
    }

    wkernel_SetResourceInstance(hResModule);

    int result = Indy3D_WinMain(hInstance, hPrevInstance, aCmdLine, nShowCmd);

    wkernel_SetResourceInstance(NULL);
    FreeLibrary(hResModule);
    return result;
}
