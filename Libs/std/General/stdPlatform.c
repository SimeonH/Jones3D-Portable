
#ifdef _WIN32
#define STRICT
#include <Windows.h> // timeGetTime, include first to avoid undefined warning due to WIN32_LEAN_AND_MEAN
#else
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <SDL3/SDL_messagebox.h>
#endif

#include "std.h"
#include "stdMemory.h"
#include "stdPlatform.h"
#include "stdUtil.h"

#include <j3dcore/j3dhook.h>
#include <std/RTI/symbols.h>

static bool stdPlatform_bAssert = false;

void stdPlatform_InstallHooks(void)
{
    J3D_HOOKFUNC(stdPlatform_InitServices);
    J3D_HOOKFUNC(stdPlatform_ClearServices);
    J3D_HOOKFUNC(stdPlatform_GetTimeMsec);
    J3D_HOOKFUNC(stdPlatform_Assert);
    J3D_HOOKFUNC(stdPlatform_Printf);
    J3D_HOOKFUNC(stdPlatform_AllocHandle);
    J3D_HOOKFUNC(stdPlatform_FreeHandle);
    J3D_HOOKFUNC(stdPlatform_ReallocHandle);
    J3D_HOOKFUNC(stdPlatform_LockHandle);
    J3D_HOOKFUNC(stdPlatform_UnlockHandle);
}


// TODO: stdFile* functions should be moved here to stdPlatform

int J3DAPI stdPlatform_InitServices(tHostServices* pHS)
{
    pHS->unknown1 = 1000.0f; // sec to msec converter constant

    pHS->pMessagePrint = stdPlatform_Printf;
    pHS->pStatusPrint  = stdPlatform_Printf;
    pHS->pWarningPrint = stdPlatform_Printf;
    pHS->pErrorPrint   = stdPlatform_Printf;
    pHS->pDebugPrint   = stdPlatform_Printf;

    pHS->pAssert = stdPlatform_Assert;
    pHS->pAtExit = NULL;

    pHS->pMalloc  = stdMemory_BlockMalloc;
    pHS->pFree    = stdMemory_BlockFree;
    pHS->pRealloc = stdMemory_BlockRealloc;

    pHS->pGetTimeMsec = stdPlatform_GetTimeMsec;

    pHS->pFileOpen      = stdFileOpen;
    pHS->pFileClose     = stdFileClose;
    pHS->pFileRead      = stdFileRead;
    pHS->pFileGets      = stdFileGets;
    pHS->pFileWrite     = stdFileWrite;
    pHS->pFileEOF       = stdFileEof;
    pHS->pFileTell      = stdFileTell;
    pHS->pFileSeek      = stdFileSeek;
    pHS->pFileSize      = stdFileSize;
    pHS->pFilePrintf    = stdFilePrintf;
    pHS->pFileGetws     = stdFileGetws;

    pHS->pAllocHandle   = stdPlatform_AllocHandle;
    pHS->pFreeHandle    = stdPlatform_FreeHandle;
    pHS->pReallocHandle = stdPlatform_ReallocHandle;

    pHS->pLockHandle   = stdPlatform_LockHandle;
    pHS->pUnlockHandle = stdPlatform_UnlockHandle;

#ifdef _WIN32
    return CoInitialize(0);
#else
    return 0;
#endif
}

void J3DAPI stdPlatform_ClearServices(tHostServices* pHS)
{
    memset(pHS, 0, sizeof(tHostServices));
#ifdef _WIN32
    CoUninitialize();
#endif
}

unsigned int stdPlatform_GetTimeMsec(void)
{
#ifdef _WIN32
    return timeGetTime(); // TODO: Use more precise timer
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned int)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
#endif
}



J3DNORETURN void J3DAPI stdPlatform_Assert(const char* pErrorStr, const char* pFilename, int linenum)
{
    if ( stdPlatform_bAssert )
    {
        DebugBreak();
        exit(1);
    }
    stdPlatform_bAssert = true;

    size_t pos     = 0;
    size_t fnpos   = 0;
    bool bFoundSep = false;

    for ( char chr = *pFilename; chr; ++pos )
    {
        if ( chr == '\\' || chr == '/' )
        {
            bFoundSep = true;
            fnpos     = pos;
        }

        chr = pFilename[pos + 1];
    }

    if ( bFoundSep )
    {
        ++fnpos;
    }

    char aText[512];
    STD_FORMAT(aText, "%s(%d):  %s\n", &pFilename[fnpos], linenum, pErrorStr);
    std_g_pHS->pErrorPrint("ASSERT: %s", aText);
#ifdef _WIN32
    MessageBoxA(NULL, aText, "Assert Handler", MB_TASKMODAL);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Assert Handler", aText, NULL);
#endif

    DebugBreak();
    exit(1);
}

int stdPlatform_Printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(std_g_genBuffer, sizeof(std_g_genBuffer), format, args);
    va_end(args);

#ifdef _WIN32
    OutputDebugStringA(std_g_genBuffer); // TODO: write also to printf
#else
    fputs(std_g_genBuffer, stderr);
#endif
    return 1;
}

void* J3DAPI stdPlatform_AllocHandle(size_t size)
{
    return malloc(size);
}

void J3DAPI stdPlatform_FreeHandle(void* pData)
{
    free(pData);
}

void* J3DAPI stdPlatform_ReallocHandle(void* pMemory, size_t newSize)
{
    return realloc(pMemory, newSize);
}

int J3DAPI stdPlatform_LockHandle(int a1)
{
    return a1;
}

void J3DAPI stdPlatform_UnlockHandle()
{

}

bool J3DAPI stdPlatform_DirExists(const char* pPath)
{
#ifdef _WIN32
    DWORD dwAttrib = GetFileAttributes(pPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    char aPath[1024];
    struct stat st;
    return stat(stdPlatform_NativePath(pPath, aPath, sizeof(aPath)), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

#ifdef _WIN32

const char* J3DAPI stdPlatform_NativePath(const char* pPath, char* pOut, size_t outSize)
{
    stdUtil_StringCopy(pOut, outSize, pPath);
    return pOut;
}

#else

// Finds the directory entry matching pName case-insensitively, the result is written to pName
static bool stdPlatform_MatchEntry(const char* pDir, char* pName)
{
    DIR* pDirStream = opendir(pDir[0] ? pDir : ".");
    if ( !pDirStream ) {
        return false;
    }

    bool bFound = false;
    struct dirent* pEntry;
    while ( (pEntry = readdir(pDirStream)) != NULL )
    {
        if ( strcasecmp(pEntry->d_name, pName) == 0 )
        {
            strcpy(pName, pEntry->d_name); // same length
            bFound = true;
            break;
        }
    }

    closedir(pDirStream);
    return bFound;
}

const char* J3DAPI stdPlatform_NativePath(const char* pPath, char* pOut, size_t outSize)
{
    if ( !pPath || outSize == 0 ) {
        return pOut;
    }

    // Convert separators, drop drive letters (e.g. "C:\")
    size_t len = 0;
    const char* pIn = pPath;
    if ( isalpha((unsigned char)pIn[0]) && pIn[1] == ':' ) {
        pIn += 2;
    }

    for ( ; *pIn && len < outSize - 1; ++pIn ) {
        pOut[len++] = *pIn == '\\' ? '/' : *pIn;
    }
    pOut[len] = 0;

    struct stat st;
    if ( stat(pOut, &st) == 0 ) {
        return pOut; // exists as is
    }

    // Resolve each component case-insensitively, stop at the first one that doesn't exist (e.g. a new file)
    char* pComp = pOut[0] == '/' ? pOut + 1 : pOut;
    while ( *pComp )
    {
        char* pSep = strchr(pComp, '/');
        if ( pSep ) {
            *pSep = 0;
        }

        // Directory part is everything before pComp
        char saved = 0;
        if ( pComp > pOut ) {
            saved = pComp[-1];
            pComp[-1] = 0;
        }

        const char* pDir = pComp > pOut ? (pOut[0] ? pOut : "/") : ".";
        bool bSpecial = strcmp(pComp, ".") == 0 || strcmp(pComp, "..") == 0 || pComp[0] == 0;
        bool bFound = bSpecial || stdPlatform_MatchEntry(pDir, pComp);

        if ( pComp > pOut ) {
            pComp[-1] = saved;
        }

        if ( pSep ) {
            *pSep = '/';
        }

        if ( !bFound || !pSep ) {
            break;
        }

        pComp = pSep + 1;
    }

    return pOut;
}

#endif // _WIN32
