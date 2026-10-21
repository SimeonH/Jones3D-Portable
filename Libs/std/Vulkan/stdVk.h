#ifndef STD_VULKAN_STDVK_H
#define STD_VULKAN_STDVK_H
#include <j3dcore/j3d.h>
#include <std/types.h>
#include <std/Gpu/stdGpu.h>

#include <volk.h>

J3D_EXTERN_C_START

// Shared Vulkan context of the Vulkan renderer backend (stdDisplay + std3D).
//
// The game's back buffer is an offscreen color image (stdVk_g.colorImage) with a depth image, rendered at the
// game resolution and blitted (scaled) to the window swapchain on present.
// CPU access to the back buffer (locks, fills, GDI DCs on Windows) goes through a 32-bit XRGB8888 mirror (a DIB section on Windows)
// of the color image (stdVk_g.pShadowPixels): it is read back from the GPU when the GPU holds newer content
// and uploaded again before the next GPU operation after the CPU wrote to it.

#define STDVK_MAX_FRAMES 2

typedef struct sStdVkAlloc
{
    VkDeviceMemory memory;
    VkDeviceSize offset;
    VkDeviceSize size;
    struct sStdVkBlock* pBlock; // NULL for dedicated allocation
} StdVkAlloc;

typedef struct sStdVkBuffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    uint8_t* pMapped;
} StdVkBuffer;

struct sStdVkTexture
{
    VkImage image;
    VkImageView view;
    StdVkAlloc alloc;
    uint32_t width;
    uint32_t height;
    uint32_t numMipLevels;
    VkDescriptorSet descSet; // sampled image, set 1 of the std3D pipeline layout
    VkDescriptorPool descPool;
    struct sStdVkTexture* pNextDeleted;
};
typedef struct sStdVkTexture StdVkTexture;

typedef struct sStdVkFrame
{
    VkCommandBuffer cmd;
    VkFence fence;
    VkSemaphore acquireSemaphore;
    bool bSubmitted;
    StdVkBuffer ring;             // host-visible vertex/index data of the frame
    VkDeviceSize ringOffset;
    StdVkTexture* pDeletedTextures; // textures freed while this frame's commands may still use them
} StdVkFrame;

typedef struct sStdVkContext
{
    bool bInstance;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties deviceProps;
    VkPhysicalDeviceMemoryProperties memProps;
    VkDevice device;
    uint32_t queueFamily;
    VkQueue queue;
    VkCommandPool cmdPool;
    float maxAnisotropy; // 0 if not supported
    VkDescriptorSetLayout samplerSetLayout; // set 0: sampler
    VkDescriptorSetLayout textureSetLayout; // set 1: sampled image
    VkDescriptorPool aTexturePools[64];
    size_t numTexturePools;
    size_t numLiveTextures;

    // Window & swapchain
    HWND hwnd;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkFormat swapchainFormat;
    VkExtent2D swapchainExtent;
    uint32_t numSwapchainImages;
    VkImage aSwapchainImages[8];
    VkSemaphore aPresentSemaphores[8];
    bool bNoVSync;
    bool bSwapchainDirty;
    float presentScale; // scale of the back buffer in the window on last present (swapchain pixels per back buffer pixel)

    // Back buffer
    uint32_t width;
    uint32_t height;
    VkImage colorImage;
    VkImageView colorView;
    StdVkAlloc colorAlloc;
    VkImageLayout colorLayout;
    VkFormat depthFormat;
    VkImage depthImage;
    VkImageView depthView;
    StdVkAlloc depthAlloc;
    VkImageLayout depthLayout;

    // CPU mirror of the back buffer
#ifdef _WIN32
    HDC hShadowDC;
    HBITMAP hShadowBitmap;
    HGDIOBJ hShadowPrevBitmap;
#endif
    uint8_t* pShadowPixels;
    StdVkBuffer shadowStaging; // upload & readback of the mirror
    bool bShadowDirty;         // CPU wrote to the mirror, upload before next GPU op
    bool bGpuNewer;            // GPU rendered to the color image since last readback

    // Frames
    StdVkFrame aFrames[STDVK_MAX_FRAMES];
    uint32_t frameIdx;
    uint32_t cmdSerial; // incremented when a new command buffer starts recording, bound state must be re-set
    bool bRecording;
    bool bRendering;

    // Texture uploads
    VkCommandBuffer uploadCmd;
    VkFence uploadFence;
    StdVkBuffer uploadStaging;
    VkDeviceSize uploadOffset;
    bool bUploadRecording;
    bool bUploadPending;
} StdVkContext;

extern StdVkContext stdVk_g;

// Context
int stdVk_Startup(void);   // Creates instance & device, returns 1 on success
void stdVk_Shutdown(void);
const char* J3DAPI stdVk_ResultString(VkResult res);

// Back buffer & swapchain
int J3DAPI stdVk_CreateBackBuffer(HWND hwnd, uint32_t width, uint32_t height);
void stdVk_DestroyBackBuffer(void);
void J3DAPI stdVk_SetNoVSync(bool bNoVSync);
int stdVk_Present(void);

// CPU mirror sync
void stdVk_SyncShadowForRead(void);  // Reads back the color image if the GPU has newer content
void stdVk_MarkShadowDirty(void);    // CPU modified the mirror
void J3DAPI stdVk_ClearColor(uint32_t xrgb, const StdRect* pRect);

// Frame recording
VkCommandBuffer stdVk_BeginCommands(void);   // Begins frame recording if needed, returns current command buffer
VkCommandBuffer stdVk_BeginRendering(void);  // Same as above, inside dynamic rendering of color & depth
void stdVk_EndRendering(void);
void stdVk_Flush(void);                      // Submits recorded commands and waits for completion
void* J3DAPI stdVk_AllocFrameData(size_t size, size_t align, VkBuffer* pBuffer, VkDeviceSize* pOffset);
void J3DAPI stdVk_ClearDepth(void);

// Memory & resources
int J3DAPI stdVk_Alloc(VkMemoryRequirements* pReq, VkMemoryPropertyFlags props, StdVkAlloc* pAlloc);
void J3DAPI stdVk_Free(StdVkAlloc* pAlloc);
int J3DAPI stdVk_CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, StdVkBuffer* pBuffer); // host visible & mapped
void J3DAPI stdVk_DestroyBuffer(StdVkBuffer* pBuffer);
VkShaderModule J3DAPI stdVk_CreateShader(const uint32_t* pCode, size_t codeSize);

// Textures
StdVkTexture* J3DAPI stdVk_CreateTexture(uint32_t width, uint32_t height, uint32_t numMipLevels, VkFormat format, const VkComponentMapping* pSwizzle);
void J3DAPI stdVk_UploadTextureLevel(StdVkTexture* pTex, uint32_t level, const uint8_t* pPixels, size_t rowSize); // 4 bytes per pixel
void J3DAPI stdVk_FinishTextureUpload(StdVkTexture* pTex);
void J3DAPI stdVk_DestroyTexture(StdVkTexture* pTex); // deferred until the GPU is done with it
void stdVk_SubmitUploads(void);

// std3D rendering of the stdGpu backend (stdVk3D.c)
int stdVk_Open3D(void);
void stdVk_Close3D(void);
void stdVk_Draw(StdGpuPrimitive primitive, const StdGpuDrawState* pState, void* pNativeTex, const StdTLVertex* aVerts, size_t numVerts, const uint16_t* aIndices, size_t numIndices);
void* stdVk_CreateGpuTexture(uint32_t width, uint32_t height, uint32_t numMipLevels, StdGpuTexLayout layout);
void stdVk_UploadGpuTextureLevel(void* pNativeTex, uint32_t level, const uint8_t* pPixels, size_t rowSize);
void stdVk_FinishGpuTextureUpload(void* pNativeTex);
void stdVk_DestroyGpuTexture(void* pNativeTex);

J3D_EXTERN_C_END
#endif // STD_VULKAN_STDVK_H
