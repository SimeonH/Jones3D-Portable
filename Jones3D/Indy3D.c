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

#include <std/Gpu/stdGpu.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static const char* appName = "Open Jones 3D";

void Indy3D_InstallHooks(void)
{
}

void Indy3D_InitProcess(void)
{
#ifdef _WIN32
    // Enable HIDPI, also for the dialogs shown before SDL is initialized
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
}

// Returns true when the data files the game can't start without are found in aDirs, see JonesFile_Open for the searched resource paths
static bool Indy3D_HasGameData(const char* const* aDirs, size_t numDirs)
{
#ifdef _WIN32
    // The standalone exe reads the game's dialogs from Indy3D.exe
    static const char* aFiles[] = { "Jones3D.GOB", "CD1.GOB", "CD2.GOB", "Indy3D.exe" };
#else
    static const char* aFiles[] = { "Jones3D.GOB", "CD1.GOB", "CD2.GOB" };
#endif
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
        if ( *p == '/' || *p == '\\' || *p == 0 )
        {
            const char c = *p;
            *p = 0;
#ifdef _WIN32
            _mkdir(aPath); // Fails when it exists or for the drive root
#else
            mkdir(aPath, 0755); // Fails when it exists
#endif
            *p = c;
            if ( !c ) {
                break;
            }
        }
    }
}

static bool Indy3D_IsAbsolutePath(const char* pPath)
{
#ifdef _WIN32
    return (pPath[0] && pPath[1] == ':') || (pPath[0] == '\\' && pPath[1] == '\\');
#else
    return pPath[0] == '/';
#endif
}

// Default install dir: ~/.Jones3D, on Windows the folder of the executable like the original installation
static void Indy3D_GetDefaultInstallPath(char* pPath, size_t size)
{
#ifdef _WIN32
    char aExePath[MAX_PATH] = { 0 };
    DWORD len = GetModuleFileNameA(NULL, aExePath, MAX_PATH);
    char* pSep = len && len < MAX_PATH ? strrchr(aExePath, '\\') : NULL;
    if ( pSep ) {
        *pSep = '\0';
    }

    // An executable in the Resource folder of an existing installation (next to the GOB files)
    // uses the game folder above it, so the saved games stay in the game folder's SaveGames
    const char* aExeDir[] = { pSep ? aExePath : "." };
    if ( pSep && Indy3D_HasGameData(aExeDir, 1) )
    {
        char* pParentSep = strrchr(aExePath, '\\');
        if ( pParentSep && pParentSep > aExePath && pParentSep[-1] != ':' ) {
            *pParentSep = '\0';
        }
    }

    stdUtil_StringCopy(pPath, size, pSep ? aExePath : ".");
#else
    const char* pHome = getenv("HOME");
    stdUtil_Format(pPath, size, "%s/.Jones3D", pHome && pHome[0] ? pHome : ".");
#endif
}

bool Indy3D_EnsureGameData(bool bForceInstaller, char* pInstallPathOut, size_t outSize)
{
    // Without the settings the data is still checked in the default install dir, JonesMain_Startup reports the settings error
    const bool bSettings = wuRegistry_Startup(JONESMAIN_SETTINGS_ROOTKEY, JONESMAIN_SETTINGS_KEY) == 0;
    if ( !bSettings ) {
        wuRegistry_Shutdown(); // Startup marks the settings started also when it fails
    }

    // The game data lives in the default install dir unless the 'Install Path' setting says otherwise
    static const char aNotSet[] = "\x01"; // Can't be a path
    char aInstallPath[128] = { 0 };
    if ( bSettings ) {
        wuRegistry_GetStr("Install Path", aInstallPath, STD_ARRAYLEN(aInstallPath), aNotSet);
    }
    else {
        stdUtil_StringCopy(aInstallPath, sizeof(aInstallPath), aNotSet);
    }

    const bool bNewConfig = strcmp(aInstallPath, aNotSet) == 0;
    if ( bNewConfig || !aInstallPath[0] )
    {
        Indy3D_GetDefaultInstallPath(aInstallPath, sizeof(aInstallPath));
        if ( bSettings ) {
            wuRegistry_SaveStr("Install Path", aInstallPath);
        }
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
    if ( bSettings && (bNewConfig || bInstalled) && wuRegistry_GetInt("Start Mode", -1) == -1 ) {
        wuRegistry_SaveInt("Start Mode", JONES_STARTMODE_STARTGAME);
    }

    // When the data isn't in the working dir (e.g. the game was started from the desktop menu), run in the install dir,
    // so the log files (JonesLog.txt) are written there. Not the Resource dir, as the working dir is also searched
    // for GOB files, which would open them twice. A relative install path is relative to the working dir, so it's kept then.
    const char* aWorkDir[] = { "." };
    if ( bOk && Indy3D_IsAbsolutePath(aInstallPath) && !Indy3D_HasGameData(aWorkDir, 1) )
    {
        char aNativePath[256];
        stdPlatform_NativePath(aInstallPath, aNativePath, sizeof(aNativePath));
#ifdef _WIN32
        if ( _chdir(aNativePath) != 0 ) {
#else
        if ( chdir(aNativePath) != 0 ) {
#endif
            fprintf(stderr, "WARNING: Couldn't change working directory to %s\n", aNativePath);
        }
    }

    // The game window is created for the renderer of the display device in the settings,
    // switching the renderer later replaces the window
    if ( bSettings )
    {
        char aDisplay[128] = { 0 };
        wuRegistry_GetStr("Display", aDisplay, STD_ARRAYLEN(aDisplay), "");
        stdGpu_SetPreferredDriver(aDisplay);
    }

    if ( pInstallPathOut ) {
        stdUtil_StringCopy(pInstallPathOut, outSize, aInstallPath);
    }

    if ( bSettings ) {
        wuRegistry_Shutdown();
    }
    return bOk;
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
