// Direct3D 11 3D rendering of the GPU std3D (see Gpu/std3D.c): shaders, sampler state and draws.
// Same shading as the Vulkan backend (Vulkan/shaders), with a pixel shader variant per texture & fog combination.
#include "stdD3D11.h"

#include <std/General/std.h>
#include <std/General/stdUtil.h>

#include <string.h>

#define STD3D_PS_TEXTURED 0x1
#define STD3D_PS_FOG      0x2
#define STD3D_NUM_PS      4

#define STD3D_NUM_SAMPLERS 36 // filter (3) x mip filter (3) x clamp u (2) x clamp v (2)

#define STD3D_VERTEXBUFFER_SIZE (4u * 1024u * 1024u)
#define STD3D_INDEXBUFFER_SIZE  (1u * 1024u * 1024u)

static const char std3D_aShader[] =
    "cbuffer Constants : register(b0)\n"
    "{\n"
    "    float2 u_pixelToNdc; // 2 / back buffer size\n"
    "    float u_alphaRef;    // fragments with alpha below are discarded (D3D alpha test GREATER ref)\n"
    "    float u_pad0;\n"
    "    float4 u_fogColor;\n"
    "    float2 u_fogRange;   // linear fog start & end in eye depth (w)\n"
    "    float2 u_pad1;\n"
    "};\n"
    "Texture2D u_tex : register(t0);\n"
    "SamplerState u_sampler : register(s0);\n"
    "// StdTLVertex (D3DTLVERTEX)\n"
    "struct VSIn\n"
    "{\n"
    "    float4 pos : POSITION;  // sx, sy, sz, rhw\n"
    "    float4 color : COLOR;   // diffuse, D3DCOLOR is ARGB (BGRA bytes)\n"
    "    float2 uv : TEXCOORD0;\n"
    "};\n"
    "struct PSIn\n"
    "{\n"
    "    float4 pos : SV_Position;\n"
    "    float4 color : COLOR;\n"
    "    float2 uv : TEXCOORD0;\n"
    "    float depth : TEXCOORD1; // eye depth (w), interpolated perspective correct\n"
    "};\n"
    "PSIn VSMain(VSIn i)\n"
    "{\n"
    "    // Pre-transformed vertex in pixels, D3D 6 pixel centers are at integer coordinates.\n"
    "    // Multiply by w to get perspective correct interpolation.\n"
    "    // Vertices behind the camera have a negative rhw, their negative w gets them clipped like in D3D 6.\n"
    "    // Row 0 of the back buffer is at the top (NDC y = 1).\n"
    "    float w = i.pos.w != 0.0 ? 1.0 / i.pos.w : 1.0;\n"
    "    float2 ndc = (i.pos.xy + 0.5) * u_pixelToNdc - 1.0;\n"
    "    ndc.y = -ndc.y;\n"
    "    PSIn o;\n"
    "    o.pos   = float4(ndc * w, i.pos.z * w, w);\n"
    "    o.color = i.color.bgra;\n"
    "    o.uv    = i.uv;\n"
    "    o.depth = w;\n"
    "    return o;\n"
    "}\n"
    "float4 PSMain(PSIn i) : SV_Target\n"
    "{\n"
    "    // D3DTOP_MODULATE of texture and diffuse for both color and alpha\n"
    "    float4 color = i.color;\n"
    "#ifdef TEXTURED\n"
    "    color *= u_tex.Sample(u_sampler, i.uv);\n"
    "#endif\n"
    "    if ( color.a < u_alphaRef ) {\n"
    "        discard;\n"
    "    }\n"
    "#ifdef FOG\n"
    "    // Linear table fog on eye depth (w-fog)\n"
    "    float f   = saturate((u_fogRange.y - i.depth) / (u_fogRange.y - u_fogRange.x));\n"
    "    color.rgb = lerp(u_fogColor.rgb, color.rgb, f);\n"
    "#endif\n"
    "    return color;\n"
    "}\n";

static ID3D11VertexShader* std3D_pVS                 = NULL;
static ID3D11InputLayout* std3D_pInputLayout         = NULL;
static ID3D11PixelShader* std3D_apPS[STD3D_NUM_PS]   = { 0 };
static ID3D11Buffer* std3D_pConstants                = NULL;
static ID3D11SamplerState* std3D_apSamplers[STD3D_NUM_SAMPLERS] = { 0 };

// Vertex & index data are appended to dynamic buffers, which are discarded when full
static ID3D11Buffer* std3D_pVertexBuffer = NULL;
static ID3D11Buffer* std3D_pIndexBuffer  = NULL;
static UINT std3D_vertexBufferSize       = 0;
static UINT std3D_indexBufferSize        = 0;
static UINT std3D_vertexBufferPos        = 0;
static UINT std3D_indexBufferPos         = 0;

// State set on the context, NULL/invalid when unknown
static StdD3D11Constants std3D_constants;
static bool std3D_bConstantsSet;
static bool std3D_bStateSet;
static ID3D11PixelShader* std3D_pCurPS;
static const StdD3D11Texture* std3D_pCurTexture;
static ID3D11SamplerState* std3D_pCurSampler;
static int std3D_curDepthWrite;
static int std3D_curPrimitive;

void stdD3D11_Invalidate3DState(void)
{
    std3D_bStateSet     = false;
    std3D_pCurPS        = NULL;
    std3D_pCurTexture   = NULL;
    std3D_pCurSampler   = NULL;
    std3D_curDepthWrite = -1;
    std3D_curPrimitive  = -1;
}

void stdD3D11_Forget3DTexture(const StdD3D11Texture* pTex)
{
    if ( std3D_pCurTexture == pTex ) {
        std3D_pCurTexture = NULL;
    }
}

static ID3D11Buffer* std3D_CreateDynamicBuffer(UINT size, UINT bindFlags)
{
    D3D11_BUFFER_DESC desc = { 0 };
    desc.ByteWidth      = size;
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = bindFlags;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ID3D11Buffer* pBuffer = NULL;
    if ( FAILED(ID3D11Device_CreateBuffer(stdD3D11_g.pDevice, &desc, NULL, &pBuffer)) )
    {
        STDLOG_ERROR("Direct3D 11: Error creating buffer of %u bytes.\n", size);
        return NULL;
    }

    return pBuffer;
}

static void std3D_DestroyShaders(void)
{
    STDD3D11_RELEASE(std3D_pVS);
    STDD3D11_RELEASE(std3D_pInputLayout);
    for ( size_t i = 0; i < STD3D_NUM_PS; ++i ) {
        STDD3D11_RELEASE(std3D_apPS[i]);
    }
}

void stdD3D11_Release3D(void)
{
    std3D_DestroyShaders();
    STDD3D11_RELEASE(std3D_pConstants);
    STDD3D11_RELEASE(std3D_pVertexBuffer);
    STDD3D11_RELEASE(std3D_pIndexBuffer);
    for ( size_t i = 0; i < STD3D_NUM_SAMPLERS; ++i ) {
        STDD3D11_RELEASE(std3D_apSamplers[i]);
    }

    std3D_vertexBufferSize = 0;
    std3D_indexBufferSize  = 0;
    std3D_bConstantsSet    = false;
    stdD3D11_Invalidate3DState();
}

static int std3D_CreateShaders(void)
{
    ID3DBlob* pVSCode = stdD3D11_CompileShader(std3D_aShader, "VSMain", "vs_4_0", NULL);
    if ( !pVSCode ) {
        return 0;
    }

    static const D3D11_INPUT_ELEMENT_DESC aElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 }, // sx, sy, sz, rhw
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,     0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // color
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // tu, tv
    };
    static_assert(sizeof(StdTLVertex) == 32, "sizeof(StdTLVertex) == 32");

    bool bOk = SUCCEEDED(ID3D11Device_CreateVertexShader(stdD3D11_g.pDevice, ID3D10Blob_GetBufferPointer(pVSCode), ID3D10Blob_GetBufferSize(pVSCode), NULL, &std3D_pVS))
        && SUCCEEDED(ID3D11Device_CreateInputLayout(stdD3D11_g.pDevice, aElements, (UINT)STD_ARRAYLEN(aElements), ID3D10Blob_GetBufferPointer(pVSCode), ID3D10Blob_GetBufferSize(pVSCode), &std3D_pInputLayout));
    STDD3D11_RELEASE(pVSCode);

    for ( size_t i = 0; bOk && i < STD3D_NUM_PS; ++i )
    {
        D3D_SHADER_MACRO aDefines[3] = { 0 };
        size_t numDefines = 0;
        if ( i & STD3D_PS_TEXTURED ) {
            aDefines[numDefines++] = (D3D_SHADER_MACRO){ "TEXTURED", "1" };
        }
        if ( i & STD3D_PS_FOG ) {
            aDefines[numDefines++] = (D3D_SHADER_MACRO){ "FOG", "1" };
        }

        ID3DBlob* pPSCode = stdD3D11_CompileShader(std3D_aShader, "PSMain", "ps_4_0", aDefines);
        bOk = pPSCode && SUCCEEDED(ID3D11Device_CreatePixelShader(stdD3D11_g.pDevice, ID3D10Blob_GetBufferPointer(pPSCode), ID3D10Blob_GetBufferSize(pPSCode), NULL, &std3D_apPS[i]));
        STDD3D11_RELEASE(pPSCode);
    }

    if ( !bOk )
    {
        STDLOG_ERROR("Direct3D 11: Error creating std3D shaders.\n");
        std3D_DestroyShaders();
        return 0;
    }

    return 1;
}

int stdD3D11_Open3D(void)
{
    if ( !stdD3D11_g.pColorTex ) {
        return 0;
    }

    if ( !std3D_pVS && !std3D_CreateShaders() ) {
        return 0;
    }

    if ( !std3D_pConstants )
    {
        D3D11_BUFFER_DESC desc = { 0 };
        desc.ByteWidth      = sizeof(StdD3D11Constants);
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if ( FAILED(ID3D11Device_CreateBuffer(stdD3D11_g.pDevice, &desc, NULL, &std3D_pConstants)) )
        {
            STDLOG_ERROR("Direct3D 11: Error creating constant buffer.\n");
            return 0;
        }
    }

    std3D_bConstantsSet = false;
    stdD3D11_Invalidate3DState();
    return 1;
}

void stdD3D11_Close3D(void)
{
    stdD3D11_Invalidate3DState();
}

static ID3D11SamplerState* std3D_GetSampler(const StdGpuDrawState* pState)
{
    size_t idx = (size_t)pState->filter + 3 * (size_t)pState->mipFilter + 9 * (size_t)pState->bClampU + 18 * (size_t)pState->bClampV;
    if ( idx >= STD3D_NUM_SAMPLERS ) {
        return NULL;
    }

    if ( std3D_apSamplers[idx] ) {
        return std3D_apSamplers[idx];
    }

    // Same as the Vulkan samplers: the bilinear mip filter picks the nearest level
    bool bMipLinear = pState->mipFilter == STD3D_MIPMAPFILTER_TRILINEAR;
    D3D11_SAMPLER_DESC desc = { 0 };
    if ( pState->filter == STDGPU_TEXFILTER_ANISOTROPIC )
    {
        desc.Filter        = D3D11_FILTER_ANISOTROPIC;
        desc.MaxAnisotropy = 16;
    }
    else if ( pState->filter == STDGPU_TEXFILTER_BILINEAR ) {
        desc.Filter = bMipLinear ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    }
    else {
        desc.Filter = bMipLinear ? D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
    }

    desc.AddressU       = pState->bClampU ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressV       = pState->bClampV ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.MinLOD         = 0.0f;
    desc.MaxLOD         = pState->mipFilter == STD3D_MIPMAPFILTER_NONE ? 0.0f : D3D11_FLOAT32_MAX;

    if ( FAILED(ID3D11Device_CreateSamplerState(stdD3D11_g.pDevice, &desc, &std3D_apSamplers[idx])) )
    {
        STDLOG_ERROR("Direct3D 11: Error creating sampler.\n");
        return NULL;
    }

    return std3D_apSamplers[idx];
}

static void std3D_SetConstants(const StdGpuDrawState* pState)
{
    StdD3D11Constants constants = { 0 };
    constants.pixelToNdc[0] = 2.0f / (float)stdD3D11_g.width;
    constants.pixelToNdc[1] = 2.0f / (float)stdD3D11_g.height;
    constants.alphaRef      = pState->alphaRef;
    if ( pState->bFog )
    {
        memcpy(constants.fogColor, pState->fogColor, sizeof(pState->fogColor));
        constants.fogRange[0] = pState->fogStart;
        constants.fogRange[1] = pState->fogEnd;
    }
    else
    {
        // Keep the fog values set, so switching fog on and off doesn't update the buffer
        memcpy(constants.fogColor, std3D_constants.fogColor, sizeof(constants.fogColor));
        memcpy(constants.fogRange, std3D_constants.fogRange, sizeof(constants.fogRange));
    }

    if ( std3D_bConstantsSet && memcmp(&constants, &std3D_constants, sizeof(constants)) == 0 ) {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    if ( FAILED(ID3D11DeviceContext_Map(stdD3D11_g.pContext, (ID3D11Resource*)std3D_pConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)) ) {
        return;
    }

    memcpy(mapped.pData, &constants, sizeof(constants));
    ID3D11DeviceContext_Unmap(stdD3D11_g.pContext, (ID3D11Resource*)std3D_pConstants, 0);
    std3D_constants     = constants;
    std3D_bConstantsSet = true;
}

// Appends data to a dynamic buffer, (re)creating it when it's too small. Returns the offset of the data or (UINT)-1.
static UINT std3D_AppendToBuffer(ID3D11Buffer** ppBuffer, UINT* pSize, UINT* pPos, UINT minSize, UINT bindFlags, const void* pData, UINT dataSize, UINT align)
{
    if ( !*ppBuffer || *pSize < dataSize )
    {
        UINT size = minSize;
        while ( size < dataSize ) {
            size *= 2;
        }

        STDD3D11_RELEASE(*ppBuffer);
        *ppBuffer = std3D_CreateDynamicBuffer(size, bindFlags);
        *pSize    = *ppBuffer ? size : 0;
        *pPos     = size; // Discard on first use
        std3D_bStateSet = false; // Buffer binding changed
        if ( !*ppBuffer ) {
            return (UINT)-1;
        }
    }

    UINT pos = (*pPos + align - 1) / align * align;
    D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
    if ( pos + dataSize > *pSize )
    {
        pos     = 0;
        mapType = D3D11_MAP_WRITE_DISCARD;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    if ( FAILED(ID3D11DeviceContext_Map(stdD3D11_g.pContext, (ID3D11Resource*)*ppBuffer, 0, mapType, 0, &mapped)) ) {
        return (UINT)-1;
    }

    memcpy((uint8_t*)mapped.pData + pos, pData, dataSize);
    ID3D11DeviceContext_Unmap(stdD3D11_g.pContext, (ID3D11Resource*)*ppBuffer, 0);
    *pPos = pos + dataSize;
    return pos;
}

void stdD3D11_Draw(StdGpuPrimitive primitive, const StdGpuDrawState* pState, void* pNativeTex, const StdTLVertex* aVerts, size_t numVerts, const uint16_t* aIndices, size_t numIndices)
{
    const StdD3D11Texture* pTexture = (const StdD3D11Texture*)pNativeTex;
    if ( !std3D_pVS || !stdD3D11_g.pColorTex || !numVerts ) {
        return;
    }

    ID3D11DeviceContext* pContext = stdD3D11_g.pContext;
    stdD3D11_BeginRendering();

    const UINT vertexOffset = std3D_AppendToBuffer(&std3D_pVertexBuffer, &std3D_vertexBufferSize, &std3D_vertexBufferPos, STD3D_VERTEXBUFFER_SIZE, D3D11_BIND_VERTEX_BUFFER,
        aVerts, (UINT)(numVerts * sizeof(StdTLVertex)), sizeof(StdTLVertex));
    if ( vertexOffset == (UINT)-1 ) {
        return;
    }

    UINT indexOffset = 0;
    if ( numIndices )
    {
        indexOffset = std3D_AppendToBuffer(&std3D_pIndexBuffer, &std3D_indexBufferSize, &std3D_indexBufferPos, STD3D_INDEXBUFFER_SIZE, D3D11_BIND_INDEX_BUFFER,
            aIndices, (UINT)(numIndices * sizeof(uint16_t)), sizeof(uint16_t));
        if ( indexOffset == (UINT)-1 ) {
            return;
        }
    }

    if ( !std3D_bStateSet )
    {
        const UINT stride = sizeof(StdTLVertex);
        const UINT offset = 0;
        ID3D11DeviceContext_IASetInputLayout(pContext, std3D_pInputLayout);
        ID3D11DeviceContext_IASetVertexBuffers(pContext, 0, 1, &std3D_pVertexBuffer, &stride, &offset);
        ID3D11DeviceContext_IASetIndexBuffer(pContext, std3D_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
        ID3D11DeviceContext_VSSetShader(pContext, std3D_pVS, NULL, 0);
        ID3D11DeviceContext_VSSetConstantBuffers(pContext, 0, 1, &std3D_pConstants);
        ID3D11DeviceContext_PSSetConstantBuffers(pContext, 0, 1, &std3D_pConstants);
        ID3D11DeviceContext_RSSetState(pContext, stdD3D11_g.pRasterState);
        ID3D11DeviceContext_OMSetBlendState(pContext, stdD3D11_g.pBlendState, NULL, 0xFFFFFFFFu);
        std3D_bStateSet = true;
    }

    if ( std3D_curDepthWrite != (int)pState->bDepthWrite )
    {
        ID3D11DeviceContext_OMSetDepthStencilState(pContext, stdD3D11_g.apDepthStates[pState->bDepthWrite ? 1 : 0], 0);
        std3D_curDepthWrite = pState->bDepthWrite;
    }

    if ( std3D_curPrimitive != (int)primitive )
    {
        static const D3D11_PRIMITIVE_TOPOLOGY aTopologies[STDGPU_NUM_PRIMITIVES] = {
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST
        };
        ID3D11DeviceContext_IASetPrimitiveTopology(pContext, aTopologies[primitive]);
        std3D_curPrimitive = primitive;
    }

    ID3D11PixelShader* pPS = std3D_apPS[(pTexture ? STD3D_PS_TEXTURED : 0) | (pState->bFog ? STD3D_PS_FOG : 0)];
    if ( std3D_pCurPS != pPS )
    {
        ID3D11DeviceContext_PSSetShader(pContext, pPS, NULL, 0);
        std3D_pCurPS = pPS;
    }

    if ( pTexture )
    {
        if ( std3D_pCurTexture != pTexture )
        {
            ID3D11DeviceContext_PSSetShaderResources(pContext, 0, 1, (ID3D11ShaderResourceView* const*)&pTexture->pView);
            std3D_pCurTexture = pTexture;
        }

        ID3D11SamplerState* pSampler = std3D_GetSampler(pState);
        if ( pSampler && std3D_pCurSampler != pSampler )
        {
            ID3D11DeviceContext_PSSetSamplers(pContext, 0, 1, &pSampler);
            std3D_pCurSampler = pSampler;
        }
    }

    std3D_SetConstants(pState);

    const INT baseVertex = (INT)(vertexOffset / sizeof(StdTLVertex));
    if ( numIndices ) {
        ID3D11DeviceContext_DrawIndexed(pContext, (UINT)numIndices, indexOffset / sizeof(uint16_t), baseVertex);
    }
    else {
        ID3D11DeviceContext_Draw(pContext, (UINT)numVerts, (UINT)baseVertex);
    }
}
