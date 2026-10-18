#include "Indy3D.h"
#include <j3dcore/j3dhook.h>
#include <Jones3D/RTI/symbols.h>
#include <Jones3D/Gui/wx/JonesWxDialogs.h>
#include <Jones3D/Main/JonesMain.h>

#include <wkernel/wkernel.h>

#include <stdio.h>
#include <string.h>

static const char* appName = "Open Jones 3D";

static bool Indy3D_RemoveDirect3D3ResolutionCap(void);

void Indy3D_InstallHooks(void)
{
}

void Indy3D_InitProcess(void)
{
    // Enable HIDPI
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Remove DX6 resolutions cap
    if ( !Indy3D_RemoveDirect3D3ResolutionCap() )
    {
        printf("WARNING: Failed to remove Direct3D3 cap on width/height of DirectDrawSurface!\n");
        JonesWx_ShowMessageBox("Jones3D", "Failed to remove DirectX 6 draw surface resolution cap!", /*bError=*/0);
    }
}

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
    JonesMain_g_mainMutex = CreateMutex(NULL, /*CreateMutex=*/TRUE, "INDY3D");
    if ( GetLastError() != ERROR_ALREADY_EXISTS )
    {
        wkernel_SetProcessProc(JonesMain_Process);
        wkernel_SetStartupCallback(Indy3D_Startup);
        wkernel_SetShutdownCallback(JonesMain_Shutdown);
        int result = wkernel_Run(hInstance, hPrevInstance, lpCmdLine, nShowCmd, appName);
        JonesWx_Shutdown();
        return result;
    }

    CloseHandle(JonesMain_g_mainMutex);
    return -1;
}

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