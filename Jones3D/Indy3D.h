#ifndef JONES3D_INDY3D_H
#define JONES3D_INDY3D_H
#include <Jones3D/types.h>
#include <Jones3D/RTI/addresses.h>
#include <j3dcore/j3d.h>
#include <rdroid/types.h>
#include <sith/types.h>
#include <std/types.h>

J3D_EXTERN_C_START

// Process-wide setup: DPI awareness
void Indy3D_InitProcess(void);

// Added: Makes sure the game data files are installed, shows the data installer when they aren't found.
// The default install path is ~/.Jones3D, on Windows the folder of the executable. pInstallPathOut (optional) receives
// the install path. Returns false when the user quit the installer.
bool Indy3D_EnsureGameData(bool bForceInstaller, char* pInstallPathOut, size_t outSize);

// Indy3D.exe entry point
int __stdcall Indy3D_WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd);

// Helper hooking functions
void Indy3D_InstallHooks(void);

J3D_EXTERN_C_END
#endif // JONES3D_INDY3D_H
