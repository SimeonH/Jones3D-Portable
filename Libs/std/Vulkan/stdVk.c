#include "stdVk.h"

#include <std/General/std.h>
#include <std/General/stdMath.h>
#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>

#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#endif

#define STDVK_BLOCK_SIZE        (64u * 1024u * 1024u)
#define STDVK_FRAME_RING_SIZE   (16u * 1024u * 1024u)
#define STDVK_UPLOAD_STAGE_SIZE (16u * 1024u * 1024u)
#define STDVK_TEXTURE_POOL_SIZE 1024u

typedef struct sStdVkRange
{
    VkDeviceSize offset;
    VkDeviceSize size;
} StdVkRange;

// Device memory block sub-allocated with a sorted first-fit free list
typedef struct sStdVkBlock
{
    VkDeviceMemory memory;
    uint32_t typeIndex;
    size_t numFree;
    size_t capFree;
    StdVkRange* aFree;
    struct sStdVkBlock* pNext;
} StdVkBlock;

StdVkContext stdVk_g = { 0 };

static StdVkBlock* stdVk_pBlocks = NULL;

// Platform: Win32 window or SDL_Window (HWND holds the SDL_Window* on non-Windows systems)

static void stdVk_GetWindowSize(uint32_t* pWidth, uint32_t* pHeight)
{
#ifdef _WIN32
    RECT rect;
    GetClientRect(stdVk_g.hwnd, &rect);
    *pWidth  = (uint32_t)(rect.right - rect.left);
    *pHeight = (uint32_t)(rect.bottom - rect.top);
#else
    int width = 0, height = 0;
    SDL_GetWindowSizeInPixels((SDL_Window*)stdVk_g.hwnd, &width, &height);
    *pWidth  = (uint32_t)width;
    *pHeight = (uint32_t)height;
#endif
}

static bool stdVk_HasPresentationSupport(VkPhysicalDevice device, uint32_t queueFamily)
{
#ifdef _WIN32
    return vkGetPhysicalDeviceWin32PresentationSupportKHR(device, queueFamily) != VK_FALSE;
#else
    return SDL_Vulkan_GetPresentationSupport(stdVk_g.instance, device, queueFamily);
#endif
}

static VkResult stdVk_CreateWindowSurface(void)
{
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR surfInfo = { 0 };
    surfInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfInfo.hinstance = (HINSTANCE)GetWindowLongPtrA(stdVk_g.hwnd, GWLP_HINSTANCE);
    surfInfo.hwnd      = stdVk_g.hwnd;
    return vkCreateWin32SurfaceKHR(stdVk_g.instance, &surfInfo, NULL, &stdVk_g.surface);
#else
    if ( !SDL_Vulkan_CreateSurface((SDL_Window*)stdVk_g.hwnd, stdVk_g.instance, NULL, &stdVk_g.surface) )
    {
        STDLOG_ERROR("SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
#endif
}

// Finishes pending GDI drawing to the back buffer mirror
static void stdVk_FlushGdi(void)
{
#ifdef _WIN32
    stdVk_FlushGdi();
#endif
}

static void stdVk_DestroySwapchain(void);
static int stdVk_CreateSwapchain(void);
static void stdVk_ProcessDeletedTextures(StdVkTexture** ppList);
static void stdVk_UploadShadow(VkCommandBuffer cmd);

#define STDVK_CHECK(expr, msg) \
    do { \
        VkResult res_ = (expr); \
        if ( res_ != VK_SUCCESS ) { \
            STDLOG_ERROR("Vulkan error %s: %s\n", stdVk_ResultString(res_), msg); \
            return 0; \
        } \
    } while ( 0 )

const char* J3DAPI stdVk_ResultString(VkResult res)
{
    switch ( res )
    {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        default: return "Unknown VkResult";
    }
}

// Context

static int stdVk_FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred)
{
    // First try with the preferred flags, then with only the required ones
    for ( int pass = 0; pass < 2; ++pass )
    {
        VkMemoryPropertyFlags flags = pass == 0 ? required | preferred : required;
        for ( uint32_t i = 0; i < stdVk_g.memProps.memoryTypeCount; ++i )
        {
            if ( (typeBits & (1u << i)) && (stdVk_g.memProps.memoryTypes[i].propertyFlags & flags) == flags ) {
                return (int)i;
            }
        }
    }

    return -1;
}

static int stdVk_PickPhysicalDevice(void)
{
    uint32_t numDevices = 0;
    STDVK_CHECK(vkEnumeratePhysicalDevices(stdVk_g.instance, &numDevices, NULL), "enumerating physical devices");
    if ( numDevices == 0 )
    {
        STDLOG_ERROR("No Vulkan devices found.\n");
        return 0;
    }

    VkPhysicalDevice aDevices[16];
    numDevices = numDevices > STD_ARRAYLEN(aDevices) ? (uint32_t)STD_ARRAYLEN(aDevices) : numDevices;
    STDVK_CHECK(vkEnumeratePhysicalDevices(stdVk_g.instance, &numDevices, aDevices), "enumerating physical devices");

    int bestScore = -1;
    for ( uint32_t i = 0; i < numDevices; ++i )
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(aDevices[i], &props);
        if ( props.apiVersion < VK_API_VERSION_1_3 )
        {
            STDLOG_STATUS("Skipping Vulkan device %s, Vulkan 1.3 is required.\n", props.deviceName);
            continue;
        }

        uint32_t numFamilies = 0;
        VkQueueFamilyProperties aFamilies[16];
        vkGetPhysicalDeviceQueueFamilyProperties(aDevices[i], &numFamilies, NULL);
        numFamilies = numFamilies > STD_ARRAYLEN(aFamilies) ? (uint32_t)STD_ARRAYLEN(aFamilies) : numFamilies;
        vkGetPhysicalDeviceQueueFamilyProperties(aDevices[i], &numFamilies, aFamilies);

        for ( uint32_t qf = 0; qf < numFamilies; ++qf )
        {
            if ( (aFamilies[qf].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 ) {
                continue;
            }

            if ( !stdVk_HasPresentationSupport(aDevices[i], qf) ) {
                continue;
            }

            int score = 1;
            if ( props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
                score = 3;
            }
            else if ( props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) {
                score = 2;
            }

            if ( score > bestScore )
            {
                bestScore                = score;
                stdVk_g.physicalDevice   = aDevices[i];
                stdVk_g.queueFamily      = qf;
            }
            break;
        }
    }

    if ( bestScore < 0 )
    {
        STDLOG_ERROR("No suitable Vulkan device found.\n");
        return 0;
    }

    vkGetPhysicalDeviceProperties(stdVk_g.physicalDevice, &stdVk_g.deviceProps);
    vkGetPhysicalDeviceMemoryProperties(stdVk_g.physicalDevice, &stdVk_g.memProps);
    STDLOG_STATUS("Using Vulkan device %s (API %u.%u.%u).\n",
        stdVk_g.deviceProps.deviceName,
        VK_API_VERSION_MAJOR(stdVk_g.deviceProps.apiVersion),
        VK_API_VERSION_MINOR(stdVk_g.deviceProps.apiVersion),
        VK_API_VERSION_PATCH(stdVk_g.deviceProps.apiVersion)
    );
    return 1;
}

static int stdVk_CreateDevice(void)
{
    VkPhysicalDeviceFeatures supported;
    vkGetPhysicalDeviceFeatures(stdVk_g.physicalDevice, &supported);

    VkPhysicalDeviceFeatures features = { 0 };
    if ( supported.samplerAnisotropy )
    {
        features.samplerAnisotropy = VK_TRUE;
        stdVk_g.maxAnisotropy = stdVk_g.deviceProps.limits.maxSamplerAnisotropy;
    }

    VkPhysicalDeviceVulkan13Features features13 = { 0 };
    features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = { 0 };
    queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = stdVk_g.queueFamily;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &priority;

    const char* aExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo info = { 0 };
    info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.pNext                   = &features13;
    info.queueCreateInfoCount    = 1;
    info.pQueueCreateInfos       = &queueInfo;
    info.enabledExtensionCount   = (uint32_t)STD_ARRAYLEN(aExtensions);
    info.ppEnabledExtensionNames = aExtensions;
    info.pEnabledFeatures        = &features;
    STDVK_CHECK(vkCreateDevice(stdVk_g.physicalDevice, &info, NULL, &stdVk_g.device), "creating device");

    volkLoadDevice(stdVk_g.device);
    vkGetDeviceQueue(stdVk_g.device, stdVk_g.queueFamily, 0, &stdVk_g.queue);
    return 1;
}

static int stdVk_CreateDescriptorLayouts(void)
{
    VkDescriptorSetLayoutBinding binding = { 0 };
    binding.binding         = 0;
    binding.descriptorCount = 1;
    binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo info = { 0 };
    info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings    = &binding;

    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    STDVK_CHECK(vkCreateDescriptorSetLayout(stdVk_g.device, &info, NULL, &stdVk_g.samplerSetLayout), "creating sampler set layout");

    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    STDVK_CHECK(vkCreateDescriptorSetLayout(stdVk_g.device, &info, NULL, &stdVk_g.textureSetLayout), "creating texture set layout");
    return 1;
}

static int stdVk_CreateFrames(void)
{
    VkCommandPoolCreateInfo poolInfo = { 0 };
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = stdVk_g.queueFamily;
    STDVK_CHECK(vkCreateCommandPool(stdVk_g.device, &poolInfo, NULL, &stdVk_g.cmdPool), "creating command pool");

    VkCommandBuffer aCmds[STDVK_MAX_FRAMES + 1];
    VkCommandBufferAllocateInfo allocInfo = { 0 };
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = stdVk_g.cmdPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = STDVK_MAX_FRAMES + 1;
    STDVK_CHECK(vkAllocateCommandBuffers(stdVk_g.device, &allocInfo, aCmds), "allocating command buffers");

    VkFenceCreateInfo fenceInfo = { 0 };
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkSemaphoreCreateInfo semInfo = { 0 };
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for ( size_t i = 0; i < STDVK_MAX_FRAMES; ++i )
    {
        StdVkFrame* pFrame = &stdVk_g.aFrames[i];
        pFrame->cmd = aCmds[i];
        STDVK_CHECK(vkCreateFence(stdVk_g.device, &fenceInfo, NULL, &pFrame->fence), "creating frame fence");
        STDVK_CHECK(vkCreateSemaphore(stdVk_g.device, &semInfo, NULL, &pFrame->acquireSemaphore), "creating semaphore");
        if ( !stdVk_CreateBuffer(STDVK_FRAME_RING_SIZE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &pFrame->ring) ) {
            return 0;
        }
    }

    stdVk_g.uploadCmd = aCmds[STDVK_MAX_FRAMES];
    STDVK_CHECK(vkCreateFence(stdVk_g.device, &fenceInfo, NULL, &stdVk_g.uploadFence), "creating upload fence");
    return stdVk_CreateBuffer(STDVK_UPLOAD_STAGE_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &stdVk_g.uploadStaging);
}

int stdVk_Startup(void)
{
    if ( stdVk_g.device ) {
        return 1;
    }

    if ( !stdVk_g.bInstance )
    {
#ifndef _WIN32
        // SDL needs its Vulkan library loaded for the instance extensions and surfaces
        if ( !SDL_Vulkan_LoadLibrary(NULL) )
        {
            STDLOG_ERROR("SDL_Vulkan_LoadLibrary failed: %s\n", SDL_GetError());
            return 0;
        }
#endif

        VkResult res = volkInitialize();
        if ( res != VK_SUCCESS )
        {
            STDLOG_ERROR("Vulkan loader (vulkan-1.dll) not found.\n");
            return 0;
        }

        if ( volkGetInstanceVersion() < VK_API_VERSION_1_3 )
        {
            STDLOG_ERROR("Vulkan 1.3 is required, the installed loader supports only %u.%u.\n",
                VK_API_VERSION_MAJOR(volkGetInstanceVersion()), VK_API_VERSION_MINOR(volkGetInstanceVersion()));
            return 0;
        }

        VkApplicationInfo appInfo = { 0 };
        appInfo.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "OpenJones3D";
        appInfo.pEngineName      = "Jones3D";
        appInfo.apiVersion       = VK_API_VERSION_1_3;

#ifdef _WIN32
        const char* aExtensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
        uint32_t numExtensions = (uint32_t)STD_ARRAYLEN(aExtensions);
        const char* const* ppExtensions = aExtensions;
#else
        uint32_t numExtensions = 0;
        const char* const* ppExtensions = SDL_Vulkan_GetInstanceExtensions(&numExtensions);
        if ( !ppExtensions )
        {
            STDLOG_ERROR("SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
            return 0;
        }
#endif

        VkInstanceCreateInfo info = { 0 };
        info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        info.pApplicationInfo        = &appInfo;
        info.enabledExtensionCount   = numExtensions;
        info.ppEnabledExtensionNames = ppExtensions;
        STDVK_CHECK(vkCreateInstance(&info, NULL, &stdVk_g.instance), "creating instance");

        volkLoadInstanceOnly(stdVk_g.instance);
        stdVk_g.bInstance = true;
    }

    if ( !stdVk_PickPhysicalDevice() || !stdVk_CreateDevice() || !stdVk_CreateDescriptorLayouts() || !stdVk_CreateFrames() )
    {
        stdVk_Shutdown();
        return 0;
    }

    return 1;
}

void stdVk_Shutdown(void)
{
    if ( stdVk_g.device )
    {
        vkDeviceWaitIdle(stdVk_g.device);
        stdVk_DestroyBackBuffer();

        for ( size_t i = 0; i < STDVK_MAX_FRAMES; ++i ) {
            stdVk_ProcessDeletedTextures(&stdVk_g.aFrames[i].pDeletedTextures);
        }

        // Materials may still hold textures (freed later), so the device is kept alive until all are gone
        if ( stdVk_g.numLiveTextures ) {
            return;
        }

        for ( size_t i = 0; i < STDVK_MAX_FRAMES; ++i )
        {
            StdVkFrame* pFrame = &stdVk_g.aFrames[i];
            stdVk_DestroyBuffer(&pFrame->ring);
            if ( pFrame->fence ) {
                vkDestroyFence(stdVk_g.device, pFrame->fence, NULL);
            }
            if ( pFrame->acquireSemaphore ) {
                vkDestroySemaphore(stdVk_g.device, pFrame->acquireSemaphore, NULL);
            }
        }

        stdVk_DestroyBuffer(&stdVk_g.uploadStaging);
        if ( stdVk_g.uploadFence ) {
            vkDestroyFence(stdVk_g.device, stdVk_g.uploadFence, NULL);
        }

        if ( stdVk_g.cmdPool ) {
            vkDestroyCommandPool(stdVk_g.device, stdVk_g.cmdPool, NULL);
        }

        for ( size_t i = 0; i < stdVk_g.numTexturePools; ++i ) {
            vkDestroyDescriptorPool(stdVk_g.device, stdVk_g.aTexturePools[i], NULL);
        }

        if ( stdVk_g.samplerSetLayout ) {
            vkDestroyDescriptorSetLayout(stdVk_g.device, stdVk_g.samplerSetLayout, NULL);
        }
        if ( stdVk_g.textureSetLayout ) {
            vkDestroyDescriptorSetLayout(stdVk_g.device, stdVk_g.textureSetLayout, NULL);
        }

        while ( stdVk_pBlocks )
        {
            StdVkBlock* pNext = stdVk_pBlocks->pNext;
            vkFreeMemory(stdVk_g.device, stdVk_pBlocks->memory, NULL);
            stdMemory_Free(stdVk_pBlocks->aFree);
            stdMemory_Free(stdVk_pBlocks);
            stdVk_pBlocks = pNext;
        }

        vkDestroyDevice(stdVk_g.device, NULL);
    }

    // The instance is kept for the lifetime of the process
    VkInstance instance = stdVk_g.instance;
    bool bInstance      = stdVk_g.bInstance;
    memset(&stdVk_g, 0, sizeof(stdVk_g));
    stdVk_g.instance  = instance;
    stdVk_g.bInstance = bInstance;
}

// Memory

static void stdVk_InsertFreeRange(StdVkBlock* pBlock, size_t idx, VkDeviceSize offset, VkDeviceSize size)
{
    if ( pBlock->numFree == pBlock->capFree )
    {
        size_t newCap = pBlock->capFree ? pBlock->capFree * 2 : 16;
        pBlock->aFree = (StdVkRange*)STDREALLOC(pBlock->aFree, newCap * sizeof(StdVkRange));
        pBlock->capFree = newCap;
    }

    memmove(&pBlock->aFree[idx + 1], &pBlock->aFree[idx], (pBlock->numFree - idx) * sizeof(StdVkRange));
    pBlock->aFree[idx].offset = offset;
    pBlock->aFree[idx].size   = size;
    ++pBlock->numFree;
}

static void stdVk_RemoveFreeRange(StdVkBlock* pBlock, size_t idx)
{
    memmove(&pBlock->aFree[idx], &pBlock->aFree[idx + 1], (pBlock->numFree - idx - 1) * sizeof(StdVkRange));
    --pBlock->numFree;
}

static bool stdVk_BlockAlloc(StdVkBlock* pBlock, VkDeviceSize size, VkDeviceSize align, VkDeviceSize* pOffset)
{
    for ( size_t i = 0; i < pBlock->numFree; ++i )
    {
        StdVkRange range   = pBlock->aFree[i];
        VkDeviceSize start = (range.offset + align - 1) & ~(align - 1);
        VkDeviceSize end   = range.offset + range.size;
        if ( start + size > end ) {
            continue;
        }

        stdVk_RemoveFreeRange(pBlock, i);
        if ( start + size < end ) {
            stdVk_InsertFreeRange(pBlock, i, start + size, end - (start + size));
        }
        if ( start > range.offset ) {
            stdVk_InsertFreeRange(pBlock, i, range.offset, start - range.offset);
        }

        *pOffset = start;
        return true;
    }

    return false;
}

static void stdVk_BlockFree(StdVkBlock* pBlock, VkDeviceSize offset, VkDeviceSize size)
{
    size_t idx = 0;
    while ( idx < pBlock->numFree && pBlock->aFree[idx].offset < offset ) {
        ++idx;
    }

    stdVk_InsertFreeRange(pBlock, idx, offset, size);

    // Merge with the next and previous range
    if ( idx + 1 < pBlock->numFree && pBlock->aFree[idx].offset + pBlock->aFree[idx].size == pBlock->aFree[idx + 1].offset )
    {
        pBlock->aFree[idx].size += pBlock->aFree[idx + 1].size;
        stdVk_RemoveFreeRange(pBlock, idx + 1);
    }

    if ( idx > 0 && pBlock->aFree[idx - 1].offset + pBlock->aFree[idx - 1].size == pBlock->aFree[idx].offset )
    {
        pBlock->aFree[idx - 1].size += pBlock->aFree[idx].size;
        stdVk_RemoveFreeRange(pBlock, idx);
    }
}

int J3DAPI stdVk_Alloc(VkMemoryRequirements* pReq, VkMemoryPropertyFlags props, StdVkAlloc* pAlloc)
{
    memset(pAlloc, 0, sizeof(*pAlloc));

    int typeIndex = stdVk_FindMemoryType(pReq->memoryTypeBits, props, 0);
    if ( typeIndex < 0 )
    {
        STDLOG_ERROR("No suitable Vulkan memory type found.\n");
        return 0;
    }

    VkMemoryAllocateInfo info = { 0 };
    info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.memoryTypeIndex = (uint32_t)typeIndex;

    // Big resources get dedicated memory
    if ( pReq->size > STDVK_BLOCK_SIZE / 4 )
    {
        info.allocationSize = pReq->size;
        STDVK_CHECK(vkAllocateMemory(stdVk_g.device, &info, NULL, &pAlloc->memory), "allocating memory");
        pAlloc->size = pReq->size;
        return 1;
    }

    for ( StdVkBlock* pBlock = stdVk_pBlocks; pBlock; pBlock = pBlock->pNext )
    {
        if ( pBlock->typeIndex == (uint32_t)typeIndex && stdVk_BlockAlloc(pBlock, pReq->size, pReq->alignment, &pAlloc->offset) )
        {
            pAlloc->memory = pBlock->memory;
            pAlloc->size   = pReq->size;
            pAlloc->pBlock = pBlock;
            return 1;
        }
    }

    StdVkBlock* pBlock = (StdVkBlock*)STDMALLOC(sizeof(StdVkBlock));
    memset(pBlock, 0, sizeof(*pBlock));
    pBlock->typeIndex = (uint32_t)typeIndex;

    info.allocationSize = STDVK_BLOCK_SIZE;
    VkResult res = vkAllocateMemory(stdVk_g.device, &info, NULL, &pBlock->memory);
    if ( res != VK_SUCCESS )
    {
        stdMemory_Free(pBlock);
        STDLOG_ERROR("Vulkan error %s allocating memory block.\n", stdVk_ResultString(res));
        return 0;
    }

    stdVk_InsertFreeRange(pBlock, 0, 0, STDVK_BLOCK_SIZE);
    pBlock->pNext = stdVk_pBlocks;
    stdVk_pBlocks = pBlock;

    stdVk_BlockAlloc(pBlock, pReq->size, pReq->alignment, &pAlloc->offset);
    pAlloc->memory = pBlock->memory;
    pAlloc->size   = pReq->size;
    pAlloc->pBlock = pBlock;
    return 1;
}

void J3DAPI stdVk_Free(StdVkAlloc* pAlloc)
{
    if ( pAlloc->pBlock ) {
        stdVk_BlockFree(pAlloc->pBlock, pAlloc->offset, pAlloc->size);
    }
    else if ( pAlloc->memory ) {
        vkFreeMemory(stdVk_g.device, pAlloc->memory, NULL);
    }

    memset(pAlloc, 0, sizeof(*pAlloc));
}

int J3DAPI stdVk_CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, StdVkBuffer* pBuffer)
{
    memset(pBuffer, 0, sizeof(*pBuffer));

    VkBufferCreateInfo info = { 0 };
    info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size        = size;
    info.usage       = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    STDVK_CHECK(vkCreateBuffer(stdVk_g.device, &info, NULL, &pBuffer->buffer), "creating buffer");

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(stdVk_g.device, pBuffer->buffer, &req);

    // Readback benefits from cached memory, flushes aren't needed with coherent memory
    int typeIndex = stdVk_FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    if ( typeIndex < 0 )
    {
        STDLOG_ERROR("No host visible Vulkan memory type found.\n");
        return 0;
    }

    VkMemoryAllocateInfo allocInfo = { 0 };
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = req.size;
    allocInfo.memoryTypeIndex = (uint32_t)typeIndex;
    STDVK_CHECK(vkAllocateMemory(stdVk_g.device, &allocInfo, NULL, &pBuffer->memory), "allocating buffer memory");
    STDVK_CHECK(vkBindBufferMemory(stdVk_g.device, pBuffer->buffer, pBuffer->memory, 0), "binding buffer memory");
    STDVK_CHECK(vkMapMemory(stdVk_g.device, pBuffer->memory, 0, VK_WHOLE_SIZE, 0, (void**)&pBuffer->pMapped), "mapping buffer memory");

    pBuffer->size = size;
    return 1;
}

void J3DAPI stdVk_DestroyBuffer(StdVkBuffer* pBuffer)
{
    if ( pBuffer->buffer ) {
        vkDestroyBuffer(stdVk_g.device, pBuffer->buffer, NULL);
    }

    if ( pBuffer->memory ) {
        vkFreeMemory(stdVk_g.device, pBuffer->memory, NULL);
    }

    memset(pBuffer, 0, sizeof(*pBuffer));
}

VkShaderModule J3DAPI stdVk_CreateShader(const uint32_t* pCode, size_t codeSize)
{
    VkShaderModuleCreateInfo info = { 0 };
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = codeSize;
    info.pCode    = pCode;

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(stdVk_g.device, &info, NULL, &module);
    if ( res != VK_SUCCESS ) {
        STDLOG_ERROR("Vulkan error %s creating shader module.\n", stdVk_ResultString(res));
    }

    return module;
}

static int stdVk_CreateImage(uint32_t width, uint32_t height, uint32_t numMips, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, const VkComponentMapping* pSwizzle, VkImage* pImage, VkImageView* pView, StdVkAlloc* pAlloc)
{
    VkImageCreateInfo info = { 0 };
    info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType     = VK_IMAGE_TYPE_2D;
    info.format        = format;
    info.extent.width  = width;
    info.extent.height = height;
    info.extent.depth  = 1;
    info.mipLevels     = numMips;
    info.arrayLayers   = 1;
    info.samples       = VK_SAMPLE_COUNT_1_BIT;
    info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    info.usage         = usage;
    info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    STDVK_CHECK(vkCreateImage(stdVk_g.device, &info, NULL, pImage), "creating image");

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(stdVk_g.device, *pImage, &req);
    if ( !stdVk_Alloc(&req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pAlloc) ) {
        return 0;
    }

    STDVK_CHECK(vkBindImageMemory(stdVk_g.device, *pImage, pAlloc->memory, pAlloc->offset), "binding image memory");

    VkImageViewCreateInfo viewInfo = { 0 };
    viewInfo.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                       = *pImage;
    viewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                      = format;
    viewInfo.subresourceRange.aspectMask = aspect;
    viewInfo.subresourceRange.levelCount = numMips;
    viewInfo.subresourceRange.layerCount = 1;
    if ( pSwizzle ) {
        viewInfo.components = *pSwizzle;
    }

    STDVK_CHECK(vkCreateImageView(stdVk_g.device, &viewInfo, NULL, pView), "creating image view");
    return 1;
}

static void stdVk_ImageBarrier(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, uint32_t numMips,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess)
{
    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask               = srcAccess;
    barrier.dstAccessMask               = dstAccess;
    barrier.oldLayout                   = oldLayout;
    barrier.newLayout                   = newLayout;
    barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                       = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.levelCount = numMips;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

// Transitions the back buffer color image, synchronizing with any previous use
static void stdVk_TransitionColor(VkCommandBuffer cmd, VkImageLayout newLayout, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess)
{
    stdVk_ImageBarrier(cmd, stdVk_g.colorImage, VK_IMAGE_ASPECT_COLOR_BIT, 1, stdVk_g.colorLayout, newLayout,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT, dstStage, dstAccess);
    stdVk_g.colorLayout = newLayout;
}

// Back buffer & swapchain

#ifdef _WIN32

// The mirror is a DIB section, so GDI can draw to the back buffer through its DC

static void stdVk_DestroyShadow(void)
{
    if ( stdVk_g.hShadowDC )
    {
        SelectObject(stdVk_g.hShadowDC, stdVk_g.hShadowPrevBitmap);
        DeleteDC(stdVk_g.hShadowDC);
    }

    if ( stdVk_g.hShadowBitmap ) {
        DeleteObject(stdVk_g.hShadowBitmap);
    }

    stdVk_g.hShadowDC         = NULL;
    stdVk_g.hShadowBitmap     = NULL;
    stdVk_g.hShadowPrevBitmap = NULL;
    stdVk_g.pShadowPixels     = NULL;
}

static uint8_t* stdVk_AllocShadowPixels(uint32_t width, uint32_t height)
{
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = (LONG)width;
    bmi.bmiHeader.biHeight      = -(LONG)height; // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    stdVk_g.hShadowBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if ( !stdVk_g.hShadowBitmap || !pBits )
    {
        STDLOG_ERROR("Error creating back buffer DIB section.\n");
        return NULL;
    }

    stdVk_g.hShadowDC = CreateCompatibleDC(NULL);
    if ( !stdVk_g.hShadowDC )
    {
        STDLOG_ERROR("Error creating back buffer DC.\n");
        return NULL;
    }

    stdVk_g.hShadowPrevBitmap = SelectObject(stdVk_g.hShadowDC, stdVk_g.hShadowBitmap);
    return (uint8_t*)pBits;
}

#else // !_WIN32

static void stdVk_DestroyShadow(void)
{
    if ( stdVk_g.pShadowPixels ) {
        stdMemory_Free(stdVk_g.pShadowPixels);
    }

    stdVk_g.pShadowPixels = NULL;
}

static uint8_t* stdVk_AllocShadowPixels(uint32_t width, uint32_t height)
{
    return (uint8_t*)STDMALLOC((size_t)width * height * 4);
}

#endif // _WIN32

static int stdVk_CreateShadow(uint32_t width, uint32_t height)
{
    stdVk_g.pShadowPixels = stdVk_AllocShadowPixels(width, height);
    if ( !stdVk_g.pShadowPixels ) {
        return 0;
    }

    memset(stdVk_g.pShadowPixels, 0, (size_t)width * height * 4);
    return stdVk_CreateBuffer((VkDeviceSize)width * height * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &stdVk_g.shadowStaging);
}

static VkFormat stdVk_FindDepthFormat(void)
{
    const VkFormat aFormats[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };
    for ( size_t i = 0; i < STD_ARRAYLEN(aFormats); ++i )
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(stdVk_g.physicalDevice, aFormats[i], &props);
        if ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) {
            return aFormats[i];
        }
    }

    return VK_FORMAT_UNDEFINED;
}

int J3DAPI stdVk_CreateBackBuffer(HWND hwnd, uint32_t width, uint32_t height)
{
    if ( !stdVk_Startup() ) {
        return 0;
    }

    stdVk_DestroyBackBuffer();

    stdVk_g.hwnd   = hwnd;
    stdVk_g.width  = width;
    stdVk_g.height = height;

    if ( !stdVk_CreateImage(width, height, 1, VK_FORMAT_B8G8R8A8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT, NULL, &stdVk_g.colorImage, &stdVk_g.colorView, &stdVk_g.colorAlloc) )
    {
        return 0;
    }

    stdVk_g.depthFormat = stdVk_FindDepthFormat();
    if ( stdVk_g.depthFormat == VK_FORMAT_UNDEFINED )
    {
        STDLOG_ERROR("No Vulkan depth format found.\n");
        return 0;
    }

    if ( !stdVk_CreateImage(width, height, 1, stdVk_g.depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT, NULL, &stdVk_g.depthImage, &stdVk_g.depthView, &stdVk_g.depthAlloc) )
    {
        return 0;
    }

    stdVk_g.colorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    stdVk_g.depthLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if ( !stdVk_CreateShadow(width, height) ) {
        return 0;
    }

    // Start with a black frame
    stdVk_g.bShadowDirty = true;
    stdVk_g.bGpuNewer    = false;

    if ( !stdVk_CreateSwapchain() ) {
        return 0;
    }

    return 1;
}

void stdVk_DestroyBackBuffer(void)
{
    if ( !stdVk_g.device ) {
        return;
    }

    stdVk_Flush();
    vkDeviceWaitIdle(stdVk_g.device);

    for ( size_t i = 0; i < STDVK_MAX_FRAMES; ++i ) {
        stdVk_g.aFrames[i].bSubmitted = false;
    }

    stdVk_DestroySwapchain();
    stdVk_DestroyShadow();
    stdVk_DestroyBuffer(&stdVk_g.shadowStaging);

    if ( stdVk_g.colorView ) {
        vkDestroyImageView(stdVk_g.device, stdVk_g.colorView, NULL);
    }
    if ( stdVk_g.colorImage ) {
        vkDestroyImage(stdVk_g.device, stdVk_g.colorImage, NULL);
    }
    stdVk_Free(&stdVk_g.colorAlloc);

    if ( stdVk_g.depthView ) {
        vkDestroyImageView(stdVk_g.device, stdVk_g.depthView, NULL);
    }
    if ( stdVk_g.depthImage ) {
        vkDestroyImage(stdVk_g.device, stdVk_g.depthImage, NULL);
    }
    stdVk_Free(&stdVk_g.depthAlloc);

    if ( stdVk_g.surface ) {
        vkDestroySurfaceKHR(stdVk_g.instance, stdVk_g.surface, NULL);
    }

    stdVk_g.surface    = VK_NULL_HANDLE;
    stdVk_g.colorImage = VK_NULL_HANDLE;
    stdVk_g.colorView  = VK_NULL_HANDLE;
    stdVk_g.depthImage = VK_NULL_HANDLE;
    stdVk_g.depthView  = VK_NULL_HANDLE;
    stdVk_g.hwnd       = NULL;
    stdVk_g.width      = 0;
    stdVk_g.height     = 0;
}

void J3DAPI stdVk_SetNoVSync(bool bNoVSync)
{
    if ( stdVk_g.bNoVSync != bNoVSync )
    {
        stdVk_g.bNoVSync        = bNoVSync;
        stdVk_g.bSwapchainDirty = true;
    }
}

static void stdVk_DestroySwapchain(void)
{
    for ( uint32_t i = 0; i < stdVk_g.numSwapchainImages; ++i )
    {
        if ( stdVk_g.aPresentSemaphores[i] ) {
            vkDestroySemaphore(stdVk_g.device, stdVk_g.aPresentSemaphores[i], NULL);
        }
        stdVk_g.aPresentSemaphores[i] = VK_NULL_HANDLE;
    }

    if ( stdVk_g.swapchain ) {
        vkDestroySwapchainKHR(stdVk_g.device, stdVk_g.swapchain, NULL);
    }

    stdVk_g.swapchain          = VK_NULL_HANDLE;
    stdVk_g.numSwapchainImages = 0;
}

static VkPresentModeKHR stdVk_ChoosePresentMode(void)
{
    if ( !stdVk_g.bNoVSync ) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkPresentModeKHR aModes[16];
    uint32_t numModes = (uint32_t)STD_ARRAYLEN(aModes);
    vkGetPhysicalDeviceSurfacePresentModesKHR(stdVk_g.physicalDevice, stdVk_g.surface, &numModes, aModes);

    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
    for ( uint32_t i = 0; i < numModes; ++i )
    {
        if ( aModes[i] == VK_PRESENT_MODE_MAILBOX_KHR ) {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
        if ( aModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR ) {
            mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    return mode;
}

static int stdVk_CreateSwapchain(void)
{
    if ( !stdVk_g.surface )
    {
        STDVK_CHECK(stdVk_CreateWindowSurface(), "creating window surface");

        VkBool32 bSupported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(stdVk_g.physicalDevice, stdVk_g.queueFamily, stdVk_g.surface, &bSupported);
        if ( !bSupported )
        {
            STDLOG_ERROR("Vulkan device can't present to the game window.\n");
            return 0;
        }
    }

    VkSurfaceCapabilitiesKHR caps;
    STDVK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(stdVk_g.physicalDevice, stdVk_g.surface, &caps), "getting surface capabilities");

    VkExtent2D extent = caps.currentExtent;
    if ( extent.width == UINT32_MAX )
    {
        uint32_t width, height;
        stdVk_GetWindowSize(&width, &height);
        extent.width  = STDMATH_CLAMP(width, caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = STDMATH_CLAMP(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    if ( extent.width == 0 || extent.height == 0 )
    {
        // Minimized, try again on next present
        stdVk_g.bSwapchainDirty = true;
        return 1;
    }

    if ( (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0 )
    {
        STDLOG_ERROR("Vulkan swapchain doesn't support transfer destination usage.\n");
        return 0;
    }

    VkSurfaceFormatKHR aFormats[32];
    uint32_t numFormats = (uint32_t)STD_ARRAYLEN(aFormats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(stdVk_g.physicalDevice, stdVk_g.surface, &numFormats, aFormats);
    if ( numFormats == 0 )
    {
        STDLOG_ERROR("No Vulkan surface formats.\n");
        return 0;
    }

    VkSurfaceFormatKHR format = aFormats[0];
    for ( uint32_t i = 0; i < numFormats; ++i )
    {
        if ( aFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM || aFormats[i].format == VK_FORMAT_R8G8B8A8_UNORM )
        {
            format = aFormats[i];
            break;
        }
    }

    uint32_t numImages = caps.minImageCount + 1;
    if ( caps.maxImageCount && numImages > caps.maxImageCount ) {
        numImages = caps.maxImageCount;
    }

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if ( (caps.supportedCompositeAlpha & compositeAlpha) == 0 ) {
        compositeAlpha = (VkCompositeAlphaFlagBitsKHR)(caps.supportedCompositeAlpha & (~caps.supportedCompositeAlpha + 1)); // lowest bit
    }

    VkSwapchainKHR oldSwapchain = stdVk_g.swapchain;

    VkSwapchainCreateInfoKHR info = { 0 };
    info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface          = stdVk_g.surface;
    info.minImageCount    = numImages;
    info.imageFormat      = format.format;
    info.imageColorSpace  = format.colorSpace;
    info.imageExtent      = extent;
    info.imageArrayLayers = 1;
    info.imageUsage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform     = caps.currentTransform;
    info.compositeAlpha   = compositeAlpha;
    info.presentMode      = stdVk_ChoosePresentMode();
    info.clipped          = VK_TRUE;
    info.oldSwapchain     = oldSwapchain;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    STDVK_CHECK(vkCreateSwapchainKHR(stdVk_g.device, &info, NULL, &swapchain), "creating swapchain");

    if ( oldSwapchain )
    {
        vkDeviceWaitIdle(stdVk_g.device);
        stdVk_DestroySwapchain();
    }

    stdVk_g.swapchain       = swapchain;
    stdVk_g.swapchainFormat = format.format;
    stdVk_g.swapchainExtent = extent;

    stdVk_g.numSwapchainImages = (uint32_t)STD_ARRAYLEN(stdVk_g.aSwapchainImages);
    STDVK_CHECK(vkGetSwapchainImagesKHR(stdVk_g.device, swapchain, &stdVk_g.numSwapchainImages, stdVk_g.aSwapchainImages), "getting swapchain images");

    VkSemaphoreCreateInfo semInfo = { 0 };
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for ( uint32_t i = 0; i < stdVk_g.numSwapchainImages; ++i ) {
        STDVK_CHECK(vkCreateSemaphore(stdVk_g.device, &semInfo, NULL, &stdVk_g.aPresentSemaphores[i]), "creating semaphore");
    }

    stdVk_g.bSwapchainDirty = false;
    STDLOG_STATUS("Vulkan swapchain %ux%u, %u images, present mode %d.\n", extent.width, extent.height, stdVk_g.numSwapchainImages, info.presentMode);
    return 1;
}

static bool stdVk_IsSwapchainOutdated(void)
{
    if ( stdVk_g.bSwapchainDirty || !stdVk_g.swapchain ) {
        return true;
    }

    // The surface extent is authoritative when the surface defines one, window sizes may change asynchronously (Wayland)
    uint32_t width, height;
    VkSurfaceCapabilitiesKHR caps;
    if ( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(stdVk_g.physicalDevice, stdVk_g.surface, &caps) == VK_SUCCESS && caps.currentExtent.width != UINT32_MAX )
    {
        width  = caps.currentExtent.width;
        height = caps.currentExtent.height;
    }
    else
    {
        stdVk_GetWindowSize(&width, &height);
    }

    return width != stdVk_g.swapchainExtent.width || height != stdVk_g.swapchainExtent.height;
}

// Frame recording

static void stdVk_WaitFrame(StdVkFrame* pFrame)
{
    if ( pFrame->bSubmitted )
    {
        vkWaitForFences(stdVk_g.device, 1, &pFrame->fence, VK_TRUE, UINT64_MAX);
        vkResetFences(stdVk_g.device, 1, &pFrame->fence);
        pFrame->bSubmitted = false;
    }
}

VkCommandBuffer stdVk_BeginCommands(void)
{
    StdVkFrame* pFrame = &stdVk_g.aFrames[stdVk_g.frameIdx];
    if ( !stdVk_g.bRecording )
    {
        stdVk_WaitFrame(pFrame);
        stdVk_ProcessDeletedTextures(&pFrame->pDeletedTextures);
        pFrame->ringOffset = 0;

        VkCommandBufferBeginInfo beginInfo = { 0 };
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkResetCommandBuffer(pFrame->cmd, 0);
        vkBeginCommandBuffer(pFrame->cmd, &beginInfo);

        stdVk_g.bRecording = true;
        ++stdVk_g.cmdSerial;
    }

    if ( stdVk_g.bShadowDirty && stdVk_g.colorImage )
    {
        stdVk_EndRendering();
        stdVk_UploadShadow(pFrame->cmd);
    }

    return pFrame->cmd;
}

VkCommandBuffer stdVk_BeginRendering(void)
{
    VkCommandBuffer cmd = stdVk_BeginCommands();
    if ( stdVk_g.bRendering ) {
        return cmd;
    }

    bool bClearDepth = stdVk_g.depthLayout == VK_IMAGE_LAYOUT_UNDEFINED;
    {
        stdVk_ImageBarrier(cmd, stdVk_g.depthImage, VK_IMAGE_ASPECT_DEPTH_BIT, 1, stdVk_g.depthLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        stdVk_g.depthLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    }

    bool bClearColor = stdVk_g.colorLayout == VK_IMAGE_LAYOUT_UNDEFINED;
    stdVk_TransitionColor(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    VkRenderingAttachmentInfo colorAtt = { 0 };
    colorAtt.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAtt.imageView   = stdVk_g.colorView;
    colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.loadOp      = bClearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAtt.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAtt = { 0 };
    depthAtt.sType                         = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAtt.imageView                     = stdVk_g.depthView;
    depthAtt.imageLayout                   = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAtt.loadOp                        = bClearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAtt.storeOp                       = VK_ATTACHMENT_STORE_OP_STORE;
    depthAtt.clearValue.depthStencil.depth = 1.0f;

    VkRenderingInfo info = { 0 };
    info.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.renderArea.extent.width  = stdVk_g.width;
    info.renderArea.extent.height = stdVk_g.height;
    info.layerCount           = 1;
    info.colorAttachmentCount = 1;
    info.pColorAttachments    = &colorAtt;
    info.pDepthAttachment     = &depthAtt;
    vkCmdBeginRendering(cmd, &info);

    VkViewport viewport = { 0.0f, 0.0f, (float)stdVk_g.width, (float)stdVk_g.height, 0.0f, 1.0f };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = { { 0, 0 }, { stdVk_g.width, stdVk_g.height } };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    stdVk_g.bRendering = true;
    stdVk_g.bGpuNewer  = true;
    return cmd;
}

void stdVk_EndRendering(void)
{
    if ( stdVk_g.bRendering )
    {
        vkCmdEndRendering(stdVk_g.aFrames[stdVk_g.frameIdx].cmd);
        stdVk_g.bRendering = false;
    }
}

void* J3DAPI stdVk_AllocFrameData(size_t size, size_t align, VkBuffer* pBuffer, VkDeviceSize* pOffset)
{
    stdVk_BeginCommands();

    StdVkFrame* pFrame  = &stdVk_g.aFrames[stdVk_g.frameIdx];
    VkDeviceSize offset = (pFrame->ringOffset + align - 1) & ~((VkDeviceSize)align - 1);
    if ( offset + size > pFrame->ring.size )
    {
        // Ring is full, finish the commands recorded so far and start over
        stdVk_Flush();
        stdVk_BeginRendering();
        pFrame = &stdVk_g.aFrames[stdVk_g.frameIdx];
        offset = 0;
        if ( size > pFrame->ring.size )
        {
            STDLOG_ERROR("Vulkan frame data of %u bytes doesn't fit to the frame buffer.\n", size);
            return NULL;
        }
    }

    pFrame->ringOffset = offset + size;
    *pBuffer = pFrame->ring.buffer;
    *pOffset = offset;
    return pFrame->ring.pMapped + offset;
}

static void stdVk_Submit(VkSemaphore waitSemaphore, VkSemaphore signalSemaphore)
{
    StdVkFrame* pFrame = &stdVk_g.aFrames[stdVk_g.frameIdx];

    stdVk_EndRendering();
    vkEndCommandBuffer(pFrame->cmd);

    // Texture uploads recorded meanwhile must execute before the frame
    stdVk_SubmitUploads();

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    VkSubmitInfo info = { 0 };
    info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount = waitSemaphore ? 1 : 0;
    info.pWaitSemaphores    = &waitSemaphore;
    info.pWaitDstStageMask  = &waitStage;
    info.commandBufferCount = 1;
    info.pCommandBuffers    = &pFrame->cmd;
    info.signalSemaphoreCount = signalSemaphore ? 1 : 0;
    info.pSignalSemaphores  = &signalSemaphore;

    VkResult res = vkQueueSubmit(stdVk_g.queue, 1, &info, pFrame->fence);
    if ( res != VK_SUCCESS ) {
        STDLOG_ERROR("Vulkan error %s submitting commands.\n", stdVk_ResultString(res));
    }

    pFrame->bSubmitted = res == VK_SUCCESS;
    stdVk_g.bRecording = false;
}

void stdVk_Flush(void)
{
    if ( !stdVk_g.bRecording ) {
        return;
    }

    stdVk_Submit(VK_NULL_HANDLE, VK_NULL_HANDLE);
    stdVk_WaitFrame(&stdVk_g.aFrames[stdVk_g.frameIdx]);
}

int stdVk_Present(void)
{
    if ( !stdVk_g.colorImage ) {
        return 1;
    }

    if ( stdVk_IsSwapchainOutdated() )
    {
        stdVk_Flush();
        if ( !stdVk_CreateSwapchain() ) {
            return 1;
        }
    }

    VkCommandBuffer cmd = stdVk_BeginCommands();
    stdVk_EndRendering();

    StdVkFrame* pFrame  = &stdVk_g.aFrames[stdVk_g.frameIdx];
    uint32_t imageIndex = 0;
    VkResult res        = VK_ERROR_OUT_OF_DATE_KHR;
    if ( stdVk_g.swapchain && !stdVk_g.bSwapchainDirty ) {
        res = vkAcquireNextImageKHR(stdVk_g.device, stdVk_g.swapchain, UINT64_MAX, pFrame->acquireSemaphore, VK_NULL_HANDLE, &imageIndex);
    }

    if ( res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR )
    {
        // Can't present this frame (window minimized or resized), keep the rendered content
        stdVk_g.bSwapchainDirty = true;
        stdVk_Flush();
        return res == VK_ERROR_OUT_OF_DATE_KHR ? 0 : 1;
    }

    VkImage swapImage = stdVk_g.aSwapchainImages[imageIndex];
    stdVk_ImageBarrier(cmd, swapImage, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    stdVk_TransitionColor(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

    // Scale to the window keeping the aspect ratio
    uint32_t dstWidth  = stdVk_g.swapchainExtent.width;
    uint32_t dstHeight = stdVk_g.swapchainExtent.height;
    uint32_t scaledWidth  = dstWidth;
    uint32_t scaledHeight = (uint32_t)((uint64_t)dstWidth * stdVk_g.height / stdVk_g.width);
    if ( scaledHeight > dstHeight )
    {
        scaledHeight = dstHeight;
        scaledWidth  = (uint32_t)((uint64_t)dstHeight * stdVk_g.width / stdVk_g.height);
    }

    stdVk_g.presentScale = (float)scaledHeight / (float)stdVk_g.height;

    if ( scaledWidth != dstWidth || scaledHeight != dstHeight )
    {
        VkClearColorValue black = { 0 };
        VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage(cmd, swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);
        stdVk_ImageBarrier(cmd, swapImage, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    }

    int32_t dstX = (int32_t)(dstWidth - scaledWidth) / 2;
    int32_t dstY = (int32_t)(dstHeight - scaledHeight) / 2;

    VkImageBlit blit = { 0 };
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[1].x           = (int32_t)stdVk_g.width;
    blit.srcOffsets[1].y           = (int32_t)stdVk_g.height;
    blit.srcOffsets[1].z           = 1;
    blit.dstSubresource            = blit.srcSubresource;
    blit.dstOffsets[0].x           = dstX;
    blit.dstOffsets[0].y           = dstY;
    blit.dstOffsets[1].x           = dstX + (int32_t)scaledWidth;
    blit.dstOffsets[1].y           = dstY + (int32_t)scaledHeight;
    blit.dstOffsets[1].z           = 1;

    VkFilter filter = scaledWidth == stdVk_g.width && scaledHeight == stdVk_g.height ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    vkCmdBlitImage(cmd, stdVk_g.colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, filter);

    stdVk_ImageBarrier(cmd, swapImage, VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);

    VkSemaphore presentSemaphore = stdVk_g.aPresentSemaphores[imageIndex];
    stdVk_Submit(pFrame->acquireSemaphore, presentSemaphore);

    VkPresentInfoKHR presentInfo = { 0 };
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &presentSemaphore;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &stdVk_g.swapchain;
    presentInfo.pImageIndices      = &imageIndex;
    res = vkQueuePresentKHR(stdVk_g.queue, &presentInfo);
    // Suboptimal swapchains still present fine (Wayland reports it when it would prefer other buffers),
    // recreating every frame wouldn't change it. Size changes are detected by stdVk_IsSwapchainOutdated.
    if ( res == VK_ERROR_OUT_OF_DATE_KHR ) {
        stdVk_g.bSwapchainDirty = true;
    }
    else if ( res != VK_SUCCESS ) {
        STDLOG_ERROR("Vulkan error %s presenting.\n", stdVk_ResultString(res));
    }

    stdVk_g.frameIdx = (stdVk_g.frameIdx + 1) % STDVK_MAX_FRAMES;
    return 0;
}

// CPU mirror

static void stdVk_UploadShadow(VkCommandBuffer cmd)
{
    // The staging buffer may still be read by the other frame in flight
    for ( size_t i = 0; i < STDVK_MAX_FRAMES; ++i )
    {
        if ( i != stdVk_g.frameIdx ) {
            stdVk_WaitFrame(&stdVk_g.aFrames[i]);
        }
    }

    stdVk_FlushGdi();
    memcpy(stdVk_g.shadowStaging.pMapped, stdVk_g.pShadowPixels, (size_t)stdVk_g.width * stdVk_g.height * 4);

    stdVk_TransitionColor(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

    VkBufferImageCopy region = { 0 };
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width           = stdVk_g.width;
    region.imageExtent.height          = stdVk_g.height;
    region.imageExtent.depth           = 1;
    vkCmdCopyBufferToImage(cmd, stdVk_g.shadowStaging.buffer, stdVk_g.colorImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    stdVk_g.bShadowDirty = false;
    stdVk_g.bGpuNewer    = false;
}

void stdVk_SyncShadowForRead(void)
{
    if ( !stdVk_g.bGpuNewer || !stdVk_g.colorImage ) {
        return;
    }

    VkCommandBuffer cmd = stdVk_BeginCommands();
    stdVk_EndRendering();

    stdVk_TransitionColor(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

    VkBufferImageCopy region = { 0 };
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width           = stdVk_g.width;
    region.imageExtent.height          = stdVk_g.height;
    region.imageExtent.depth           = 1;
    vkCmdCopyImageToBuffer(cmd, stdVk_g.colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stdVk_g.shadowStaging.buffer, 1, &region);

    VkMemoryBarrier barrier = { 0 };
    barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, NULL, 0, NULL);

    stdVk_Flush();

    stdVk_FlushGdi();
    memcpy(stdVk_g.pShadowPixels, stdVk_g.shadowStaging.pMapped, (size_t)stdVk_g.width * stdVk_g.height * 4);
    stdVk_g.bGpuNewer = false;
}

void stdVk_MarkShadowDirty(void)
{
    stdVk_FlushGdi();
    stdVk_g.bShadowDirty = true;
    stdVk_g.bGpuNewer    = false;
}

void J3DAPI stdVk_ClearColor(uint32_t xrgb, const StdRect* pRect)
{
    if ( !stdVk_g.colorImage ) {
        return;
    }

    VkClearRect rect = { 0 };
    rect.layerCount = 1;
    if ( pRect )
    {
        // StdRect is (left, top, width, height) here, like in stdDisplay_VBufferFill
        int32_t x0 = STDMATH_CLAMP(pRect->left, 0, (int32_t)stdVk_g.width);
        int32_t y0 = STDMATH_CLAMP(pRect->top, 0, (int32_t)stdVk_g.height);
        int32_t x1 = STDMATH_CLAMP(pRect->left + pRect->right, 0, (int32_t)stdVk_g.width);
        int32_t y1 = STDMATH_CLAMP(pRect->top + pRect->bottom, 0, (int32_t)stdVk_g.height);
        if ( x1 <= x0 || y1 <= y0 ) {
            return;
        }

        rect.rect.offset.x      = x0;
        rect.rect.offset.y      = y0;
        rect.rect.extent.width  = (uint32_t)(x1 - x0);
        rect.rect.extent.height = (uint32_t)(y1 - y0);
    }
    else
    {
        rect.rect.extent.width  = stdVk_g.width;
        rect.rect.extent.height = stdVk_g.height;
    }

    VkClearAttachment att = { 0 };
    att.aspectMask                  = VK_IMAGE_ASPECT_COLOR_BIT;
    att.clearValue.color.float32[0] = (float)((xrgb >> 16) & 0xFF) / 255.0f;
    att.clearValue.color.float32[1] = (float)((xrgb >> 8) & 0xFF) / 255.0f;
    att.clearValue.color.float32[2] = (float)(xrgb & 0xFF) / 255.0f;
    att.clearValue.color.float32[3] = 1.0f;

    VkCommandBuffer cmd = stdVk_BeginRendering();
    vkCmdClearAttachments(cmd, 1, &att, 1, &rect);
}

void J3DAPI stdVk_ClearDepth(void)
{
    if ( !stdVk_g.depthImage ) {
        return;
    }

    VkClearAttachment att = { 0 };
    att.aspectMask                    = VK_IMAGE_ASPECT_DEPTH_BIT;
    att.clearValue.depthStencil.depth = 1.0f;

    VkClearRect rect = { 0 };
    rect.rect.extent.width  = stdVk_g.width;
    rect.rect.extent.height = stdVk_g.height;
    rect.layerCount         = 1;

    VkCommandBuffer cmd = stdVk_BeginRendering();
    vkCmdClearAttachments(cmd, 1, &att, 1, &rect);
}

// Textures

static VkDescriptorSet stdVk_AllocTextureSet(VkDescriptorPool* pPool)
{
    VkDescriptorSetAllocateInfo info = { 0 };
    info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorSetCount = 1;
    info.pSetLayouts        = &stdVk_g.textureSetLayout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    for ( size_t i = stdVk_g.numTexturePools; i-- > 0; )
    {
        info.descriptorPool = stdVk_g.aTexturePools[i];
        if ( vkAllocateDescriptorSets(stdVk_g.device, &info, &set) == VK_SUCCESS )
        {
            *pPool = stdVk_g.aTexturePools[i];
            return set;
        }
    }

    if ( stdVk_g.numTexturePools >= STD_ARRAYLEN(stdVk_g.aTexturePools) )
    {
        STDLOG_ERROR("Out of Vulkan texture descriptor pools.\n");
        return VK_NULL_HANDLE;
    }

    VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, STDVK_TEXTURE_POOL_SIZE };
    VkDescriptorPoolCreateInfo poolInfo = { 0 };
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets       = STDVK_TEXTURE_POOL_SIZE;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    if ( vkCreateDescriptorPool(stdVk_g.device, &poolInfo, NULL, &pool) != VK_SUCCESS )
    {
        STDLOG_ERROR("Error creating Vulkan texture descriptor pool.\n");
        return VK_NULL_HANDLE;
    }

    stdVk_g.aTexturePools[stdVk_g.numTexturePools++] = pool;
    info.descriptorPool = pool;
    if ( vkAllocateDescriptorSets(stdVk_g.device, &info, &set) != VK_SUCCESS ) {
        return VK_NULL_HANDLE;
    }

    *pPool = pool;
    return set;
}

static void stdVk_FreeTexture(StdVkTexture* pTex)
{
    if ( pTex->descSet ) {
        vkFreeDescriptorSets(stdVk_g.device, pTex->descPool, 1, &pTex->descSet);
    }
    if ( pTex->view ) {
        vkDestroyImageView(stdVk_g.device, pTex->view, NULL);
    }
    if ( pTex->image ) {
        vkDestroyImage(stdVk_g.device, pTex->image, NULL);
    }

    stdVk_Free(&pTex->alloc);
    stdMemory_Free(pTex);
    --stdVk_g.numLiveTextures;
}

StdVkTexture* J3DAPI stdVk_CreateTexture(uint32_t width, uint32_t height, uint32_t numMipLevels, VkFormat format, const VkComponentMapping* pSwizzle)
{
    StdVkTexture* pTex = (StdVkTexture*)STDMALLOC(sizeof(StdVkTexture));
    if ( !pTex ) {
        return NULL;
    }

    memset(pTex, 0, sizeof(*pTex));
    pTex->width        = width;
    pTex->height       = height;
    pTex->numMipLevels = numMipLevels;
    ++stdVk_g.numLiveTextures;

    if ( !stdVk_CreateImage(width, height, numMipLevels, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT, pSwizzle, &pTex->image, &pTex->view, &pTex->alloc) )
    {
        stdVk_FreeTexture(pTex);
        return NULL;
    }

    pTex->descSet = stdVk_AllocTextureSet(&pTex->descPool);
    if ( !pTex->descSet )
    {
        stdVk_FreeTexture(pTex);
        return NULL;
    }

    VkDescriptorImageInfo imageInfo = { 0 };
    imageInfo.imageView   = pTex->view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write = { 0 };
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = pTex->descSet;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo      = &imageInfo;
    vkUpdateDescriptorSets(stdVk_g.device, 1, &write, 0, NULL);

    return pTex;
}

static VkCommandBuffer stdVk_BeginUploads(void)
{
    if ( !stdVk_g.bUploadRecording )
    {
        if ( stdVk_g.bUploadPending )
        {
            vkWaitForFences(stdVk_g.device, 1, &stdVk_g.uploadFence, VK_TRUE, UINT64_MAX);
            vkResetFences(stdVk_g.device, 1, &stdVk_g.uploadFence);
            stdVk_g.bUploadPending = false;
        }

        VkCommandBufferBeginInfo beginInfo = { 0 };
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkResetCommandBuffer(stdVk_g.uploadCmd, 0);
        vkBeginCommandBuffer(stdVk_g.uploadCmd, &beginInfo);

        stdVk_g.uploadOffset     = 0;
        stdVk_g.bUploadRecording = true;
    }

    return stdVk_g.uploadCmd;
}

void stdVk_SubmitUploads(void)
{
    if ( !stdVk_g.bUploadRecording ) {
        return;
    }

    vkEndCommandBuffer(stdVk_g.uploadCmd);

    VkSubmitInfo info = { 0 };
    info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers    = &stdVk_g.uploadCmd;

    VkResult res = vkQueueSubmit(stdVk_g.queue, 1, &info, stdVk_g.uploadFence);
    if ( res != VK_SUCCESS ) {
        STDLOG_ERROR("Vulkan error %s submitting texture uploads.\n", stdVk_ResultString(res));
    }

    stdVk_g.bUploadRecording = false;
    stdVk_g.bUploadPending   = res == VK_SUCCESS;
}

static void stdVk_WaitUploads(void)
{
    stdVk_SubmitUploads();
    if ( stdVk_g.bUploadPending )
    {
        vkWaitForFences(stdVk_g.device, 1, &stdVk_g.uploadFence, VK_TRUE, UINT64_MAX);
        vkResetFences(stdVk_g.device, 1, &stdVk_g.uploadFence);
        stdVk_g.bUploadPending = false;
    }
}

void J3DAPI stdVk_UploadTextureLevel(StdVkTexture* pTex, uint32_t level, const uint8_t* pPixels, size_t rowSize)
{
    uint32_t width  = pTex->width >> level;
    uint32_t height = pTex->height >> level;
    width  = width ? width : 1;
    height = height ? height : 1;

    VkDeviceSize size = (VkDeviceSize)width * height * 4;
    if ( stdVk_g.uploadOffset + size > stdVk_g.uploadStaging.size )
    {
        // Out of staging memory, wait for the pending uploads
        // Note, the barrier to transfer layout recorded for this texture is submitted too.
        stdVk_WaitUploads();
        if ( size > stdVk_g.uploadStaging.size )
        {
            stdVk_DestroyBuffer(&stdVk_g.uploadStaging);
            if ( !stdVk_CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &stdVk_g.uploadStaging) ) {
                return;
            }
        }
    }

    VkCommandBuffer cmd = stdVk_BeginUploads();
    if ( level == 0 )
    {
        stdVk_ImageBarrier(cmd, pTex->image, VK_IMAGE_ASPECT_COLOR_BIT, pTex->numMipLevels, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    }

    uint8_t* pDst = stdVk_g.uploadStaging.pMapped + stdVk_g.uploadOffset;
    for ( uint32_t row = 0; row < height; ++row ) {
        memcpy(pDst + (size_t)row * width * 4, pPixels + row * rowSize, (size_t)width * 4);
    }

    VkBufferImageCopy region = { 0 };
    region.bufferOffset                = stdVk_g.uploadOffset;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel   = level;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width           = width;
    region.imageExtent.height          = height;
    region.imageExtent.depth           = 1;
    vkCmdCopyBufferToImage(cmd, stdVk_g.uploadStaging.buffer, pTex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    stdVk_g.uploadOffset = (stdVk_g.uploadOffset + size + 15) & ~(VkDeviceSize)15;
}

void J3DAPI stdVk_FinishTextureUpload(StdVkTexture* pTex)
{
    VkCommandBuffer cmd = stdVk_BeginUploads();
    stdVk_ImageBarrier(cmd, pTex->image, VK_IMAGE_ASPECT_COLOR_BIT, pTex->numMipLevels, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
}

void J3DAPI stdVk_DestroyTexture(StdVkTexture* pTex)
{
    if ( !pTex ) {
        return;
    }

    // Submitted frames may still use the texture. It's queued to the frame slot being recorded,
    // or to the last submitted one, and freed when that slot is recorded again after its fence is waited (see stdVk_BeginCommands).
    uint32_t slot = stdVk_g.bRecording ? stdVk_g.frameIdx : (stdVk_g.frameIdx + STDVK_MAX_FRAMES - 1) % STDVK_MAX_FRAMES;
    StdVkFrame* pFrame = &stdVk_g.aFrames[slot];
    pTex->pNextDeleted = pFrame->pDeletedTextures;
    pFrame->pDeletedTextures = pTex;
}

static void stdVk_ProcessDeletedTextures(StdVkTexture** ppList)
{
    if ( !*ppList ) {
        return;
    }

    // Uploads of the texture may still be in flight
    stdVk_WaitUploads();

    StdVkTexture* pTex = *ppList;
    while ( pTex )
    {
        StdVkTexture* pNext = pTex->pNextDeleted;
        stdVk_FreeTexture(pTex);
        pTex = pNext;
    }

    *ppList = NULL;
}
