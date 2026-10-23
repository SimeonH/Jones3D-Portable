#include "wuRegistry.h"
#include <j3dcore/j3dhook.h>
#include <w32util/RTI/symbols.h>
#include <std/General/std.h>
#include <std/General/stdUtil.h>

static bool wuRegistry_bStarted = false;
static LPCSTR appSubKey = NULL;
static HKEY appRootKey  = NULL;

// Added: Opens the app key with only the access needed
static LSTATUS wuRegistry_OpenKey(HKEY rootKey, REGSAM access, PHKEY phKey)
{
    *phKey = 0;
    return rootKey ? RegOpenKeyEx(rootKey, appSubKey, 0, access, phKey) : ERROR_FILE_NOT_FOUND;
}

// Added: Queries value from the app key
static LSTATUS wuRegistry_QueryValue(const char* pKey, DWORD flags, void* pData, DWORD* pSize)
{
    HKEY hKey;
    LSTATUS res = wuRegistry_OpenKey(appRootKey, KEY_READ, &hKey);
    if ( res == ERROR_SUCCESS )
    {
        res = RegGetValue(hKey, NULL, pKey, flags, NULL, pData, pSize);
        RegCloseKey(hKey);
    }

    return res;
}

void wuRegistry_InstallHooks(void)
{
    J3D_HOOKFUNC(wuRegistry_Startup);
    J3D_HOOKFUNC(wuRegistry_Shutdown);
    J3D_HOOKFUNC(wuRegistry_SaveInt);
    J3D_HOOKFUNC(wuRegistry_SaveFloat);
    J3D_HOOKFUNC(wuRegistry_GetInt);
    J3D_HOOKFUNC(wuRegistry_GetFloat);
    J3D_HOOKFUNC(wuRegistry_SaveIntEx);
    J3D_HOOKFUNC(wuRegistry_GetIntEx);
    J3D_HOOKFUNC(wuRegistry_SaveStr);
    J3D_HOOKFUNC(wuRegistry_GetStr);
}


int J3DAPI wuRegistry_Startup(HKEY rootKey, LPCSTR subKey)
{
    STD_ASSERTREL(!wuRegistry_bStarted);

    // Added: Added HKEY_CURRENT_USER_LOCAL_SETTINGS & Removed HKEY_DYN_DATA 
    STD_ASSERTREL(
        rootKey == HKEY_CLASSES_ROOT
     || rootKey == HKEY_CURRENT_USER
     || rootKey == HKEY_LOCAL_MACHINE
     || rootKey == HKEY_USERS
     || rootKey == HKEY_PERFORMANCE_DATA
     || rootKey == HKEY_CURRENT_CONFIG
     || rootKey == HKEY_CURRENT_USER_LOCAL_SETTINGS
    );

    wuRegistry_bStarted = true;
    appSubKey  = subKey;
    appRootKey = rootKey;

    HKEY hkResult = 0;
    DWORD dwDisposition;
    if ( RegCreateKeyEx(
        rootKey,
        subKey,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE | KEY_READ, // 0x2001F -  KEY_WRITE | KEY_READ
        NULL,
        &hkResult,
        &dwDisposition) == ERROR_SUCCESS
      && RegCloseKey(hkResult) == ERROR_SUCCESS )
    {
        return 0;
    }

    if ( hkResult )
    {
        RegCloseKey(hkResult);
    }

    return 1;
}

bool wuRegistry_HasStarted(void)
{
    return wuRegistry_bStarted;
}

void J3DAPI wuRegistry_Shutdown()
{
    appSubKey           = NULL;
    appRootKey          = NULL;
    wuRegistry_bStarted = false;
}

int J3DAPI wuRegistry_SaveInt(const char* pKey, int value)
{
    STD_ASSERTREL(wuRegistry_bStarted);

    // Added: Added HKEY_CURRENT_USER_LOCAL_SETTINGS & Removed HKEY_DYN_DATA 
    STD_ASSERTREL(
        appRootKey == HKEY_CLASSES_ROOT
     || appRootKey == HKEY_CURRENT_USER
     || appRootKey == HKEY_LOCAL_MACHINE
     || appRootKey == HKEY_USERS
     || appRootKey == HKEY_PERFORMANCE_DATA
     || appRootKey == HKEY_CURRENT_CONFIG
     || appRootKey == HKEY_CURRENT_USER_LOCAL_SETTINGS
    );

    HKEY hkResult = 0;
    if ( wuRegistry_OpenKey(appRootKey, KEY_SET_VALUE, &hkResult) == ERROR_SUCCESS
      && RegSetValueEx(hkResult, pKey, 0, REG_DWORD, (const BYTE*)&value, sizeof(value)) == ERROR_SUCCESS
      && RegCloseKey(hkResult) == ERROR_SUCCESS )
    {
        return 0;
    }

    if ( hkResult )
    {
        RegCloseKey(hkResult);
    }

    return 1;
}

int J3DAPI wuRegistry_SaveFloat(const char* pKey, float value)
{
    STD_ASSERTREL(wuRegistry_bStarted);

    // Added: Added HKEY_CURRENT_USER_LOCAL_SETTINGS & Removed HKEY_DYN_DATA 
    STD_ASSERTREL(
        appRootKey == HKEY_CLASSES_ROOT
     || appRootKey == HKEY_CURRENT_USER
     || appRootKey == HKEY_LOCAL_MACHINE
     || appRootKey == HKEY_USERS
     || appRootKey == HKEY_PERFORMANCE_DATA
     || appRootKey == HKEY_CURRENT_CONFIG
     || appRootKey == HKEY_CURRENT_USER_LOCAL_SETTINGS
    );

    HKEY hkResult = 0;
    if ( wuRegistry_OpenKey(appRootKey, KEY_SET_VALUE, &hkResult) == ERROR_SUCCESS
      && RegSetValueEx(hkResult, pKey, 0, REG_BINARY, (const BYTE*)&value, sizeof(value)) == ERROR_SUCCESS
      && RegCloseKey(hkResult) == ERROR_SUCCESS )
    {
        return 0;
    }

    if ( hkResult )
    {
        RegCloseKey(hkResult);
    }

    return 1;
}

int J3DAPI wuRegistry_GetInt(const char* pKey, int defaultValue)
{
    STD_ASSERTREL(wuRegistry_bStarted);

    uint32_t value = 0;
    DWORD size = sizeof(value);
    if ( wuRegistry_QueryValue(pKey, RRF_RT_ANY, &value, &size) == ERROR_SUCCESS && size == sizeof(value) ) {
        return (int)value;
    }

    return defaultValue;
}

float J3DAPI wuRegistry_GetFloat(const char* pKey, float defaultValue)
{
    STD_ASSERTREL(wuRegistry_bStarted);

    float value = 0.0f;
    DWORD size = sizeof(value);
    if ( wuRegistry_QueryValue(pKey, RRF_RT_ANY, &value, &size) == ERROR_SUCCESS && size == sizeof(value) ) {
        return value;
    }

    return defaultValue;
}

int J3DAPI wuRegistry_SaveIntEx(const char* pKey, int defaultValue)
{
    STD_ASSERTREL(wuRegistry_bStarted);
    return wuRegistry_SaveInt(pKey, defaultValue);
}

int J3DAPI wuRegistry_GetIntEx(const char* pKey, int defaultValue)
{
    STD_ASSERTREL(wuRegistry_bStarted);
    return wuRegistry_GetInt(pKey, defaultValue);
}

int J3DAPI wuRegistry_SaveStr(const char* pKey, const char* pStr)
{
    STD_ASSERTREL(wuRegistry_bStarted);

    // Added: Added HKEY_CURRENT_USER_LOCAL_SETTINGS & Removed HKEY_DYN_DATA 
    STD_ASSERTREL(
        appRootKey == HKEY_CLASSES_ROOT
     || appRootKey == HKEY_CURRENT_USER
     || appRootKey == HKEY_LOCAL_MACHINE
     || appRootKey == HKEY_USERS
     || appRootKey == HKEY_PERFORMANCE_DATA
     || appRootKey == HKEY_CURRENT_CONFIG
     || appRootKey == HKEY_CURRENT_USER_LOCAL_SETTINGS
    );

    HKEY hkResult = 0;
    if ( wuRegistry_OpenKey(appRootKey, KEY_SET_VALUE, &hkResult) == ERROR_SUCCESS
      && RegSetValueEx(hkResult, pKey, 0, REG_SZ, (const uint8_t*)pStr, strlen(pStr)) == ERROR_SUCCESS
      && RegCloseKey(hkResult) == ERROR_SUCCESS )
    {
        return 0;
    }

    if ( hkResult )
    {
        RegCloseKey(hkResult);
    }

    return 1;
}

int J3DAPI wuRegistry_GetStr(const char* pKey, char* pDstStr, size_t size, const char* pDefaultValue)
{
    STD_ASSERTREL(wuRegistry_bStarted);

    // Added: Changed call RegQueryValueEx -> RegGetValue to handle null termination
    DWORD dataSize = (DWORD)size;
    if ( wuRegistry_QueryValue(pKey, RRF_RT_REG_SZ, pDstStr, &dataSize) == ERROR_SUCCESS ) {
        return 0;
    }

    if ( pDefaultValue ) {
        stdUtil_StringCopy(pDstStr, size, pDefaultValue);
    }

    return 1;
}
