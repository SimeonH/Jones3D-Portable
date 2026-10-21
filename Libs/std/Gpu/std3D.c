// std3D implementation of the GPU renderer backends (Vulkan, OpenGL ES 2.0), see stdGpu.h.
// Emulates the Direct3D 6 fixed function state the game uses: pre-transformed (TL) vertices, texture * diffuse
// modulation, alpha blending (SRCALPHA, INVSRCALPHA), alpha test (GREATER ref), z-buffer (LESSEQUAL) and linear w-fog.
#include <std/Win95/std3D.h>
#include <std/Win95/stdDisplay.h>
#include <std/Gpu/stdGpu.h>

#include <j3dcore/j3dhook.h>
#include <std/General/std.h>
#include <std/General/stdColor.h>
#include <std/General/stdMath.h>
#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>
#include <std/RTI/symbols.h>

#include <math.h>
#include <stddef.h>
#include <string.h>

#define STD3D_MAX_VERTICES     65535 // indices are 16 bit
#define STD3D_MAX_TEXTURE_SIZE 4096

static bool std3D_bStartup = false;
static bool std3D_bOpen    = false;

static size_t std3D_frameCount                  = 1;
static Std3DRenderState std3D_renderState       = 0;
static tStd3DTexture std3D_pCurTex              = NULL;
static Std3DMipmapFilterType std3D_mipmapFilter = STD3D_MIPMAPFILTER_TRILINEAR;

static bool std3D_bRenderFog    = true;
static float std3D_fogColor[3]  = { 0 };
static float std3D_fogStartDepth = 0.0f;
static float std3D_fogEndDepth   = 0.0f;

static Device3D* std3D_pCurDevice = NULL;
static size_t std3D_numDevices    = 0;
static Device3D std3D_aDevices[1] = { 0 };

static size_t std3D_numCachedTextures       = 0;
static tSystemTexture* std3D_pFirstTexCache = NULL;
static tSystemTexture* std3D_pLastTexCache  = NULL;

// Formats the game converts textures to (see std3D_Open in Win95/std3D.c), uploaded as is
enum
{
    STD3D_TEXFORMAT_RGB  = 0, // stdColor_cfBGR8888, R8G8B8X8 in memory
    STD3D_TEXFORMAT_RGBA = 1, // stdColor_cfRGBA8888, A8B8G8R8 in memory
};

static size_t std3D_numTextureFormats            = 0;
static StdTextureFormat std3D_aTextureFormats[2] = { 0 };

static void J3DAPI std3D_AddTextureToCacheList(tSystemTexture* pTexture);
static void J3DAPI std3D_RemoveTextureFromCacheList(tSystemTexture* pCacheTexture);

void std3D_InstallHooks(void)
{
    J3D_HOOKFUNC(std3D_Startup);
    J3D_HOOKFUNC(std3D_Shutdown);
    J3D_HOOKFUNC(std3D_GetNumDevices);
    J3D_HOOKFUNC(std3D_GetAllDevices);
    J3D_HOOKFUNC(std3D_Open);
    J3D_HOOKFUNC(std3D_Close);
    J3D_HOOKFUNC(std3D_GetTextureFormat);
    J3D_HOOKFUNC(std3D_GetColorFormat);
    J3D_HOOKFUNC(std3D_GetNumTextureFormats);
    J3D_HOOKFUNC(std3D_StartScene);
    J3D_HOOKFUNC(std3D_EndScene);
    J3D_HOOKFUNC(std3D_DrawRenderList);
    J3D_HOOKFUNC(std3D_SetWireframeRenderState);
    J3D_HOOKFUNC(std3D_DrawLineStrip);
    J3D_HOOKFUNC(std3D_DrawPointList);
    J3D_HOOKFUNC(std3D_SetRenderState);
    J3D_HOOKFUNC(std3D_AllocSystemTexture);
    J3D_HOOKFUNC(std3D_GetValidDimensions);
    J3D_HOOKFUNC(std3D_ClearSystemTexture);
    J3D_HOOKFUNC(std3D_AddToTextureCache);
    J3D_HOOKFUNC(std3D_GetMipMapCount);
    J3D_HOOKFUNC(std3D_ResetTextureCache);
    J3D_HOOKFUNC(std3D_UpdateFrameCount);
    J3D_HOOKFUNC(std3D_FindClosestFormat);
    J3D_HOOKFUNC(std3D_SetMipmapFilter);
    J3D_HOOKFUNC(std3D_SetProjection);
    J3D_HOOKFUNC(std3D_EnableFog);
    J3D_HOOKFUNC(std3D_SetFog);
    J3D_HOOKFUNC(std3D_ClearZBuffer);
    J3D_HOOKFUNC(std3D_BuildDisplayEnvironment);
    J3D_HOOKFUNC(std3D_FreeDisplayEnvironment);
    J3D_HOOKFUNC(std3D_SetFindAllDevices);
}

float std3D_g_fogDensity = 1.0f;
size_t std3D_g_maxVertices;

int std3D_Startup(void)
{
    STD_ASSERTREL(std3D_bStartup == 0);
    memset(std3D_aDevices, 0, sizeof(std3D_aDevices));

    if ( !stdGpu_IsStarted() )
    {
        STDLOG_ERROR("%s device not created yet!\n", stdGpu_GetApiName());
        return 0;
    }

    uint32_t maxTexSize = stdGpu_GetMaxTextureSize();
    maxTexSize = maxTexSize > STD3D_MAX_TEXTURE_SIZE ? STD3D_MAX_TEXTURE_SIZE : maxTexSize;

    Device3D* pDevice = &std3D_aDevices[0];
    pDevice->bHAL                         = 1;
    pDevice->bTexturePerspectiveSupported = 1;
    pDevice->hasZBuffer                   = 1;
    pDevice->bColorkeyTextureSupported    = 0;
    pDevice->bAlphaTextureSupported       = 1;
    pDevice->bStippledShadeSupported      = 0;
    pDevice->bAlphaBlendSupported         = 1;
    pDevice->bSqareOnlyTexture            = 0;
    pDevice->minTexWidth                  = 1;
    pDevice->minTexHeight                 = 1;
    pDevice->maxTexWidth                  = maxTexSize;
    pDevice->maxTexHeight                 = maxTexSize;
    pDevice->maxVertexCount               = STD3D_MAX_VERTICES;

    // The game picks the default device by "HAL" in the description
    STD_FORMAT(pDevice->deviceName, "%s HAL", stdGpu_GetApiName());
    STD_FORMAT(pDevice->deviceDescription, "%s HAL (%s)", stdGpu_GetApiName(), stdGpu_GetDeviceName());

    // The display options read the supported render depths from the D3D device description
    pDevice->d3dDesc.dwSize                  = sizeof(pDevice->d3dDesc);
    pDevice->d3dDesc.dwFlags                 = D3DDD_DEVICERENDERBITDEPTH | D3DDD_DEVICEZBUFFERBITDEPTH;
    pDevice->d3dDesc.dwDeviceRenderBitDepth  = DDBD_16 | DDBD_24 | DDBD_32;
    pDevice->d3dDesc.dwDeviceZBufferBitDepth = DDBD_32;
    pDevice->d3dDesc.dwMaxVertexCount        = STD3D_MAX_VERTICES;

    STDLOG_STATUS("Found |Z|Alpha|Blend|No Colorkey| D3D Device\n");
    STDLOG_STATUS("Description: %s [%s]\n", pDevice->deviceName, pDevice->deviceDescription);

    std3D_numDevices = 1;
    std3D_bStartup   = true;
    return 1;
}

void std3D_Shutdown(void)
{
    if ( std3D_bOpen ) {
        std3D_Close();
    }

    memset(std3D_aTextureFormats, 0, sizeof(std3D_aTextureFormats));
    memset(std3D_aDevices, 0, sizeof(std3D_aDevices));

    std3D_numDevices = 0;
    std3D_bStartup   = false;
}

size_t std3D_GetNumDevices(void)
{
    return std3D_numDevices;
}

const Device3D* std3D_GetAllDevices(void)
{
    return std3D_aDevices;
}

int J3DAPI std3D_Open(size_t deviceNum)
{
    STD_ASSERTREL(std3D_bStartup == 1);
    if ( std3D_bOpen )
    {
        STDLOG_ERROR("Warning: System already open!\n");
        return 0;
    }

    if ( !stdDisplay_IsOpen() || !stdGpu_HasBackBuffer() )
    {
        STDLOG_ERROR("std3D_Open: Display system is not opened!\n");
        return 0;
    }

    if ( deviceNum >= std3D_numDevices ) {
        return 0;
    }

    std3D_pCurDevice = &std3D_aDevices[deviceNum];

    if ( !stdGpu_Open3D() ) {
        return 0;
    }

    // The device limits are final once the back buffer exists (OpenGL ES has no context before)
    uint32_t maxTexSize = stdGpu_GetMaxTextureSize();
    maxTexSize = maxTexSize > STD3D_MAX_TEXTURE_SIZE ? STD3D_MAX_TEXTURE_SIZE : maxTexSize;
    std3D_pCurDevice->maxTexWidth  = maxTexSize;
    std3D_pCurDevice->maxTexHeight = maxTexSize;

    std3D_aTextureFormats[STD3D_TEXFORMAT_RGB].ci  = stdColor_cfBGR8888;
    std3D_aTextureFormats[STD3D_TEXFORMAT_RGBA].ci = stdColor_cfRGBA8888;
    std3D_numTextureFormats = 2;

    std3D_g_maxVertices = std3D_pCurDevice->maxVertexCount;
    STDLOG_STATUS("Max vertices: %d.\n", std3D_g_maxVertices);

    std3D_frameCount        = 1;
    std3D_numCachedTextures = 0;
    std3D_pFirstTexCache    = NULL;
    std3D_pLastTexCache     = NULL;
    std3D_pCurTex           = NULL;

    std3D_renderState = STD3D_RS_UNKNOWN_1 | STD3D_RS_UNKNOWN_2 | STD3D_RS_SUBPIXEL_CORRECTION
        | (stdGpu_HasAnisotropy() ? STD3D_RS_TEXFILTER_ANISOTROPIC : STD3D_RS_TEXFILTER_BILINEAR);
    std3D_mipmapFilter = STD3D_MIPMAPFILTER_TRILINEAR; // Same as the DirectDraw backend default

    stdDisplay_GetTextureMemory(&std3D_pCurDevice->totalMemory, &std3D_pCurDevice->availableMemory);

    std3D_bOpen = true;
    return 1;
}

void J3DAPI std3D_Close()
{
    std3D_ResetTextureCache();
    stdGpu_Close3D();

    std3D_numTextureFormats = 0;
    std3D_pCurDevice        = NULL;
    std3D_bOpen             = false;
}

void J3DAPI std3D_GetTextureFormat(StdColorFormatType type, ColorInfo* pDest, int* pbColorKeySet, LPDDCOLORKEY* ppColorKey)
{
    size_t format = type == STDCOLOR_FORMAT_RGB ? STD3D_TEXFORMAT_RGB : STD3D_TEXFORMAT_RGBA;
    *pbColorKeySet = 0;
    if ( type != STDCOLOR_FORMAT_RGB ) {
        *ppColorKey = NULL;
    }

    memcpy(pDest, &std3D_aTextureFormats[format].ci, sizeof(ColorInfo));
}

StdColorFormatType J3DAPI std3D_GetColorFormat(const ColorInfo* pCi)
{
    if ( pCi->alphaBPP == 0 ) {
        return STDCOLOR_FORMAT_RGB;
    }

    if ( pCi->alphaBPP == 1 ) {
        return STDCOLOR_FORMAT_RGBA_1BIT_ALPHA;
    }

    return STDCOLOR_FORMAT_RGBA;
}

size_t std3D_GetNumTextureFormats(void)
{
    return std3D_numTextureFormats;
}

int std3D_StartScene(void)
{
    ++std3D_frameCount;
    std3D_pCurTex = NULL;
    return 0;
}

void std3D_EndScene(void)
{
    std3D_pCurTex = NULL;
}

static void J3DAPI std3D_Draw(StdGpuPrimitive primitive, const StdTLVertex* aVerts, size_t numVerts, const uint16_t* aIndices, size_t numIndices)
{
    if ( !std3D_bOpen || numVerts == 0 ) {
        return;
    }

    StdGpuDrawState state;
    memset(&state, 0, sizeof(state));
    state.pTexture = std3D_pCurTex;

    if ( (std3D_renderState & STD3D_RS_TEXFILTER_ANISOTROPIC) != 0 ) {
        state.filter = STDGPU_TEXFILTER_ANISOTROPIC;
    }
    else if ( (std3D_renderState & STD3D_RS_TEXFILTER_BILINEAR) != 0 ) {
        state.filter = STDGPU_TEXFILTER_BILINEAR;
    }
    else {
        state.filter = STDGPU_TEXFILTER_POINT;
    }

    state.mipFilter   = std3D_mipmapFilter;
    state.bClampU     = (std3D_renderState & STD3D_RS_TEX_CPAMP_U) != 0;
    state.bClampV     = (std3D_renderState & STD3D_RS_TEX_CPAMP_V) != 0;
    state.bDepthWrite = (std3D_renderState & STD3D_RS_ZWRITE_DISABLED) == 0;

    // D3DCMP_GREATER against 8-bit alpha ref
    uint32_t alphaRef = (std3D_renderState & STD3D_RS_ALPHAREF_SET) != 0 ? 0xA0 : 0;
    state.alphaRef    = ((float)alphaRef + 0.5f) / 255.0f;

    if ( (std3D_renderState & STD3D_RS_FOG_ENABLED) != 0 && std3D_bRenderFog && std3D_g_fogDensity != 0.0f && std3D_fogEndDepth > std3D_fogStartDepth )
    {
        state.bFog        = true;
        state.fogColor[0] = std3D_fogColor[0];
        state.fogColor[1] = std3D_fogColor[1];
        state.fogColor[2] = std3D_fogColor[2];
        state.fogStart    = std3D_fogStartDepth;
        state.fogEnd      = std3D_fogEndDepth;
    }

    stdGpu_Draw(primitive, &state, aVerts, numVerts, aIndices, numIndices);
}

void J3DAPI std3D_DrawRenderList(tStd3DTexture pTex, Std3DRenderState rdflags, StdTLVertex* aVerts, size_t numVerts, uint16_t* aIndices, size_t numIndices)
{
    if ( numVerts > std3D_g_maxVertices )
    {
        STDLOG_ERROR("Error %d > %d maxVertices.\n", numVerts, std3D_g_maxVertices);
        return;
    }

    std3D_SetRenderState(rdflags);
    std3D_pCurTex = pTex;
    std3D_Draw(STDGPU_PRIMITIVE_TRIANGLES, aVerts, numVerts, aIndices, numIndices);
}

void std3D_SetWireframeRenderState(void)
{
    Std3DRenderState rdstate = std3D_renderState & ~(STD3D_RS_FOG_ENABLED | STD3D_RS_UNKNOWN_400 | STD3D_RS_UNKNOWN_200);
    std3D_SetRenderState(rdstate);
    std3D_pCurTex = NULL;
}

void J3DAPI std3D_DrawLineStrip(StdTLVertex* aVerts, size_t numVerts)
{
    if ( numVerts > std3D_g_maxVertices )
    {
        STDLOG_ERROR("Error %d > %d maxVertices.\n", numVerts, std3D_g_maxVertices);
        return;
    }

    std3D_Draw(STDGPU_PRIMITIVE_LINESTRIP, aVerts, numVerts, NULL, 0);
}

void J3DAPI std3D_DrawPointList(StdTLVertex* aVerts, size_t numVerts)
{
    if ( numVerts > std3D_g_maxVertices )
    {
        STDLOG_ERROR("Error %d > %d maxVertices.\n", numVerts, std3D_g_maxVertices);
        return;
    }

    std3D_Draw(STDGPU_PRIMITIVE_POINTS, aVerts, numVerts, NULL, 0);
}

void J3DAPI std3D_SetRenderState(Std3DRenderState rdflags)
{
    // Applied per draw from std3D_renderState
    std3D_renderState = rdflags;
}

// Converts a pixel of any RGB(A) format to R8G8B8A8
static uint32_t std3D_ConvertPixel(uint32_t pixel, const ColorInfo* pCi)
{
    uint32_t r = ((pixel >> pCi->redPosShift) & ((1u << pCi->redBPP) - 1)) << pCi->redPosShiftRight;
    uint32_t g = ((pixel >> pCi->greenPosShift) & ((1u << pCi->greenBPP) - 1)) << pCi->greenPosShiftRight;
    uint32_t b = ((pixel >> pCi->bluePosShift) & ((1u << pCi->blueBPP) - 1)) << pCi->bluePosShiftRight;
    uint32_t a = 0xFF;
    if ( pCi->alphaBPP )
    {
        a = ((pixel >> pCi->alphaPosShift) & ((1u << pCi->alphaBPP) - 1)) << pCi->alphaPosShiftRight;
        a = pCi->alphaBPP == 1 ? (a ? 0xFF : 0) : a | (a >> pCi->alphaBPP);
    }

    // Replicate high bits to the low ones
    r |= r >> pCi->redBPP;
    g |= g >> pCi->greenBPP;
    b |= b >> pCi->blueBPP;
    return (r & 0xFF) | ((g & 0xFF) << 8) | ((b & 0xFF) << 16) | ((a & 0xFF) << 24);
}

void J3DAPI std3D_AllocSystemTexture(tSystemTexture* pTexture, tVBuffer** apVBuffers, size_t numMipLevels, StdColorFormatType formatType)
{
    J3D_UNUSED(formatType);
    memset(pTexture, 0, sizeof(tSystemTexture));

    if ( !std3D_numTextureFormats ) {
        return;
    }

    // Skip mip levels too big for the device
    tVBuffer* pVBuffer = *apVBuffers;
    uint32_t texHeight = 0, texWidth = 0;
    std3D_GetValidDimensions(pVBuffer->rasterInfo.width, pVBuffer->rasterInfo.height, &texWidth, &texHeight);
    while ( numMipLevels > 1 && (pVBuffer->rasterInfo.width > texWidth || pVBuffer->rasterInfo.height > texHeight) )
    {
        --numMipLevels;
        pVBuffer = *++apVBuffers;
        std3D_GetValidDimensions(pVBuffer->rasterInfo.width, pVBuffer->rasterInfo.height, &texWidth, &texHeight);
    }

    if ( pVBuffer->rasterInfo.width > texWidth || pVBuffer->rasterInfo.height > texHeight )
    {
        STDLOG_ERROR("Texture %dx%d is too big.\n", pVBuffer->rasterInfo.width, pVBuffer->rasterInfo.height);
        return;
    }

    if ( std3D_mipmapFilter == STD3D_MIPMAPFILTER_NONE ) {
        numMipLevels = 1;
    }

    // Use only the mip levels of a regular mip chain
    uint32_t width  = pVBuffer->rasterInfo.width;
    uint32_t height = pVBuffer->rasterInfo.height;
    size_t numLevels = 1;
    while ( numLevels < numMipLevels )
    {
        uint32_t mipWidth  = width >> numLevels;
        uint32_t mipHeight = height >> numLevels;
        const tRasterInfo* pInfo = &apVBuffers[numLevels]->rasterInfo;
        if ( pInfo->width != (mipWidth ? mipWidth : 1) || pInfo->height != (mipHeight ? mipHeight : 1) ) {
            break;
        }
        ++numLevels;
    }

    const ColorInfo* pCi = &pVBuffer->rasterInfo.colorInfo;
    if ( pCi->colorMode == STDCOLOR_PAL )
    {
        STDLOG_ERROR("Can't use paletized textures.\n");
        return;
    }

    // Textures already in a texture format are uploaded as is, others are converted to R8G8B8A8
    StdGpuTexLayout layout = STDGPU_TEXLAYOUT_RGBA;
    bool bConvert = false;
    if ( memcmp(pCi, &stdColor_cfBGR8888, sizeof(ColorInfo)) == 0 )
    {
        layout = STDGPU_TEXLAYOUT_RGBX;
    }
    else if ( memcmp(pCi, &stdColor_cfRGBA8888, sizeof(ColorInfo)) == 0 )
    {
        layout = STDGPU_TEXLAYOUT_ABGR;
    }
    else
    {
        bConvert = true;
    }

    tStd3DTexture pTex = stdGpu_CreateTexture(width, height, (uint32_t)numLevels, layout);
    if ( !pTex )
    {
        STDLOG_ERROR("Error creating %s texture.\n", stdGpu_GetApiName());
        return;
    }

    uint32_t* pConverted = NULL;
    if ( bConvert ) {
        pConverted = (uint32_t*)STDMALLOC((size_t)width * height * 4);
    }

    for ( size_t level = 0; level < numLevels; ++level )
    {
        tVBuffer* pLevel = apVBuffers[level];
        stdDisplay_VBufferLock(pLevel);

        if ( bConvert && pConverted )
        {
            const tRasterInfo* pInfo = &pLevel->rasterInfo;
            size_t bytesPerPixel = pInfo->colorInfo.bpp / 8;
            for ( uint32_t y = 0; y < pInfo->height; ++y )
            {
                const uint8_t* pRow = pLevel->pPixels + pInfo->rowSize * y;
                for ( uint32_t x = 0; x < pInfo->width; ++x )
                {
                    uint32_t pixel = 0;
                    memcpy(&pixel, pRow + x * bytesPerPixel, bytesPerPixel);
                    pConverted[(size_t)y * pInfo->width + x] = std3D_ConvertPixel(pixel, &pInfo->colorInfo);
                }
            }

            stdGpu_UploadTextureLevel(pTex, (uint32_t)level, (const uint8_t*)pConverted, (size_t)pInfo->width * 4);
        }
        else
        {
            stdGpu_UploadTextureLevel(pTex, (uint32_t)level, pLevel->pPixels, pLevel->rasterInfo.rowSize);
        }

        stdDisplay_VBufferUnlock(pLevel);
    }

    if ( pConverted ) {
        stdMemory_Free(pConverted);
    }

    stdGpu_FinishTextureUpload(pTex);

    pTexture->pSrcTexture = pTex;
    pTexture->textureSize = (size_t)width * height * 4;
}

void J3DAPI std3D_GetValidDimensions(uint32_t width, uint32_t height, uint32_t* pOutWidth, uint32_t* pOutHeight)
{
    size_t maxSize = std3D_pCurDevice ? std3D_pCurDevice->maxTexWidth : STD3D_MAX_TEXTURE_SIZE;
    *pOutWidth  = STDMATH_CLAMP(width, 1, maxSize);
    *pOutHeight = STDMATH_CLAMP(height, 1, maxSize);
}

void J3DAPI std3D_ClearSystemTexture(tSystemTexture* pTex)
{
    if ( pTex->pCachedTex ) {
        std3D_RemoveTextureFromCacheList(pTex);
    }

    if ( pTex->pSrcTexture )
    {
        if ( std3D_pCurTex == pTex->pSrcTexture ) {
            std3D_pCurTex = NULL;
        }

        stdGpu_DestroyTexture(pTex->pSrcTexture);
    }

    memset(pTex, 0, sizeof(tSystemTexture));
}

void J3DAPI std3D_AddToTextureCache(tSystemTexture* pCacheTexture, StdColorFormatType format)
{
    J3D_UNUSED(format);
    STD_ASSERTREL(pCacheTexture);

    if ( !pCacheTexture->pSrcTexture )
    {
        STDLOG_ERROR("No Source texture.\n");
        pCacheTexture->pCachedTex = NULL;
        pCacheTexture->frameNum   = 0;
        return;
    }

    // Textures stay in video memory, the cache only tracks which ones are in use
    pCacheTexture->pCachedTex = pCacheTexture->pSrcTexture;
    pCacheTexture->frameNum   = std3D_frameCount;
    std3D_AddTextureToCacheList(pCacheTexture);
}

size_t J3DAPI std3D_GetMipMapCount(const tSystemTexture* pTexture)
{
    if ( !pTexture || !pTexture->pSrcTexture ) {
        return 0;
    }

    return stdGpu_GetTextureMipLevels(pTexture->pSrcTexture);
}

void J3DAPI std3D_ResetTextureCache()
{
    tSystemTexture* pCurTex = std3D_pFirstTexCache;
    while ( pCurTex )
    {
        tSystemTexture* pNextTex = pCurTex->pNextCachedTexture;
        pCurTex->pCachedTex         = NULL;
        pCurTex->frameNum           = 0;
        pCurTex->pNextCachedTexture = NULL;
        pCurTex->pPrevCachedTexture = NULL;
        pCurTex = pNextTex;
    }

    std3D_pFirstTexCache    = NULL;
    std3D_pLastTexCache     = NULL;
    std3D_numCachedTextures = 0;

    if ( std3D_pCurDevice ) {
        std3D_pCurDevice->availableMemory = std3D_pCurDevice->totalMemory;
    }

    std3D_pCurTex    = NULL;
    std3D_frameCount = 1;
}

void J3DAPI std3D_UpdateFrameCount(tSystemTexture* pTexture)
{
    pTexture->frameNum = std3D_frameCount;
    std3D_RemoveTextureFromCacheList(pTexture);
    std3D_AddTextureToCacheList(pTexture);
}

size_t J3DAPI std3D_FindClosestFormat(const ColorInfo* pMatch)
{
    for ( size_t i = 0; i < std3D_numTextureFormats; ++i )
    {
        if ( memcmp(&std3D_aTextureFormats[i].ci, pMatch, sizeof(ColorInfo)) == 0 ) {
            return i;
        }
    }

    return pMatch->alphaBPP ? STD3D_TEXFORMAT_RGBA : STD3D_TEXFORMAT_RGB;
}

int J3DAPI std3D_SetMipmapFilter(Std3DMipmapFilterType filter)
{
    std3D_mipmapFilter = filter;
    return 0;
}

int J3DAPI std3D_SetProjection(float fov, float nearPlane, float farPlane)
{
    // Vertices are pre-transformed, the projection was only used by D3D for w-fog
    J3D_UNUSED(fov);
    if ( fabs(farPlane - nearPlane) < 0.009999999776482582 ) {
        return 1;
    }

    return 0;
}

void J3DAPI std3D_EnableFog(int bEnabled, float density)
{
    J3D_UNUSED(density);
    std3D_bRenderFog = bEnabled != 0;
}

void J3DAPI std3D_SetFog(float red, float green, float blue, float startDepth, float endDepth)
{
    std3D_fogColor[0]   = red;
    std3D_fogColor[1]   = green;
    std3D_fogColor[2]   = blue;
    std3D_fogStartDepth = startDepth;
    std3D_fogEndDepth   = endDepth;
}

void std3D_ClearZBuffer(void)
{
    if ( std3D_bOpen ) {
        stdGpu_ClearDepth();
    }
}

static void J3DAPI std3D_AddTextureToCacheList(tSystemTexture* pTexture)
{
    if ( std3D_pFirstTexCache )
    {
        std3D_pLastTexCache->pNextCachedTexture = pTexture;
        pTexture->pPrevCachedTexture            = std3D_pLastTexCache;
        pTexture->pNextCachedTexture            = NULL;
        std3D_pLastTexCache                     = pTexture;
    }
    else
    {
        std3D_pLastTexCache          = pTexture;
        std3D_pFirstTexCache         = pTexture;
        pTexture->pPrevCachedTexture = NULL;
        pTexture->pNextCachedTexture = NULL;
    }

    ++std3D_numCachedTextures;
    if ( std3D_pCurDevice ) {
        std3D_pCurDevice->availableMemory -= pTexture->textureSize;
    }
}

static void J3DAPI std3D_RemoveTextureFromCacheList(tSystemTexture* pCacheTexture)
{
    if ( pCacheTexture == std3D_pFirstTexCache )
    {
        std3D_pFirstTexCache = pCacheTexture->pNextCachedTexture;
        if ( std3D_pFirstTexCache )
        {
            std3D_pFirstTexCache->pPrevCachedTexture = NULL;
            if ( !std3D_pFirstTexCache->pNextCachedTexture ) {
                std3D_pLastTexCache = std3D_pFirstTexCache;
            }
        }
        else {
            std3D_pLastTexCache = NULL;
        }
    }
    else if ( pCacheTexture == std3D_pLastTexCache )
    {
        std3D_pLastTexCache = pCacheTexture->pPrevCachedTexture;
        pCacheTexture->pPrevCachedTexture->pNextCachedTexture = NULL;
    }
    else
    {
        pCacheTexture->pPrevCachedTexture->pNextCachedTexture = pCacheTexture->pNextCachedTexture;
        pCacheTexture->pNextCachedTexture->pPrevCachedTexture = pCacheTexture->pPrevCachedTexture;
    }

    pCacheTexture->pNextCachedTexture = NULL;
    pCacheTexture->pPrevCachedTexture = NULL;
    pCacheTexture->frameNum           = 0;

    --std3D_numCachedTextures;
    if ( std3D_pCurDevice ) {
        std3D_pCurDevice->availableMemory += pCacheTexture->textureSize;
    }
}

StdDisplayEnvironment* J3DAPI std3D_BuildDisplayEnvironment()
{
    StdDisplayEnvironment* pDeviceList = (StdDisplayEnvironment*)STDMALLOC(sizeof(StdDisplayEnvironment));
    memset(pDeviceList, 0, sizeof(StdDisplayEnvironment));

    if ( !stdDisplay_Startup() )
    {
        STDLOG_ERROR("Error starting stdDisplay system.\n");
        std3D_FreeDisplayEnvironment(pDeviceList);
        return NULL;
    }

    pDeviceList->numInfos = stdDisplay_GetNumDevices();
    if ( pDeviceList->numInfos )
    {
        pDeviceList->aDisplayInfos = (StdDisplayInfo*)STDMALLOC(sizeof(StdDisplayInfo) * pDeviceList->numInfos);

        StdDisplayInfo* pCurInfo = pDeviceList->aDisplayInfos;
        for ( size_t deviceNum = 0; deviceNum < pDeviceList->numInfos; ++deviceNum, ++pCurInfo )
        {
            memset(pCurInfo, 0, sizeof(StdDisplayInfo));

            if ( stdDisplay_GetDevice(deviceNum, &pCurInfo->displayDevice) || !stdDisplay_Open(deviceNum) )
            {
                STDLOG_ERROR("Error opening stdDisplay device.\n");
                std3D_FreeDisplayEnvironment(pDeviceList);
                return NULL;
            }

            pCurInfo->numModes = stdDisplay_GetNumVideoModes();
            if ( pCurInfo->numModes )
            {
                pCurInfo->aModes = (StdVideoMode*)STDMALLOC(sizeof(StdVideoMode) * pCurInfo->numModes);
                for ( size_t modeNum = 0; modeNum < pCurInfo->numModes; ++modeNum ) {
                    stdDisplay_GetVideoMode(modeNum, &pCurInfo->aModes[modeNum]);
                }
            }

            if ( pCurInfo->displayDevice.bHAL )
            {
                if ( !std3D_Startup() )
                {
                    STDLOG_ERROR("Error starting std3D system.\n");
                    std3D_FreeDisplayEnvironment(pDeviceList);
                    return NULL;
                }

                pCurInfo->numDevices = std3D_GetNumDevices();
                if ( pCurInfo->numDevices )
                {
                    size_t listSize = sizeof(Device3D) * pCurInfo->numDevices;
                    pCurInfo->aDevices = (Device3D*)STDMALLOC(listSize);
                    memcpy(pCurInfo->aDevices, std3D_GetAllDevices(), listSize);
                }

                std3D_Shutdown();
            }

            stdDisplay_Close();
        }
    }

    stdDisplay_Shutdown();
    return pDeviceList;
}

void J3DAPI std3D_FreeDisplayEnvironment(StdDisplayEnvironment* pEnv)
{
    if ( pEnv->aDisplayInfos )
    {
        for ( size_t i = 0; i < pEnv->numInfos; ++i )
        {
            StdDisplayInfo* pInfo = &pEnv->aDisplayInfos[i];
            if ( pInfo->aDevices ) {
                stdMemory_Free(pInfo->aDevices);
            }

            if ( pInfo->aModes ) {
                stdMemory_Free(pInfo->aModes);
            }
        }

        stdMemory_Free(pEnv->aDisplayInfos);
        pEnv->aDisplayInfos = NULL;
    }

    stdMemory_Free(pEnv);
}

void J3DAPI std3D_SetFindAllDevices(int bFindAll)
{
    J3D_UNUSED(bFindAll);
}
