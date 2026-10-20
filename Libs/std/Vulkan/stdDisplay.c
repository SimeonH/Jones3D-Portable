// Vulkan implementation of stdDisplay.
// The front and back buffer are both views of the Vulkan back buffer (see stdVk.h):
// their pixels are the CPU mirror of the color image and their DCs the DC of the mirror's DIB section.
#include <std/Win95/stdDisplay.h>
#include <std/Win95/stdWin95.h>
#include "stdVk.h"

#include <j3dcore/j3dhook.h>
#include <std/General/std.h>
#include <std/General/stdBmp.h>
#include <std/General/stdColor.h>
#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>
#include <std/RTI/symbols.h>

#include <stdlib.h>

#ifndef _WIN32
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#endif

static bool stdDisplay_bStartup    = false;
static bool stdDisplay_bOpen       = false;
static bool stdDisplay_bModeSet    = false;
static bool stdDisplay_bFullscreen = false;

static StdVideoMode* stdDisplay_pCurVideoMode   = NULL;
static StdVideoMode stdDisplay_primaryVideoMode = { 0 };

static size_t stdDisplay_numVideoModes         = 0;
static StdVideoMode stdDisplay_aVideoModes[64] = { 0 };

static size_t stdDisplay_curDevice;
static size_t stdDisplay_numDevices = 0;
static StdDisplayDevice stdDisplay_aDisplayDevices[1] = { 0 };

#ifdef _WIN32
static HFONT stdDisplay_hFont;
#endif

// Back buffer pixel format, XRGB8888 (BGRX in memory) as DIB sections and VK_FORMAT_B8G8R8A8_UNORM
static const ColorInfo stdDisplay_cfBackBuffer = {
    .colorMode          = STDCOLOR_RGB,
    .bpp                = 32,
    .redBPP             = 8,
    .greenBPP           = 8,
    .blueBPP            = 8,
    .redPosShift        = 16,
    .greenPosShift      = 8,
    .bluePosShift       = 0,
    .redPosShiftRight   = 0,
    .greenPosShiftRight = 0,
    .bluePosShiftRight  = 0,
    .alphaBPP           = 0,
    .alphaPosShift      = 0,
    .alphaPosShiftRight = 0
};

void stdDisplay_InstallHooks(void)
{
    J3D_HOOKFUNC(stdDisplay_Startup);
    J3D_HOOKFUNC(stdDisplay_Shutdown);
    J3D_HOOKFUNC(stdDisplay_Open);
    J3D_HOOKFUNC(stdDisplay_Close);
    J3D_HOOKFUNC(stdDisplay_SetMode);
    J3D_HOOKFUNC(stdDisplay_ClearMode);
    J3D_HOOKFUNC(stdDisplay_GetNumDevices);
    J3D_HOOKFUNC(stdDisplay_GetDevice);
    J3D_HOOKFUNC(stdDisplay_GetCurrentDevice);
    J3D_HOOKFUNC(stdDisplay_Refresh);
    J3D_HOOKFUNC(stdDisplay_VBufferNew);
    J3D_HOOKFUNC(stdDisplay_VBufferFree);
    J3D_HOOKFUNC(stdDisplay_VBufferLock);
    J3D_HOOKFUNC(stdDisplay_VBufferUnlock);
    J3D_HOOKFUNC(stdDisplay_VBufferFill);
    J3D_HOOKFUNC(stdDisplay_VBufferConvertColorFormat);
    J3D_HOOKFUNC(stdDisplay_GetTextureMemory);
    J3D_HOOKFUNC(stdDisplay_GetTotalMemory);
    J3D_HOOKFUNC(stdDisplay_Update);
    J3D_HOOKFUNC(stdDisplay_BackBufferFill);
    J3D_HOOKFUNC(stdDisplay_SaveScreen);
    J3D_HOOKFUNC(stdDisplay_SetDefaultResolution);
    J3D_HOOKFUNC(stdDisplay_GetBackBufferSize);
    J3D_HOOKFUNC(stdDisplay_GetNumVideoModes);
    J3D_HOOKFUNC(stdDisplay_GetVideoMode);
    J3D_HOOKFUNC(stdDisplay_GetCurrentVideoMode);
    J3D_HOOKFUNC(stdDisplay_GetFrontBufferDC);
    J3D_HOOKFUNC(stdDisplay_ReleaseFrontBufferDC);
    J3D_HOOKFUNC(stdDisplay_GetBackBufferDC);
    J3D_HOOKFUNC(stdDisplay_ReleaseBackBufferDC);
    J3D_HOOKFUNC(stdDisplay_FlipToGDISurface);
    J3D_HOOKFUNC(stdDisplay_CanRenderWindowed);
    J3D_HOOKFUNC(stdDisplay_SetBufferClipper);
    J3D_HOOKFUNC(stdDisplay_RemoveBufferClipper);
    J3D_HOOKFUNC(stdDisplay_IsFullscreen);
    J3D_HOOKFUNC(stdDisplay_LockBackBuffer);
    J3D_HOOKFUNC(stdDisplay_UnlockBackBuffer);
    J3D_HOOKFUNC(stdDisplay_EncodeFromRGB565);
}

tVBuffer stdDisplay_g_frontBuffer;
tVBuffer stdDisplay_g_backBuffer;

static void J3DAPI stdDisplay_SetPixels16(uint16_t* pPixels16, uint16_t pixel, size_t size)
{
    for ( size_t i = 0; i < size; ++i ) {
        pPixels16[i] = pixel;
    }
}

static void J3DAPI stdDisplay_SetPixels32(uint32_t* pPixels32, uint32_t pixel, size_t size)
{
    for ( size_t i = 0; i < size; ++i ) {
        pPixels32[i] = pixel;
    }
}

static void J3DAPI stdDisplay_InitRasterInfo(tRasterInfo* pInfo, uint32_t width, uint32_t height)
{
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->width     = width;
    pInfo->height    = height;
    pInfo->colorInfo = stdDisplay_cfBackBuffer;
    pInfo->rowSize   = width * 4;
    pInfo->rowWidth  = width;
    pInfo->size      = width * height * 4;
}

static void J3DAPI stdDisplay_SetAspectRatio(StdVideoMode* pMode)
{
    if ( (pMode->rasterInfo.width == 320 && pMode->rasterInfo.height == 200)
        || (pMode->rasterInfo.width == 320 && pMode->rasterInfo.height == 400)
        || (pMode->rasterInfo.width == 640 && pMode->rasterInfo.height == 400) )
    {
        pMode->aspectRatio = 0.75f;
    }
    else
    {
        pMode->aspectRatio = 1.0f;
    }
}

static int J3DAPI stdDisplay_VideoModeCompare(const StdVideoMode* pMode1, const StdVideoMode* pMode2)
{
    if ( pMode1->rasterInfo.width != pMode2->rasterInfo.width ) {
        return (int)pMode1->rasterInfo.width - (int)pMode2->rasterInfo.width;
    }

    return (int)pMode1->rasterInfo.height - (int)pMode2->rasterInfo.height;
}

static void stdDisplay_AddVideoMode(uint32_t width, uint32_t height)
{
    if ( width < 640 || height < 480 || stdDisplay_numVideoModes >= STD_ARRAYLEN(stdDisplay_aVideoModes) ) {
        return;
    }

    for ( size_t i = 0; i < stdDisplay_numVideoModes; ++i )
    {
        if ( stdDisplay_aVideoModes[i].rasterInfo.width == width && stdDisplay_aVideoModes[i].rasterInfo.height == height ) {
            return;
        }
    }

    StdVideoMode* pMode = &stdDisplay_aVideoModes[stdDisplay_numVideoModes++];
    stdDisplay_InitRasterInfo(&pMode->rasterInfo, width, height);
    stdDisplay_SetAspectRatio(pMode);
}

// Fullscreen modes are the resolutions of the primary monitor.
// The game is rendered at the mode resolution and scaled to a borderless window covering the monitor.
static void stdDisplay_EnumVideoModes(void)
{
    stdDisplay_numVideoModes = 0;

#ifdef _WIN32
    DEVMODEA dm = { 0 };
    dm.dmSize = sizeof(dm);
    for ( DWORD i = 0; EnumDisplaySettingsA(NULL, i, &dm); ++i )
    {
        if ( dm.dmBitsPerPel >= 24 ) {
            stdDisplay_AddVideoMode(dm.dmPelsWidth, dm.dmPelsHeight);
        }
    }
#else
    int numModes = 0;
    SDL_DisplayMode** apModes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), &numModes);
    if ( apModes )
    {
        for ( int i = 0; i < numModes; ++i ) {
            stdDisplay_AddVideoMode((uint32_t)apModes[i]->w, (uint32_t)apModes[i]->h);
        }
        SDL_free(apModes);
    }

    // Wayland doesn't expose modes, offer the desktop resolution and common ones below it
    const SDL_DisplayMode* pDesktop = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if ( pDesktop )
    {
        stdDisplay_AddVideoMode((uint32_t)pDesktop->w, (uint32_t)pDesktop->h);

        static const uint32_t aCommon[][2] = { { 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1280, 720 }, { 1280, 960 }, { 1600, 900 }, { 1600, 1200 }, { 1920, 1080 }, { 2560, 1440 } };
        for ( size_t i = 0; i < STD_ARRAYLEN(aCommon); ++i )
        {
            if ( aCommon[i][0] <= (uint32_t)pDesktop->w && aCommon[i][1] <= (uint32_t)pDesktop->h ) {
                stdDisplay_AddVideoMode(aCommon[i][0], aCommon[i][1]);
            }
        }
    }
#endif

    qsort(
        stdDisplay_aVideoModes,
        stdDisplay_numVideoModes,
        sizeof(StdVideoMode),
        (int(__cdecl*)(const void*, const void*))stdDisplay_VideoModeCompare
    );
}

static size_t stdDisplay_GetDeviceLocalMemory(void)
{
    VkDeviceSize total = 0;
    for ( uint32_t i = 0; i < stdVk_g.memProps.memoryHeapCount; ++i )
    {
        if ( stdVk_g.memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
            total += stdVk_g.memProps.memoryHeaps[i].size;
        }
    }

    // Keep it in a signed 32-bit range, the game does arithmetic on these values
    return total > 0x7FF00000u ? 0x7FF00000u : (size_t)total;
}

int stdDisplay_Startup(void)
{
    if ( stdDisplay_bStartup ) {
        return 1;
    }

    memset(&stdDisplay_g_frontBuffer, 0, sizeof(stdDisplay_g_frontBuffer));
    memset(&stdDisplay_g_backBuffer, 0, sizeof(stdDisplay_g_backBuffer));
    memset(stdDisplay_aDisplayDevices, 0, sizeof(stdDisplay_aDisplayDevices));

    stdDisplay_numDevices = 0;
    if ( !stdVk_Startup() )
    {
        STDLOG_ERROR("Error initializing Vulkan.\n");
        return 0;
    }

    StdDisplayDevice* pDevice = &stdDisplay_aDisplayDevices[0];
    STD_STRCPY(pDevice->aDeviceName, stdVk_g.deviceProps.deviceName);
    STD_STRCPY(pDevice->aDriverName, "Vulkan");
    pDevice->bHAL                      = 1;
    pDevice->bGuidNotSet               = 1;
    pDevice->bWindowRenderNotSupported = 0;
    pDevice->totalVideoMemory          = stdDisplay_GetDeviceLocalMemory();
    pDevice->freeVideoMemory           = pDevice->totalVideoMemory;
    stdDisplay_numDevices = 1;

    // TODO: Change default res
    stdDisplay_primaryVideoMode.rasterInfo.width  = 640;
    stdDisplay_primaryVideoMode.rasterInfo.height = 480;

    stdDisplay_bStartup = true;
    STDLOG_STATUS("Found %d Display Devices.\n", stdDisplay_numDevices);
    return 1;
}

void stdDisplay_Shutdown(void)
{
    if ( stdDisplay_bOpen ) {
        stdDisplay_Close();
    }

    stdVk_Shutdown();

    memset(stdDisplay_aDisplayDevices, 0, sizeof(stdDisplay_aDisplayDevices));
    memset(&stdDisplay_g_frontBuffer, 0, sizeof(stdDisplay_g_frontBuffer));
    memset(&stdDisplay_g_backBuffer, 0, sizeof(stdDisplay_g_backBuffer));

    stdDisplay_pCurVideoMode = NULL;
    stdDisplay_numDevices    = 0;
    stdDisplay_numVideoModes = 0;
    stdDisplay_bStartup      = false;
}

int J3DAPI stdDisplay_Open(size_t deviceNum)
{
    STD_ASSERTREL(stdDisplay_bStartup == true);
    if ( stdDisplay_bOpen )
    {
        STDLOG_ERROR("Warning: System already open!\n");
        stdDisplay_Close();
    }

    if ( deviceNum >= stdDisplay_numDevices ) {
        return 0;
    }

    stdDisplay_curDevice = deviceNum;
    stdDisplay_EnumVideoModes();

    stdDisplay_bOpen = true;
    return 1;
}

bool stdDisplay_IsOpen(void)
{
    return stdDisplay_bOpen;
}

void stdDisplay_Close(void)
{
    if ( !stdDisplay_bStartup )
    {
        STDLOG_ERROR("stdDisplay_Close when not started.\n");
        return;
    }

    if ( !stdDisplay_bOpen )
    {
        STDLOG_ERROR("Warning: System already closed!\n");
        return;
    }

    if ( stdDisplay_bModeSet ) {
        stdDisplay_ClearMode();
    }

    memset(&stdDisplay_g_frontBuffer, 0, sizeof(stdDisplay_g_frontBuffer));
    memset(&stdDisplay_g_backBuffer, 0, sizeof(stdDisplay_g_backBuffer));

    stdDisplay_curDevice = 0;
    stdDisplay_bOpen     = false;
}

static void J3DAPI stdDisplay_InitDisplayBuffer(tVBuffer* pBuffer, const tRasterInfo* pRasterInfo)
{
    memset(pBuffer, 0, sizeof(*pBuffer));
    pBuffer->lockRefCount = 1; // display surface
    pBuffer->bVideoMemory = 1;
    pBuffer->rasterInfo   = *pRasterInfo;
    pBuffer->pPixels      = stdVk_g.pShadowPixels;
}

int J3DAPI stdDisplay_SetMode(size_t modeNum, int bFullscreen, size_t numBackBuffers)
{
    J3D_UNUSED(numBackBuffers);

    if ( bFullscreen && modeNum >= stdDisplay_numVideoModes ) {
        return 1;
    }

    if ( stdDisplay_bModeSet ) {
        stdDisplay_ClearMode();
    }

    stdDisplay_pCurVideoMode = bFullscreen ? &stdDisplay_aVideoModes[modeNum] : &stdDisplay_primaryVideoMode;

    uint32_t width  = stdDisplay_pCurVideoMode->rasterInfo.width;
    uint32_t height = stdDisplay_pCurVideoMode->rasterInfo.height;
    stdDisplay_InitRasterInfo(&stdDisplay_pCurVideoMode->rasterInfo, width, height);
    stdDisplay_SetAspectRatio(stdDisplay_pCurVideoMode);

    STDLOG_STATUS("Set video mode %d %d %d (%s).\n", width, height, 32, bFullscreen ? "fullscreen" : "windowed");
    if ( !stdVk_CreateBackBuffer(stdWin95_GetWindow(), width, height) )
    {
        STDLOG_ERROR("Error creating Vulkan back buffer.\n");
        stdVk_DestroyBackBuffer();
        return 1;
    }

    stdDisplay_InitDisplayBuffer(&stdDisplay_g_backBuffer, &stdDisplay_pCurVideoMode->rasterInfo);
    stdDisplay_InitDisplayBuffer(&stdDisplay_g_frontBuffer, &stdDisplay_pCurVideoMode->rasterInfo);

#ifdef _WIN32
    // The game window is created topmost. Same as DirectDraw stdDisplay_SetWindowMode, a window
    // in window mode is a normal window, so it doesn't cover other windows and the game dialogs.
    SetWindowPos(stdWin95_GetWindow(), bFullscreen ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    int fheight = width < 640 ? 12 : 24;
    stdDisplay_hFont = CreateFontA(fheight, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, "Arial");
#else
    SDL_SetWindowFullscreen((SDL_Window*)stdWin95_GetWindow(), bFullscreen != 0); // borderless desktop fullscreen
#endif

    stdDisplay_bModeSet    = true;
    stdDisplay_bFullscreen = bFullscreen;

    stdDisplay_VBufferFill(&stdDisplay_g_backBuffer, 0, NULL); // 0 - BLACK
    stdDisplay_Update();
    return 0;
}

void stdDisplay_ClearMode(void)
{
    if ( stdDisplay_bModeSet )
    {
        stdVk_DestroyBackBuffer();
        memset(&stdDisplay_g_backBuffer, 0, sizeof(stdDisplay_g_backBuffer));
        memset(&stdDisplay_g_frontBuffer, 0, sizeof(stdDisplay_g_frontBuffer));
    }

#ifdef _WIN32
    if ( stdDisplay_hFont )
    {
        DeleteObject(stdDisplay_hFont);
        stdDisplay_hFont = NULL;
    }
#endif

    stdDisplay_bModeSet = false;
}

size_t stdDisplay_GetNumDevices(void)
{
    return stdDisplay_numDevices;
}

int J3DAPI stdDisplay_GetDevice(size_t deviceNum, StdDisplayDevice* pDest)
{
    if ( deviceNum >= stdDisplay_numDevices ) {
        return 1;
    }

    memcpy(pDest, &stdDisplay_aDisplayDevices[deviceNum], sizeof(StdDisplayDevice));
    return 0;
}

int J3DAPI stdDisplay_GetCurrentDevice(StdDisplayDevice* pDevice)
{
    if ( stdDisplay_numDevices == 0 ) {
        return 1;
    }

    memcpy(pDevice, &stdDisplay_aDisplayDevices[stdDisplay_curDevice], sizeof(StdDisplayDevice));
    return 0;
}

void J3DAPI stdDisplay_Refresh(int bReload)
{
    // Vulkan surfaces aren't lost, the swapchain is recreated on present when needed
    J3D_UNUSED(bReload);
}

tVBuffer* J3DAPI stdDisplay_VBufferNew(const tRasterInfo* pRasterInfo, int bUseVSurface, int bUseVideoMemory)
{
    // There are no offscreen display surfaces, all vbuffers are in system memory
    J3D_UNUSED(bUseVSurface);
    J3D_UNUSED(bUseVideoMemory);

    STD_ASSERTREL((pRasterInfo->colorInfo.bpp % 8) == 0);
    tVBuffer* vbuffer = (tVBuffer*)STDMALLOC(sizeof(tVBuffer));
    if ( !vbuffer )
    {
        STDLOG_ERROR("Error allocating vbuffer.\n");
        return NULL;
    }

    memset(vbuffer, 0, sizeof(*vbuffer));
    memcpy(&vbuffer->rasterInfo, pRasterInfo, sizeof(vbuffer->rasterInfo));

    unsigned int bbp             = (unsigned int)vbuffer->rasterInfo.colorInfo.bpp / 8;
    vbuffer->rasterInfo.rowSize  = vbuffer->rasterInfo.width * bbp;
    vbuffer->rasterInfo.rowWidth = vbuffer->rasterInfo.width;
    vbuffer->rasterInfo.size     = vbuffer->rasterInfo.height * vbuffer->rasterInfo.width * bbp;

    vbuffer->pPixels = (uint8_t*)STDMALLOC(vbuffer->rasterInfo.size);
    if ( !vbuffer->pPixels )
    {
        stdMemory_Free(vbuffer);
        return NULL;
    }

    vbuffer->lockSurfRefCount = 1;
    return vbuffer;
}

void J3DAPI stdDisplay_VBufferFree(tVBuffer* pVBuffer)
{
    STD_ASSERTREL(pVBuffer != NULL);
    if ( !pVBuffer->lockRefCount && pVBuffer->pPixels )
    {
        stdMemory_Free(pVBuffer->pPixels);
        pVBuffer->pPixels = NULL;
    }

    stdMemory_Free(pVBuffer);
}

int J3DAPI stdDisplay_VBufferLock(tVBuffer* pVBuffer)
{
    STD_ASSERTREL(pVBuffer != NULL);

    if ( pVBuffer->lockRefCount == 1 )
    {
        // Display buffer
        if ( !stdDisplay_bModeSet || !stdVk_g.pShadowPixels ) {
            return 0;
        }

        stdVk_SyncShadowForRead();
        pVBuffer->pPixels = stdVk_g.pShadowPixels;
    }

    ++pVBuffer->lockSurfRefCount;
    return 1;
}

int J3DAPI stdDisplay_VBufferUnlock(tVBuffer* pVBuffer)
{
    STD_ASSERTREL(pVBuffer != NULL);
    if ( pVBuffer->lockRefCount == 0 )
    {
        if ( pVBuffer->lockSurfRefCount ) {
            --pVBuffer->lockSurfRefCount;
        }

        return 1;
    }

    if ( pVBuffer->lockRefCount != 1 ) {
        return 1;
    }

    if ( pVBuffer->lockSurfRefCount == 0 ) {
        return 0;
    }

    // The CPU may have written to the display buffer
    stdVk_MarkShadowDirty();
    --pVBuffer->lockSurfRefCount;
    return 0;
}

int J3DAPI stdDisplay_VBufferFill(tVBuffer* pVBuffer, uint32_t color, const StdRect* pRect)
{
    STD_ASSERTREL(pVBuffer != NULL);

    if ( pVBuffer->lockRefCount )
    {
        if ( pVBuffer->lockRefCount != 1 )
        {
            // Skip filling because more than 1 refs is using this buffer
            return 1;
        }

        if ( pRect && (pRect->right == 0 || pRect->bottom == 0) ) {
            return 0;
        }

        stdVk_ClearColor(color, pRect);
        return 1;
    }

    switch ( pVBuffer->rasterInfo.colorInfo.bpp )
    {
        case 8:
            if ( pRect )
            {
                uint8_t* pPixels8 = &pVBuffer->pPixels[pVBuffer->rasterInfo.rowWidth * pRect->top + pRect->left];
                for ( int32_t height = 0; height < pRect->bottom; ++height )
                {
                    memset(pPixels8, (uint8_t)color, pRect->right);
                    pPixels8 += pVBuffer->rasterInfo.rowSize;
                }
            }
            else {
                memset(pVBuffer->pPixels, (uint8_t)color, pVBuffer->rasterInfo.size);
            }
            break;

        case 16:
            if ( pRect )
            {
                uint16_t* pPixels16 = (uint16_t*)&pVBuffer->pPixels[2 * pVBuffer->rasterInfo.rowWidth * pRect->top + 2 * pRect->left];
                for ( int32_t height = 0; height < pRect->bottom; ++height )
                {
                    stdDisplay_SetPixels16(pPixels16, (uint16_t)color, pRect->right);
                    pPixels16 = (uint16_t*)((char*)pPixels16 + pVBuffer->rasterInfo.rowSize);
                }
            }
            else {
                stdDisplay_SetPixels16((uint16_t*)pVBuffer->pPixels, (uint16_t)color, (size_t)(pVBuffer->rasterInfo.size / 2));
            }
            break;

        case 24:
            STDLOG_FATAL("0"); // TODO: implement
            break;

        case 32:
            if ( pRect )
            {
                uint32_t* pPixels32 = (uint32_t*)&pVBuffer->pPixels[4 * pVBuffer->rasterInfo.rowWidth * pRect->top + 4 * pRect->left];
                for ( int32_t height = 0; height < pRect->bottom; ++height )
                {
                    stdDisplay_SetPixels32(pPixels32, color, pRect->right);
                    pPixels32 = (uint32_t*)((char*)pPixels32 + pVBuffer->rasterInfo.rowSize);
                }
            }
            else {
                stdDisplay_SetPixels32((uint32_t*)pVBuffer->pPixels, color, (size_t)(pVBuffer->rasterInfo.size / 4));
            }
            break;
    }

    return 1;
}

tVBuffer* J3DAPI stdDisplay_VBufferConvertColorFormat(const ColorInfo* pDesiredColorFormat, tVBuffer* pSrc, int bColorKey, LPDDCOLORKEY pColorKey)
{
    STD_ASSERTREL(pSrc != NULL);
    if ( memcmp(pDesiredColorFormat, &pSrc->rasterInfo.colorInfo, sizeof(ColorInfo)) == 0 ) {
        return pSrc;
    }

    if ( pSrc->rasterInfo.colorInfo.colorMode == STDCOLOR_PAL )
    {
        if ( pDesiredColorFormat->colorMode == STDCOLOR_PAL ) {
            return pSrc;
        }

        STD_ASSERTREL(pSrc->rasterInfo.colorInfo.colorMode != STDCOLOR_PAL);
    }

    const ColorInfo* pSrcColorFormat = &pSrc->rasterInfo.colorInfo;
    STD_ASSERTREL(pDesiredColorFormat->colorMode != STDCOLOR_PAL);
    STD_ASSERTREL(pSrcColorFormat->redBPP != 0);
    STD_ASSERTREL(pSrcColorFormat->greenBPP != 0);
    STD_ASSERTREL(pSrcColorFormat->blueBPP != 0);
    STD_ASSERTREL(pDesiredColorFormat->redBPP != 0);
    STD_ASSERTREL(pDesiredColorFormat->greenBPP != 0);
    STD_ASSERTREL(pDesiredColorFormat->blueBPP != 0);

    tVBuffer* pDest = pSrc;
    if ( pSrc->rasterInfo.colorInfo.bpp != pDesiredColorFormat->bpp )
    {
        tRasterInfo rasterInfo;
        memcpy(&rasterInfo, &pSrc->rasterInfo, sizeof(rasterInfo));
        memcpy(&rasterInfo.colorInfo, pDesiredColorFormat, sizeof(rasterInfo.colorInfo));

        pDest = stdDisplay_VBufferNew(&rasterInfo, 0, 0);
        if ( !pDest )
        {
            STDLOG_ERROR("Unable to allocate memory for new tVBuffer");
            return NULL;
        }
    }

    STD_ASSERTREL(pSrc->pPixels != NULL);
    STD_ASSERTREL(pDest->pPixels != NULL);

    stdDisplay_VBufferLock(pSrc);
    stdDisplay_VBufferLock(pDest);

    for ( size_t row = 0; row < pDest->rasterInfo.height; ++row )
    {
        const uint8_t* pSrcRow = &pSrc->pPixels[pSrc->rasterInfo.rowSize * row];
        uint8_t* pDestRow      = &pDest->pPixels[pDest->rasterInfo.rowSize * row];
        stdColor_ColorConvertOneRow(pDestRow, pDesiredColorFormat, pSrcRow, &pSrc->rasterInfo.colorInfo, pDest->rasterInfo.width, bColorKey, pColorKey);
    }

    stdDisplay_VBufferUnlock(pSrc);
    stdDisplay_VBufferUnlock(pDest);

    memcpy(&pDest->rasterInfo.colorInfo, pDesiredColorFormat, sizeof(pDest->rasterInfo.colorInfo));

    if ( pDest != pSrc ) {
        stdDisplay_VBufferFree(pSrc);
    }

    return pDest;
}

int J3DAPI stdDisplay_GetTextureMemory(size_t* pTotal, size_t* pFree)
{
    if ( !stdVk_g.device ) {
        return 1;
    }

    *pTotal = stdDisplay_GetDeviceLocalMemory();
    *pFree  = *pTotal;
    return 0;
}

int J3DAPI stdDisplay_GetTotalMemory(size_t* pTotal, size_t* pFree)
{
    return stdDisplay_GetTextureMemory(pTotal, pFree);
}

float stdDisplay_GetPresentScale(void)
{
    return stdDisplay_bModeSet ? stdVk_g.presentScale : 0.0f;
}

void stdDisplay_DisableVSync(bool bDisable)
{
    stdVk_SetNoVSync(bDisable);
}

// Keeps the borderless fullscreen window over its whole monitor, the game sizes it to the mode resolution
static void stdDisplay_UpdateFullscreenWindow(void)
{
#ifndef _WIN32
    // SDL fullscreen windows already cover the display
#else
    HWND hwnd = stdWin95_GetWindow();
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(mi);
    if ( !GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi) ) {
        return;
    }

    RECT rect;
    GetWindowRect(hwnd, &rect);
    if ( memcmp(&rect, &mi.rcMonitor, sizeof(rect)) != 0 )
    {
        SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
#endif
}

int stdDisplay_Update(void)
{
    if ( !stdDisplay_bModeSet ) {
        return 1;
    }

    if ( stdDisplay_bFullscreen ) {
        stdDisplay_UpdateFullscreenWindow();
    }

#ifndef _WIN32
    // Keep the window responsive while the game doesn't process its events, e.g. on the loading screen.
    // The events stay queued for the game's event loop.
    SDL_PumpEvents();
#endif

    return stdVk_Present();
}

int J3DAPI stdDisplay_BackBufferFill(uint32_t color, const StdRect* pRect)
{
    if ( !stdDisplay_bModeSet ) {
        return 1;
    }

    if ( pRect && (pRect->right == 0 || pRect->bottom == 0) ) {
        return 1;
    }

    stdVk_ClearColor(color, pRect);
    return 0;
}

int J3DAPI stdDisplay_SaveScreen(const char* pFilename)
{
    stdDisplay_VBufferLock(&stdDisplay_g_backBuffer);
    int res = stdBmp_WriteVBuffer(pFilename, &stdDisplay_g_backBuffer);
    stdDisplay_VBufferUnlock(&stdDisplay_g_backBuffer);
    return res;
}

void J3DAPI stdDisplay_SetDefaultResolution(uint32_t width, uint32_t height)
{
    stdDisplay_primaryVideoMode.rasterInfo.width  = width;
    stdDisplay_primaryVideoMode.rasterInfo.height = height;
}

void J3DAPI stdDisplay_GetBackBufferSize(uint32_t* pWidth, uint32_t* pHeight)
{
    *pWidth  = stdDisplay_g_backBuffer.rasterInfo.width;
    *pHeight = stdDisplay_g_backBuffer.rasterInfo.height;
}

size_t stdDisplay_GetNumVideoModes(void)
{
    return stdDisplay_numVideoModes;
}

int J3DAPI stdDisplay_GetVideoMode(size_t modeNum, StdVideoMode* pDestMode)
{
    if ( modeNum >= stdDisplay_numVideoModes ) {
        return 1;
    }

    memcpy(pDestMode, &stdDisplay_aVideoModes[modeNum], sizeof(StdVideoMode));
    return 0;
}

int J3DAPI stdDisplay_GetCurrentVideoMode(StdVideoMode* pDisplayMode)
{
    if ( !stdDisplay_pCurVideoMode ) {
        return 1;
    }

    memcpy(pDisplayMode, stdDisplay_pCurVideoMode, sizeof(StdVideoMode));
    return 0;
}

HDC stdDisplay_GetFrontBufferDC(void)
{
    // The front buffer is what was last presented, which is the content of the back buffer
    return stdDisplay_GetBackBufferDC();
}

void J3DAPI stdDisplay_ReleaseFrontBufferDC(HDC hdc)
{
    stdDisplay_ReleaseBackBufferDC(hdc);
}

HDC stdDisplay_GetBackBufferDC(void)
{
#ifdef _WIN32
    if ( !stdDisplay_bOpen || !stdDisplay_bModeSet || !stdVk_g.hShadowDC ) {
        return NULL;
    }

    stdVk_SyncShadowForRead();
    return stdVk_g.hShadowDC;
#else
    return NULL; // No GDI
#endif
}

void J3DAPI stdDisplay_ReleaseBackBufferDC(HDC hdc)
{
#ifdef _WIN32
    if ( stdDisplay_bOpen && stdDisplay_bModeSet && hdc == stdVk_g.hShadowDC ) {
        stdVk_MarkShadowDirty();
    }
#else
    J3D_UNUSED(hdc);
#endif
}

int stdDisplay_FlipToGDISurface(void)
{
    return 0;
}

int J3DAPI stdDisplay_CanRenderWindowed()
{
    return DDCAPS2_CANRENDERWINDOWED;
}

int J3DAPI stdDisplay_SetBufferClipper(int bFrontBuffer)
{
    J3D_UNUSED(bFrontBuffer);
    return 0;
}

HRESULT J3DAPI stdDisplay_RemoveBufferClipper(int bFrontBuffer)
{
    J3D_UNUSED(bFrontBuffer);
    return 0;
}

int J3DAPI stdDisplay_IsFullscreen()
{
    return stdDisplay_bFullscreen;
}

int J3DAPI stdDisplay_LockBackBuffer(void** pSurface, uint32_t* pWidth, uint32_t* pHeight, int32_t* pPitch)
{
    if ( !stdDisplay_bOpen || !stdDisplay_bModeSet || !stdVk_g.pShadowPixels ) {
        return 1;
    }

    stdVk_SyncShadowForRead();
    *pSurface = stdVk_g.pShadowPixels;
    *pWidth   = stdDisplay_g_backBuffer.rasterInfo.width;
    *pHeight  = stdDisplay_g_backBuffer.rasterInfo.height;
    *pPitch   = (int32_t)stdDisplay_g_backBuffer.rasterInfo.rowSize;
    return 0;
}

void stdDisplay_UnlockBackBuffer(void)
{
    if ( stdDisplay_bOpen && stdDisplay_bModeSet ) {
        stdVk_MarkShadowDirty();
    }
}

uint32_t J3DAPI stdDisplay_EncodeFromRGB565(uint16_t pixel)
{
    ColorInfo colorInfo;
    memcpy(&colorInfo, &stdDisplay_pCurVideoMode->rasterInfo.colorInfo, sizeof(colorInfo));

    uint8_t red = 8 * (pixel >> 11);
    if ( (red & 8) != 0 ) {
        red |= 7;
    }

    uint8_t green = 4 * (pixel >> 5);
    if ( (green & 4) != 0 ) {
        green |= 3;
    }

    uint8_t blue = 8 * pixel;
    if ( (blue & 8) != 0 ) {
        blue = blue | 7;
    }

    return (red >> (colorInfo.redPosShiftRight & 0xFF) << (colorInfo.redPosShift & 0xFF))
        | (green >> (colorInfo.greenPosShiftRight & 0xFF) << (colorInfo.greenPosShift & 0xFF))
        | (blue >> (colorInfo.bluePosShiftRight & 0xFF) << (colorInfo.bluePosShift & 0xFF));
}
