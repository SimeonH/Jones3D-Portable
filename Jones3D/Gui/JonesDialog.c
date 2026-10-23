#include "JonesDialog.h"
#include <j3dcore/j3dhook.h>
#include <wkernel/wkernel.h>

#include <Jones3D/Main/JonesMain.h>
#include <Jones3D/RTI/symbols.h>

#include <std/General/std.h>
#include <std/General/stdUtil.h>

#include <string.h>
#include <std/Win95/stdDisplay.h>
#include <std/Win95/stdControl.h>
#include <std/Win95/stdWin95.h>

// Changed: The dialogs (wx) are separate top-level windows over the SDL game window, so there's no game frame
// or cursor to draw behind them with GDI as the original did on the DirectDraw surfaces.
// Only the game state is paused and restored.

void JonesDialog_InstallHooks(void)
{
}

void J3DAPI JonesDialog_SetGameState(JonesDialogGameState* pState, int bWindowModeSupported)
{
    J3D_UNUSED(bWindowModeSupported);
    memset(pState, 0, sizeof(*pState));

    pState->bGamePaused = JonesMain_IsGamePaused();
    if ( !pState->bGamePaused ) {
        JonesMain_PauseGame();
    }

    pState->bControlsActive = stdControl_ControlsActive();
    if ( pState->bControlsActive && !stdControl_SetActivation(0) ) {
        stdControl_ShowMouseCursor(1);
    }
}

void J3DAPI JonesDialog_ResetGameState(JonesDialogGameState* pState, int bWindowModeSupported)
{
    J3D_UNUSED(bWindowModeSupported);
    if ( !stdControl_SetActivation(pState->bControlsActive) ) {
        stdControl_ShowMouseCursor(pState->bControlsActive ? 0 : 1);
    }

    if ( !pState->bGamePaused ) {
        JonesMain_ResumeGame();
    }
}

