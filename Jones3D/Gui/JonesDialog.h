#ifndef JONES3D_JONESDIALOG_H
#define JONES3D_JONESDIALOG_H
#include <Jones3D/types.h>
#include <Jones3D/RTI/addresses.h>
#include <j3dcore/j3d.h>
#include <rdroid/types.h>
#include <sith/types.h>
#include <std/types.h>

J3D_EXTERN_C_START

void J3DAPI JonesDialog_SetGameState(JonesDialogGameState* pState, int bWindowModeSupported);
void J3DAPI JonesDialog_ResetGameState(JonesDialogGameState* pState, int bWindowModeSupported);

// Helper hooking functions
void JonesDialog_InstallHooks(void);

J3D_EXTERN_C_END
#endif // JONES3D_JONESDIALOG_H
