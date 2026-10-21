#ifndef STD_GPU_STDGPU_H
#define STD_GPU_STDGPU_H
#include <j3dcore/j3d.h>
#include <std/types.h>

#ifndef _WIN32
#include <std/Win95/stdWin95.h>
#endif

J3D_EXTERN_C_START

// GPU renderer backends (Vulkan/, GLES2/) of the shared stdDisplay and std3D implementations in Gpu/.
// Each backend is a display device of stdDisplay, the game picks one in the display settings.
//
// The game's back buffer is an offscreen color target with a depth buffer, rendered at the game resolution
// and scaled to the window on present.
// CPU access to the back buffer (locks, fills, GDI DCs on Windows) goes through a 32-bit XRGB8888 mirror
// (BGRX in memory, a DIB section on Windows): it's read back from the GPU when the GPU holds newer content
// and uploaded again before the next GPU operation after the CPU wrote to it.
//
// Textures outlive display restarts (materials keep them), when the display switches to another backend they're
// recreated in it from a CPU copy of their pixels, which is kept when more than one backend is built.

typedef enum eStdGpuPrimitive
{
    STDGPU_PRIMITIVE_TRIANGLES,
    STDGPU_PRIMITIVE_LINESTRIP,
    STDGPU_PRIMITIVE_POINTS,
    STDGPU_NUM_PRIMITIVES
} StdGpuPrimitive;

typedef enum eStdGpuTexFilter
{
    STDGPU_TEXFILTER_POINT,
    STDGPU_TEXFILTER_BILINEAR,
    STDGPU_TEXFILTER_ANISOTROPIC
} StdGpuTexFilter;

// Direct3D 6 state the game uses for pre-transformed vertices: texture * diffuse modulation,
// alpha blending (SRCALPHA, INVSRCALPHA), alpha test, z-buffer (LESSEQUAL) and linear w-fog
typedef struct sStdGpuDrawState
{
    tStd3DTexture pTexture; // NULL draws untextured
    StdGpuTexFilter filter;
    Std3DMipmapFilterType mipFilter;
    bool bClampU;
    bool bClampV;
    bool bDepthWrite;
    float alphaRef; // fragments with alpha below are discarded
    bool bFog;
    float fogColor[3];
    float fogStart; // linear fog range in eye depth (w)
    float fogEnd;
} StdGpuDrawState;

// Memory layout of the 4-byte texture pixels passed to stdGpu_UploadTextureLevel
typedef enum eStdGpuTexLayout
{
    STDGPU_TEXLAYOUT_RGBA, // R8G8B8A8 in memory
    STDGPU_TEXLAYOUT_RGBX, // R8G8B8X8 in memory, opaque (stdColor_cfBGR8888)
    STDGPU_TEXLAYOUT_ABGR, // A8B8G8R8 in memory (stdColor_cfRGBA8888)
} StdGpuTexLayout;

// Backend interface. Textures are the backend's own objects (pNativeTex).
typedef struct sStdGpuBackend
{
    const char* pApiName; // Driver name of the display device
#ifndef _WIN32
    StdWindowApi windowApi;
#endif

    // Device. Startup doesn't need the game window, so all backends can be listed.
    // Shutdown keeps the device alive while it has textures, calling it again after they're destroyed finishes it.
    int (*pfStartup)(void); // Returns 1 on success
    void (*pfShutdown)(void);
    bool (*pfIsStarted)(void);
    const char* (*pfGetDeviceName)(void);
    size_t (*pfGetVideoMemory)(void); // Kept in a signed 32-bit range, the game does arithmetic on it
    uint32_t (*pfGetMaxTextureSize)(void);
    bool (*pfHasAnisotropy)(void);

    // Back buffer
    int (*pfCreateBackBuffer)(HWND hwnd, uint32_t width, uint32_t height); // Returns 1 on success
    void (*pfDestroyBackBuffer)(void);
    bool (*pfHasBackBuffer)(void);
    void (*pfSetNoVSync)(bool bNoVSync);
    int (*pfPresent)(void);             // Returns 0 on success
    float (*pfGetPresentScale)(void);   // Window pixels per back buffer pixel on last present
    uint8_t* (*pfGetShadowPixels)(void);
#ifdef _WIN32
    HDC (*pfGetShadowDC)(void);
#endif
    void (*pfSyncShadowForRead)(void);
    void (*pfMarkShadowDirty)(void);
    void (*pfClearColor)(uint32_t xrgb, const StdRect* pRect); // pRect is (left, top, width, height), NULL for all
    void (*pfClearDepth)(void);

    // 3D
    int (*pfOpen3D)(void); // Returns 1 on success
    void (*pfClose3D)(void);
    void (*pfDraw)(StdGpuPrimitive primitive, const StdGpuDrawState* pState, void* pNativeTex, const StdTLVertex* aVerts, size_t numVerts, const uint16_t* aIndices, size_t numIndices);

    // Textures
    void* (*pfCreateTexture)(uint32_t width, uint32_t height, uint32_t numMipLevels, StdGpuTexLayout layout);
    void (*pfUploadTextureLevel)(void* pNativeTex, uint32_t level, const uint8_t* pPixels, size_t rowSize); // 4 bytes per pixel
    void (*pfFinishTextureUpload)(void* pNativeTex);
    void (*pfDestroyTexture)(void* pNativeTex);
} StdGpuBackend;

#ifdef J3D_RENDERER_VULKAN
extern const StdGpuBackend stdVk_g_backend;
#endif
#ifdef J3D_RENDERER_GLES2
extern const StdGpuBackend stdGl_g_backend;
#endif

// Backends
size_t stdGpu_GetNumBackends(void);
const StdGpuBackend* J3DAPI stdGpu_GetBackend(size_t idx);
const StdGpuBackend* stdGpu_GetCurBackend(void);
void J3DAPI stdGpu_SelectBackend(const StdGpuBackend* pBackend); // Backend of the opened display device
#ifndef _WIN32
// Creates the game window at startup for the backend of the display device saved in the settings
void J3DAPI stdGpu_SetPreferredDriver(const char* pDriverName);
#endif

// Selected backend
const char* stdGpu_GetApiName(void);
bool stdGpu_IsStarted(void);
const char* stdGpu_GetDeviceName(void);
size_t stdGpu_GetVideoMemory(void);
uint32_t stdGpu_GetMaxTextureSize(void);
bool stdGpu_HasAnisotropy(void);

// Creates the back buffer in the selected backend. When the textures and the game window belong to another backend,
// they're moved to the selected one first.
int J3DAPI stdGpu_CreateBackBuffer(HWND hwnd, uint32_t width, uint32_t height);
void stdGpu_DestroyBackBuffer(void);
bool stdGpu_HasBackBuffer(void);
void J3DAPI stdGpu_SetNoVSync(bool bNoVSync);
int stdGpu_Present(void);
float stdGpu_GetPresentScale(void);
uint8_t* stdGpu_GetShadowPixels(void);
#ifdef _WIN32
HDC stdGpu_GetShadowDC(void);
#endif
void stdGpu_SyncShadowForRead(void);
void stdGpu_MarkShadowDirty(void);
void J3DAPI stdGpu_ClearColor(uint32_t xrgb, const StdRect* pRect);
void stdGpu_ClearDepth(void);

int stdGpu_Open3D(void);
void stdGpu_Close3D(void);
void J3DAPI stdGpu_Draw(StdGpuPrimitive primitive, const StdGpuDrawState* pState, const StdTLVertex* aVerts, size_t numVerts, const uint16_t* aIndices, size_t numIndices);

tStd3DTexture J3DAPI stdGpu_CreateTexture(uint32_t width, uint32_t height, uint32_t numMipLevels, StdGpuTexLayout layout);
void J3DAPI stdGpu_UploadTextureLevel(tStd3DTexture pTex, uint32_t level, const uint8_t* pPixels, size_t rowSize);
void J3DAPI stdGpu_FinishTextureUpload(tStd3DTexture pTex);
void J3DAPI stdGpu_DestroyTexture(tStd3DTexture pTex);
uint32_t J3DAPI stdGpu_GetTextureMipLevels(tStd3DTexture pTex);

J3D_EXTERN_C_END
#endif // STD_GPU_STDGPU_H
