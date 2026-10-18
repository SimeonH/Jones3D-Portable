#include "std.h"
#include "stdFileUtil.h"
#include "stdFnames.h"
#include "stdMemory.h"
#include "stdUtil.h"

#include "stdPlatform.h"

#include <j3dcore/j3dhook.h>
#include <std/RTI/symbols.h>

#ifndef _WIN32
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

void stdFileUtil_InstallHooks(void)
{
    J3D_HOOKFUNC(stdFileUtil_NewFind);
    J3D_HOOKFUNC(stdFileUtil_DisposeFind);
    J3D_HOOKFUNC(stdFileUtil_FindNext);
    J3D_HOOKFUNC(stdFileUtil_MkDir);
    J3D_HOOKFUNC(stdFileUtil_FileExists);
}


#ifdef _WIN32
time_t FileTimeToUnixTime(const FILETIME* ft)
{
    // Windows FILETIME starts on January 1, 1601
    // Unix time starts on January 1, 1970
    // The difference between the two epochs is 11644473600 seconds
    static const ULONGLONG EPOCH_DIFF = 11644473600ULL;

    ULARGE_INTEGER ull;
    ull.LowPart  = ft->dwLowDateTime;
    ull.HighPart = ft->dwHighDateTime;

    // Convert from 100-nanosecond intervals to seconds and adjust the epoch
    ull.QuadPart /= 10000000ULL; // Convert to seconds
    ull.QuadPart -= EPOCH_DIFF;  // Subtract the epoch difference

    return (time_t)ull.QuadPart;
}
#endif

FindFileData* J3DAPI stdFileUtil_NewFind(const char* path, int mode, const char* pFilter)
{
    FindFileData* pData = (FindFileData*)STDMALLOC(sizeof(FindFileData));
    if ( !pData ) {
        return NULL;
    }

    memset(pData, 0, sizeof(FindFileData));

    if ( mode < 0 ) {
        return pData;
    }

    if ( mode <= 2 )
    {
        stdFnames_MakePath(pData->aSearchFilter, sizeof(pData->aSearchFilter), path, "*.*");
        return pData;
    }

    if ( mode != 3 ) {
        return pData;
    }

    if ( *pFilter == '.' ) {
        pFilter++;
    }

    STD_FORMAT(std_g_genBuffer, "*.%s", pFilter);
    stdFnames_MakePath(pData->aSearchFilter, sizeof(pData->aSearchFilter), path, std_g_genBuffer);
    return pData;
}

#ifdef _WIN32

void J3DAPI stdFileUtil_DisposeFind(FindFileData* ffData)
{
    if ( ffData )
    {
        if ( ffData->nFoundFiles ) {
            FindClose(ffData->handle);
        }
        stdMemory_Free(ffData);
    }
}

int J3DAPI stdFileUtil_FindNext(FindFileData* ffData, tFoundFileInfo* pFileInfo)
{
    WIN32_FIND_DATA findData;

    if ( !ffData ) {
        return 0;
    }

    int nFoundFiles = ffData->nFoundFiles;
    ffData->nFoundFiles = nFoundFiles + 1;
    if ( nFoundFiles )
    {
        if ( !FindNextFile(ffData->handle, &findData) ) {
            return 0;
        }
    }
    else
    {
        ffData->handle = FindFirstFile(ffData->aSearchFilter, &findData);
        if ( ffData->handle == INVALID_HANDLE_VALUE ) {
            return 0;
        }
    }

    STD_STRCPY(pFileInfo->aName, findData.cFileName);

    pFileInfo->lastChanged  = FileTimeToUnixTime(&findData.ftLastWriteTime); // TODO: After all code that uses find file functions is defined change the type of `lastChanged` to time_t
    pFileInfo->bIsDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return 1;
}

#else // !_WIN32

// The search filter is "<dir>\\<pattern>", the handle is the DIR* of the directory

static void stdFileUtil_SplitFilter(const char* pFilter, char* pDir, size_t dirSize, const char** ppPattern)
{
    const char* pSep = strrchr(pFilter, '\\');
    const char* pSep2 = strrchr(pFilter, '/');
    if ( pSep2 > pSep ) {
        pSep = pSep2;
    }

    if ( pSep )
    {
        size_t len = (size_t)(pSep - pFilter);
        len = len < dirSize - 1 ? len : dirSize - 1;
        memcpy(pDir, pFilter, len);
        pDir[len] = 0;
        *ppPattern = pSep + 1;
    }
    else
    {
        snprintf(pDir, dirSize, ".");
        *ppPattern = pFilter;
    }
}

void J3DAPI stdFileUtil_DisposeFind(FindFileData* ffData)
{
    if ( ffData )
    {
        if ( ffData->handle && ffData->handle != INVALID_HANDLE_VALUE ) {
            closedir((DIR*)ffData->handle);
        }
        stdMemory_Free(ffData);
    }
}

int J3DAPI stdFileUtil_FindNext(FindFileData* ffData, tFoundFileInfo* pFileInfo)
{
    if ( !ffData ) {
        return 0;
    }

    char aDir[256];
    const char* pPattern;
    stdFileUtil_SplitFilter(ffData->aSearchFilter, aDir, sizeof(aDir), &pPattern);

    char aNativeDir[1024];
    stdPlatform_NativePath(aDir, aNativeDir, sizeof(aNativeDir));

    if ( ffData->nFoundFiles++ == 0 )
    {
        ffData->handle = opendir(aNativeDir);
        if ( !ffData->handle )
        {
            ffData->handle = INVALID_HANDLE_VALUE;
            return 0;
        }
    }

    if ( ffData->handle == INVALID_HANDLE_VALUE ) {
        return 0;
    }

    // "*.*" matches all names on Windows, also names without extension
    if ( strcmp(pPattern, "*.*") == 0 ) {
        pPattern = "*";
    }

    struct dirent* pEntry;
    while ( (pEntry = readdir((DIR*)ffData->handle)) != NULL )
    {
        if ( fnmatch(pPattern, pEntry->d_name, FNM_CASEFOLD) != 0 ) {
            continue;
        }

        STD_STRCPY(pFileInfo->aName, pEntry->d_name);

        char aPath[1280];
        STD_FORMAT(aPath, "%s/%s", aNativeDir, pEntry->d_name);
        struct stat st;
        if ( stat(aPath, &st) == 0 )
        {
            pFileInfo->lastChanged  = (uint32_t)st.st_mtime;
            pFileInfo->bIsDirectory = S_ISDIR(st.st_mode);
        }
        else
        {
            pFileInfo->lastChanged  = 0;
            pFileInfo->bIsDirectory = 0;
        }

        return 1;
    }

    return 0;
}

#endif // _WIN32

int J3DAPI stdFileUtil_FindQuick(const char* pPath, int mode, const char* pFilter, tFoundFileInfo* pFileInfo)
{
    FindFileData* pFileData = stdFileUtil_NewFind(pPath, mode, pFilter);
    if ( !pFileData )
    {
        return 0;
    }

    int bFound = stdFileUtil_FindNext(pFileData, pFileInfo);
    stdFileUtil_DisposeFind(pFileData);
    return bFound;
}

int J3DAPI stdFileUtil_CountMatches(const char* pPath, int mode, const char* pFilter)
{
    tFoundFileInfo fileInfo;

    int count = 0;
    FindFileData* pFileData = stdFileUtil_NewFind(pPath, mode, pFilter);
    if ( !pFileData )
    {
        return 0;
    }

    while ( stdFileUtil_FindNext(pFileData, &fileInfo) ) {
        ++count;
    }

    stdFileUtil_DisposeFind(pFileData);
    return count;
}

#ifdef _WIN32

int J3DAPI stdFileUtil_MkDir(const char* pPath)
{
    return CreateDirectoryA(pPath, NULL);
}

int J3DAPI stdFileUtil_FileExists(const char* pFilename)
{
    WIN32_FIND_DATAA findFileData;
    return FindFirstFileA(pFilename, &findFileData) != INVALID_HANDLE_VALUE;
}

int J3DAPI stdFileUtil_RmDir(const char* pDir)
{
    return RemoveDirectoryA(pDir);
}

int J3DAPI stdFileUtil_DelFile(const char* pFilename)
{
    return DeleteFileA(pFilename);
}

#else // !_WIN32

// Return non-zero on success like the Win32 functions

int J3DAPI stdFileUtil_MkDir(const char* pPath)
{
    char aPath[1024];
    return mkdir(stdPlatform_NativePath(pPath, aPath, sizeof(aPath)), 0755) == 0;
}

int J3DAPI stdFileUtil_FileExists(const char* pFilename)
{
    char aPath[1024];
    struct stat st;
    return stat(stdPlatform_NativePath(pFilename, aPath, sizeof(aPath)), &st) == 0;
}

int J3DAPI stdFileUtil_RmDir(const char* pDir)
{
    char aPath[1024];
    return rmdir(stdPlatform_NativePath(pDir, aPath, sizeof(aPath))) == 0;
}

int J3DAPI stdFileUtil_DelFile(const char* pFilename)
{
    char aPath[1024];
    return unlink(stdPlatform_NativePath(pFilename, aPath, sizeof(aPath))) == 0;
}

#endif // _WIN32
