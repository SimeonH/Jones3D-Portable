// Win32 file functions used by the game code on non-Windows systems.
// Paths are game paths (backslashes, case-insensitive), see stdPlatform_NativePath.
#include <j3dcore/j3d.h>
#include <std/General/stdMemory.h>
#include <std/General/stdPlatform.h>
#include <std/General/stdUtil.h>

#include <dirent.h>
#include <fnmatch.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct sFindHandle
{
    DIR* pDir;
    char aDir[1024];
    char aPattern[MAX_PATH];
} FindHandle;

static bool stdWin32File_Exists(const char* pPath)
{
    char aPath[1024];
    struct stat st;
    return stat(stdPlatform_NativePath(pPath, aPath, sizeof(aPath)), &st) == 0;
}

DWORD SearchPathA(LPCSTR lpPath, LPCSTR lpFileName, LPCSTR lpExtension, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart)
{
    char aPath[1024];
    const char* pExt = (lpExtension && !strchr(lpFileName, '.')) ? lpExtension : "";
    if ( lpPath && lpPath[0] ) {
        STD_FORMAT(aPath, "%s\\%s%s", lpPath, lpFileName, pExt);
    }
    else {
        STD_FORMAT(aPath, "%s%s", lpFileName, pExt);
    }

    if ( !stdWin32File_Exists(aPath) ) {
        return 0;
    }

    DWORD len = (DWORD)strlen(aPath);
    if ( len + 1 > nBufferLength ) {
        return len + 1; // required size
    }

    memcpy(lpBuffer, aPath, len + 1);
    if ( lpFilePart )
    {
        char* pSep = strrchr(lpBuffer, '\\');
        *lpFilePart = pSep ? pSep + 1 : lpBuffer;
    }

    return len;
}

DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer)
{
    char aPath[1024];
    if ( !getcwd(aPath, sizeof(aPath)) ) {
        return 0;
    }

    DWORD len = (DWORD)strlen(aPath);
    if ( len + 1 > nBufferLength ) {
        return len + 1;
    }

    memcpy(lpBuffer, aPath, len + 1);
    return len;
}

BOOL SetCurrentDirectoryA(LPCSTR lpPathName)
{
    char aPath[1024];
    return chdir(stdPlatform_NativePath(lpPathName, aPath, sizeof(aPath))) == 0;
}

BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    J3D_UNUSED(lpSecurityAttributes);
    char aPath[1024];
    return mkdir(stdPlatform_NativePath(lpPathName, aPath, sizeof(aPath)), 0755) == 0;
}

static BOOL stdWin32File_Next(FindHandle* pFind, LPWIN32_FIND_DATAA pData)
{
    struct dirent* pEntry;
    while ( (pEntry = readdir(pFind->pDir)) != NULL )
    {
        if ( fnmatch(pFind->aPattern, pEntry->d_name, FNM_CASEFOLD) != 0 ) {
            continue;
        }

        memset(pData, 0, sizeof(*pData));
        STD_STRCPY(pData->cFileName, pEntry->d_name);

        char aPath[1280];
        struct stat st;
        STD_FORMAT(aPath, "%s/%s", pFind->aDir, pEntry->d_name);
        if ( stat(aPath, &st) == 0 )
        {
            pData->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            pData->nFileSizeLow     = (DWORD)st.st_size;

            // Unix time to FILETIME (100 ns intervals since 1601)
            uint64_t ft = ((uint64_t)st.st_mtime + 11644473600ULL) * 10000000ULL;
            pData->ftLastWriteTime.dwLowDateTime  = (DWORD)ft;
            pData->ftLastWriteTime.dwHighDateTime = (DWORD)(ft >> 32);
        }

        return TRUE;
    }

    return FALSE;
}

HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
    FindHandle* pFind = (FindHandle*)STDMALLOC(sizeof(FindHandle));
    if ( !pFind ) {
        return INVALID_HANDLE_VALUE;
    }

    memset(pFind, 0, sizeof(*pFind));

    // Split to directory and pattern
    char aDir[1024] = ".";
    const char* pPattern = lpFileName;
    const char* pSep = strrchr(lpFileName, '\\');
    const char* pSep2 = strrchr(lpFileName, '/');
    if ( pSep2 > pSep ) {
        pSep = pSep2;
    }

    if ( pSep )
    {
        size_t len = (size_t)(pSep - lpFileName);
        len = len < sizeof(aDir) - 1 ? len : sizeof(aDir) - 1;
        memcpy(aDir, lpFileName, len);
        aDir[len] = 0;
        pPattern = pSep + 1;
    }

    stdPlatform_NativePath(aDir, pFind->aDir, sizeof(pFind->aDir));
    STD_STRCPY(pFind->aPattern, strcmp(pPattern, "*.*") == 0 ? "*" : pPattern);

    pFind->pDir = opendir(pFind->aDir);
    if ( !pFind->pDir || !stdWin32File_Next(pFind, lpFindFileData) )
    {
        FindClose(pFind);
        return INVALID_HANDLE_VALUE;
    }

    return pFind;
}

BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
    if ( !hFindFile || hFindFile == INVALID_HANDLE_VALUE ) {
        return FALSE;
    }

    return stdWin32File_Next((FindHandle*)hFindFile, lpFindFileData);
}

BOOL FindClose(HANDLE hFindFile)
{
    if ( !hFindFile || hFindFile == INVALID_HANDLE_VALUE ) {
        return FALSE;
    }

    FindHandle* pFind = (FindHandle*)hFindFile;
    if ( pFind->pDir ) {
        closedir(pFind->pDir);
    }

    stdMemory_Free(pFind);
    return TRUE;
}
