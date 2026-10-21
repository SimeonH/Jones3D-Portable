#ifndef JONES3D_JONESWXDIALOGS_H
#define JONES3D_JONESWXDIALOGS_H
// C interface to dialogs implemented with wxWidgets which are internal to jonesConfig.
// Public dialog functions are declared in Jones3D/Display/jonesConfig.h.
#include <Jones3D/types.h>
#include <j3dcore/j3d.h>

J3D_EXTERN_C_START

// Joystick AXIS key IDs
#define JONESCONFIG_JOYAXISKID_UP       0x8000u
#define JONESCONFIG_JOYAXISKID_RIGHT    0x8001u
#define JONESCONFIG_JOYAXISKID_DOWN     0x8002u
#define JONESCONFIG_JOYAXISKID_LEFT     0x8003u
#define JONESCONFIG_JOYAXISKID_TWISTL   0x8004u
#define JONESCONFIG_JOYAXISKID_TWISTR   0x8005u
#define JONESCONFIG_JOYAXISKID_AIR      0x8006u
#define JONESCONFIG_JOYAXISKID_GROUND   0x8007u
#define JONESCONFIG_NUM_JOYAXISKIDS     8u

#define JONESCONFIG_CONTROLSTRING_MAXLEN 256u

#define JONESCONFIG_NUMDEFAULTCONTROLSCHEMES 3u

int J3DAPI jonesConfig_ShowOverwriteSaveGameDlg(HWND hWnd, const char* aFilePath);
int J3DAPI jonesConfig_ShowLoadGameWarningMsgBox(HWND hWnd);

// Game logic helpers implemented in jonesConfig.c

// Finds the auto save file of the current level, pPath is empty string if not found
void J3DAPI jonesConfig_GetAutoSaveFilePath(char* pPath, size_t size);

// Returns cached current level num, see jonesConfig_UpdateCurrentLevelNum
int jonesConfig_GetCurrentLevelNum(void);

// Game play option "Default to run"
int jonesConfig_GetDefaultRun(void);
void J3DAPI jonesConfig_SetDefaultRun(int bDefaultRun);

// Control schemes
JonesControlsConfig* jonesConfig_GetControlOptionsConfig(void);
int jonesConfig_ControlOptions_InitControlsConfig(void);
int J3DAPI jonesConfig_CopyControlConfig(JonesControlsConfig* pSrc, JonesControlsConfig* pDst);
void J3DAPI jonesConfig_FreeControlConfigEntry(JonesControlsConfig* pConfig);
void J3DAPI jonesConfig_FreeControlScheme(JonesControlsScheme* pScheme);
JonesControlsScheme* J3DAPI jonesConfig_CloneControlSchemesList(JonesControlsScheme* aSchemes, size_t start, size_t numSchemes);
int J3DAPI jonesConfig_ControlOptions_WriteScheme(JonesControlsScheme* pScheme);
JonesControlsScheme* J3DAPI jonesConfig_GetDefaultControlScheme(JonesControlsConfig* pConfig);
const char* jonesConfig_GetDefaultControlSchemeName(void);
JonesControlsScheme* jonesConfig_LoadActiveControlScheme(void);
void J3DAPI jonesConfig_BindControls(JonesControlsScheme* pScheme);
const char* jonesConfig_GetKeySetsDirPath(void);

// Returns localized name of control action
const char* J3DAPI jonesConfig_GetControlActionName(size_t actionId, int bCapitalized);

// Writes display name of control (key, button, axis) to pDest of size JONESCONFIG_CONTROLSTRING_MAXLEN
void J3DAPI jonesConfig_ControlToString(size_t controlId, char* pDest);

// Returns list index of existing binding for keyControlID or -1 if not bound
int J3DAPI jonesConfig_AssignKey_CheckBindForKey(JonesControlsScheme* pScheme, size_t keyControlID, size_t* pFunctionId, int listID, size_t* pOffset);

// JonesMain dev dialog
StdDisplayEnvironment* JonesMain_GetStartupDisplayEnvironment(void);
const StdVideoMode* JonesMain_GetCurVideoMode(void);
bool J3DAPI JonesMain_CurDisplaySupportsBPP(const JonesDisplaySettings* pSettings, size_t bpp);
void J3DAPI JonesMain_SaveDevSettings(const JonesState* pState);

int J3DAPI jonesConfig_DisplaySettings_Get3DDeviceSupportsBPP(const StdDisplayInfo* pDisplayInfo, const JonesDisplaySettings* pSettings, int bpp);

// Shows modal message box
void JonesWx_ShowMessageBox(const char* pTitle, const char* pText, int bError);

// Shows error message, safe to call from assert & fatal error handlers
void JonesWx_ShowFatalError(const char* pTitle, const char* pText);

// Shuts down wxWidgets
void JonesWx_Shutdown(void);

// Shows the game data installer which copies the data files from a Steam installation, the GOG offline installer
// or the original CDs to pInstallPath (native path). Returns 1 when the data was installed, 0 when the user quit.
int JonesWx_ShowDataInstaller(const char* pInstallPath);

// Results of JonesWx_ShowLauncher
#define JONESLAUNCHER_EXIT  0 // Exit
#define JONESLAUNCHER_START 1 // Start the game with the start mode

// Shows the launcher of the original game (Jones3D.exe) in its installed mode for the game in pInstallPath (game path).
// pStartMode receives the JONES_STARTMODE_* to start the game with when it returns JONESLAUNCHER_START.
// The game is started up to the start mode handling, its sound and display settings dialogs are opened over the launcher.
int JonesWx_ShowLauncher(const char* pInstallPath, int* pStartMode);

J3D_EXTERN_C_END
#endif // JONES3D_JONESWXDIALOGS_H
