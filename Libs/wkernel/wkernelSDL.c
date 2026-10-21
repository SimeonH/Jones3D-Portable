// wkernel on SDL3 (non-Windows builds).
// The game window is an SDL window, its SDL_Window* is the HWND of the game (stdWin95_GetWindow).
// SDL events are translated to the Win32 window messages the game window procedures handle,
// so the game code processes them the same way on all platforms.
#include "wkernel.h"

#include <std/SDL/stdSDL.h>
#include <std/Win95/stdWin95.h>

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#define WKERNEL_MAXTIMERS 16

typedef struct sWKernelTimer
{
    UINT_PTR id;
    uint32_t interval;
    uint64_t nextTime;
} WKernelTimer;

static SDL_Window* wkernel_pWindow = NULL;
static StdWindowApi wkernel_windowApi = STDWINDOW_API_VULKAN;
static bool wkernel_bQuit          = false;
static Uint32 wkernel_postEventType = 0;

static WKERNELPROC wkernel_pfProcess            = NULL;
static WKERNELSTARTUPPROC wkernel_pfOnStartup   = NULL;
static WKERNELSHUTDOWNPROC wkernel_pfOnShutdown = NULL;
static WKERNELWNDPROC wkernel_pfWndProc         = NULL;

static WKernelTimer wkernel_aTimers[WKERNEL_MAXTIMERS];
static GUID wkernel_guid = { 0x82CE4DA0, 0x9CBF, 0x01D1, { 0x90, 0x85, 0x00, 0x60, 0x97, 0x76, 0xEA, 0x02 } };

static int wkernel_CreateWindow(LPCSTR lpWindowName);
static HWND wkernel_SetWindowApi(StdWindowApi api);
static int wkernel_ProcessEvent(const SDL_Event* pEvent);

void wkernel_InstallHooks(void)
{
}

// Sends a message to the game window procedure
static LRESULT wkernel_SendMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if ( uMsg == WM_CLOSE )
    {
        // Like DefWindowProc on WM_CLOSE: shut down, destroy the window and quit
        if ( wkernel_pfOnShutdown ) {
            wkernel_pfOnShutdown();
        }

        if ( wkernel_pWindow )
        {
            SDL_DestroyWindow(wkernel_pWindow);
            wkernel_pWindow = NULL;
        }

        wkernel_bQuit = true;
        return 0;
    }

    int retValue = 0;
    if ( wkernel_pfWndProc && wkernel_pWindow && wkernel_pfWndProc(wkernel_pWindow, uMsg, wParam, lParam, &retValue) ) {
        return retValue;
    }

    return DefWindowProc(wkernel_pWindow, uMsg, wParam, lParam);
}

int J3DAPI wkernel_Run(HINSTANCE hinstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd, LPCSTR lpWindowName)
{
    J3D_UNUSED(hPrevInstance);
    J3D_UNUSED(nShowCmd);

    if ( wkernel_pfProcess == NULL )
    {
        fprintf(stderr, "ERROR: wkernel_Run: No main process set!\n");
        return -1;
    }

    if ( !SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD) )
    {
        fprintf(stderr, "ERROR: wkernel_Run: SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    wkernel_postEventType = SDL_RegisterEvents(1);
    stdWin95_SetWindowApiFunc(wkernel_SetWindowApi);

    if ( wkernel_CreateWindow(lpWindowName) )
    {
        SDL_Quit();
        return -1;
    }

    stdWin95_SetWindow(wkernel_pWindow);
    stdWin95_SetInstance(hinstance);
    stdWin95_SetGuid(&wkernel_guid);

    int result = 0;
    if ( wkernel_pfOnStartup && wkernel_pfOnStartup(lpCmdLine) ) {
        result = -1;
    }

    while ( result != -1 )
    {
        if ( result == 1 ) // Finish
        {
            result = 0;
            break;
        }
        else if ( result )
        {
            fprintf(stdout, "WARNING: wkernel_Run: Unhandled result code: %d, skipping...\n", result);
        }

        result = wkernel_pfProcess();
    }

    if ( wkernel_pWindow )
    {
        SDL_DestroyWindow(wkernel_pWindow);
        wkernel_pWindow = NULL;
    }

    stdWin95_SetWindowApiFunc(NULL);
    SDL_Quit();
    return result;
}

WKERNELPROC J3DAPI wkernel_SetProcessProc(WKERNELPROC pfProc)
{
    WKERNELPROC pfCurProc = wkernel_pfProcess;
    wkernel_pfProcess = pfProc;
    return pfCurProc;
}

WKERNELSTARTUPPROC J3DAPI wkernel_SetStartupCallback(WKERNELSTARTUPPROC pfOnStartup)
{
    WKERNELSTARTUPPROC pfCurProc = wkernel_pfOnStartup;
    wkernel_pfOnStartup = pfOnStartup;
    return pfCurProc;
}

WKERNELSHUTDOWNPROC J3DAPI wkernel_SetShutdownCallback(WKERNELSHUTDOWNPROC pfOnShutdown)
{
    WKERNELSHUTDOWNPROC pfCurProc = wkernel_pfOnShutdown;
    wkernel_pfOnShutdown = pfOnShutdown;
    return pfCurProc;
}

static void wkernel_ProcessTimers(void)
{
    uint64_t now = SDL_GetTicks();
    for ( size_t i = 0; i < WKERNEL_MAXTIMERS && !wkernel_bQuit; ++i )
    {
        WKernelTimer* pTimer = &wkernel_aTimers[i];
        if ( pTimer->id && now >= pTimer->nextTime )
        {
            pTimer->nextTime = now + pTimer->interval;
            wkernel_SendMessage(WM_TIMER, pTimer->id, 0);
        }
    }
}

// Processes all pending events, returns 1 when the game should quit
int wkernel_PeekProcessEvents(void)
{
    SDL_Event event;
    while ( !wkernel_bQuit && SDL_PollEvent(&event) ) {
        wkernel_ProcessEvent(&event);
    }

    wkernel_ProcessTimers();
    return wkernel_bQuit ? 1 : 0;
}

// Waits for an event and processes all pending events, returns 1 when the game should quit
int wkernel_ProcessEvents(void)
{
    SDL_Event event;
    if ( !wkernel_bQuit && SDL_WaitEventTimeout(&event, 10) ) {
        wkernel_ProcessEvent(&event);
    }

    return wkernel_PeekProcessEvents();
}

void J3DAPI wkernel_SetWindowStyle(LONG dwNewLong)
{
    if ( !wkernel_pWindow ) {
        return;
    }

    // WS_POPUP is the fullscreen style, WS_OVERLAPPEDWINDOW the windowed one
    bool bBordered = (dwNewLong & WS_CAPTION) != 0;
    SDL_SetWindowBordered(wkernel_pWindow, bBordered);
    SDL_SetWindowResizable(wkernel_pWindow, (dwNewLong & WS_THICKFRAME) != 0);
    if ( dwNewLong & WS_VISIBLE ) {
        SDL_ShowWindow(wkernel_pWindow);
    }
}

BOOL J3DAPI wkernel_SetWindowSize(int width, int height)
{
    // SDL sizes the client area, there's no border to account for.
    // Wait for the compositor to apply it, the renderer sizes the swapchain by the window.
    if ( !wkernel_pWindow || !SDL_SetWindowSize(wkernel_pWindow, width, height) ) {
        return FALSE;
    }

    SDL_SyncWindow(wkernel_pWindow);
    return TRUE;
}

void J3DAPI wkernel_SetWindowProc(WKERNELWNDPROC pfProc)
{
    wkernel_pfWndProc = pfProc;
}

void J3DAPI wkernel_SetResourceInstance(HINSTANCE hInstance)
{
    J3D_UNUSED(hInstance);
}

HINSTANCE wkernel_GetResourceInstance(void)
{
    return NULL;
}

// The window is created for the graphics API of the renderer, see stdWin95_SetWindowApi
static SDL_Window* wkernel_NewWindow(const char* pTitle, int width, int height, SDL_WindowFlags flags, StdWindowApi api)
{
    if ( api == STDWINDOW_API_OPENGL ) {
        stdSDL_SetGLAttributes();
    }

    flags |= api == STDWINDOW_API_OPENGL ? SDL_WINDOW_OPENGL : SDL_WINDOW_VULKAN;
    SDL_Window* pWindow = SDL_CreateWindow(pTitle, width, height, flags | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if ( !pWindow )
    {
        fprintf(stderr, "ERROR: wkernel_CreateWindow: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_StartTextInput(pWindow); // WM_CHAR
    return pWindow;
}

static int wkernel_CreateWindow(LPCSTR lpWindowName)
{
    wkernel_windowApi = stdWin95_GetPreferredWindowApi();
    // Shown when the display is opened (wkernel_SetWindowStyle with WS_VISIBLE), so there's no empty window
    // behind the launcher and the dialogs shown at startup
    wkernel_pWindow   = wkernel_NewWindow(lpWindowName, 640, 480, SDL_WINDOW_HIDDEN, wkernel_windowApi);
    return wkernel_pWindow ? 0 : 1;
}

static HWND wkernel_SetWindowApi(StdWindowApi api)
{
    if ( !wkernel_pWindow || api == wkernel_windowApi ) {
        return wkernel_pWindow;
    }

    // A window can't switch its API, replace it with a new window in the same state.
    // The new window is created hidden, so the old one keeps its place until it's shown.
    SDL_WindowFlags oldFlags = SDL_GetWindowFlags(wkernel_pWindow);
    SDL_WindowFlags flags    = SDL_WINDOW_HIDDEN | (oldFlags & (SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN));

    int x = 0, y = 0, width = 640, height = 480;
    SDL_GetWindowPosition(wkernel_pWindow, &x, &y);
    SDL_GetWindowSize(wkernel_pWindow, &width, &height);

    char aTitle[256];
    SDL_strlcpy(aTitle, SDL_GetWindowTitle(wkernel_pWindow), sizeof(aTitle));

    SDL_Window* pWindow = wkernel_NewWindow(aTitle, width, height, flags, api);
    if ( !pWindow ) {
        return wkernel_pWindow;
    }

    if ( !(oldFlags & SDL_WINDOW_FULLSCREEN) ) {
        SDL_SetWindowPosition(pWindow, x, y);
    }

    SDL_DestroyWindow(wkernel_pWindow);
    wkernel_pWindow   = pWindow;
    wkernel_windowApi = api;
    stdWin95_SetWindow(pWindow);

    if ( !(oldFlags & SDL_WINDOW_HIDDEN) )
    {
        SDL_ShowWindow(pWindow);
        SDL_RaiseWindow(pWindow);
    }

    return pWindow;
}

static LPARAM wkernel_MakeKeyLParam(const SDL_KeyboardEvent* pKey, bool bUp)
{
    // repeat count | scan code << 16 | extended << 24 | previous state << 30 | transition << 31
    LPARAM lParam = 1;
    lParam |= (LPARAM)stdSDL_ScancodeToDIK(pKey->scancode) << 16;
    if ( pKey->repeat ) {
        lParam |= (LPARAM)1 << 30;
    }
    if ( bUp ) {
        lParam |= ((LPARAM)1 << 30) | ((LPARAM)1 << 31);
    }

    return lParam;
}

// Sends WM_CHAR for each character of an UTF-8 text input. Characters outside Latin-1 are dropped.
static void wkernel_SendText(const char* pText)
{
    const unsigned char* p = (const unsigned char*)pText;
    while ( *p && !wkernel_bQuit )
    {
        uint32_t cp = *p++;
        if ( cp >= 0xC0 )
        {
            int numExtra = cp >= 0xF0 ? 3 : cp >= 0xE0 ? 2 : 1;
            cp &= 0x3F >> numExtra;
            while ( numExtra-- && (*p & 0xC0) == 0x80 ) {
                cp = (cp << 6) | (*p++ & 0x3F);
            }
        }

        if ( cp <= 0xFF ) {
            wkernel_SendMessage(WM_CHAR, cp, 1);
        }
    }
}

static int wkernel_ProcessEvent(const SDL_Event* pEvent)
{
    // Skip queued events of a game window replaced by wkernel_SetWindowApi
    if ( pEvent->type >= SDL_EVENT_WINDOW_FIRST && pEvent->type <= SDL_EVENT_WINDOW_LAST
        && wkernel_pWindow && pEvent->window.windowID != SDL_GetWindowID(wkernel_pWindow) )
    {
        return 0;
    }

    switch ( pEvent->type )
    {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            wkernel_SendMessage(WM_CLOSE, 0, 0);
            break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            wkernel_SendMessage(WM_ACTIVATEAPP, TRUE, 0);
            wkernel_SendMessage(WM_ACTIVATE, WA_ACTIVE, 0);
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            wkernel_SendMessage(WM_ACTIVATEAPP, FALSE, 0);
            wkernel_SendMessage(WM_ACTIVATE, WA_INACTIVE, 0);
            break;

        case SDL_EVENT_WINDOW_EXPOSED:
            wkernel_SendMessage(WM_PAINT, 0, 0);
            break;

        case SDL_EVENT_KEY_DOWN:
        {
            UINT msg = (pEvent->key.mod & SDL_KMOD_ALT) ? WM_SYSKEYDOWN : WM_KEYDOWN;
            uint32_t vk = stdSDL_KeycodeToVK(pEvent->key.key);
            if ( vk ) {
                wkernel_SendMessage(msg, vk, wkernel_MakeKeyLParam(&pEvent->key, false));
            }

            // TranslateMessage generates WM_CHAR for these keys too
            switch ( vk )
            {
                case VK_RETURN: wkernel_SendMessage(WM_CHAR, '\r', 1); break;
                case VK_BACK:   wkernel_SendMessage(WM_CHAR, '\b', 1); break;
                case VK_ESCAPE: wkernel_SendMessage(WM_CHAR, 0x1B, 1); break;
                case VK_TAB:    wkernel_SendMessage(WM_CHAR, '\t', 1); break;
                default: break;
            }
            break;
        }

        case SDL_EVENT_KEY_UP:
        {
            UINT msg = (pEvent->key.mod & SDL_KMOD_ALT) ? WM_SYSKEYUP : WM_KEYUP;
            uint32_t vk = stdSDL_KeycodeToVK(pEvent->key.key);
            if ( vk ) {
                wkernel_SendMessage(msg, vk, wkernel_MakeKeyLParam(&pEvent->key, true));
            }
            break;
        }

        case SDL_EVENT_TEXT_INPUT:
            wkernel_SendText(pEvent->text.text);
            break;

        case SDL_EVENT_MOUSE_MOTION:
            wkernel_SendMessage(WM_MOUSEMOVE, 0, MAKELONG((int)pEvent->motion.x, (int)pEvent->motion.y));
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            bool bDown = pEvent->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
            LPARAM lParam = MAKELONG((int)pEvent->button.x, (int)pEvent->button.y);
            if ( pEvent->button.button == SDL_BUTTON_LEFT ) {
                wkernel_SendMessage(bDown ? WM_LBUTTONDOWN : WM_LBUTTONUP, bDown ? MK_LBUTTON : 0, lParam);
            }
            else if ( pEvent->button.button == SDL_BUTTON_RIGHT ) {
                wkernel_SendMessage(bDown ? WM_RBUTTONDOWN : WM_RBUTTONUP, bDown ? MK_RBUTTON : 0, lParam);
            }
            break;
        }

        default:
            if ( pEvent->type == wkernel_postEventType )
            {
                // Message posted with PostMessage
                wkernel_SendMessage((UINT)pEvent->user.code, (WPARAM)pEvent->user.data1, (LPARAM)pEvent->user.data2);
            }
            break;
    }

    return 0;
}

// Win32 functions used by the game code

UINT_PTR SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc)
{
    J3D_UNUSED(hWnd);
    J3D_UNUSED(lpTimerFunc);

    WKernelTimer* pFree = NULL;
    for ( size_t i = 0; i < WKERNEL_MAXTIMERS; ++i )
    {
        if ( wkernel_aTimers[i].id == nIDEvent )
        {
            pFree = &wkernel_aTimers[i];
            break;
        }

        if ( !pFree && !wkernel_aTimers[i].id ) {
            pFree = &wkernel_aTimers[i];
        }
    }

    if ( !pFree || !nIDEvent ) {
        return 0;
    }

    pFree->id       = nIDEvent;
    pFree->interval = uElapse;
    pFree->nextTime = SDL_GetTicks() + uElapse;
    return nIDEvent;
}

BOOL KillTimer(HWND hWnd, UINT_PTR uIDEvent)
{
    J3D_UNUSED(hWnd);
    for ( size_t i = 0; i < WKERNEL_MAXTIMERS; ++i )
    {
        if ( wkernel_aTimers[i].id == uIDEvent )
        {
            wkernel_aTimers[i].id = 0;
            return TRUE;
        }
    }

    return FALSE;
}

BOOL PostMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    J3D_UNUSED(hWnd);

    SDL_Event event = { 0 };
    event.type       = wkernel_postEventType;
    event.user.code  = (Sint32)Msg;
    event.user.data1 = (void*)wParam;
    event.user.data2 = (void*)lParam;
    return SDL_PushEvent(&event);
}

LRESULT DefWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    J3D_UNUSED(hWnd);
    J3D_UNUSED(Msg);
    J3D_UNUSED(wParam);
    J3D_UNUSED(lParam);
    return 0;
}

BOOL ShowWindow(HWND hWnd, int nCmdShow)
{
    if ( !hWnd ) {
        return FALSE;
    }

    return nCmdShow == SW_HIDE ? SDL_HideWindow((SDL_Window*)hWnd) : SDL_ShowWindow((SDL_Window*)hWnd);
}

HWND GetForegroundWindow(void)
{
    // The game window when it has the input focus
    return wkernel_pWindow && (SDL_GetWindowFlags(wkernel_pWindow) & SDL_WINDOW_INPUT_FOCUS) ? wkernel_pWindow : NULL;
}
