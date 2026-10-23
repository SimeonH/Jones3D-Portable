#include <SDL3/SDL.h>
#include <std/SDL/stdSDL.h>

#include "stdControl.h"
#include "stdWin95.h"

#include <j3dcore/j3dhook.h>
#include <std/General/std.h>
#include <std/General/stdMath.h>
#include <std/General/stdPlatform.h>
#include <std/General/stdUtil.h>
#include <std/RTI/symbols.h>

#define STDCONTROL_MOUSE_BUFFERSIZE 32u

static bool stdControl_bStartup = false;
static bool stdControl_bOpen    = false;

static bool stdControl_bReadMouse               = true;
static POINT stdControl_mousePos                = { 0, 0 };
static bool stdControl_bMouseSensitivityEnabled = false;
static int stdControl_cursorDisplayCounter      = 0;

static uint8_t stdControl_aKeyboardState[STDCONTROL_MAX_KEYBOARD_BUTTONS] = { 0 };

static bool stdControl_bReadJoysticks        = false;
static size_t  stdControl_numJoystickDevices = 0;
static StdControlJoystickDevice stdControl_aJoystickDevices[STDCONTROL_MAX_JOYSTICK_DEVICES] = { 0 };

static StdControlAxis stdControl_aAxes[STDCONTROL_MAX_AXES] = { 0 };
static int stdControl_aAxisStates[STDCONTROL_MAX_AXES]      = { 0 };

static uint32_t stdControl_aKeyIdleTimes[STDCONTROL_MAX_KEYID] = { 0 };
static bool stdControl_aKeyInfo[STDCONTROL_MAX_KEYID]          = { 0 }; // Stores key pressed down state for each key
static int stdControl_aKeyPressed[STDCONTROL_MAX_KEYID]        = { 0 }; // Stores key press state for each key

static bool stdControl_bControlsIdle;
static bool stdControl_bControlsActive;

static float sithControl_secFPS  = 0.0;
static float sithControl_msecFPS = 0.0;

static uint32_t stdControl_curReadTime   = 0;
static uint32_t stdControl_lastReadTime  = 0;
static uint32_t stdControl_readDeltaTime = 0;

static float stdControl_mouseXRange = 0.0;
static float stdControl_mouseYRange = 0.0;

static char stdControl_aStrBuf[128] = { 0 };

void stdControl_InitJoysticks(void);
void J3DAPI stdControl_InitKeyboard(int bForeground);
void stdControl_InitMouse(void);

void J3DAPI stdControl_EnableAxisRead(size_t axis);

void stdControl_ReadKeyboard(void);
void stdControl_ReadJoysticks(void);
void stdControl_ReadMouse(void);

const char* J3DAPI stdControl_DIGetStatus(int HRESULT);

void stdControl_InstallHooks(void)
{
    J3D_HOOKFUNC(stdControl_Startup);
    J3D_HOOKFUNC(stdControl_Shutdown);
    J3D_HOOKFUNC(stdControl_Open);
    J3D_HOOKFUNC(stdControl_Close);
    J3D_HOOKFUNC(stdControl_Reset);
    J3D_HOOKFUNC(stdControl_DisableReadJoysticks);
    J3D_HOOKFUNC(stdControl_EnableAxis);
    J3D_HOOKFUNC(stdControl_ReadControls);
    J3D_HOOKFUNC(stdControl_ReadAxis);
    J3D_HOOKFUNC(stdControl_ReadAxisRaw);
    J3D_HOOKFUNC(stdControl_ReadKeyAsAxis);
    J3D_HOOKFUNC(stdControl_ReadAxisAsKey);
    J3D_HOOKFUNC(stdControl_ReadAxisAsKeyEx);
    J3D_HOOKFUNC(stdControl_ReadKey);
    J3D_HOOKFUNC(stdControl_ControlsActive);
    J3D_HOOKFUNC(stdControl_SetActivation);
    J3D_HOOKFUNC(stdControl_ToggleMouse);
    J3D_HOOKFUNC(stdControl_EnableMouse);
    J3D_HOOKFUNC(stdControl_ControlsIdle);
    J3D_HOOKFUNC(stdControl_TestAxisFlag);
    J3D_HOOKFUNC(stdControl_SetAxisFlags);
    J3D_HOOKFUNC(stdControl_InitJoysticks);
    J3D_HOOKFUNC(stdControl_InitKeyboard);
    J3D_HOOKFUNC(stdControl_InitMouse);
    J3D_HOOKFUNC(stdControl_EnableAxisRead);
    J3D_HOOKFUNC(stdControl_ReadKeyboard);
    J3D_HOOKFUNC(stdControl_ReadJoysticks);
    J3D_HOOKFUNC(stdControl_ReadMouse);
    J3D_HOOKFUNC(stdControl_RegisterAxis);
    J3D_HOOKFUNC(stdControl_ResetMousePos);
    J3D_HOOKFUNC(stdControl_GetMaxJoystickButtons);
    J3D_HOOKFUNC(stdControl_GetNumJoysticks);
    J3D_HOOKFUNC(stdControl_EnableMouseSensitivity);
    J3D_HOOKFUNC(stdControl_ShowMouseCursor);
    J3D_HOOKFUNC(stdControl_IsGamePad);
}

bool stdControl_HasStarted(void)
{
    return stdControl_bStartup;
}

int stdControl_Open(void)
{
    if ( !stdControl_bStartup )
    {
        return 1;
    }

    stdControl_bOpen = true;
    stdControl_SetActivation(1);
    return 0;
}

bool stdControl_IsOpen(void)
{
    return stdControl_bStartup;
}

void stdControl_Close(void)
{
    if ( stdControl_bOpen )
    {
        stdControl_SetActivation(0);
        stdControl_bOpen = false;
    }
}

void stdControl_Reset(void)
{
    stdControl_bReadMouse     = false;
    stdControl_bReadJoysticks = false;
    stdControl_bMouseSensitivityEnabled = false;
    for ( size_t i = 0; i < STDCONTROL_MAX_AXES; i++ )
    {
        stdControl_aAxes[i].flags &= ~STDCONTROL_AXIS_ENABLED;
    }
}

void stdControl_DisableReadJoysticks(void)
{
    stdControl_bReadJoysticks = false;
}

int J3DAPI stdControl_EnableAxis(int axisID)
{
    size_t axis = STDCONTROL_GETAID(axisID);
    if ( axis >= STDCONTROL_MAX_AXES )
    {
        return 0;
    }

    if ( (stdControl_aAxes[axis].flags & STDCONTROL_AXIS_REGISTERED) == 0 )
    {
        return 0;
    }

    stdControl_aAxes[axis].flags |= STDCONTROL_AXIS_ENABLED;
    stdControl_EnableAxisRead(axis);
    return 1;
}

static void stdControl_UpdateJoystickDevices(void);

void stdControl_ReadControls(void)
{
    STD_ASSERTREL(stdControl_bStartup && stdControl_bOpen);
    stdControl_UpdateJoystickDevices();
    if ( stdControl_bControlsActive )
    {
        stdControl_bControlsIdle = true;
        memset(stdControl_aKeyIdleTimes, 0, sizeof(stdControl_aKeyIdleTimes));
        memset(stdControl_aKeyPressed, 0, sizeof(stdControl_aKeyPressed));

        stdControl_curReadTime   = stdPlatform_GetTimeMsec();
        stdControl_readDeltaTime = stdControl_curReadTime - stdControl_lastReadTime;

        sithControl_secFPS  = 1.0f / (float)(stdControl_curReadTime - stdControl_lastReadTime);
        sithControl_msecFPS = 1000.0f * sithControl_secFPS;

        if ( stdControl_bMouseSensitivityEnabled )
        {
            memset(stdControl_aAxisStates, 0, 7u);
        }
        else
        {
            memset(stdControl_aAxisStates, 0, sizeof(stdControl_aAxisStates));
        }

        stdControl_ReadKeyboard();

        if ( stdControl_bReadJoysticks )
        {
            stdControl_ReadJoysticks();
        }

        stdControl_ReadMouse();
        stdControl_lastReadTime = stdControl_curReadTime;
    }
}

float J3DAPI stdControl_ReadAxis(size_t axis)
{
    size_t aid = STDCONTROL_GETAID(axis);
    if ( aid >= STDCONTROL_MAX_AXES )
    {
        return 0.0f;
    }

    if ( !stdControl_bControlsActive )
    {
        return 0.0f;
    }

    if ( (stdControl_aAxes[aid].flags & STDCONTROL_AXIS_ENABLED) == 0 )
    {
        return 0.0f;
    }

    int curPos = stdControl_aAxisStates[aid] - stdControl_aAxes[aid].center;
    if ( !curPos )
    {
        return 0.0f;
    }

    if ( (stdControl_aAxes[aid].flags & STDCONTROL_AXIS_HASDEADZONE) == 0 && stdControl_aAxes[aid].deadzoneThreshold )
    {
        int pos = curPos >= 0 ? stdControl_aAxisStates[aid] - stdControl_aAxes[aid].center : stdControl_aAxes[aid].center - stdControl_aAxisStates[aid];
        if ( pos < stdControl_aAxes[aid].deadzoneThreshold )
        {
            return 0.0f;
        }
    }

    float pos = (float)curPos * stdControl_aAxes[aid].scale;
    pos = stdMath_ClipNearZero(pos);

    if ( !stdControl_bControlsIdle )
    {
        return pos;
    }

    if ( pos != 0.0f )
    {
        stdControl_bControlsIdle = false;
    }

    return pos;
}

int J3DAPI stdControl_ReadAxisRaw(size_t axis)
{
    size_t aid = STDCONTROL_GETAID(axis);
    if ( aid >= STDCONTROL_MAX_AXES )
    {
        return 0;
    }

    if ( !stdControl_bControlsActive )
    {
        return 0;
    }

    if ( (stdControl_aAxes[aid].flags & STDCONTROL_AXIS_ENABLED) == 0 )
    {
        return 0;
    }

    int pos = stdControl_aAxisStates[aid] - stdControl_aAxes[aid].center;
    if ( !pos )
    {
        return 0;
    }

    if ( stdControl_bControlsIdle )
    {
        stdControl_bControlsIdle = false;
    }

    return pos;
}

float J3DAPI stdControl_ReadKeyAsAxis(size_t keyId)
{
    STD_ASSERTREL((keyId < STDCONTROL_MAX_KEYID));
    if ( !stdControl_bControlsActive )
    {
        return 0.0f;
    }

    uint32_t time = stdControl_aKeyIdleTimes[keyId];
    if ( !time )
    {
        if ( !stdControl_aKeyInfo[keyId] )
        {
            return 0.0f;
        }

        time = stdControl_readDeltaTime;
    }

    if ( time >= stdControl_readDeltaTime )
    {
        time = stdControl_readDeltaTime;
    }

    float deltaTime = (float)time * sithControl_secFPS;
    if ( !stdControl_bControlsIdle )
    {
        return deltaTime;
    }

    if ( deltaTime != 0.0f )
    {
        stdControl_bControlsIdle = false;
    }

    return deltaTime;
}

int J3DAPI stdControl_ReadAxisAsKey(size_t axis, int* pbPressed)
{
    if ( (axis & STDCONTROL_AID_LOW_SENSITIVITY) == 0 || (stdControl_aAxes[axis].flags & STDCONTROL_AXIS_GAMEPAD) != 0 )
    {
        return stdControl_ReadAxisAsKeyEx(axis, pbPressed, 0.25f);
    }
    else
    {
        return stdControl_ReadAxisAsKeyEx(axis, pbPressed, 0.75f);
    }
}

int J3DAPI stdControl_ReadAxisAsKeyEx(size_t axis, int* pbPressed, float lowValue)
{
    float pos = stdControl_ReadAxis(axis);
    if ( (axis & (STDCONTROL_AID_LOW_SENSITIVITY | STDCONTROL_AID_POSITIVE_AXIS | STDCONTROL_AID_NEGATIVE_AXIS)) != 0 )
    {
        int axisFlag = axis & STDCONTROL_AID_NEGATIVE_AXIS;
        if ( axisFlag == STDCONTROL_AID_POSITIVE_AXIS && pos > (double)lowValue )
        {
            // Fixed: Increment num key pressed by 1
            if ( pbPressed ) {
                *pbPressed = 1;
            }
            return 1;
        }

        if ( axisFlag == STDCONTROL_AID_NEGATIVE_AXIS && -lowValue > pos )
        {
            // Fixed: Increment num key pressed by 1
            if ( pbPressed ) {
                *pbPressed = 1;
            }
            return 1;
        }
    }
    else
    {
        if ( fabsf(pos) > lowValue )
        {
            // Fixed: Increment num key pressed by 1
            if ( pbPressed ) {
                *pbPressed = 1;
            }
            return 1;
        }
    }

    return 0;
}

int J3DAPI stdControl_ReadKey(size_t keyNum, int* pbPressed)
{
    STD_ASSERTREL(keyNum < STDCONTROL_MAX_KEYID);
    if ( stdControl_bControlsActive )
    {
        if ( pbPressed )
        {
            // Fixed: Was `=`; the original accumulates. sithControl_GetKey zeroes the state once and then calls this
            //        for every key bound to the control (e.g. jump: X, LALT, RALT), so assigning made the last bound
            //        key's press count overwrite the pressed key's, losing the key-press event.
            *pbPressed += stdControl_aKeyPressed[keyNum];
        }

        if ( stdControl_bControlsIdle && stdControl_aKeyInfo[keyNum] )
        {
            stdControl_bControlsIdle = false;
        }

        return stdControl_aKeyInfo[keyNum];
    }
    else
    {
        if ( pbPressed )
        {
            *pbPressed = 0;
        }

        return 0;
    }
}

void stdControl_FinishRead(void)
{}

void J3DAPI stdControl_RegisterMouseAxesXY(float xrange, float yrange)
{
    stdControl_mouseXRange = xrange;
    stdControl_mouseYRange = yrange;
    if ( (stdControl_aAxes[STDCONTROL_AID_MOUSE_X].flags & STDCONTROL_AXIS_REGISTERED) != 0 )
    {
        int range = lround(stdControl_mouseXRange * 250.0f);
        stdControl_RegisterAxis(STDCONTROL_AID_MOUSE_X, -range, range, 0.0f);
    }

    if ( (stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].flags & STDCONTROL_AXIS_REGISTERED) != 0 )
    {
        int range = lround(stdControl_mouseXRange * 200.0f);
        stdControl_RegisterAxis(STDCONTROL_AID_MOUSE_Y, -range, range, 0.0f);
    }
}

int stdControl_ControlsActive(void)
{
    return stdControl_bControlsActive ? 1 : 0;
}

void J3DAPI stdControl_UpdateKeyState(int keyId, int bPressed, unsigned int tickTime)
{
    if ( bPressed && !stdControl_aKeyInfo[keyId] )
    {
        stdControl_aKeyInfo[keyId] = true;
        stdControl_aKeyIdleTimes[keyId] = stdControl_curReadTime - tickTime;
        ++stdControl_aKeyPressed[keyId];
    }
    else if ( !bPressed && stdControl_aKeyInfo[keyId] )
    {
        stdControl_aKeyInfo[keyId] = false;
        if ( !stdControl_aKeyIdleTimes[keyId] )
        {
            stdControl_aKeyIdleTimes[keyId] = stdControl_readDeltaTime;
        }

        stdControl_aKeyIdleTimes[keyId] -= stdControl_curReadTime - tickTime;
    }
}

int J3DAPI stdControl_EnableMouse(int bEnable)
{
    if ( bEnable != stdControl_bReadMouse )
    {
        stdControl_ToggleMouse();
    }

    return 1;
}

bool stdControl_IsMouseEnabled(void)
{
    return stdControl_bReadMouse;
}

int stdControl_ControlsIdle(void)
{
    return stdControl_bControlsIdle ? 1 : 0;
}

int J3DAPI stdControl_TestAxisFlag(size_t axis, StdControlAxisFlag flags)
{
    uint32_t bPositiveAxis = axis & STDCONTROL_AID_POSITIVE_AXIS;
    uint32_t bNegativeAxis = axis & STDCONTROL_AID_NEGATIVE_AXIS;
    size_t aid = STDCONTROL_GETAID(axis);
    if ( aid >= STDCONTROL_MAX_AXES )
    {
        return 0;
    }

    if ( (flags & STDCONTROL_AXIS_ENABLED) == 0 )
    {
        return flags & stdControl_aAxes[aid].flags;
    }

    if ( bPositiveAxis )
    {
        flags = flags & ~(STDCONTROL_AXIS_POSITIVE | STDCONTROL_AXIS_ENABLED) | STDCONTROL_AXIS_POSITIVE;
    }

    if ( bNegativeAxis )
    {
        flags = flags & ~(STDCONTROL_AXIS_NEGATIVE | STDCONTROL_AXIS_ENABLED) | STDCONTROL_AXIS_NEGATIVE;
    }

    return flags & stdControl_aAxes[aid].flags;
}

void J3DAPI stdControl_SetAxisFlags(size_t axis, StdControlAxisFlag flags)
{
    uint32_t bPositiveAxis = axis & STDCONTROL_AID_POSITIVE_AXIS;
    uint32_t  bNegativeAxis = axis & STDCONTROL_AID_NEGATIVE_AXIS;
    size_t aid = STDCONTROL_GETAID(axis);
    if ( aid < STDCONTROL_MAX_AXES )
    {
        if ( (flags & STDCONTROL_AXIS_ENABLED) != 0 )
        {
            if ( bPositiveAxis )
            {
                flags |= STDCONTROL_AXIS_POSITIVE;
            }

            if ( bNegativeAxis )
            {
                flags |= STDCONTROL_AXIS_NEGATIVE;
            }
        }

        stdControl_aAxes[aid].flags |= flags;
    }
}

void J3DAPI stdControl_EnableAxisRead(size_t axis)
{
    if ( axis >= STDCONTROL_AID_MOUSE_X && axis < STDCONTROL_MAX_AXES )
    {
        stdControl_bReadMouse = true;
    }

    else if ( axis < STDCONTROL_AID_MOUSE_X )
    {
        stdControl_bReadJoysticks = true;
    }
}

void J3DAPI stdControl_RegisterAxis(size_t aid, int min, int max, float deadzoneScale)
{
    STD_ASSERTREL(max > min);
    STD_ASSERTREL(max - min > 0);
    STD_ASSERTREL(aid < STDCONTROL_MAX_AXES);

    int center = (max - min + 1) / 2 + min;

    stdControl_aAxes[aid].flags |= STDCONTROL_AXIS_REGISTERED;
    stdControl_aAxes[aid].min    = min;
    stdControl_aAxes[aid].max    = max;
    stdControl_aAxes[aid].center = center;
    stdControl_aAxes[aid].scale  = 1.0f / (float)(max - center);

    if ( deadzoneScale == 0.0f )
    {
        stdControl_aAxes[aid].deadzoneThreshold = 0;
    }
    else
    {
        stdControl_aAxes[aid].deadzoneThreshold = lround((float)(max - center) * deadzoneScale);
    }
}

void stdControl_ResetMousePos(void)
{
    // TODO: missing `mousePos` reset
    stdControl_aAxisStates[STDCONTROL_AID_MOUSE_X] = 0;
    stdControl_aAxisStates[STDCONTROL_AID_MOUSE_Y] = 0;
    stdControl_aAxisStates[STDCONTROL_AID_MOUSE_Z] = 0;
}

size_t stdControl_GetNumJoysticks(void)
{
    stdControl_UpdateJoystickDevices();
    return stdControl_numJoystickDevices;
}

int stdControl_MouseSensitivityEnabled(void)
{
    return stdControl_bMouseSensitivityEnabled;
}

void J3DAPI stdControl_EnableMouseSensitivity(int bEnable)
{
    stdControl_bMouseSensitivityEnabled = bEnable != 0;
}

void J3DAPI stdControl_SetMouseSensitivity(float xSensitivity, float ySensitivity)
{
    if ( (stdControl_aAxes[STDCONTROL_AID_MOUSE_X].flags & STDCONTROL_AXIS_REGISTERED) != 0 )
    {
        stdControl_aAxes[STDCONTROL_AID_MOUSE_X].deadzoneThreshold = 0;
        stdControl_aAxes[STDCONTROL_AID_MOUSE_X].max       = (int)(xSensitivity * 250.0);
        stdControl_aAxes[STDCONTROL_AID_MOUSE_X].min       = -stdControl_aAxes[STDCONTROL_AID_MOUSE_X].max;
        stdControl_aAxes[STDCONTROL_AID_MOUSE_X].flags    |= STDCONTROL_AXIS_REGISTERED;
        stdControl_aAxes[STDCONTROL_AID_MOUSE_X].center    = (2 * stdControl_aAxes[STDCONTROL_AID_MOUSE_X].max + 1) / 2 - stdControl_aAxes[STDCONTROL_AID_MOUSE_X].max;
        stdControl_aAxes[STDCONTROL_AID_MOUSE_X].scale     = 1.0f / (float)(stdControl_aAxes[STDCONTROL_AID_MOUSE_X].max - stdControl_aAxes[STDCONTROL_AID_MOUSE_X].center);
    }

    if ( (stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].flags & STDCONTROL_AXIS_REGISTERED) != 0 )
    {
        stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].deadzoneThreshold   = 0;
        stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].max       = (int)(ySensitivity * 200.0);
        stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].min       = -stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].max;
        stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].flags    |= STDCONTROL_AXIS_REGISTERED;
        stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].center    = (2 * stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].max + 1) / 2 - stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].max;
        stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].scale     = 1.0f / (float)(stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].max - stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].center);
    }
}

// SDL3 input: keyboard state, relative mouse and gamepads.
// Gamepads are mapped like an XInput controller seen through DirectInput, so the joystick bindings keep their meaning:
// axes X/Y left stick, Z triggers, RX/RY right stick; buttons A, B, X, Y, LB, RB, Back, Start, LS, RS; d-pad is POV 0.

static int stdControl_mouseWheel = 0;
static bool stdControl_bMouseActive     = false;
static bool stdControl_bJoysticksChanged = false;

static bool SDLCALL stdControl_EventWatch(void* pUserData, SDL_Event* pEvent)
{
    J3D_UNUSED(pUserData);
    if ( pEvent->type == SDL_EVENT_MOUSE_WHEEL ) {
        stdControl_mouseWheel += (int)(pEvent->wheel.y * 120.0f); // WHEEL_DELTA units like DirectInput
    }
    else if ( pEvent->type == SDL_EVENT_GAMEPAD_ADDED || pEvent->type == SDL_EVENT_GAMEPAD_REMOVED ) {
        stdControl_bJoysticksChanged = true; // Gamepads are enumerated again by stdControl_UpdateJoystickDevices
    }
    return true;
}

static void stdControl_CloseJoysticks(void)
{
    for ( size_t i = 0; i < stdControl_numJoystickDevices; i++ )
    {
        if ( stdControl_aJoystickDevices[i].pGamepad ) {
            SDL_CloseGamepad((SDL_Gamepad*)stdControl_aJoystickDevices[i].pGamepad);
        }
    }

    stdControl_numJoystickDevices = 0;
    memset(stdControl_aJoystickDevices, 0, sizeof(stdControl_aJoystickDevices));
}

// Added: Enumerates the gamepads again when one was connected or disconnected while the game runs,
//        the original enumerated the joysticks only at startup
static void stdControl_UpdateJoystickDevices(void)
{
    if ( !stdControl_bStartup || !stdControl_bJoysticksChanged ) {
        return;
    }

    stdControl_bJoysticksChanged = false;

    // Release the controls of the current devices
    for ( size_t joyNum = 0; joyNum < stdControl_numJoystickDevices; ++joyNum )
    {
        for ( size_t btnNum = 0; btnNum < STDCONTROL_NUM_JOYSTICK_BUTTONS; ++btnNum ) {
            stdControl_UpdateKeyState(STDCONTROL_JOYSTICK_GETBUTTON(joyNum, btnNum), 0, stdControl_curReadTime);
        }

        stdControl_UpdateKeyState(STDCONTROL_JOYSTICK_GETPOVLEFT(joyNum, 0), 0, stdControl_curReadTime);
        stdControl_UpdateKeyState(STDCONTROL_JOYSTICK_GETPOVUP(joyNum, 0), 0, stdControl_curReadTime);
        stdControl_UpdateKeyState(STDCONTROL_JOYSTICK_GETPOVRIGHT(joyNum, 0), 0, stdControl_curReadTime);
        stdControl_UpdateKeyState(STDCONTROL_JOYSTICK_GETPOVDOWN(joyNum, 0), 0, stdControl_curReadTime);

        for ( size_t axis = 0; axis < STDCONTROL_JOYSTICK_NUMAXES; ++axis ) {
            stdControl_aAxisStates[STDCONTROL_GET_JOYSTICK_AXIS(joyNum, axis)] = 0;
        }
    }

    stdControl_CloseJoysticks();
    stdControl_InitJoysticks();
}

static SDL_Window* stdControl_GetSDLWindow(void)
{
    return (SDL_Window*)stdWin95_GetWindow();
}

static void stdControl_UpdateRelativeMouse(void)
{
    SDL_Window* pWindow = stdControl_GetSDLWindow();
    if ( pWindow ) {
        SDL_SetWindowRelativeMouseMode(pWindow, stdControl_bControlsActive && stdControl_bReadMouse && stdControl_bMouseActive);
    }
}

int J3DAPI stdControl_Startup(int bKeyboardForeground)
{
    J3D_UNUSED(bKeyboardForeground);
    if ( stdControl_bStartup ) {
        return 1;
    }

    memset(stdControl_aKeyIdleTimes, 0, sizeof(stdControl_aKeyIdleTimes));
    memset(stdControl_aKeyInfo, 0, sizeof(stdControl_aKeyInfo));
    memset(stdControl_aAxes, 0, sizeof(stdControl_aAxes));
    memset(stdControl_aAxisStates, 0, sizeof(stdControl_aAxisStates));

    stdControl_mousePos.x = 0;
    stdControl_mousePos.y = 0;
    stdControl_cursorDisplayCounter = 0;

    stdControl_numJoystickDevices = 0;
    memset(stdControl_aJoystickDevices, 0, sizeof(stdControl_aJoystickDevices));

    SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    SDL_AddEventWatch(stdControl_EventWatch, NULL);
    stdControl_bJoysticksChanged = false; // Enumerated below

    stdControl_InitKeyboard(bKeyboardForeground);
    stdControl_InitJoysticks();
    stdControl_InitMouse();
    stdControl_Reset();

    stdControl_bStartup = true;
    return 0;
}

void stdControl_Shutdown(void)
{
    if ( stdControl_bStartup )
    {
        stdControl_bStartup = false;
        SDL_RemoveEventWatch(stdControl_EventWatch, NULL);
        stdControl_CloseJoysticks();

        stdControl_bMouseActive = false;
        stdControl_UpdateRelativeMouse();
    }
}

int J3DAPI stdControl_SetActivation(int bActive)
{
    if ( !stdControl_bOpen ) {
        return 1;
    }

    for ( size_t keyId = 0; keyId < STDCONTROL_MAX_KEYBOARD_BUTTONS; ++keyId ) {
        stdControl_UpdateKeyState(keyId, 0, stdControl_curReadTime);
    }

    for ( size_t keyId = 0; keyId < STDCONTROL_MAX_MOUSE_BUTTONS; ++keyId ) {
        stdControl_UpdateKeyState(STDCONTROL_KID_MOUSE_LBUTTON + keyId, 0, stdControl_curReadTime);
    }

    stdControl_bControlsActive = bActive != 0;
    stdControl_bMouseActive    = bActive != 0;
    stdControl_UpdateRelativeMouse();
    SDL_GetRelativeMouseState(NULL, NULL); // discard the motion while inactive
    return 0;
}

int stdControl_ToggleMouse(void)
{
    stdControl_bReadMouse = !stdControl_bReadMouse;
    stdControl_ShowMouseCursor(!stdControl_bReadMouse);
    stdControl_UpdateRelativeMouse();
    return stdControl_bReadMouse;
}

void stdControl_InitJoysticks(void)
{
    int numGamepads = 0;
    SDL_JoystickID* aIds = SDL_GetGamepads(&numGamepads);
    for ( int i = 0; aIds && i < numGamepads && stdControl_numJoystickDevices < STDCONTROL_MAX_JOYSTICK_DEVICES; ++i )
    {
        SDL_Gamepad* pGamepad = SDL_OpenGamepad(aIds[i]);
        if ( !pGamepad )
        {
            STDLOG_STATUS("Error opening gamepad: %s\n", SDL_GetError());
            continue;
        }

        size_t joyNum = stdControl_numJoystickDevices++;
        stdControl_aJoystickDevices[joyNum].pGamepad   = pGamepad;
        stdControl_aJoystickDevices[joyNum].numButtons = 10;

        stdControl_RegisterAxis(STDCONTROL_GET_JOYSTICK_AXIS_X(joyNum), -32768, 32767, 0.2f);
        stdControl_RegisterAxis(STDCONTROL_GET_JOYSTICK_AXIS_Y(joyNum), -32768, 32767, 0.2f);
        stdControl_RegisterAxis(STDCONTROL_GET_JOYSTICK_AXIS_Z(joyNum), -32768, 32767, 0.2f);
        stdControl_RegisterAxis(STDCONTROL_GET_JOYSTICK_AXIS_RX(joyNum), -32768, 32767, 0.2f);
        stdControl_RegisterAxis(STDCONTROL_GET_JOYSTICK_AXIS_RY(joyNum), -32768, 32767, 0.2f);
        stdControl_RegisterAxis(STDCONTROL_GET_JOYSTICK_AXIS_RZ(joyNum), -32768, 32767, 0.2f);

        for ( size_t axis = 0; axis < STDCONTROL_JOYSTICK_NUMAXES; ++axis ) {
            stdControl_aAxes[STDCONTROL_GET_JOYSTICK_AXIS(joyNum, axis)].flags |= STDCONTROL_AXIS_GAMEPAD;
        }

        STDLOG_STATUS("Found gamepad %s.\n", SDL_GetGamepadName(pGamepad));
    }

    SDL_free(aIds);
}

void J3DAPI stdControl_InitKeyboard(int bForeground)
{
    J3D_UNUSED(bForeground);
}

void stdControl_InitMouse(void)
{
    stdControl_RegisterAxis(STDCONTROL_AID_MOUSE_X, -250, 250, 0.0f);
    stdControl_RegisterAxis(STDCONTROL_AID_MOUSE_Y, -200, 200, 0.0f);
    stdControl_RegisterAxis(STDCONTROL_AID_MOUSE_Z, -20, 20, 0.0f);
}

void stdControl_ReadKeyboard(void)
{
    uint8_t aState[STDCONTROL_MAX_KEYBOARD_BUTTONS] = { 0 };

    int numKeys = 0;
    const bool* aKeys = SDL_GetKeyboardState(&numKeys);
    for ( int sc = 0; sc < numKeys; ++sc )
    {
        if ( aKeys[sc] )
        {
            uint8_t dik = stdSDL_ScancodeToDIK((SDL_Scancode)sc);
            if ( dik ) {
                aState[dik] = 0x80;
            }
        }
    }

    for ( size_t keyId = 0; keyId < STDCONTROL_MAX_KEYBOARD_BUTTONS; ++keyId ) {
        stdControl_UpdateKeyState(keyId, aState[keyId] & 0x80, stdControl_curReadTime);
    }
}

void stdControl_ReadJoysticks(void)
{
    static const SDL_GamepadButton aButtons[] = {
        SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_START,
        SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK
    };

    for ( size_t joyNum = 0; joyNum < stdControl_numJoystickDevices; joyNum++ )
    {
        SDL_Gamepad* pGamepad = (SDL_Gamepad*)stdControl_aJoystickDevices[joyNum].pGamepad;
        if ( !pGamepad ) {
            continue;
        }

        int leftTrigger  = SDL_GetGamepadAxis(pGamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);  // 0 - 32767
        int rightTrigger = SDL_GetGamepadAxis(pGamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

        stdControl_aAxisStates[STDCONTROL_GET_JOYSTICK_AXIS_X(joyNum)]  = SDL_GetGamepadAxis(pGamepad, SDL_GAMEPAD_AXIS_LEFTX);
        stdControl_aAxisStates[STDCONTROL_GET_JOYSTICK_AXIS_Y(joyNum)]  = SDL_GetGamepadAxis(pGamepad, SDL_GAMEPAD_AXIS_LEFTY);
        stdControl_aAxisStates[STDCONTROL_GET_JOYSTICK_AXIS_Z(joyNum)]  = leftTrigger - rightTrigger; // XInput triggers share the Z axis in DirectInput
        stdControl_aAxisStates[STDCONTROL_GET_JOYSTICK_AXIS_RX(joyNum)] = SDL_GetGamepadAxis(pGamepad, SDL_GAMEPAD_AXIS_RIGHTX);
        stdControl_aAxisStates[STDCONTROL_GET_JOYSTICK_AXIS_RY(joyNum)] = SDL_GetGamepadAxis(pGamepad, SDL_GAMEPAD_AXIS_RIGHTY);
        stdControl_aAxisStates[STDCONTROL_GET_JOYSTICK_AXIS_RZ(joyNum)] = 0;

        for ( size_t btnNum = 0; btnNum < STDCONTROL_NUM_JOYSTICK_BUTTONS; ++btnNum )
        {
            int bDown = btnNum < STD_ARRAYLEN(aButtons) && SDL_GetGamepadButton(pGamepad, aButtons[btnNum]);
            stdControl_UpdateKeyState(STDCONTROL_JOYSTICK_GETBUTTON(joyNum, btnNum), bDown, stdControl_curReadTime);
        }

        // D-pad as POV 0: left, up, right, down
        stdControl_UpdateKeyState(STDCONTROL_JOYSTICK_GETPOVLEFT(joyNum, 0), SDL_GetGamepadButton(pGamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT), stdControl_curReadTime);
        stdControl_UpdateKeyState(STDCONTROL_JOYSTICK_GETPOVUP(joyNum, 0), SDL_GetGamepadButton(pGamepad, SDL_GAMEPAD_BUTTON_DPAD_UP), stdControl_curReadTime);
        stdControl_UpdateKeyState(STDCONTROL_JOYSTICK_GETPOVRIGHT(joyNum, 0), SDL_GetGamepadButton(pGamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT), stdControl_curReadTime);
        stdControl_UpdateKeyState(STDCONTROL_JOYSTICK_GETPOVDOWN(joyNum, 0), SDL_GetGamepadButton(pGamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN), stdControl_curReadTime);
    }
}

void stdControl_ReadMouse(void)
{
    if ( !stdControl_bReadMouse ) {
        return;
    }

    float dx = 0.0f, dy = 0.0f;
    SDL_MouseButtonFlags buttons = SDL_GetRelativeMouseState(&dx, &dy);
    int lX = (int)dx;
    int lY = (int)dy;
    int lZ = stdControl_mouseWheel;
    stdControl_mouseWheel = 0;

    // Update axis state
    if ( stdControl_bMouseSensitivityEnabled )
    {
        stdControl_aAxisStates[STDCONTROL_AID_MOUSE_X] += lX;
        stdControl_aAxisStates[STDCONTROL_AID_MOUSE_X]  = STDMATH_CLAMP(stdControl_aAxisStates[STDCONTROL_AID_MOUSE_X], stdControl_aAxes[STDCONTROL_AID_MOUSE_X].min, stdControl_aAxes[STDCONTROL_AID_MOUSE_X].max);

        stdControl_aAxisStates[STDCONTROL_AID_MOUSE_Y] += lY;
        stdControl_aAxisStates[STDCONTROL_AID_MOUSE_Y]  = STDMATH_CLAMP(stdControl_aAxisStates[STDCONTROL_AID_MOUSE_Y], stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].min, stdControl_aAxes[STDCONTROL_AID_MOUSE_Y].max);

        stdControl_aAxisStates[STDCONTROL_AID_MOUSE_Z] += lZ;
        stdControl_aAxisStates[STDCONTROL_AID_MOUSE_Z]  = STDMATH_CLAMP(stdControl_aAxisStates[STDCONTROL_AID_MOUSE_Z], stdControl_aAxes[STDCONTROL_AID_MOUSE_Z].min, stdControl_aAxes[STDCONTROL_AID_MOUSE_Z].max);
    }
    else
    {
        stdControl_aAxisStates[STDCONTROL_AID_MOUSE_X] = lX;
        stdControl_aAxisStates[STDCONTROL_AID_MOUSE_Y] = lY;
        stdControl_aAxisStates[STDCONTROL_AID_MOUSE_Z] = lZ;
        if ( stdControl_readDeltaTime < 25 )
        {
            stdControl_aAxisStates[STDCONTROL_AID_MOUSE_X] = (stdControl_mousePos.x + lX) / 2;
            stdControl_aAxisStates[STDCONTROL_AID_MOUSE_Y] = (stdControl_mousePos.y + lY) / 2;
        }

        stdControl_mousePos.x = lX;
        stdControl_mousePos.y = lY;
    }

    stdControl_UpdateKeyState(STDCONTROL_KID_MOUSE_LBUTTON, (buttons & SDL_BUTTON_LMASK) != 0, stdControl_curReadTime);
    stdControl_UpdateKeyState(STDCONTROL_KID_MOUSE_RBUTTON, (buttons & SDL_BUTTON_RMASK) != 0, stdControl_curReadTime);
    stdControl_UpdateKeyState(STDCONTROL_KID_MOUSE_MBUTTON, (buttons & SDL_BUTTON_MMASK) != 0, stdControl_curReadTime);
    stdControl_UpdateKeyState(STDCONTROL_KID_MOUSE_XBUTTON, (buttons & SDL_BUTTON_X1MASK) != 0, stdControl_curReadTime);
}

size_t stdControl_GetMaxJoystickButtons(void)
{
    size_t maxButtons = 0;
    for ( size_t i = 0; i < stdControl_numJoystickDevices; i++ )
    {
        if ( stdControl_aJoystickDevices[i].numButtons > maxButtons ) {
            maxButtons = stdControl_aJoystickDevices[i].numButtons;
        }
    }

    return maxButtons;
}

const char* J3DAPI stdControl_GetJoysticDescription(int joyNum)
{
    const char* pName = SDL_GetGamepadName((SDL_Gamepad*)stdControl_aJoystickDevices[joyNum].pGamepad);
    STD_FORMAT(stdControl_aStrBuf, "%s", pName ? pName : "Gamepad");
    return stdControl_aStrBuf;
}

void J3DAPI stdControl_ShowMouseCursor(int bShow)
{
    if ( bShow ) {
        SDL_ShowCursor();
    }
    else {
        SDL_HideCursor();
    }
    stdControl_cursorDisplayCounter = bShow ? 0 : -1;
}

int J3DAPI stdControl_IsGamePad(int joyNum)
{
    J3D_UNUSED(joyNum);
    return 1;
}

