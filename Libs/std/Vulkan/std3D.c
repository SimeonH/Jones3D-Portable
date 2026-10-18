// Vulkan implementation of std3D.
// Emulates the Direct3D 6 fixed function state the game uses: pre-transformed (TL) vertices, texture * diffuse
// modulation, alpha blending (SRCALPHA, INVSRCALPHA), alpha test (GREATER ref), z-buffer (LESSEQUAL) and linear w-fog.
#include <std/Win95/std3D.h>
#include <std/Win95/stdDisplay.h>
#include "stdVk.h"

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

#define STD3D_SHADER_TEXTURED 0x1u
#define STD3D_SHADER_FOG      0x2u

enum
{
    STD3D_PIPELINE_TRIANGLES,
    STD3D_PIPELINE_LINES,
    STD3D_PIPELINE_POINTS,
    STD3D_NUM_PIPELINES
};

// Sampler key: filter (point, linear, anisotropic) x mip filter (none, point, linear) x clamp u x clamp v
#define STD3D_NUM_SAMPLERS (3 * 3 * 2 * 2)

// Must match std3D_common.glsl
typedef struct sStdVkPushConstants
{
    float pixelToNdc[2];
    float alphaRef;
    uint32_t flags;
    float fogColor[4];
    float fogStart;
    float fogEnd;
} StdVkPushConstants;

static const uint32_t std3D_aVertShader[] =
#include "std3D.vert.spv.h"
;

static const uint32_t std3D_aFragShader[] =
#include "std3D.frag.spv.h"
;

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

static VkPipelineLayout std3D_pipelineLayout              = VK_NULL_HANDLE;
static VkPipeline std3D_aPipelines[STD3D_NUM_PIPELINES]   = { 0 };
static VkSampler std3D_aSamplers[STD3D_NUM_SAMPLERS]      = { 0 };
static VkDescriptorSet std3D_aSamplerSets[STD3D_NUM_SAMPLERS] = { 0 };
static VkDescriptorPool std3D_samplerPool                 = VK_NULL_HANDLE;
static StdVkTexture* std3D_pWhiteTexture                  = NULL;

// Command buffer state cache, valid while stdVk_g.cmdSerial == std3D_cmdSerial
static uint32_t std3D_cmdSerial          = 0;
static VkPipeline std3D_boundPipeline    = VK_NULL_HANDLE;
static VkDescriptorSet std3D_boundSampler = VK_NULL_HANDLE;
static VkDescriptorSet std3D_boundTexture = VK_NULL_HANDLE;
static int std3D_boundDepthWrite         = -1;
static StdVkPushConstants std3D_boundConstants;

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

    if ( !stdVk_g.device )
    {
        STDLOG_ERROR("Vulkan device not created yet!\n");
        return 0;
    }

    uint32_t maxTexSize = stdVk_g.deviceProps.limits.maxImageDimension2D;
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
    STD_STRCPY(pDevice->deviceName, "Vulkan HAL");
    STD_FORMAT(pDevice->deviceDescription, "Vulkan HAL (%s)", stdVk_g.deviceProps.deviceName);

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

static int std3D_CreatePipelines(void)
{
    VkDescriptorSetLayout aSetLayouts[2] = { stdVk_g.samplerSetLayout, stdVk_g.textureSetLayout };

    VkPushConstantRange pushRange = { 0 };
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.size       = sizeof(StdVkPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo = { 0 };
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = 2;
    layoutInfo.pSetLayouts            = aSetLayouts;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushRange;
    if ( vkCreatePipelineLayout(stdVk_g.device, &layoutInfo, NULL, &std3D_pipelineLayout) != VK_SUCCESS )
    {
        STDLOG_ERROR("Error creating std3D pipeline layout.\n");
        return 0;
    }

    VkShaderModule vertModule = stdVk_CreateShader(std3D_aVertShader, sizeof(std3D_aVertShader));
    VkShaderModule fragModule = stdVk_CreateShader(std3D_aFragShader, sizeof(std3D_aFragShader));
    if ( !vertModule || !fragModule ) {
        return 0;
    }

    VkPipelineShaderStageCreateInfo aStages[2] = { 0 };
    aStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    aStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    aStages[0].module = vertModule;
    aStages[0].pName  = "main";
    aStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    aStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    aStages[1].module = fragModule;
    aStages[1].pName  = "main";

    VkVertexInputBindingDescription binding = { 0, sizeof(StdTLVertex), VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription aAttributes[3] = {
        { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(StdTLVertex, sx) },
        { 1, 0, VK_FORMAT_B8G8R8A8_UNORM,      offsetof(StdTLVertex, color) }, // D3DCOLOR is ARGB
        { 2, 0, VK_FORMAT_R32G32_SFLOAT,       offsetof(StdTLVertex, tu) },
    };

    VkPipelineVertexInputStateCreateInfo vertexInput = { 0 };
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &binding;
    vertexInput.vertexAttributeDescriptionCount = (uint32_t)STD_ARRAYLEN(aAttributes);
    vertexInput.pVertexAttributeDescriptions    = aAttributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { 0 };
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

    VkPipelineViewportStateCreateInfo viewportState = { 0 };
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo raster = { 0 };
    raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode    = VK_CULL_MODE_NONE;
    raster.frontFace   = VK_FRONT_FACE_CLOCKWISE;
    raster.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = { 0 };
    multisample.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil = { 0 };
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable  = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE; // dynamic
    depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState blendAttachment = { 0 };
    blendAttachment.blendEnable         = VK_TRUE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend = { 0 };
    blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments    = &blendAttachment;

    VkDynamicState aDynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE };
    VkPipelineDynamicStateCreateInfo dynamicState = { 0 };
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = (uint32_t)STD_ARRAYLEN(aDynamicStates);
    dynamicState.pDynamicStates    = aDynamicStates;

    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkPipelineRenderingCreateInfo renderingInfo = { 0 };
    renderingInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount    = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat   = stdVk_g.depthFormat;

    VkGraphicsPipelineCreateInfo info = { 0 };
    info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext               = &renderingInfo;
    info.stageCount          = 2;
    info.pStages             = aStages;
    info.pVertexInputState   = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState      = &viewportState;
    info.pRasterizationState = &raster;
    info.pMultisampleState   = &multisample;
    info.pDepthStencilState  = &depthStencil;
    info.pColorBlendState    = &blend;
    info.pDynamicState       = &dynamicState;
    info.layout              = std3D_pipelineLayout;

    const VkPrimitiveTopology aTopologies[STD3D_NUM_PIPELINES] = {
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST
    };

    int bSuccess = 1;
    for ( size_t i = 0; i < STD3D_NUM_PIPELINES; ++i )
    {
        inputAssembly.topology = aTopologies[i];
        if ( vkCreateGraphicsPipelines(stdVk_g.device, VK_NULL_HANDLE, 1, &info, NULL, &std3D_aPipelines[i]) != VK_SUCCESS )
        {
            STDLOG_ERROR("Error creating std3D pipeline %d.\n", i);
            bSuccess = 0;
            break;
        }
    }

    vkDestroyShaderModule(stdVk_g.device, vertModule, NULL);
    vkDestroyShaderModule(stdVk_g.device, fragModule, NULL);
    return bSuccess;
}

static int std3D_CreateSamplers(void)
{
    VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_SAMPLER, STD3D_NUM_SAMPLERS };
    VkDescriptorPoolCreateInfo poolInfo = { 0 };
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = STD3D_NUM_SAMPLERS;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    if ( vkCreateDescriptorPool(stdVk_g.device, &poolInfo, NULL, &std3D_samplerPool) != VK_SUCCESS )
    {
        STDLOG_ERROR("Error creating std3D sampler pool.\n");
        return 0;
    }

    for ( size_t i = 0; i < STD3D_NUM_SAMPLERS; ++i )
    {
        size_t filter = i % 3;
        size_t mip    = (i / 3) % 3;
        bool bClampU  = ((i / 9) & 1) != 0;
        bool bClampV  = ((i / 18) & 1) != 0;

        VkSamplerCreateInfo info = { 0 };
        info.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter    = filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        info.minFilter    = info.magFilter;
        info.mipmapMode   = mip == STD3D_MIPMAPFILTER_TRILINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        info.addressModeU = bClampU ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = bClampV ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.minLod       = 0.0f;
        info.maxLod       = mip == STD3D_MIPMAPFILTER_NONE ? 0.0f : VK_LOD_CLAMP_NONE;
        if ( filter == 2 && stdVk_g.maxAnisotropy > 1.0f )
        {
            info.anisotropyEnable = VK_TRUE;
            info.maxAnisotropy    = stdVk_g.maxAnisotropy > 16.0f ? 16.0f : stdVk_g.maxAnisotropy;
        }

        if ( vkCreateSampler(stdVk_g.device, &info, NULL, &std3D_aSamplers[i]) != VK_SUCCESS )
        {
            STDLOG_ERROR("Error creating std3D sampler.\n");
            return 0;
        }

        VkDescriptorSetAllocateInfo allocInfo = { 0 };
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = std3D_samplerPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &stdVk_g.samplerSetLayout;
        if ( vkAllocateDescriptorSets(stdVk_g.device, &allocInfo, &std3D_aSamplerSets[i]) != VK_SUCCESS )
        {
            STDLOG_ERROR("Error allocating std3D sampler set.\n");
            return 0;
        }

        VkDescriptorImageInfo imageInfo = { 0 };
        imageInfo.sampler = std3D_aSamplers[i];

        VkWriteDescriptorSet write = { 0 };
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = std3D_aSamplerSets[i];
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        write.pImageInfo      = &imageInfo;
        vkUpdateDescriptorSets(stdVk_g.device, 1, &write, 0, NULL);
    }

    return 1;
}

static void std3D_DestroyResources(void)
{
    if ( !stdVk_g.device ) {
        return;
    }

    stdVk_Flush();
    vkDeviceWaitIdle(stdVk_g.device);

    for ( size_t i = 0; i < STD3D_NUM_PIPELINES; ++i )
    {
        if ( std3D_aPipelines[i] ) {
            vkDestroyPipeline(stdVk_g.device, std3D_aPipelines[i], NULL);
        }
        std3D_aPipelines[i] = VK_NULL_HANDLE;
    }

    if ( std3D_pipelineLayout ) {
        vkDestroyPipelineLayout(stdVk_g.device, std3D_pipelineLayout, NULL);
    }
    std3D_pipelineLayout = VK_NULL_HANDLE;

    for ( size_t i = 0; i < STD3D_NUM_SAMPLERS; ++i )
    {
        if ( std3D_aSamplers[i] ) {
            vkDestroySampler(stdVk_g.device, std3D_aSamplers[i], NULL);
        }
        std3D_aSamplers[i]    = VK_NULL_HANDLE;
        std3D_aSamplerSets[i] = VK_NULL_HANDLE;
    }

    if ( std3D_samplerPool ) {
        vkDestroyDescriptorPool(stdVk_g.device, std3D_samplerPool, NULL);
    }
    std3D_samplerPool = VK_NULL_HANDLE;

    stdVk_DestroyTexture(std3D_pWhiteTexture);
    std3D_pWhiteTexture = NULL;
}

int J3DAPI std3D_Open(size_t deviceNum)
{
    STD_ASSERTREL(std3D_bStartup == 1);
    if ( std3D_bOpen )
    {
        STDLOG_ERROR("Warning: System already open!\n");
        return 0;
    }

    if ( !stdDisplay_IsOpen() || !stdVk_g.colorImage )
    {
        STDLOG_ERROR("std3D_Open: Display system is not opened!\n");
        return 0;
    }

    if ( deviceNum >= std3D_numDevices ) {
        return 0;
    }

    std3D_pCurDevice = &std3D_aDevices[deviceNum];

    if ( !std3D_CreatePipelines() || !std3D_CreateSamplers() )
    {
        std3D_DestroyResources();
        return 0;
    }

    // Bound when drawing without a texture
    std3D_pWhiteTexture = stdVk_CreateTexture(1, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, NULL);
    if ( !std3D_pWhiteTexture )
    {
        std3D_DestroyResources();
        return 0;
    }

    const uint32_t white = 0xFFFFFFFF;
    stdVk_UploadTextureLevel(std3D_pWhiteTexture, 0, (const uint8_t*)&white, 4);
    stdVk_FinishTextureUpload(std3D_pWhiteTexture);

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
        | (stdVk_g.maxAnisotropy > 1.0f ? STD3D_RS_TEXFILTER_ANISOTROPIC : STD3D_RS_TEXFILTER_BILINEAR);
    std3D_mipmapFilter = STD3D_MIPMAPFILTER_TRILINEAR; // Same as the DirectDraw backend default

    stdDisplay_GetTextureMemory(&std3D_pCurDevice->totalMemory, &std3D_pCurDevice->availableMemory);

    std3D_cmdSerial = stdVk_g.cmdSerial - 1;
    std3D_bOpen     = true;
    return 1;
}

void J3DAPI std3D_Close()
{
    std3D_ResetTextureCache();
    std3D_DestroyResources();

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

static size_t std3D_GetSamplerIndex(Std3DRenderState rdflags)
{
    size_t filter = 0;
    if ( (rdflags & STD3D_RS_TEXFILTER_ANISOTROPIC) != 0 ) {
        filter = 2;
    }
    else if ( (rdflags & STD3D_RS_TEXFILTER_BILINEAR) != 0 ) {
        filter = 1;
    }

    size_t clampU = (rdflags & STD3D_RS_TEX_CPAMP_U) != 0;
    size_t clampV = (rdflags & STD3D_RS_TEX_CPAMP_V) != 0;
    return filter + 3 * (size_t)std3D_mipmapFilter + 9 * clampU + 18 * clampV;
}

static void J3DAPI std3D_Draw(size_t pipeline, const StdTLVertex* aVerts, size_t numVerts, const uint16_t* aIndices, size_t numIndices)
{
    if ( !std3D_bOpen || numVerts == 0 ) {
        return;
    }

    // Vertices and indices go to a single allocation, an allocation may restart the frame data
    size_t vertSize  = numVerts * sizeof(StdTLVertex);
    size_t indexSize = numIndices * sizeof(uint16_t);
    VkBuffer buffer;
    VkDeviceSize offset;
    uint8_t* pData = (uint8_t*)stdVk_AllocFrameData(vertSize + indexSize, sizeof(StdTLVertex), &buffer, &offset);
    if ( !pData ) {
        return;
    }

    memcpy(pData, aVerts, vertSize);
    if ( numIndices ) {
        memcpy(pData + vertSize, aIndices, indexSize);
    }

    VkCommandBuffer cmd = stdVk_BeginRendering();
    if ( std3D_cmdSerial != stdVk_g.cmdSerial )
    {
        std3D_cmdSerial       = stdVk_g.cmdSerial;
        std3D_boundPipeline   = VK_NULL_HANDLE;
        std3D_boundSampler    = VK_NULL_HANDLE;
        std3D_boundTexture    = VK_NULL_HANDLE;
        std3D_boundDepthWrite = -1;
        memset(&std3D_boundConstants, 0xFF, sizeof(std3D_boundConstants));
    }

    if ( std3D_boundPipeline != std3D_aPipelines[pipeline] )
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, std3D_aPipelines[pipeline]);
        std3D_boundPipeline = std3D_aPipelines[pipeline];
    }

    int bDepthWrite = (std3D_renderState & STD3D_RS_ZWRITE_DISABLED) == 0;
    if ( std3D_boundDepthWrite != bDepthWrite )
    {
        vkCmdSetDepthWriteEnable(cmd, bDepthWrite ? VK_TRUE : VK_FALSE);
        std3D_boundDepthWrite = bDepthWrite;
    }

    VkDescriptorSet samplerSet = std3D_aSamplerSets[std3D_GetSamplerIndex(std3D_renderState)];
    if ( std3D_boundSampler != samplerSet )
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, std3D_pipelineLayout, 0, 1, &samplerSet, 0, NULL);
        std3D_boundSampler = samplerSet;
    }

    VkDescriptorSet textureSet = std3D_pCurTex ? std3D_pCurTex->descSet : std3D_pWhiteTexture->descSet;
    if ( std3D_boundTexture != textureSet )
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, std3D_pipelineLayout, 1, 1, &textureSet, 0, NULL);
        std3D_boundTexture = textureSet;
    }

    StdVkPushConstants constants;
    memset(&constants, 0, sizeof(constants));
    constants.pixelToNdc[0] = 2.0f / (float)stdVk_g.width;
    constants.pixelToNdc[1] = 2.0f / (float)stdVk_g.height;

    // D3DCMP_GREATER against 8-bit alpha ref
    uint32_t alphaRef  = (std3D_renderState & STD3D_RS_ALPHAREF_SET) != 0 ? 0xA0 : 0;
    constants.alphaRef = ((float)alphaRef + 0.5f) / 255.0f;

    if ( std3D_pCurTex ) {
        constants.flags |= STD3D_SHADER_TEXTURED;
    }

    if ( (std3D_renderState & STD3D_RS_FOG_ENABLED) != 0 && std3D_bRenderFog && std3D_g_fogDensity != 0.0f && std3D_fogEndDepth > std3D_fogStartDepth )
    {
        constants.flags      |= STD3D_SHADER_FOG;
        constants.fogColor[0] = std3D_fogColor[0];
        constants.fogColor[1] = std3D_fogColor[1];
        constants.fogColor[2] = std3D_fogColor[2];
        constants.fogColor[3] = 1.0f;
        constants.fogStart    = std3D_fogStartDepth;
        constants.fogEnd      = std3D_fogEndDepth;
    }

    if ( memcmp(&constants, &std3D_boundConstants, sizeof(constants)) != 0 )
    {
        vkCmdPushConstants(cmd, std3D_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(constants), &constants);
        std3D_boundConstants = constants;
    }

    vkCmdBindVertexBuffers(cmd, 0, 1, &buffer, &offset);
    if ( numIndices )
    {
        vkCmdBindIndexBuffer(cmd, buffer, offset + vertSize, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd, (uint32_t)numIndices, 1, 0, 0, 0);
    }
    else
    {
        vkCmdDraw(cmd, (uint32_t)numVerts, 1, 0, 0);
    }
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
    std3D_Draw(STD3D_PIPELINE_TRIANGLES, aVerts, numVerts, aIndices, numIndices);
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

    std3D_Draw(STD3D_PIPELINE_LINES, aVerts, numVerts, NULL, 0);
}

void J3DAPI std3D_DrawPointList(StdTLVertex* aVerts, size_t numVerts)
{
    if ( numVerts > std3D_g_maxVertices )
    {
        STDLOG_ERROR("Error %d > %d maxVertices.\n", numVerts, std3D_g_maxVertices);
        return;
    }

    std3D_Draw(STD3D_PIPELINE_POINTS, aVerts, numVerts, NULL, 0);
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

    // Vulkan requires a complete mip chain, use only the levels matching it
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
    VkComponentMapping swizzle = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    bool bConvert = false;
    if ( memcmp(pCi, &stdColor_cfBGR8888, sizeof(ColorInfo)) == 0 )
    {
        swizzle.a = VK_COMPONENT_SWIZZLE_ONE;
    }
    else if ( memcmp(pCi, &stdColor_cfRGBA8888, sizeof(ColorInfo)) == 0 )
    {
        swizzle.r = VK_COMPONENT_SWIZZLE_A;
        swizzle.g = VK_COMPONENT_SWIZZLE_B;
        swizzle.b = VK_COMPONENT_SWIZZLE_G;
        swizzle.a = VK_COMPONENT_SWIZZLE_R;
    }
    else
    {
        bConvert = true;
    }

    StdVkTexture* pTex = stdVk_CreateTexture(width, height, (uint32_t)numLevels, VK_FORMAT_R8G8B8A8_UNORM, &swizzle);
    if ( !pTex )
    {
        STDLOG_ERROR("Error creating Vulkan texture.\n");
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

            stdVk_UploadTextureLevel(pTex, (uint32_t)level, (const uint8_t*)pConverted, (size_t)pInfo->width * 4);
        }
        else
        {
            stdVk_UploadTextureLevel(pTex, (uint32_t)level, pLevel->pPixels, pLevel->rasterInfo.rowSize);
        }

        stdDisplay_VBufferUnlock(pLevel);
    }

    if ( pConverted ) {
        stdMemory_Free(pConverted);
    }

    stdVk_FinishTextureUpload(pTex);

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

        stdVk_DestroyTexture(pTex->pSrcTexture);
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

    return pTexture->pSrcTexture->numMipLevels;
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
        stdVk_ClearDepth();
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
