// Dispatch of the GPU renderer interface (stdGpu.h) to the backend of the selected display device
#include "stdGpu.h"

#include <std/General/std.h>
#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>

#include <string.h>

static const StdGpuBackend* const stdGpu_apBackends[] = {
#ifdef J3D_RENDERER_VULKAN
    &stdVk_g_backend,
#endif
#ifdef J3D_RENDERER_GLES2
    &stdGl_g_backend,
#endif
};

// Textures keep a CPU copy of their pixels to be recreated when the display switches backends
#define STDGPU_KEEP_PIXELS (STD_ARRAYLEN(stdGpu_apBackends) > 1)

struct sStdGpuTexture
{
    const StdGpuBackend* pBackend; // Backend of pNative
    void* pNative;
    uint32_t width;
    uint32_t height;
    uint32_t numMipLevels;
    StdGpuTexLayout layout;
    uint8_t* pPixels; // CPU copy of all levels, rows tightly packed
    struct sStdGpuTexture* pPrev;
    struct sStdGpuTexture* pNext;
};

static const StdGpuBackend* stdGpu_pCurBackend    = NULL; // Backend of the opened display device
static const StdGpuBackend* stdGpu_pActiveBackend = NULL; // Backend that last created the back buffer
static tStd3DTexture stdGpu_pTextures             = NULL;
static bool stdGpu_bNoVSync                       = false;

static const StdGpuBackend* stdGpu_Cur(void)
{
    if ( !stdGpu_pCurBackend ) {
        stdGpu_pCurBackend = stdGpu_apBackends[0];
    }

    return stdGpu_pCurBackend;
}

size_t stdGpu_GetNumBackends(void)
{
    return STD_ARRAYLEN(stdGpu_apBackends);
}

const StdGpuBackend* J3DAPI stdGpu_GetBackend(size_t idx)
{
    return idx < STD_ARRAYLEN(stdGpu_apBackends) ? stdGpu_apBackends[idx] : NULL;
}

const StdGpuBackend* stdGpu_GetCurBackend(void)
{
    return stdGpu_Cur();
}

void J3DAPI stdGpu_SelectBackend(const StdGpuBackend* pBackend)
{
    if ( pBackend ) {
        stdGpu_pCurBackend = pBackend;
    }
}

#ifndef _WIN32
void J3DAPI stdGpu_SetPreferredDriver(const char* pDriverName)
{
    for ( size_t i = 0; i < STD_ARRAYLEN(stdGpu_apBackends); ++i )
    {
        if ( pDriverName && strcmp(stdGpu_apBackends[i]->pApiName, pDriverName) == 0 )
        {
            stdWin95_SetPreferredWindowApi(stdGpu_apBackends[i]->windowApi);
            return;
        }
    }
}
#endif

const char* stdGpu_GetApiName(void)
{
    return stdGpu_Cur()->pApiName;
}

bool stdGpu_IsStarted(void)
{
    return stdGpu_Cur()->pfIsStarted();
}

const char* stdGpu_GetDeviceName(void)
{
    return stdGpu_Cur()->pfGetDeviceName();
}

size_t stdGpu_GetVideoMemory(void)
{
    return stdGpu_Cur()->pfGetVideoMemory();
}

uint32_t stdGpu_GetMaxTextureSize(void)
{
    return stdGpu_Cur()->pfGetMaxTextureSize();
}

bool stdGpu_HasAnisotropy(void)
{
    return stdGpu_Cur()->pfHasAnisotropy();
}

// Destroys the native textures of a backend, they're recreated from their pixels when used again
static void stdGpu_ReleaseTextures(const StdGpuBackend* pBackend)
{
    for ( tStd3DTexture pTex = stdGpu_pTextures; pTex; pTex = pTex->pNext )
    {
        if ( pTex->pNative && pTex->pBackend == pBackend && pTex->pPixels )
        {
            pBackend->pfDestroyTexture(pTex->pNative);
            pTex->pNative  = NULL;
            pTex->pBackend = NULL;
        }
    }
}

int J3DAPI stdGpu_CreateBackBuffer(HWND hwnd, uint32_t width, uint32_t height)
{
    const StdGpuBackend* pBackend = stdGpu_Cur();
    if ( stdGpu_pActiveBackend && stdGpu_pActiveBackend != pBackend )
    {
        // Switching backends: move the textures and finish the old backend while its window still exists
        STDLOG_STATUS("Switching renderer from %s to %s.\n", stdGpu_pActiveBackend->pApiName, pBackend->pApiName);
        stdGpu_pActiveBackend->pfDestroyBackBuffer();
        stdGpu_ReleaseTextures(stdGpu_pActiveBackend);
        stdGpu_pActiveBackend->pfShutdown();
    }

#ifndef _WIN32
    hwnd = stdWin95_SetWindowApi(pBackend->windowApi);
#endif

    stdGpu_pActiveBackend = pBackend;
    pBackend->pfSetNoVSync(stdGpu_bNoVSync); // A backend forgets it when it's shut down
    return pBackend->pfCreateBackBuffer(hwnd, width, height);
}

void stdGpu_DestroyBackBuffer(void)
{
    stdGpu_Cur()->pfDestroyBackBuffer();
}

bool stdGpu_HasBackBuffer(void)
{
    return stdGpu_Cur()->pfHasBackBuffer();
}

void J3DAPI stdGpu_SetNoVSync(bool bNoVSync)
{
    stdGpu_bNoVSync = bNoVSync;
    stdGpu_Cur()->pfSetNoVSync(bNoVSync);
}

int stdGpu_Present(void)
{
    return stdGpu_Cur()->pfPresent();
}

float stdGpu_GetPresentScale(void)
{
    return stdGpu_Cur()->pfGetPresentScale();
}

uint8_t* stdGpu_GetShadowPixels(void)
{
    return stdGpu_Cur()->pfGetShadowPixels();
}

#ifdef _WIN32
HDC stdGpu_GetShadowDC(void)
{
    return stdGpu_Cur()->pfGetShadowDC();
}
#endif

void stdGpu_SyncShadowForRead(void)
{
    stdGpu_Cur()->pfSyncShadowForRead();
}

void stdGpu_MarkShadowDirty(void)
{
    stdGpu_Cur()->pfMarkShadowDirty();
}

void J3DAPI stdGpu_ClearColor(uint32_t xrgb, const StdRect* pRect)
{
    stdGpu_Cur()->pfClearColor(xrgb, pRect);
}

void stdGpu_ClearDepth(void)
{
    stdGpu_Cur()->pfClearDepth();
}

int stdGpu_Open3D(void)
{
    return stdGpu_Cur()->pfOpen3D();
}

void stdGpu_Close3D(void)
{
    stdGpu_Cur()->pfClose3D();
}

// Textures

static size_t stdGpu_GetLevelSize(const tStd3DTexture pTex, uint32_t level, uint32_t* pWidth, uint32_t* pHeight)
{
    uint32_t width  = pTex->width >> level;
    uint32_t height = pTex->height >> level;
    *pWidth  = width ? width : 1;
    *pHeight = height ? height : 1;
    return (size_t)*pWidth * *pHeight * 4;
}

static size_t stdGpu_GetLevelOffset(const tStd3DTexture pTex, uint32_t level)
{
    size_t offset = 0;
    for ( uint32_t i = 0; i < level; ++i )
    {
        uint32_t width, height;
        offset += stdGpu_GetLevelSize(pTex, i, &width, &height);
    }

    return offset;
}

// Returns the texture of the current backend, recreating it from the CPU copy when needed
static void* stdGpu_GetNativeTexture(tStd3DTexture pTex)
{
    const StdGpuBackend* pBackend = stdGpu_Cur();
    if ( pTex->pNative && pTex->pBackend == pBackend ) {
        return pTex->pNative;
    }

    if ( !pTex->pPixels ) {
        return NULL;
    }

    if ( pTex->pNative ) {
        pTex->pBackend->pfDestroyTexture(pTex->pNative);
    }

    pTex->pBackend = pBackend;
    pTex->pNative  = pBackend->pfCreateTexture(pTex->width, pTex->height, pTex->numMipLevels, pTex->layout);
    if ( !pTex->pNative ) {
        return NULL;
    }

    for ( uint32_t level = 0; level < pTex->numMipLevels; ++level )
    {
        uint32_t width, height;
        stdGpu_GetLevelSize(pTex, level, &width, &height);
        pBackend->pfUploadTextureLevel(pTex->pNative, level, pTex->pPixels + stdGpu_GetLevelOffset(pTex, level), (size_t)width * 4);
    }

    pBackend->pfFinishTextureUpload(pTex->pNative);
    return pTex->pNative;
}

void J3DAPI stdGpu_Draw(StdGpuPrimitive primitive, const StdGpuDrawState* pState, const StdTLVertex* aVerts, size_t numVerts, const uint16_t* aIndices, size_t numIndices)
{
    void* pNativeTex = pState->pTexture ? stdGpu_GetNativeTexture(pState->pTexture) : NULL;
    stdGpu_Cur()->pfDraw(primitive, pState, pNativeTex, aVerts, numVerts, aIndices, numIndices);
}

tStd3DTexture J3DAPI stdGpu_CreateTexture(uint32_t width, uint32_t height, uint32_t numMipLevels, StdGpuTexLayout layout)
{
    tStd3DTexture pTex = (tStd3DTexture)STDMALLOC(sizeof(struct sStdGpuTexture));
    if ( !pTex ) {
        return NULL;
    }

    memset(pTex, 0, sizeof(*pTex));
    pTex->width        = width;
    pTex->height       = height;
    pTex->numMipLevels = numMipLevels;
    pTex->layout       = layout;

    if ( STDGPU_KEEP_PIXELS )
    {
        pTex->pPixels = (uint8_t*)STDMALLOC(stdGpu_GetLevelOffset(pTex, numMipLevels));
        if ( !pTex->pPixels )
        {
            stdMemory_Free(pTex);
            return NULL;
        }
    }

    pTex->pBackend = stdGpu_Cur();
    pTex->pNative  = pTex->pBackend->pfCreateTexture(width, height, numMipLevels, layout);
    if ( !pTex->pNative )
    {
        if ( pTex->pPixels ) {
            stdMemory_Free(pTex->pPixels);
        }
        stdMemory_Free(pTex);
        return NULL;
    }

    pTex->pNext = stdGpu_pTextures;
    if ( stdGpu_pTextures ) {
        stdGpu_pTextures->pPrev = pTex;
    }
    stdGpu_pTextures = pTex;
    return pTex;
}

void J3DAPI stdGpu_UploadTextureLevel(tStd3DTexture pTex, uint32_t level, const uint8_t* pPixels, size_t rowSize)
{
    if ( !pTex || level >= pTex->numMipLevels ) {
        return;
    }

    if ( pTex->pPixels )
    {
        uint32_t width, height;
        stdGpu_GetLevelSize(pTex, level, &width, &height);
        uint8_t* pDst = pTex->pPixels + stdGpu_GetLevelOffset(pTex, level);
        for ( uint32_t row = 0; row < height; ++row ) {
            memcpy(pDst + (size_t)row * width * 4, pPixels + row * rowSize, (size_t)width * 4);
        }
    }

    if ( pTex->pNative ) {
        pTex->pBackend->pfUploadTextureLevel(pTex->pNative, level, pPixels, rowSize);
    }
}

void J3DAPI stdGpu_FinishTextureUpload(tStd3DTexture pTex)
{
    if ( pTex && pTex->pNative ) {
        pTex->pBackend->pfFinishTextureUpload(pTex->pNative);
    }
}

void J3DAPI stdGpu_DestroyTexture(tStd3DTexture pTex)
{
    if ( !pTex ) {
        return;
    }

    if ( pTex->pNative ) {
        pTex->pBackend->pfDestroyTexture(pTex->pNative);
    }

    if ( pTex->pPrev ) {
        pTex->pPrev->pNext = pTex->pNext;
    }
    else {
        stdGpu_pTextures = pTex->pNext;
    }

    if ( pTex->pNext ) {
        pTex->pNext->pPrev = pTex->pPrev;
    }

    if ( pTex->pPixels ) {
        stdMemory_Free(pTex->pPixels);
    }

    stdMemory_Free(pTex);
}

uint32_t J3DAPI stdGpu_GetTextureMipLevels(tStd3DTexture pTex)
{
    return pTex ? pTex->numMipLevels : 0;
}
