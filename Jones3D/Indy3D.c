#include "Indy3D.h"
#include <j3dcore/j3dhook.h>
#include <Jones3D/RTI/symbols.h>
#include <Jones3D/Gui/wx/JonesWxDialogs.h>
#include <Jones3D/Main/JonesMain.h>

#include <std/General/stdFileUtil.h>
#include <std/General/stdFnames.h>
#include <std/General/stdPlatform.h>
#include <std/General/stdUtil.h>
#include <w32util/wuRegistry.h>
#include <wkernel/wkernel.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

static const char* appName = "Open Jones 3D";

#ifdef _WIN32
static bool Indy3D_RemoveDirect3D3ResolutionCap(void);
#endif

void Indy3D_InstallHooks(void)
{
}

void Indy3D_InitProcess(void)
{
#ifdef _WIN32
    // Enable HIDPI
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Remove DX6 resolutions cap
    if ( !Indy3D_RemoveDirect3D3ResolutionCap() )
    {
        printf("WARNING: Failed to remove Direct3D3 cap on width/height of DirectDrawSurface!\n");
        JonesWx_ShowMessageBox("Jones3D", "Failed to remove DirectX 6 draw surface resolution cap!", /*bError=*/0);
    }
#endif
}

#ifndef _WIN32

// Returns true when the data files the game can't start without are found in aDirs, see JonesFile_Open for the searched resource paths
static bool Indy3D_HasGameData(const char* const* aDirs, size_t numDirs)
{
    static const char* aFiles[] = { "Jones3D.GOB", "CD1.GOB", "CD2.GOB" };
    for ( size_t i = 0; i < STD_ARRAYLEN(aFiles); ++i )
    {
        bool bFound = false;
        for ( size_t j = 0; j < numDirs && !bFound; ++j )
        {
            char aPath[256];
            STD_MAKEPATH(aPath, aDirs[j], aFiles[i]);
            bFound = stdFileUtil_FileExists(aPath) != 0;
        }

        if ( !bFound ) {
            return false;
        }
    }

    return true;
}

// Creates dir pPath including its parent dirs
static void Indy3D_MakeDirs(const char* pPath)
{
    char aPath[256];
    stdPlatform_NativePath(pPath, aPath, sizeof(aPath));
    for ( char* p = aPath + 1; ; ++p )
    {
        if ( *p == '/' || *p == 0 )
        {
            const char c = *p;
            *p = 0;
            mkdir(aPath, 0755); // Fails when it exists
            *p = c;
            if ( !c ) {
                break;
            }
        }
    }
}

bool Indy3D_EnsureGameData(bool bForceInstaller)
{
    if ( wuRegistry_Startup(HKEY_LOCAL_MACHINE, "Software\\LucasArts Entertainment Company LLC\\Indiana Jones and the Infernal Machine\\v1.0") ) {
        return true; // JonesMain_Startup reports the error
    }

    // The game data lives in ~/.Jones3D unless the 'Install Path' setting says otherwise
    static const char aNotSet[] = "\x01"; // Can't be a path
    char aInstallPath[128] = { 0 };
    wuRegistry_GetStr("Install Path", aInstallPath, STD_ARRAYLEN(aInstallPath), aNotSet);
    const bool bNewConfig = strcmp(aInstallPath, aNotSet) == 0;
    if ( bNewConfig || !aInstallPath[0] )
    {
        const char* pHome = getenv("HOME");
        STD_FORMAT(aInstallPath, "%s/.Jones3D", pHome && pHome[0] ? pHome : ".");
        wuRegistry_SaveStr("Install Path", aInstallPath);
    }

    // The game creates SaveGames and ScreenShots in the install dir with a single-level mkdir, make sure the install dir exists,
    // also when the data is used from the working dir and the data installer never ran
    Indy3D_MakeDirs(aInstallPath);

    char aResourceDir[128];
    STD_MAKEPATH(aResourceDir, aInstallPath, "Resource");
    const char* aDirs[] = { aResourceDir, aInstallPath, "." };

    bool bOk        = true;
    bool bInstalled = false;
    if ( bForceInstaller || !Indy3D_HasGameData(aDirs, STD_ARRAYLEN(aDirs)) )
    {
        char aNativePath[256];
        bOk        = JonesWx_ShowDataInstaller(stdPlatform_NativePath(aInstallPath, aNativePath, sizeof(aNativePath))) != 0;
        bInstalled = bOk;
    }

    // The original installer set up the game to start normally, without it the developer dialog is shown first
    if ( (bNewConfig || bInstalled) && wuRegistry_GetInt("Start Mode", -1) == -1 ) {
        wuRegistry_SaveInt("Start Mode", JONES_STARTMODE_STARTGAME);
    }

    // When the data isn't in the working dir (e.g. the game was started from the desktop menu), run in the install dir,
    // so the log files (JonesLog.txt) are written there. Not the Resource dir, as the working dir is also searched
    // for GOB files, which would open them twice. A relative install path is relative to the working dir, so it's kept then.
    const char* aWorkDir[] = { "." };
    if ( bOk && aInstallPath[0] == '/' && !Indy3D_HasGameData(aWorkDir, 1) )
    {
        char aNativePath[256];
        stdPlatform_NativePath(aInstallPath, aNativePath, sizeof(aNativePath));
        if ( chdir(aNativePath) != 0 ) {
            fprintf(stderr, "WARNING: Couldn't change working directory to %s\n", aNativePath);
        }
    }

    wuRegistry_Shutdown();
    return bOk;
}

#endif // !_WIN32

static int Indy3D_Startup(const char* aCmd)
{
    if ( JonesMain_Startup(aCmd) )
    {
        JonesMain_Shutdown();
        return -1;
    }
    return 0;
}

// Indy3D.exe entry point
int WINAPI Indy3D_WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
#ifdef _WIN32
    JonesMain_g_mainMutex = CreateMutex(NULL, /*CreateMutex=*/TRUE, "INDY3D");
    if ( GetLastError() != ERROR_ALREADY_EXISTS )
#endif
    {
        wkernel_SetProcessProc(JonesMain_Process);
        wkernel_SetStartupCallback(Indy3D_Startup);
        wkernel_SetShutdownCallback(JonesMain_Shutdown);
        int result = wkernel_Run(hInstance, hPrevInstance, lpCmdLine, nShowCmd, appName);
        JonesWx_Shutdown();
        return result;
    }

#ifdef _WIN32
    CloseHandle(JonesMain_g_mainMutex);
    return -1;
#endif
}

#ifdef _WIN32

static void* Indy3D_memfind(const void* l, size_t l_len, const void* s, size_t s_len)
{
    register char* cur, * last;
    const char* cl = (const char*)l;
    const char* cs = (const char*)s;

    if ( !l_len || !s_len )
        return NULL;

    if ( l_len < s_len )
        return NULL;

    // Special case where s_len == 1
    if ( s_len == 1 )
        return memchr(l, (int)*cs, l_len);

    // The last position where it's possible to find "s" in "l"
    last = (char*)cl + l_len - s_len;
    for ( cur = (char*)cl; cur <= last; cur++ )
        if ( cur[0] == cs[0] && !memcmp(cur, cs, s_len) )
            return cur;

    return NULL;
}

/**
 * @brief Removes IDirect3D3 2048-pixel resolution cap on device creation
 *
 * @details Loads d3dim.dll and patches in memory IDirect3D3_CreateDevice function
 *          to remove the 2048-pixel width/height limit check when creating a Direct3D device
 *          from DirectDrawSurface, allowing rendering to surfaces larger than 2048x2048.
 *
 * @return true if resolution cap was successfully removed, false if operation failed
 */
static bool Indy3D_RemoveDirect3D3ResolutionCap(void)
{
    // From: https://github.com/UCyborg/LegacyD3DResolutionHack

    static const BYTE wantedBytes[] = { 0xB8, 0x00, 0x08, 0x00, 0x00, 0x39 };

    char szPath[MAX_PATH];
    GetSystemDirectory(szPath, MAX_PATH);
    strcat_s(szPath, sizeof(szPath), "\\d3dim.dll");

    HMODULE hD3DIm = LoadLibrary(szPath);
    if ( !hD3DIm ) {
        return false;
    }

    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hD3DIm;
    PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((char*)pDosHeader + pDosHeader->e_lfanew);
    DWORD dwCodeBase = (DWORD)hD3DIm + pNtHeader->OptionalHeader.BaseOfCode;
    DWORD dwCodeSize = pNtHeader->OptionalHeader.SizeOfCode;

    uintptr_t dwPatchBase = (uintptr_t)Indy3D_memfind((void*)dwCodeBase, dwCodeSize, wantedBytes, sizeof(wantedBytes));
    if ( dwPatchBase )
    {
        dwPatchBase++;

        DWORD dwOldProtect;
        VirtualProtect((LPVOID)dwPatchBase, 4, PAGE_EXECUTE_READWRITE, &dwOldProtect);
        *(DWORD*)dwPatchBase = (DWORD)-1;
        VirtualProtect((LPVOID)dwPatchBase, 4, dwOldProtect, &dwOldProtect);
    }

    return true;
}

#endif // _WIN32
