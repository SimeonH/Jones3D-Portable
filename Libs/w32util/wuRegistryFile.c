// wuRegistry for non-Windows systems: the game settings are stored as "key=value" lines in
// $XDG_CONFIG_HOME/OpenJones3D/settings.cfg (default ~/.config/OpenJones3D/settings.cfg).
// Return values match the registry implementation: 0 on success, 1 on failure.
#include "wuRegistry.h"

#include <std/General/std.h>
#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define WUREGISTRY_MAXKEY   128
#define WUREGISTRY_MAXVALUE 512

typedef struct sRegistryEntry
{
    char aKey[WUREGISTRY_MAXKEY];
    char aValue[WUREGISTRY_MAXVALUE];
} RegistryEntry;

static bool wuRegistry_bStarted = false;
static char wuRegistry_aFilePath[1024];
static RegistryEntry* wuRegistry_aEntries = NULL;
static size_t wuRegistry_numEntries = 0;
static size_t wuRegistry_capEntries = 0;

void wuRegistry_InstallHooks(void)
{
}

static RegistryEntry* wuRegistry_Find(const char* pKey)
{
    for ( size_t i = 0; i < wuRegistry_numEntries; ++i )
    {
        if ( strcasecmp(wuRegistry_aEntries[i].aKey, pKey) == 0 ) {
            return &wuRegistry_aEntries[i];
        }
    }

    return NULL;
}

static void wuRegistry_Set(const char* pKey, const char* pValue)
{
    RegistryEntry* pEntry = wuRegistry_Find(pKey);
    if ( !pEntry )
    {
        if ( wuRegistry_numEntries == wuRegistry_capEntries )
        {
            size_t newCap = wuRegistry_capEntries ? wuRegistry_capEntries * 2 : 64;
            RegistryEntry* aEntries = (RegistryEntry*)realloc(wuRegistry_aEntries, newCap * sizeof(RegistryEntry));
            if ( !aEntries ) {
                return;
            }

            wuRegistry_aEntries   = aEntries;
            wuRegistry_capEntries = newCap;
        }

        pEntry = &wuRegistry_aEntries[wuRegistry_numEntries++];
        STD_STRCPY(pEntry->aKey, pKey);
    }

    STD_STRCPY(pEntry->aValue, pValue);
}

static void wuRegistry_MakeDirs(const char* pPath)
{
    char aDir[1024];
    STD_STRCPY(aDir, pPath);
    for ( char* p = aDir + 1; *p; ++p )
    {
        if ( *p == '/' )
        {
            *p = 0;
            mkdir(aDir, 0755);
            *p = '/';
        }
    }
}

static void wuRegistry_Load(void)
{
    FILE* pFile = fopen(wuRegistry_aFilePath, "r");
    if ( !pFile ) {
        return;
    }

    char aLine[WUREGISTRY_MAXKEY + WUREGISTRY_MAXVALUE + 2];
    while ( fgets(aLine, sizeof(aLine), pFile) )
    {
        aLine[strcspn(aLine, "\r\n")] = 0;
        if ( aLine[0] == '#' || aLine[0] == ';' ) {
            continue;
        }

        char* pSep = strchr(aLine, '=');
        if ( !pSep ) {
            continue;
        }

        *pSep = 0;
        wuRegistry_Set(aLine, pSep + 1);
    }

    fclose(pFile);
}

static int wuRegistry_Store(void)
{
    wuRegistry_MakeDirs(wuRegistry_aFilePath);
    FILE* pFile = fopen(wuRegistry_aFilePath, "w");
    if ( !pFile ) {
        return 1;
    }

    fprintf(pFile, "# OpenJones3D settings\n");
    for ( size_t i = 0; i < wuRegistry_numEntries; ++i ) {
        fprintf(pFile, "%s=%s\n", wuRegistry_aEntries[i].aKey, wuRegistry_aEntries[i].aValue);
    }

    fclose(pFile);
    return 0;
}

int J3DAPI wuRegistry_Startup(HKEY hKey, LPCSTR lpSubKey)
{
    J3D_UNUSED(hKey);
    J3D_UNUSED(lpSubKey);
    STD_ASSERTREL(!wuRegistry_bStarted);

    const char* pConfigHome = getenv("XDG_CONFIG_HOME");
    if ( pConfigHome && pConfigHome[0] ) {
        STD_FORMAT(wuRegistry_aFilePath, "%s/OpenJones3D/settings.cfg", pConfigHome);
    }
    else
    {
        const char* pHome = getenv("HOME");
        STD_FORMAT(wuRegistry_aFilePath, "%s/.config/OpenJones3D/settings.cfg", pHome ? pHome : ".");
    }

    wuRegistry_numEntries = 0;
    wuRegistry_Load();
    wuRegistry_bStarted = true;
    return 0;
}

bool wuRegistry_HasStarted(void)
{
    return wuRegistry_bStarted;
}

void J3DAPI wuRegistry_Shutdown()
{
    free(wuRegistry_aEntries);
    wuRegistry_aEntries   = NULL;
    wuRegistry_numEntries = 0;
    wuRegistry_capEntries = 0;
    wuRegistry_bStarted   = false;
}

int J3DAPI wuRegistry_SaveInt(const char* pKey, int value)
{
    STD_ASSERTREL(wuRegistry_bStarted);
    char aValue[32];
    STD_FORMAT(aValue, "%d", value);
    wuRegistry_Set(pKey, aValue);
    return wuRegistry_Store();
}

int J3DAPI wuRegistry_SaveFloat(const char* pKey, float value)
{
    STD_ASSERTREL(wuRegistry_bStarted);
    char aValue[32];
    STD_FORMAT(aValue, "%.9g", value);
    wuRegistry_Set(pKey, aValue);
    return wuRegistry_Store();
}

int J3DAPI wuRegistry_GetInt(const char* pKey, int defaultValue)
{
    STD_ASSERTREL(wuRegistry_bStarted);
    const RegistryEntry* pEntry = wuRegistry_Find(pKey);
    return pEntry ? (int)strtol(pEntry->aValue, NULL, 0) : defaultValue;
}

float J3DAPI wuRegistry_GetFloat(const char* pKey, float defaultValue)
{
    STD_ASSERTREL(wuRegistry_bStarted);
    const RegistryEntry* pEntry = wuRegistry_Find(pKey);
    return pEntry ? strtof(pEntry->aValue, NULL) : defaultValue;
}

int J3DAPI wuRegistry_SaveIntEx(const char* pKey, int defaultValue)
{
    return wuRegistry_SaveInt(pKey, defaultValue);
}

int J3DAPI wuRegistry_GetIntEx(const char* pKey, int defaultValue)
{
    return wuRegistry_GetInt(pKey, defaultValue);
}

int J3DAPI wuRegistry_SaveStr(const char* pKey, const char* pStr)
{
    STD_ASSERTREL(wuRegistry_bStarted);
    wuRegistry_Set(pKey, pStr ? pStr : "");
    return wuRegistry_Store();
}

int J3DAPI wuRegistry_GetStr(const char* pKey, char* pDstStr, size_t size, const char* pDefaultValue)
{
    STD_ASSERTREL(wuRegistry_bStarted);
    const RegistryEntry* pEntry = wuRegistry_Find(pKey);
    if ( pEntry )
    {
        stdUtil_StringCopy(pDstStr, size, pEntry->aValue);
        return 0;
    }

    if ( pDefaultValue ) {
        stdUtil_StringCopy(pDstStr, size, pDefaultValue);
    }

    return 1;
}
