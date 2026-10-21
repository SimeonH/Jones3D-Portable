// Vulkan 3D rendering of the GPU std3D (see Gpu/std3D.c): pipelines, samplers and draws.
#include "stdVk.h"
#include <std/Gpu/stdGpu.h>

#include <std/General/std.h>
#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>

#include <stddef.h>
#include <string.h>

#define STD3D_SHADER_TEXTURED 0x1u
#define STD3D_SHADER_FOG      0x2u

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

static VkPipelineLayout std3D_pipelineLayout              = VK_NULL_HANDLE;
static VkPipeline std3D_aPipelines[STDGPU_NUM_PRIMITIVES] = { 0 };
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

    const VkPrimitiveTopology aTopologies[STDGPU_NUM_PRIMITIVES] = {
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST
    };

    int bSuccess = 1;
    for ( size_t i = 0; i < STDGPU_NUM_PRIMITIVES; ++i )
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

    for ( size_t i = 0; i < STDGPU_NUM_PRIMITIVES; ++i )
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

int stdVk_Open3D(void)
{
    if ( !stdVk_g.colorImage ) {
        return 0;
    }

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

    std3D_cmdSerial = stdVk_g.cmdSerial - 1;
    return 1;
}

void stdVk_Close3D(void)
{
    std3D_DestroyResources();
}

static size_t std3D_GetSamplerIndex(const StdGpuDrawState* pState)
{
    return (size_t)pState->filter + 3 * (size_t)pState->mipFilter + 9 * (size_t)pState->bClampU + 18 * (size_t)pState->bClampV;
}

void stdVk_Draw(StdGpuPrimitive primitive, const StdGpuDrawState* pState, void* pNativeTex, const StdTLVertex* aVerts, size_t numVerts, const uint16_t* aIndices, size_t numIndices)
{
    StdVkTexture* pTexture = (StdVkTexture*)pNativeTex;
    if ( !std3D_aPipelines[primitive] ) {
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

    if ( std3D_boundPipeline != std3D_aPipelines[primitive] )
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, std3D_aPipelines[primitive]);
        std3D_boundPipeline = std3D_aPipelines[primitive];
    }

    int bDepthWrite = pState->bDepthWrite;
    if ( std3D_boundDepthWrite != bDepthWrite )
    {
        vkCmdSetDepthWriteEnable(cmd, bDepthWrite ? VK_TRUE : VK_FALSE);
        std3D_boundDepthWrite = bDepthWrite;
    }

    VkDescriptorSet samplerSet = std3D_aSamplerSets[std3D_GetSamplerIndex(pState)];
    if ( std3D_boundSampler != samplerSet )
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, std3D_pipelineLayout, 0, 1, &samplerSet, 0, NULL);
        std3D_boundSampler = samplerSet;
    }

    VkDescriptorSet textureSet = pTexture ? pTexture->descSet : std3D_pWhiteTexture->descSet;
    if ( std3D_boundTexture != textureSet )
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, std3D_pipelineLayout, 1, 1, &textureSet, 0, NULL);
        std3D_boundTexture = textureSet;
    }

    StdVkPushConstants constants;
    memset(&constants, 0, sizeof(constants));
    constants.pixelToNdc[0] = 2.0f / (float)stdVk_g.width;
    constants.pixelToNdc[1] = 2.0f / (float)stdVk_g.height;
    constants.alphaRef      = pState->alphaRef;

    if ( pTexture ) {
        constants.flags |= STD3D_SHADER_TEXTURED;
    }

    if ( pState->bFog )
    {
        constants.flags      |= STD3D_SHADER_FOG;
        constants.fogColor[0] = pState->fogColor[0];
        constants.fogColor[1] = pState->fogColor[1];
        constants.fogColor[2] = pState->fogColor[2];
        constants.fogColor[3] = 1.0f;
        constants.fogStart    = pState->fogStart;
        constants.fogEnd      = pState->fogEnd;
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

void* stdVk_CreateGpuTexture(uint32_t width, uint32_t height, uint32_t numMipLevels, StdGpuTexLayout layout)
{
    VkComponentMapping swizzle = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    if ( layout == STDGPU_TEXLAYOUT_RGBX )
    {
        swizzle.a = VK_COMPONENT_SWIZZLE_ONE;
    }
    else if ( layout == STDGPU_TEXLAYOUT_ABGR )
    {
        swizzle.r = VK_COMPONENT_SWIZZLE_A;
        swizzle.g = VK_COMPONENT_SWIZZLE_B;
        swizzle.b = VK_COMPONENT_SWIZZLE_G;
        swizzle.a = VK_COMPONENT_SWIZZLE_R;
    }

    return stdVk_CreateTexture(width, height, numMipLevels, VK_FORMAT_R8G8B8A8_UNORM, &swizzle);
}

void stdVk_UploadGpuTextureLevel(void* pNativeTex, uint32_t level, const uint8_t* pPixels, size_t rowSize)
{
    stdVk_UploadTextureLevel((StdVkTexture*)pNativeTex, level, pPixels, rowSize);
}

void stdVk_FinishGpuTextureUpload(void* pNativeTex)
{
    stdVk_FinishTextureUpload((StdVkTexture*)pNativeTex);
}

void stdVk_DestroyGpuTexture(void* pNativeTex)
{
    stdVk_DestroyTexture((StdVkTexture*)pNativeTex);
}
