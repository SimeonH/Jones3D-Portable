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

#ifdef _WIN32

static int JonesDialog_bWindowResized = 1;
static int JonesDialog_cursorPointIdx = 1;

static JonesDialogGetDrawBufferDCFunc JonesDialog_pfGetDrawBufferDC;
static CHAR JonesDialog_szErrorBuffer[256];
static GDIDIBSectionInfo JonesDialog_GDIDIBSectionInfoOffScreen;
static JonesDialogReleaseDCFunc JonesDialog_pfReleaseDC;
static JonesDialogCursorInfo JonesDialog_cursorInfo;
static GDIDIBSectionInfo JonesDialog_GDIDIBSectionInfo;
static JonesDialogFlipPageFunc JonesDialog_pfFlipPage;
static int JonesDialog_dword_5532E4;
static int JonesDialog_bCursorTrackingInited;

void JonesDialog_InstallHooks(void)
{
    J3D_HOOKFUNC(JonesDialog_InitCursorTracking);
    J3D_HOOKFUNC(JonesDialog_TrackCursor);
    J3D_HOOKFUNC(JonesDialog_ResetCursorTracking);
    J3D_HOOKFUNC(JonesDialog_CopyRectToDrawBuffer);
    J3D_HOOKFUNC(JonesDialog_RestoreBackground);
    J3D_HOOKFUNC(JonesDialog_SetGameState);
    J3D_HOOKFUNC(JonesDialog_ResetGameState);
    J3D_HOOKFUNC(JonesDialog_AllocScreenDBISection);
    J3D_HOOKFUNC(JonesDialog_AllocOffScreenGDISection);
    J3D_HOOKFUNC(JonesDialog_FreeScreenDIBSection);
    J3D_HOOKFUNC(JonesDialog_AllocOffScreenDIBSection);
    J3D_HOOKFUNC(JonesDialog_FreeOffScreenDIBSection);
}


int J3DAPI JonesDialog_InitCursorTracking(int* pAlreadyInited)
{
    uint32_t dwError;
    HDC hDrawBufferDC;
    RECT recz;
    int height;
    int width;
    int canRenderWindowed;

    if ( JonesDialog_bCursorTrackingInited )
    {
        *pAlreadyInited = 1;
        return 0;
    }

    *pAlreadyInited = 0;
    memset(&JonesDialog_cursorInfo, 0, sizeof(JonesDialog_cursorInfo));

    canRenderWindowed = stdDisplay_CanRenderWindowed();
    if ( canRenderWindowed == -1 )
    {
        return 1;
    }

    if ( canRenderWindowed )
    {
        return 0;
    }

    JonesDialog_cursorInfo.hCursor = LoadCursor(NULL, (LPCSTR)IDC_ARROW);
    if ( !GetIconInfo(JonesDialog_cursorInfo.hCursor, &JonesDialog_cursorInfo.info) )// TODO: 2nd param should be made
    {
        STDLOG_ERROR("JonesDialog_InitCursorTracking: Error calling GetIconInfo().\n");
        JonesDialog_ResetCursorTracking();
        return 1;
    }

    hDrawBufferDC = JonesDialog_pfGetDrawBufferDC();
    JonesDialog_cursorInfo.hDrawDC = CreateCompatibleDC(hDrawBufferDC);
    JonesDialog_pfReleaseDC(hDrawBufferDC);

    if ( !JonesDialog_cursorInfo.hDrawDC )
    {
        STDLOG_ERROR("JonesDialog_InitCursorTracking: Error creating memory DC.\n");
        JonesDialog_ResetCursorTracking();
        return 1;
    }

    if ( JonesDialog_cursorInfo.info.hbmColor )
    {
        if ( !GetBitmapDimensionEx(JonesDialog_cursorInfo.info.hbmColor, &JonesDialog_cursorInfo.size) )
        {
            STDLOG_ERROR("JonesDialog_InitCursorTracking: Error calling GetBitmapDimensionEx() for color bitmap.\n");
            JonesDialog_ResetCursorTracking();
            return 1;
        }

        if ( SelectObject(JonesDialog_cursorInfo.hDrawDC, JonesDialog_cursorInfo.info.hbmColor) == (HGDIOBJ)HGDI_ERROR )
        {
            STDLOG_ERROR("JonesDialog_InitCursorTracking: Error calling SelectObject().\n");
            JonesDialog_ResetCursorTracking();
            return 1;
        }
    }
    else
    {
        if ( !JonesDialog_cursorInfo.info.hbmMask )
        {
            STDLOG_ERROR("JonesDialog_InitCursorTracking: What the heck kinda cursor is this?.\n");
            JonesDialog_ResetCursorTracking();
            return 1;
        }

        if ( !GetBitmapDimensionEx(JonesDialog_cursorInfo.info.hbmMask, &JonesDialog_cursorInfo.size) )
        {
            STDLOG_ERROR("JonesDialog_InitCursorTracking: Error calling GetBitmapDimensionEx() for color bitmap.\n");
            JonesDialog_ResetCursorTracking();
            return 1;
        }

        if ( JonesDialog_cursorInfo.size.cy )
        {
            JonesDialog_cursorInfo.size.cy /= 2;
        }

        if ( SelectObject(JonesDialog_cursorInfo.hDrawDC, JonesDialog_cursorInfo.info.hbmMask) == (HGDIOBJ)HGDI_ERROR )
        {
            STDLOG_ERROR("JonesDialog_InitCursorTracking: Error calling SelectObject().\n");
            JonesDialog_ResetCursorTracking();
            return 1;
        }
    }

    GetClipCursor(&JonesDialog_cursorInfo.rect);

    recz.left = 0;
    recz.top = 0;

    stdDisplay_GetBackBufferSize((unsigned int*)&width, (unsigned int*)&height);

    recz.right = width - 1;
    recz.bottom = height - 1;

    if ( !ClipCursor(&recz) )
    {
        dwError = GetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dwError, 0, JonesDialog_szErrorBuffer, STD_ARRAYLEN(JonesDialog_szErrorBuffer) - 1, 0);
        STDLOG_ERROR("JonesDialog_InitCursorTracking: Error calling ClipCursor().  Reason: %s", JonesDialog_szErrorBuffer);
        JonesDialog_ResetCursorTracking();
        return 1;
    }

    JonesDialog_cursorInfo.size.cx = 32;
    JonesDialog_cursorInfo.size.cy = 32;

    JonesDialog_cursorInfo.point[0].x = -1;
    JonesDialog_cursorInfo.point[0].y = -1;

    JonesDialog_cursorInfo.point[1].x = -1;
    JonesDialog_cursorInfo.point[1].y = -1;

    JonesDialog_bCursorTrackingInited = 1;
    return 0;
}

int J3DAPI JonesDialog_TrackCursor(int bReDraw)
{
    HWND hwnd;
    uint32_t dwError;
    uint32_t LastError;
    uint32_t dwError_1;
    int i;
    RECT Rect;
    HDC hDrawBufferDC;
    RECT rect;
    POINT curPos;
    POINT screenPos;
    int v12;
    int CanRenderWindowed;

    if ( !JonesDialog_bCursorTrackingInited )
    {
        return 1;
    }

    if ( !JonesDialog_bWindowResized )
    {
        return 0;
    }

    CanRenderWindowed = stdDisplay_CanRenderWindowed();
    if ( CanRenderWindowed == -1 )
    {
        return 1;
    }

    if ( CanRenderWindowed )
    {
        return 0;
    }

    if ( !JonesDialog_cursorInfo.hDrawDC )
    {
        return 0;
    }

    screenPos.x = 0;
    screenPos.y = 0;
    v12 = 1;                                    // TODO: remove decomp. leftover

    GetCursorPos(&curPos);
    if ( curPos.x == JonesDialog_cursorInfo.point[1].x && curPos.y == JonesDialog_cursorInfo.point[1].y && !bReDraw )
    {
        return 0;
    }

    rect.left = JonesDialog_cursorInfo.point[JonesDialog_cursorPointIdx].x;
    rect.top = JonesDialog_cursorInfo.point[JonesDialog_cursorPointIdx].y;
    rect.right = JonesDialog_cursorInfo.size.cx + JonesDialog_cursorInfo.point[JonesDialog_cursorPointIdx].x;
    rect.bottom = JonesDialog_cursorInfo.size.cy + JonesDialog_cursorInfo.point[JonesDialog_cursorPointIdx].y;

    JonesDialog_CopyRectToDrawBuffer(
        &JonesDialog_GDIDIBSectionInfoOffScreen,
        &JonesDialog_cursorInfo.point[JonesDialog_cursorPointIdx],
        &rect);
    if ( JonesDialog_cursorInfo.numHandels )
    {
        for ( i = 0; i < JonesDialog_cursorInfo.numHandels; ++i )
        {
            GetWindowRect(JonesDialog_cursorInfo.aHwnd[i], &Rect);
            screenPos.x = Rect.left;
            screenPos.y = Rect.top;
            JonesDialog_CopyRectToDrawBuffer(&JonesDialog_GDIDIBSectionInfo, &screenPos, &Rect);
        }
    }

    JonesDialog_cursorInfo.point[0] = JonesDialog_cursorInfo.point[1];
    JonesDialog_cursorInfo.point[1] = curPos;
    JonesDialog_cursorInfo.hCursor = (HCURSOR)screenPos.x;// TODO: wtf???
    JonesDialog_cursorInfo.info.fIcon = screenPos.y;

    rect.left = curPos.x;
    rect.top = curPos.y;
    rect.right = JonesDialog_cursorInfo.size.cx + curPos.x;
    rect.bottom = JonesDialog_cursorInfo.size.cy + curPos.y;
    screenPos.x = 0;
    screenPos.y = 0;

    hwnd = stdWin95_GetWindow();
    ClientToScreen(hwnd, &screenPos);

    hDrawBufferDC = JonesDialog_pfGetDrawBufferDC();
    if ( !BitBlt(
        hDrawBufferDC,
        screenPos.x + rect.left,
        screenPos.y + rect.top,
        JonesDialog_cursorInfo.size.cx,
        JonesDialog_cursorInfo.size.cy,
        JonesDialog_cursorInfo.hDrawDC,
        0,
        0,
        SRCAND) )
    {
        dwError = GetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dwError, 0, JonesDialog_szErrorBuffer, STD_ARRAYLEN(JonesDialog_szErrorBuffer) - 1, 0);
        STDLOG_ERROR("JonesDialog_TrackCursor: BitBlt of cursor, SRCAND failed.  Reason: %s", JonesDialog_szErrorBuffer);
    }

    if ( JonesDialog_cursorInfo.info.hbmColor )
    {
        SelectObject(JonesDialog_cursorInfo.hDrawDC, JonesDialog_cursorInfo.info.hbmColor);
        if ( !BitBlt(
            hDrawBufferDC,
            screenPos.x + rect.left,
            screenPos.y + rect.top,
            JonesDialog_cursorInfo.size.cx,
            JonesDialog_cursorInfo.size.cy,
            JonesDialog_cursorInfo.hDrawDC,
            0,
            0,
            SRCPAINT) )
        {
            LastError = GetLastError();
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, LastError, 0, JonesDialog_szErrorBuffer, STD_ARRAYLEN(JonesDialog_szErrorBuffer) - 1, 0);
            STDLOG_ERROR("JonesDialog_TrackCursor: BitBlt of cursor, SRCPAINT failed.  Reason: %s", JonesDialog_szErrorBuffer);
        }

        SelectObject(JonesDialog_cursorInfo.hDrawDC, JonesDialog_cursorInfo.info.hbmMask);
    }
    else if ( !BitBlt(
        hDrawBufferDC,
        screenPos.x + rect.left,
        screenPos.y + rect.top,
        JonesDialog_cursorInfo.size.cx,
        JonesDialog_cursorInfo.size.cy,
        JonesDialog_cursorInfo.hDrawDC,
        0,
        JonesDialog_cursorInfo.size.cy,
        SRCPAINT) )
    {
        dwError_1 = GetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dwError_1, 0, JonesDialog_szErrorBuffer, STD_ARRAYLEN(JonesDialog_szErrorBuffer) - 1, 0);
        STDLOG_ERROR("JonesDialog_TrackCursor: BitBlt of cursor, SRCPAINT failed.  Reason: %s", JonesDialog_szErrorBuffer);
    }

    JonesDialog_pfReleaseDC(hDrawBufferDC);
    JonesDialog_pfFlipPage();
    return 0;
}

int J3DAPI JonesDialog_ResetCursorTracking()
{
    int bCanRenderWindowed;

    bCanRenderWindowed = stdDisplay_CanRenderWindowed();
    if ( bCanRenderWindowed == -1 )
    {
        return 1;
    }

    if ( !bCanRenderWindowed )
    {
        if ( JonesDialog_cursorInfo.info.hbmColor )
        {
            DeleteObject(JonesDialog_cursorInfo.info.hbmColor);
        }


        if ( JonesDialog_cursorInfo.info.hbmMask )
        {
            DeleteObject(JonesDialog_cursorInfo.info.hbmMask);
        }


        if ( JonesDialog_cursorInfo.hDrawDC )
        {
            DeleteDC(JonesDialog_cursorInfo.hDrawDC);
        }


        ClipCursor(&JonesDialog_cursorInfo.rect);
    }

    memset(&JonesDialog_cursorInfo, 0, sizeof(JonesDialog_cursorInfo));
    JonesDialog_bCursorTrackingInited = 0;
    return 0;
}

int J3DAPI JonesDialog_CopyRectToDrawBuffer(GDIDIBSectionInfo* pInfo, const PPOINT lpSourceOrigin, LPRECT pDstRect)
{
    uint32_t dwError;
    uint32_t dwError_1;
    HDC hDC;
    HDC hDrawBufferDC;

    if ( lpSourceOrigin )
    {
        hDrawBufferDC = JonesDialog_pfGetDrawBufferDC();
        if ( hDrawBufferDC )
        {
            if ( pDstRect->left < pDstRect->right && pDstRect->top < pDstRect->bottom )
            {
                if ( pInfo == &JonesDialog_GDIDIBSectionInfo && pInfo->bHasDC )
                {
                    hDC = GetDC(0);
                    if ( !BitBlt(
                        JonesDialog_GDIDIBSectionInfo.hScreenDC,
                        pDstRect->left,
                        pDstRect->top,
                        pDstRect->right - pDstRect->left,
                        pDstRect->bottom - pDstRect->top,
                        hDC,
                        lpSourceOrigin->x,
                        lpSourceOrigin->y,
                        SRCCOPY) )
                    {
                        dwError = GetLastError();
                        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dwError, 0, JonesDialog_szErrorBuffer, STD_ARRAYLEN(JonesDialog_szErrorBuffer) - 1, 0);
                        STDLOG_ERROR("JonesDialog_CopyRectToDrawBuffer: BitBlt to virtual desktop failed.  Reason: %s", JonesDialog_szErrorBuffer);
                    }

                    ReleaseDC(0, hDC);
                }

                if ( !BitBlt(
                    hDrawBufferDC,
                    pDstRect->left,
                    pDstRect->top,
                    pDstRect->right - pDstRect->left,
                    pDstRect->bottom - pDstRect->top,
                    pInfo->hScreenDC,
                    lpSourceOrigin->x,
                    lpSourceOrigin->y,
                    SRCCOPY) )
                {
                    dwError_1 = GetLastError();
                    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dwError_1, 0, JonesDialog_szErrorBuffer, STD_ARRAYLEN(JonesDialog_szErrorBuffer) - 1, 0);
                    STDLOG_ERROR("JonesDialog_CopyRectToDrawBuffer: BitBlt failed.  Reason: %s", JonesDialog_szErrorBuffer);
                }

                JonesDialog_pfReleaseDC(hDrawBufferDC);
                JonesDialog_pfFlipPage(); // Added
                return 1;
            }
            else
            {
                JonesDialog_pfReleaseDC(hDrawBufferDC);
                return 0;
            }
        }
        else
        {
            STDLOG_ERROR("JonesDialog_CopyRectToDrawBuffer: hDrawBufferDC is NULL.\n");
            return 0;
        }
    }
    else
    {
        STDLOG_ERROR("JonesDialog_CopyRectToDrawBuffer: lpSourceOrigin is NULL.\n");
        return 0;
    }
}

int J3DAPI JonesDialog_RestoreBackground(HDC hdc, HWND hwnd, LPPOINT a3, LPRECT a4)
{
    J3D_UNUSED(hwnd);
    uint32_t dwError;

    if ( !JonesDialog_GDIDIBSectionInfoOffScreen.hScreenDC || !a3 || !a4 )
    {
        return 0;
    }

    if ( BitBlt(
        hdc,
        a4->left,
        a4->top,
        a4->right - a4->left + 1,
        a4->bottom - a4->top + 1,
        JonesDialog_GDIDIBSectionInfoOffScreen.hScreenDC,
        a3->x + a4->left,
        a3->y + a4->top,
        SRCCOPY) )
    {
        JonesDialog_pfFlipPage(); // Added
        return 1;
    }

    dwError = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dwError, 0, JonesDialog_szErrorBuffer, STD_ARRAYLEN(JonesDialog_szErrorBuffer) - 1, 0);
    STDLOG_ERROR("JonesDialog_RestoreBackground: BitBlt failed.  Reason: %s", JonesDialog_szErrorBuffer);
    return 1;
}

void J3DAPI JonesDialog_SetGameState(JonesDialogGameState* pState, int bWindowModeSupported)
{
    uint32_t dwError;
    uint32_t dwError_1;
    HDC hDC;
    RECT pDstRect;
    unsigned int height;
    POINT SourceOrigin;
    unsigned int width;

    SourceOrigin.x = 0;
    SourceOrigin.y = 0;
    pDstRect.left = 0;
    pDstRect.top = 0;

    stdDisplay_GetBackBufferSize(&width, &height);

    pDstRect.right = width;
    pDstRect.bottom = height;

    if ( bWindowModeSupported )
    {
        JonesDialog_pfGetDrawBufferDC = stdDisplay_GetFrontBufferDC;
        JonesDialog_pfReleaseDC = stdDisplay_ReleaseFrontBufferDC;
        JonesDialog_cursorPointIdx = 1;
        if ( stdDisplay_IsFullscreen() )
        {
            JonesDialog_pfFlipPage = stdDisplay_FlipToGDISurface;
            stdDisplay_FlipToGDISurface();
        }
        else
        {
            //JonesDialog_pfFlipPage = JonesDialog_DummyFlipGDI;
            JonesDialog_pfFlipPage = stdDisplay_Update; // Changed: Fixes game background rendering when dialog is open
        }
    }
    else
    {
        JonesDialog_pfGetDrawBufferDC = stdDisplay_GetBackBufferDC;
        JonesDialog_pfReleaseDC = stdDisplay_ReleaseBackBufferDC;
        JonesDialog_cursorPointIdx = 0;
        JonesDialog_pfFlipPage = stdDisplay_Update;
    }

    if ( JonesDialog_AllocOffScreenDIBSection(&pState->bOffScreenDBIAllocSkipped) )
    {
        JonesDialog_FreeOffScreenDIBSection();
    }
    else if ( JonesDialog_AllocScreenDBISection(&pState->bScreenDBIAllocSkipped) )
    {
        JonesDialog_FreeScreenDIBSection();
    }
    else
    {
        pState->unknown7 = JonesDialog_dword_5532E4;

        if ( !JonesDialog_dword_5532E4 && !bWindowModeSupported )
        {
            JonesDialog_CopyRectToDrawBuffer(&JonesDialog_GDIDIBSectionInfoOffScreen, &SourceOrigin, &pDstRect);
            JonesDialog_pfFlipPage();
            JonesDialog_CopyRectToDrawBuffer(&JonesDialog_GDIDIBSectionInfoOffScreen, &SourceOrigin, &pDstRect);
            JonesDialog_dword_5532E4 = 1;
        }

        if ( !pState->bScreenDBIAllocSkipped )
        {
            if ( !BitBlt(
                JonesDialog_GDIDIBSectionInfo.hScreenDC,
                0,
                0,
                width,
                height,
                JonesDialog_GDIDIBSectionInfoOffScreen.hScreenDC,
                0,
                0,
                SRCCOPY) )
            {
                dwError = GetLastError();
                FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dwError, 0, JonesDialog_szErrorBuffer, STD_ARRAYLEN(JonesDialog_szErrorBuffer) - 1, 0);
                STDLOG_ERROR("JonesDialog_SetGameState: BitBlt of background to virtual desktop failed.  Reason: %s", JonesDialog_szErrorBuffer);
            }

            hDC = GetDC(0);
            if ( hDC != JonesDialog_GDIDIBSectionInfo.hScreenDC
                && !BitBlt(hDC, 0, 0, width, height, JonesDialog_GDIDIBSectionInfoOffScreen.hScreenDC, 0, 0, SRCCOPY) )
            {
                dwError_1 = GetLastError();
                FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dwError_1, 0, JonesDialog_szErrorBuffer, STD_ARRAYLEN(JonesDialog_szErrorBuffer) - 1, 0);
                STDLOG_ERROR("JonesDialog_SetGameState: BitBlt of background to actual desktop failed.  Reason: %s", JonesDialog_szErrorBuffer);
            }

            ReleaseDC(0, hDC);
        }

        JonesDialog_pfFlipPage();

        pState->bGamePaused = JonesMain_IsGamePaused();
        if ( !pState->bGamePaused )
        {
            JonesMain_PauseGame();
        }

        pState->bControlsActive = stdControl_ControlsActive();
        if ( pState->bControlsActive && !stdControl_SetActivation(0) )
        {
            stdControl_ShowMouseCursor(1);
        }

        JonesDialog_InitCursorTracking(&pState->unknown4);
        pState->bFrontBufferClipper = stdDisplay_SetBufferClipper(1);
        if ( !bWindowModeSupported )
        {
            pState->bBackBufferClipper = stdDisplay_SetBufferClipper(0);
        }
    }
}

void J3DAPI JonesDialog_ResetGameState(JonesDialogGameState* pState, int bWindowModeSupported)
{
    if ( !pState->bFrontBufferClipper )
    {
        stdDisplay_RemoveBufferClipper(1);
    }

    if ( !bWindowModeSupported && !pState->bBackBufferClipper )
    {
        stdDisplay_RemoveBufferClipper(0);
    }

    if ( !pState->unknown4 )
    {
        JonesDialog_ResetCursorTracking();
    }

    if ( !pState->bOffScreenDBIAllocSkipped )
    {
        JonesDialog_FreeOffScreenDIBSection();
    }

    if ( !pState->bScreenDBIAllocSkipped )
    {
        JonesDialog_FreeScreenDIBSection();
    }

    stdControl_SetActivation(pState->bControlsActive);
    if ( !stdControl_SetActivation(pState->bControlsActive) )
    {
        if ( pState->bControlsActive )
        {
            stdControl_ShowMouseCursor(0);
        }
        else
        {
            stdControl_ShowMouseCursor(1);
        }
    }

    JonesDialog_dword_5532E4 = pState->unknown7;
    if ( !pState->bGamePaused )
    {
        JonesMain_ResumeGame();
    }
}

int J3DAPI JonesDialog_AllocScreenDBISection(int* pbSkippedAllocation)
{
    int bOffScreen;
    HDC hdcFront;
    HDC hdc;
    HDC hDC;
    StdVideoMode pDisplayMode;

    if ( stdDisplay_GetCurrentVideoMode(&pDisplayMode) )
    {
        return 1;
    }

    hdc = GetDC(0);
    bOffScreen = !stdDisplay_CanRenderWindowed()
        && pDisplayMode.rasterInfo.colorInfo.bpp == 16
        && GetDeviceCaps(hdc, BITSPIXEL) == 16;

    if ( bOffScreen )
    {
        hdcFront = stdDisplay_GetFrontBufferDC();
        ReleaseDC(0, hdc);
        hDC = CreateCompatibleDC(hdcFront);
        stdDisplay_ReleaseFrontBufferDC(hdcFront);
        if ( JonesDialog_AllocOffScreenGDISection(&JonesDialog_GDIDIBSectionInfo, hDC, bOffScreen, pbSkippedAllocation) )
        {
            return 1;
        }
    }
    else
    {
        memset(&JonesDialog_GDIDIBSectionInfo, 0, sizeof(JonesDialog_GDIDIBSectionInfo));
        JonesDialog_GDIDIBSectionInfo.hScreenDC = hdc;
    }

    JonesDialog_GDIDIBSectionInfo.bHasDC = bOffScreen;
    if ( bOffScreen )
    {
        return JonesDialog_GDIDIBSectionInfo.hBitmap == 0;
    }
    else
    {
        return JonesDialog_GDIDIBSectionInfo.hScreenDC == 0;
    }
}

int J3DAPI JonesDialog_AllocOffScreenGDISection(GDIDIBSectionInfo* pInfo, HDC hdc, int b24bpp, int* pbSkipedAllocation)
{
    uint32_t dwError;
    StdVideoMode pDisplayMode;

    stdDisplay_GetCurrentVideoMode(&pDisplayMode);
    if ( pInfo->hBitmap )
    {
        if ( pbSkipedAllocation )
        {
            *pbSkipedAllocation = 1;
        }

        return 0;
    }
    else
    {
        *pbSkipedAllocation = 0;
        if ( stdDisplay_GetCurrentVideoMode(&pDisplayMode) )
        {
            return 1;
        }
        else
        {
            memset(&pInfo->bmpInfo, 0, sizeof(pInfo->bmpInfo));

            static_assert(sizeof(pInfo->bmpInfo.bmiHeader) == 40, "sizeof(pInfo->bmpInfo.bmiHeader) == 40");
            pInfo->bmpInfo.bmiHeader.biSize   = sizeof(pInfo->bmpInfo.bmiHeader);
            pInfo->bmpInfo.bmiHeader.biWidth  = pDisplayMode.rasterInfo.width;
            pInfo->bmpInfo.bmiHeader.biHeight = pDisplayMode.rasterInfo.height;
            pInfo->bmpInfo.bmiHeader.biPlanes = 1;
            if ( b24bpp )
            {
                pInfo->bmpInfo.bmiHeader.biBitCount = 24;
            }
            else
            {
                pInfo->bmpInfo.bmiHeader.biBitCount = pDisplayMode.rasterInfo.colorInfo.bpp;
            }

            pInfo->bmpInfo.bmiHeader.biCompression = 0;
            pInfo->bmpInfo.bmiHeader.biSizeImage = ((unsigned int)pDisplayMode.rasterInfo.colorInfo.bpp >> 3)
                * 4
                * ((pDisplayMode.rasterInfo.width + 3) >> 2)
                * pDisplayMode.rasterInfo.height;

            pInfo->hScreenDC = hdc;
            pInfo->hBitmap   = CreateDIBSection(pInfo->hScreenDC, &pInfo->bmpInfo, 0, (void**)&pInfo->pPixels, 0, 0);
            if ( !pInfo->hBitmap )
            {
                dwError = GetLastError();
                FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dwError, 0, JonesDialog_szErrorBuffer, STD_ARRAYLEN(JonesDialog_szErrorBuffer) - 1, 0);
                STDLOG_ERROR("JonesDialog_AllocOffScreenGDISection: CreateDIBSection failed.  Reason: %s", JonesDialog_szErrorBuffer);
            }

            SelectObject(pInfo->hScreenDC, pInfo->hBitmap);
            return pInfo->hBitmap == 0;
        }
    }
}

void J3DAPI JonesDialog_FreeScreenDIBSection()
{
    if ( JonesDialog_GDIDIBSectionInfo.hScreenDC )
    {
        if ( JonesDialog_GDIDIBSectionInfo.bHasDC )
        {
            DeleteDC(JonesDialog_GDIDIBSectionInfo.hScreenDC);
        }
        else
        {
            ReleaseDC(0, JonesDialog_GDIDIBSectionInfo.hScreenDC);
        }
    }

    if ( JonesDialog_GDIDIBSectionInfo.hBitmap )
    {
        DeleteObject(JonesDialog_GDIDIBSectionInfo.hBitmap);
    }

    JonesDialog_GDIDIBSectionInfo.hScreenDC = NULL;
    JonesDialog_GDIDIBSectionInfo.hBitmap   = NULL;
    JonesDialog_GDIDIBSectionInfo.pPixels   = NULL;
}

int J3DAPI JonesDialog_AllocOffScreenDIBSection(int* pBSkipedAllocation)
{
    uint32_t LastError;
    HDC drawDC;
    StdVideoMode pDisplayMode;
    HDC screenDC;

    if ( stdDisplay_GetCurrentVideoMode(&pDisplayMode) )
    {
        return 1;
    }

    drawDC = stdDisplay_GetFrontBufferDC();
    screenDC = CreateCompatibleDC(drawDC);
    if ( JonesDialog_AllocOffScreenGDISection(&JonesDialog_GDIDIBSectionInfoOffScreen, screenDC, 0, pBSkipedAllocation) )
    {
        return 1;
    }

    JonesDialog_GDIDIBSectionInfoOffScreen.bHasDC = 1;
    if ( !*pBSkipedAllocation
        && !BitBlt(JonesDialog_GDIDIBSectionInfoOffScreen.hScreenDC, 0, 0, pDisplayMode.rasterInfo.width, pDisplayMode.rasterInfo.height, drawDC, 0, 0, SRCCOPY) )
    {
        LastError = GetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, LastError, 0, JonesDialog_szErrorBuffer, STD_ARRAYLEN(JonesDialog_szErrorBuffer) - 1, 0);
        STDLOG_ERROR("JonesDialog_AllocOffScreenDIBSection: BitBlt failed.  Reason: %s", JonesDialog_szErrorBuffer);
    }

    stdDisplay_ReleaseFrontBufferDC(drawDC);
    JonesDialog_pfFlipPage(); // Added: Fixes rendering of game background when dialog is open.
    return JonesDialog_GDIDIBSectionInfoOffScreen.hBitmap == NULL;
}

int J3DAPI JonesDialog_FreeOffScreenDIBSection()
{
    if ( JonesDialog_GDIDIBSectionInfoOffScreen.hScreenDC )
    {
        DeleteDC(JonesDialog_GDIDIBSectionInfoOffScreen.hScreenDC);
    }

    if ( JonesDialog_GDIDIBSectionInfoOffScreen.hBitmap )
    {
        DeleteObject(JonesDialog_GDIDIBSectionInfoOffScreen.hBitmap);
    }

    JonesDialog_GDIDIBSectionInfoOffScreen.hScreenDC = NULL;
    JonesDialog_GDIDIBSectionInfoOffScreen.hBitmap   = NULL;
    JonesDialog_GDIDIBSectionInfoOffScreen.pPixels   = NULL;
    return 0;
}

#else // !_WIN32

// Non-Windows builds: the dialogs (wx) are separate top-level windows, so there's no game frame or cursor
// to draw behind them with GDI. Only the game state is paused and restored.

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

#endif // _WIN32
