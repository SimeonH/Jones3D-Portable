#include "stdD3D11.h"

#include <std/General/std.h>
#include <std/General/stdMath.h>
#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>

#include <SDL3/SDL_video.h>

#include <string.h>

#define STDD3D11_MAX_VIDEO_MEMORY 0x7FFFFFFFu // The game does signed 32-bit arithmetic on it

StdD3D11Context stdD3D11_g = { 0 };

static SDL_Window* stdD3D11_pWindow = NULL; // Game window of the swap chain

static const char stdD3D11_aPresentShader[] =
    "Texture2D u_tex : register(t0);\n"
    "SamplerState u_sampler : register(s0);\n"
    "struct VSOut\n"
    "{\n"
    "    float4 pos : SV_Position;\n"
    "    float2 uv : TEXCOORD0;\n"
    "};\n"
    "VSOut VSMain(uint id : SV_VertexID)\n"
    "{\n"
    "    // Fullscreen triangle, the top of the window samples row 0 of the back buffer\n"
    "    VSOut o;\n"
    "    o.uv  = float2((id << 1) & 2, id & 2);\n"
    "    o.pos = float4(o.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n"
    "    return o;\n"
    "}\n"
    "float4 PSMain(VSOut i) : SV_Target\n"
    "{\n"
    "    return float4(u_tex.Sample(u_sampler, i.uv).rgb, 1.0);\n"
    "}\n";

ID3DBlob* stdD3D11_CompileShader(const char* pSource, const char* pEntryPoint, const char* pTarget, const D3D_SHADER_MACRO* aDefines)
{
    if ( !stdD3D11_g.pfCompile ) {
        return NULL;
    }

    ID3DBlob* pCode   = NULL;
    ID3DBlob* pErrors = NULL;
    HRESULT hr = stdD3D11_g.pfCompile(pSource, strlen(pSource), "std3D", aDefines, NULL, pEntryPoint, pTarget, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pCode, &pErrors);
    if ( FAILED(hr) )
    {
        STDLOG_ERROR("Error compiling Direct3D 11 shader %s: %s\n", pEntryPoint, pErrors ? (const char*)ID3D10Blob_GetBufferPointer(pErrors) : "?");
        STDD3D11_RELEASE(pErrors);
        STDD3D11_RELEASE(pCode);
        return NULL;
    }

    STDD3D11_RELEASE(pErrors);
    return pCode;
}

static void stdD3D11_LoadCompiler(void)
{
    if ( stdD3D11_g.pfCompile ) {
        return;
    }

    stdD3D11_g.hCompiler = LoadLibraryA("d3dcompiler_47.dll");
    if ( stdD3D11_g.hCompiler ) {
        stdD3D11_g.pfCompile = (pD3DCompile)(void*)GetProcAddress(stdD3D11_g.hCompiler, "D3DCompile");
    }

    if ( !stdD3D11_g.pfCompile ) {
        STDLOG_ERROR("Direct3D 11: d3dcompiler_47.dll not found.\n");
    }
}

static void stdD3D11_ReadAdapterInfo(void)
{
    IDXGIDevice* pDxgiDevice = NULL;
    IDXGIAdapter* pAdapter   = NULL;
    if ( FAILED(ID3D11Device_QueryInterface(stdD3D11_g.pDevice, &IID_IDXGIDevice, (void**)&pDxgiDevice))
        || FAILED(IDXGIDevice_GetAdapter(pDxgiDevice, &pAdapter)) )
    {
        STDD3D11_RELEASE(pDxgiDevice);
        STD_STRCPY(stdD3D11_g.aDeviceName, "Direct3D 11 device");
        return;
    }

    DXGI_ADAPTER_DESC desc;
    if ( SUCCEEDED(IDXGIAdapter_GetDesc(pAdapter, &desc)) )
    {
        WideCharToMultiByte(CP_ACP, 0, desc.Description, -1, stdD3D11_g.aDeviceName, (int)sizeof(stdD3D11_g.aDeviceName), NULL, NULL);
        stdD3D11_g.aDeviceName[sizeof(stdD3D11_g.aDeviceName) - 1] = '\0';

        // Integrated GPUs have little or no dedicated memory, they use shared system memory
        size_t memory = desc.DedicatedVideoMemory ? desc.DedicatedVideoMemory : desc.SharedSystemMemory;
        stdD3D11_g.videoMemory = memory > STDD3D11_MAX_VIDEO_MEMORY ? STDD3D11_MAX_VIDEO_MEMORY : memory;
    }

    IDXGIAdapter_GetParent(pAdapter, &IID_IDXGIFactory1, (void**)&stdD3D11_g.pFactory);
    STDD3D11_RELEASE(pAdapter);
    STDD3D11_RELEASE(pDxgiDevice);
}

static int stdD3D11_CreateStates(void)
{
    D3D11_RASTERIZER_DESC raster = { 0 };
    raster.FillMode        = D3D11_FILL_SOLID;
    raster.CullMode        = D3D11_CULL_NONE;
    raster.DepthClipEnable = TRUE;
    if ( FAILED(ID3D11Device_CreateRasterizerState(stdD3D11_g.pDevice, &raster, &stdD3D11_g.pRasterState)) ) {
        return 0;
    }

    // Direct3D 6 alpha blending of the game (SRCALPHA, INVSRCALPHA)
    D3D11_BLEND_DESC blend = { 0 };
    blend.RenderTarget[0].BlendEnable           = TRUE;
    blend.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if ( FAILED(ID3D11Device_CreateBlendState(stdD3D11_g.pDevice, &blend, &stdD3D11_g.pBlendState)) ) {
        return 0;
    }

    blend.RenderTarget[0].BlendEnable = FALSE;
    if ( FAILED(ID3D11Device_CreateBlendState(stdD3D11_g.pDevice, &blend, &stdD3D11_g.pNoBlendState)) ) {
        return 0;
    }

    D3D11_DEPTH_STENCIL_DESC depth = { 0 };
    depth.DepthEnable = TRUE;
    depth.DepthFunc   = D3D11_COMPARISON_LESS_EQUAL;
    for ( size_t i = 0; i < STD_ARRAYLEN(stdD3D11_g.apDepthStates); ++i )
    {
        depth.DepthWriteMask = i ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        if ( FAILED(ID3D11Device_CreateDepthStencilState(stdD3D11_g.pDevice, &depth, &stdD3D11_g.apDepthStates[i])) ) {
            return 0;
        }
    }

    depth.DepthEnable    = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    if ( FAILED(ID3D11Device_CreateDepthStencilState(stdD3D11_g.pDevice, &depth, &stdD3D11_g.pNoDepthState)) ) {
        return 0;
    }

    return 1;
}

static void stdD3D11_Shutdown(void);

// Creates the device, it doesn't need the game window
static int stdD3D11_Startup(void)
{
    if ( stdD3D11_g.pDevice ) {
        return 1;
    }

    static const D3D_FEATURE_LEVEL aLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, aLevels, (UINT)STD_ARRAYLEN(aLevels), D3D11_SDK_VERSION,
        &stdD3D11_g.pDevice, &stdD3D11_g.featureLevel, &stdD3D11_g.pContext);
    if ( hr == E_INVALIDARG )
    {
        // Direct3D 11.0 runtime doesn't know feature level 11_1
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, aLevels + 1, (UINT)STD_ARRAYLEN(aLevels) - 1, D3D11_SDK_VERSION,
            &stdD3D11_g.pDevice, &stdD3D11_g.featureLevel, &stdD3D11_g.pContext);
    }

    if ( FAILED(hr) )
    {
        STDLOG_ERROR("Direct3D 11: Error creating device (0x%08lX).\n", (unsigned long)hr);
        return 0;
    }

    ID3D11DeviceContext_QueryInterface(stdD3D11_g.pContext, &IID_ID3D11DeviceContext1, (void**)&stdD3D11_g.pContext1);
    stdD3D11_ReadAdapterInfo();
    stdD3D11_LoadCompiler();

    if ( !stdD3D11_g.pFactory || !stdD3D11_g.pfCompile || !stdD3D11_CreateStates() )
    {
        STDLOG_ERROR("Direct3D 11: Error initializing device.\n");
        stdD3D11_Shutdown();
        return 0;
    }

    STDLOG_STATUS("Direct3D 11: %s, feature level %X.%X, video memory %u MB.\n", stdD3D11_g.aDeviceName,
        (unsigned)(stdD3D11_g.featureLevel >> 12), (unsigned)((stdD3D11_g.featureLevel >> 8) & 0xF), (unsigned)(stdD3D11_g.videoMemory >> 20));
    return 1;
}

static void stdD3D11_DestroyBackBuffer(void);
static void stdD3D11_DestroySwapChain(void);

static void stdD3D11_Shutdown(void)
{
    stdD3D11_DestroyBackBuffer();
    stdD3D11_DestroySwapChain();

    // Materials may still hold textures (freed later), so the device is kept alive until all are gone
    if ( stdD3D11_g.numLiveTextures ) {
        return;
    }

    stdD3D11_Release3D();

    STDD3D11_RELEASE(stdD3D11_g.pPresentVS);
    STDD3D11_RELEASE(stdD3D11_g.pPresentPS);
    for ( size_t i = 0; i < STD_ARRAYLEN(stdD3D11_g.apPresentSamplers); ++i ) {
        STDD3D11_RELEASE(stdD3D11_g.apPresentSamplers[i]);
    }

    STDD3D11_RELEASE(stdD3D11_g.pRasterState);
    STDD3D11_RELEASE(stdD3D11_g.pBlendState);
    STDD3D11_RELEASE(stdD3D11_g.pNoBlendState);
    for ( size_t i = 0; i < STD_ARRAYLEN(stdD3D11_g.apDepthStates); ++i ) {
        STDD3D11_RELEASE(stdD3D11_g.apDepthStates[i]);
    }
    STDD3D11_RELEASE(stdD3D11_g.pNoDepthState);

    if ( stdD3D11_g.pContext ) {
        ID3D11DeviceContext_ClearState(stdD3D11_g.pContext);
    }

    STDD3D11_RELEASE(stdD3D11_g.pFactory);
    STDD3D11_RELEASE(stdD3D11_g.pContext1);
    STDD3D11_RELEASE(stdD3D11_g.pContext);
    STDD3D11_RELEASE(stdD3D11_g.pDevice);

    if ( stdD3D11_g.hCompiler ) {
        FreeLibrary(stdD3D11_g.hCompiler);
    }

    bool bNoVSync = stdD3D11_g.bNoVSync;
    memset(&stdD3D11_g, 0, sizeof(stdD3D11_g));
    stdD3D11_g.bNoVSync = bNoVSync;
}

// Swap chain

static void stdD3D11_DestroySwapChain(void)
{
    if ( stdD3D11_g.pContext ) {
        ID3D11DeviceContext_OMSetRenderTargets(stdD3D11_g.pContext, 0, NULL, NULL);
    }

    stdD3D11_g.bTargetBound = false;
    STDD3D11_RELEASE(stdD3D11_g.pSwapView);
    STDD3D11_RELEASE(stdD3D11_g.pSwapChain);
    stdD3D11_g.hwnd       = NULL;
    stdD3D11_g.swapWidth  = 0;
    stdD3D11_g.swapHeight = 0;
    stdD3D11_pWindow      = NULL;
}

static int stdD3D11_CreateSwapView(void)
{
    ID3D11Texture2D* pBuffer = NULL;
    if ( FAILED(IDXGISwapChain_GetBuffer(stdD3D11_g.pSwapChain, 0, &IID_ID3D11Texture2D, (void**)&pBuffer)) ) {
        return 0;
    }

    HRESULT hr = ID3D11Device_CreateRenderTargetView(stdD3D11_g.pDevice, (ID3D11Resource*)pBuffer, NULL, &stdD3D11_g.pSwapView);
    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D_GetDesc(pBuffer, &desc);
    stdD3D11_g.swapWidth  = desc.Width;
    stdD3D11_g.swapHeight = desc.Height;
    ID3D11Texture2D_Release(pBuffer);
    return SUCCEEDED(hr);
}

static int stdD3D11_CreateSwapChain(SDL_Window* pWindow)
{
    HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(pWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if ( !hwnd )
    {
        STDLOG_ERROR("Direct3D 11: The game window has no Win32 window.\n");
        return 0;
    }

    if ( stdD3D11_g.pSwapChain && stdD3D11_g.hwnd == hwnd ) {
        return 1;
    }

    stdD3D11_DestroySwapChain();

    // Flip model on Windows 10 (FLIP_DISCARD) and 8 (FLIP_SEQUENTIAL), else the blit model
    static const DXGI_SWAP_EFFECT aEffects[] = { DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_SWAP_EFFECT_DISCARD };
    HRESULT hr = E_FAIL;
    for ( size_t i = 0; i < STD_ARRAYLEN(aEffects) && FAILED(hr); ++i )
    {
        DXGI_SWAP_CHAIN_DESC desc = { 0 };
        desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // Size 0: from the window
        desc.SampleDesc.Count  = 1;
        desc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount       = aEffects[i] == DXGI_SWAP_EFFECT_DISCARD ? 1 : 2;
        desc.OutputWindow      = hwnd;
        desc.Windowed          = TRUE; // Fullscreen is a borderless SDL window
        desc.SwapEffect        = aEffects[i];
        hr = IDXGIFactory1_CreateSwapChain(stdD3D11_g.pFactory, (IUnknown*)stdD3D11_g.pDevice, &desc, &stdD3D11_g.pSwapChain);
        stdD3D11_g.bFlipModel = aEffects[i] != DXGI_SWAP_EFFECT_DISCARD;
    }

    if ( FAILED(hr) )
    {
        STDLOG_ERROR("Direct3D 11: Error creating swap chain (0x%08lX).\n", (unsigned long)hr);
        return 0;
    }

    // SDL handles fullscreen, DXGI shouldn't switch modes on Alt+Enter
    IDXGIFactory1_MakeWindowAssociation(stdD3D11_g.pFactory, hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);

    stdD3D11_g.hwnd  = hwnd;
    stdD3D11_pWindow = pWindow;
    if ( !stdD3D11_CreateSwapView() )
    {
        STDLOG_ERROR("Direct3D 11: Error creating swap chain view.\n");
        stdD3D11_DestroySwapChain();
        return 0;
    }

    return 1;
}

// Resizes the swap chain buffers to the window
static void stdD3D11_UpdateSwapChainSize(uint32_t width, uint32_t height)
{
    if ( width == stdD3D11_g.swapWidth && height == stdD3D11_g.swapHeight ) {
        return;
    }

    ID3D11DeviceContext_OMSetRenderTargets(stdD3D11_g.pContext, 0, NULL, NULL);
    stdD3D11_g.bTargetBound = false;
    STDD3D11_RELEASE(stdD3D11_g.pSwapView);

    HRESULT hr = IDXGISwapChain_ResizeBuffers(stdD3D11_g.pSwapChain, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if ( FAILED(hr) ) {
        STDLOG_ERROR("Direct3D 11: Error resizing swap chain (0x%08lX).\n", (unsigned long)hr);
    }

    if ( !stdD3D11_CreateSwapView() ) {
        STDLOG_ERROR("Direct3D 11: Error creating swap chain view.\n");
    }
}

static int stdD3D11_CreatePresentShaders(void)
{
    if ( stdD3D11_g.pPresentVS ) {
        return 1;
    }

    ID3DBlob* pVS = stdD3D11_CompileShader(stdD3D11_aPresentShader, "VSMain", "vs_4_0", NULL);
    ID3DBlob* pPS = stdD3D11_CompileShader(stdD3D11_aPresentShader, "PSMain", "ps_4_0", NULL);
    bool bOk = pVS && pPS
        && SUCCEEDED(ID3D11Device_CreateVertexShader(stdD3D11_g.pDevice, ID3D10Blob_GetBufferPointer(pVS), ID3D10Blob_GetBufferSize(pVS), NULL, &stdD3D11_g.pPresentVS))
        && SUCCEEDED(ID3D11Device_CreatePixelShader(stdD3D11_g.pDevice, ID3D10Blob_GetBufferPointer(pPS), ID3D10Blob_GetBufferSize(pPS), NULL, &stdD3D11_g.pPresentPS));
    STDD3D11_RELEASE(pVS);
    STDD3D11_RELEASE(pPS);

    for ( size_t i = 0; bOk && i < STD_ARRAYLEN(stdD3D11_g.apPresentSamplers); ++i )
    {
        D3D11_SAMPLER_DESC desc = { 0 };
        desc.Filter   = i ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MaxLOD   = 0.0f;
        bOk = SUCCEEDED(ID3D11Device_CreateSamplerState(stdD3D11_g.pDevice, &desc, &stdD3D11_g.apPresentSamplers[i]));
    }

    if ( !bOk )
    {
        STDD3D11_RELEASE(stdD3D11_g.pPresentVS);
        STDD3D11_RELEASE(stdD3D11_g.pPresentPS);
        for ( size_t i = 0; i < STD_ARRAYLEN(stdD3D11_g.apPresentSamplers); ++i ) {
            STDD3D11_RELEASE(stdD3D11_g.apPresentSamplers[i]);
        }
        return 0;
    }

    return 1;
}

// Back buffer

static int stdD3D11_CreateBackBuffer(HWND hwnd, uint32_t width, uint32_t height)
{
    if ( !stdD3D11_Startup() || !stdD3D11_CreateSwapChain((SDL_Window*)hwnd) || !stdD3D11_CreatePresentShaders() ) {
        return 0;
    }

    stdD3D11_DestroyBackBuffer();

    stdD3D11_g.width  = width;
    stdD3D11_g.height = height;

    D3D11_TEXTURE2D_DESC desc = { 0 };
    desc.Width            = width;
    desc.Height           = height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if ( FAILED(ID3D11Device_CreateTexture2D(stdD3D11_g.pDevice, &desc, NULL, &stdD3D11_g.pColorTex))
        || FAILED(ID3D11Device_CreateRenderTargetView(stdD3D11_g.pDevice, (ID3D11Resource*)stdD3D11_g.pColorTex, NULL, &stdD3D11_g.pColorView))
        || FAILED(ID3D11Device_CreateShaderResourceView(stdD3D11_g.pDevice, (ID3D11Resource*)stdD3D11_g.pColorTex, NULL, &stdD3D11_g.pColorSrv)) )
    {
        STDLOG_ERROR("Direct3D 11: Error creating back buffer %ux%u.\n", width, height);
        return 0;
    }

    desc.Usage          = D3D11_USAGE_STAGING;
    desc.BindFlags      = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if ( FAILED(ID3D11Device_CreateTexture2D(stdD3D11_g.pDevice, &desc, NULL, &stdD3D11_g.pStagingTex)) )
    {
        STDLOG_ERROR("Direct3D 11: Error creating back buffer readback texture.\n");
        return 0;
    }

    desc.Format         = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.Usage          = D3D11_USAGE_DEFAULT;
    desc.BindFlags      = D3D11_BIND_DEPTH_STENCIL;
    desc.CPUAccessFlags = 0;
    if ( FAILED(ID3D11Device_CreateTexture2D(stdD3D11_g.pDevice, &desc, NULL, &stdD3D11_g.pDepthTex))
        || FAILED(ID3D11Device_CreateDepthStencilView(stdD3D11_g.pDevice, (ID3D11Resource*)stdD3D11_g.pDepthTex, NULL, &stdD3D11_g.pDepthView)) )
    {
        STDLOG_ERROR("Direct3D 11: Error creating depth buffer.\n");
        return 0;
    }

    stdD3D11_g.pShadowPixels = (uint8_t*)STDMALLOC((size_t)width * height * 4);
    if ( !stdD3D11_g.pShadowPixels ) {
        return 0;
    }

    memset(stdD3D11_g.pShadowPixels, 0, (size_t)width * height * 4);

    // Start with a black frame and a cleared depth buffer
    ID3D11DeviceContext_ClearDepthStencilView(stdD3D11_g.pContext, stdD3D11_g.pDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
    stdD3D11_g.bShadowDirty = true;
    stdD3D11_g.bGpuNewer    = false;
    stdD3D11_g.bTargetBound = false;
    return 1;
}

static void stdD3D11_DestroyBackBuffer(void)
{
    if ( stdD3D11_g.pContext )
    {
        ID3D11DeviceContext_OMSetRenderTargets(stdD3D11_g.pContext, 0, NULL, NULL);
        ID3D11ShaderResourceView* pNullView = NULL;
        ID3D11DeviceContext_PSSetShaderResources(stdD3D11_g.pContext, 0, 1, &pNullView);
    }

    STDD3D11_RELEASE(stdD3D11_g.pColorSrv);
    STDD3D11_RELEASE(stdD3D11_g.pColorView);
    STDD3D11_RELEASE(stdD3D11_g.pColorTex);
    STDD3D11_RELEASE(stdD3D11_g.pDepthView);
    STDD3D11_RELEASE(stdD3D11_g.pDepthTex);
    STDD3D11_RELEASE(stdD3D11_g.pStagingTex);

    if ( stdD3D11_g.pShadowPixels ) {
        stdMemory_Free(stdD3D11_g.pShadowPixels);
    }

    stdD3D11_g.pShadowPixels = NULL;
    stdD3D11_g.width         = 0;
    stdD3D11_g.height        = 0;
    stdD3D11_g.bShadowDirty  = false;
    stdD3D11_g.bGpuNewer     = false;
    stdD3D11_g.bTargetBound  = false;
}

static void stdD3D11_UploadShadow(void)
{
    ID3D11DeviceContext_UpdateSubresource(stdD3D11_g.pContext, (ID3D11Resource*)stdD3D11_g.pColorTex, 0, NULL, stdD3D11_g.pShadowPixels, stdD3D11_g.width * 4, 0);
    stdD3D11_g.bShadowDirty = false;
    stdD3D11_g.bGpuNewer    = false;
}

void stdD3D11_BeginRendering(void)
{
    if ( stdD3D11_g.bShadowDirty ) {
        stdD3D11_UploadShadow();
    }

    if ( !stdD3D11_g.bTargetBound )
    {
        // The back buffer may still be bound for reading from the last present
        ID3D11ShaderResourceView* pNullView = NULL;
        ID3D11DeviceContext_PSSetShaderResources(stdD3D11_g.pContext, 0, 1, &pNullView);
        ID3D11DeviceContext_OMSetRenderTargets(stdD3D11_g.pContext, 1, &stdD3D11_g.pColorView, stdD3D11_g.pDepthView);

        D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (float)stdD3D11_g.width, (float)stdD3D11_g.height, 0.0f, 1.0f };
        ID3D11DeviceContext_RSSetViewports(stdD3D11_g.pContext, 1, &viewport);
        ID3D11DeviceContext_RSSetState(stdD3D11_g.pContext, stdD3D11_g.pRasterState);
        ID3D11DeviceContext_OMSetBlendState(stdD3D11_g.pContext, stdD3D11_g.pBlendState, NULL, 0xFFFFFFFFu);
        stdD3D11_g.bTargetBound = true;
    }

    stdD3D11_g.bGpuNewer = true;
}

static void stdD3D11_SetNoVSync(bool bNoVSync)
{
    stdD3D11_g.bNoVSync = bNoVSync;
}

static int stdD3D11_Present(void)
{
    if ( !stdD3D11_g.pColorTex || !stdD3D11_g.pSwapChain ) {
        return 1;
    }

    if ( stdD3D11_g.bShadowDirty ) {
        stdD3D11_UploadShadow();
    }

    int winWidth = 0, winHeight = 0;
    SDL_GetWindowSizeInPixels(stdD3D11_pWindow, &winWidth, &winHeight);
    if ( winWidth <= 0 || winHeight <= 0 ) {
        return 0; // Minimized
    }

    stdD3D11_UpdateSwapChainSize((uint32_t)winWidth, (uint32_t)winHeight);
    if ( !stdD3D11_g.pSwapView ) {
        return 0;
    }

    // Scale to the window keeping the aspect ratio
    uint32_t dstWidth     = stdD3D11_g.swapWidth;
    uint32_t dstHeight    = stdD3D11_g.swapHeight;
    uint32_t scaledWidth  = dstWidth;
    uint32_t scaledHeight = (uint32_t)((uint64_t)dstWidth * stdD3D11_g.height / stdD3D11_g.width);
    if ( scaledHeight > dstHeight )
    {
        scaledHeight = dstHeight;
        scaledWidth  = (uint32_t)((uint64_t)dstHeight * stdD3D11_g.width / stdD3D11_g.height);
    }

    stdD3D11_g.presentScale = (float)scaledHeight / (float)stdD3D11_g.height;

    ID3D11DeviceContext* pContext = stdD3D11_g.pContext;
    ID3D11DeviceContext_OMSetRenderTargets(pContext, 1, &stdD3D11_g.pSwapView, NULL);
    stdD3D11_g.bTargetBound = false;

    static const float aBlack[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    ID3D11DeviceContext_ClearRenderTargetView(pContext, stdD3D11_g.pSwapView, aBlack);

    D3D11_VIEWPORT viewport = { (float)((dstWidth - scaledWidth) / 2), (float)((dstHeight - scaledHeight) / 2), (float)scaledWidth, (float)scaledHeight, 0.0f, 1.0f };
    ID3D11DeviceContext_RSSetViewports(pContext, 1, &viewport);
    ID3D11DeviceContext_RSSetState(pContext, stdD3D11_g.pRasterState);
    ID3D11DeviceContext_OMSetBlendState(pContext, stdD3D11_g.pNoBlendState, NULL, 0xFFFFFFFFu);
    ID3D11DeviceContext_OMSetDepthStencilState(pContext, stdD3D11_g.pNoDepthState, 0);

    bool bLinear = scaledWidth != stdD3D11_g.width || scaledHeight != stdD3D11_g.height;
    ID3D11DeviceContext_IASetInputLayout(pContext, NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(pContext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(pContext, stdD3D11_g.pPresentVS, NULL, 0);
    ID3D11DeviceContext_PSSetShader(pContext, stdD3D11_g.pPresentPS, NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(pContext, 0, 1, &stdD3D11_g.pColorSrv);
    ID3D11DeviceContext_PSSetSamplers(pContext, 0, 1, &stdD3D11_g.apPresentSamplers[bLinear ? 1 : 0]);
    ID3D11DeviceContext_Draw(pContext, 3, 0);

    ID3D11ShaderResourceView* pNullView = NULL;
    ID3D11DeviceContext_PSSetShaderResources(pContext, 0, 1, &pNullView);

    HRESULT hr = IDXGISwapChain_Present(stdD3D11_g.pSwapChain, stdD3D11_g.bNoVSync ? 0 : 1, 0);
    if ( FAILED(hr) ) {
        STDLOG_ERROR("Direct3D 11: Error presenting (0x%08lX).\n", (unsigned long)hr);
    }

    // The 3D state was changed for the present
    stdD3D11_Invalidate3DState();
    return 0;
}

static float stdD3D11_GetPresentScale(void)
{
    return stdD3D11_g.presentScale;
}

// CPU mirror

static uint8_t* stdD3D11_GetShadowPixels(void)
{
    return stdD3D11_g.pShadowPixels;
}

static void stdD3D11_SyncShadowForRead(void)
{
    if ( !stdD3D11_g.bGpuNewer || !stdD3D11_g.pColorTex ) {
        return;
    }

    ID3D11DeviceContext_CopyResource(stdD3D11_g.pContext, (ID3D11Resource*)stdD3D11_g.pStagingTex, (ID3D11Resource*)stdD3D11_g.pColorTex);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if ( FAILED(ID3D11DeviceContext_Map(stdD3D11_g.pContext, (ID3D11Resource*)stdD3D11_g.pStagingTex, 0, D3D11_MAP_READ, 0, &mapped)) )
    {
        STDLOG_ERROR("Direct3D 11: Error reading back buffer.\n");
        return;
    }

    const size_t rowSize = (size_t)stdD3D11_g.width * 4;
    for ( uint32_t y = 0; y < stdD3D11_g.height; ++y ) {
        memcpy(stdD3D11_g.pShadowPixels + rowSize * y, (const uint8_t*)mapped.pData + (size_t)mapped.RowPitch * y, rowSize);
    }

    ID3D11DeviceContext_Unmap(stdD3D11_g.pContext, (ID3D11Resource*)stdD3D11_g.pStagingTex, 0);
    stdD3D11_g.bGpuNewer = false;
}

static void stdD3D11_MarkShadowDirty(void)
{
    stdD3D11_g.bShadowDirty = true;
    stdD3D11_g.bGpuNewer    = false;
}

static void stdD3D11_ClearColor(uint32_t xrgb, const StdRect* pRect)
{
    if ( !stdD3D11_g.pColorTex ) {
        return;
    }

    const float aColor[4] = { (float)((xrgb >> 16) & 0xFF) / 255.0f, (float)((xrgb >> 8) & 0xFF) / 255.0f, (float)(xrgb & 0xFF) / 255.0f, 1.0f };
    if ( !pRect )
    {
        stdD3D11_BeginRendering();
        ID3D11DeviceContext_ClearRenderTargetView(stdD3D11_g.pContext, stdD3D11_g.pColorView, aColor);
        return;
    }

    // StdRect is (left, top, width, height) here, like in stdDisplay_VBufferFill
    int32_t x0 = STDMATH_CLAMP(pRect->left, 0, (int32_t)stdD3D11_g.width);
    int32_t y0 = STDMATH_CLAMP(pRect->top, 0, (int32_t)stdD3D11_g.height);
    int32_t x1 = STDMATH_CLAMP(pRect->left + pRect->right, 0, (int32_t)stdD3D11_g.width);
    int32_t y1 = STDMATH_CLAMP(pRect->top + pRect->bottom, 0, (int32_t)stdD3D11_g.height);
    if ( x1 <= x0 || y1 <= y0 ) {
        return;
    }

    if ( stdD3D11_g.pContext1 )
    {
        stdD3D11_BeginRendering();
        D3D11_RECT rect = { x0, y0, x1, y1 };
        ID3D11DeviceContext1_ClearView(stdD3D11_g.pContext1, (ID3D11View*)stdD3D11_g.pColorView, aColor, &rect, 1);
        return;
    }

    // Direct3D 11.0 can't clear a part of a view, fill it in the CPU mirror
    stdD3D11_SyncShadowForRead();
    for ( int32_t y = y0; y < y1; ++y )
    {
        uint32_t* pRow = (uint32_t*)(stdD3D11_g.pShadowPixels + (size_t)stdD3D11_g.width * 4 * y);
        for ( int32_t x = x0; x < x1; ++x ) {
            pRow[x] = xrgb | 0xFF000000u;
        }
    }

    stdD3D11_MarkShadowDirty();
}

static void stdD3D11_ClearDepth(void)
{
    if ( !stdD3D11_g.pDepthView ) {
        return;
    }

    stdD3D11_BeginRendering();
    ID3D11DeviceContext_ClearDepthStencilView(stdD3D11_g.pContext, stdD3D11_g.pDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

// Device

static bool stdD3D11_IsStarted(void)
{
    return stdD3D11_g.pDevice != NULL;
}

static const char* stdD3D11_GetDeviceName(void)
{
    return stdD3D11_g.aDeviceName[0] ? stdD3D11_g.aDeviceName : "Direct3D 11 device";
}

static size_t stdD3D11_GetVideoMemory(void)
{
    return stdD3D11_g.videoMemory;
}

static uint32_t stdD3D11_GetMaxTextureSize(void)
{
    if ( !stdD3D11_g.pDevice ) {
        return 4096;
    }

    return stdD3D11_g.featureLevel >= D3D_FEATURE_LEVEL_11_0 ? D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION : D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
}

static bool stdD3D11_HasAnisotropy(void)
{
    return stdD3D11_g.pDevice != NULL; // 16x on all feature levels from 10_0
}

static bool stdD3D11_HasBackBuffer(void)
{
    return stdD3D11_g.pColorTex != NULL;
}

// Textures

static void* stdD3D11_CreateTexture(uint32_t width, uint32_t height, uint32_t numMipLevels, StdGpuTexLayout layout)
{
    if ( !stdD3D11_g.pDevice ) {
        return NULL;
    }

    StdD3D11Texture* pTex = (StdD3D11Texture*)STDMALLOC(sizeof(StdD3D11Texture));
    if ( !pTex ) {
        return NULL;
    }

    memset(pTex, 0, sizeof(*pTex));
    pTex->width        = width;
    pTex->height       = height;
    pTex->numMipLevels = numMipLevels ? numMipLevels : 1;
    pTex->layout       = layout;

    D3D11_TEXTURE2D_DESC desc = { 0 };
    desc.Width            = width;
    desc.Height           = height;
    desc.MipLevels        = pTex->numMipLevels; // The levels the game gives, the chain needn't go down to 1x1
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    if ( FAILED(ID3D11Device_CreateTexture2D(stdD3D11_g.pDevice, &desc, NULL, &pTex->pTexture))
        || FAILED(ID3D11Device_CreateShaderResourceView(stdD3D11_g.pDevice, (ID3D11Resource*)pTex->pTexture, NULL, &pTex->pView)) )
    {
        STDLOG_ERROR("Direct3D 11: Error creating texture %ux%u.\n", width, height);
        STDD3D11_RELEASE(pTex->pTexture);
        stdMemory_Free(pTex);
        return NULL;
    }

    ++stdD3D11_g.numLiveTextures;
    return pTex;
}

// Converts a level to R8G8B8A8 rows
static void stdD3D11_ConvertTextureLevel(uint8_t* pDst, const uint8_t* pSrc, size_t rowSize, uint32_t width, uint32_t height, StdGpuTexLayout layout)
{
    for ( uint32_t y = 0; y < height; ++y )
    {
        const uint8_t* pRow = pSrc + rowSize * y;
        uint8_t* pDstRow    = pDst + (size_t)width * 4 * y;
        switch ( layout )
        {
            case STDGPU_TEXLAYOUT_RGBA:
                memcpy(pDstRow, pRow, (size_t)width * 4);
                break;

            case STDGPU_TEXLAYOUT_RGBX:
                for ( uint32_t x = 0; x < width; ++x )
                {
                    pDstRow[x * 4 + 0] = pRow[x * 4 + 0];
                    pDstRow[x * 4 + 1] = pRow[x * 4 + 1];
                    pDstRow[x * 4 + 2] = pRow[x * 4 + 2];
                    pDstRow[x * 4 + 3] = 0xFF;
                }
                break;

            case STDGPU_TEXLAYOUT_ABGR:
                for ( uint32_t x = 0; x < width; ++x )
                {
                    pDstRow[x * 4 + 0] = pRow[x * 4 + 3];
                    pDstRow[x * 4 + 1] = pRow[x * 4 + 2];
                    pDstRow[x * 4 + 2] = pRow[x * 4 + 1];
                    pDstRow[x * 4 + 3] = pRow[x * 4 + 0];
                }
                break;
        }
    }
}

static void stdD3D11_UploadTextureLevel(void* pNativeTex, uint32_t level, const uint8_t* pPixels, size_t rowSize)
{
    StdD3D11Texture* pTex = (StdD3D11Texture*)pNativeTex;
    if ( !pTex || level >= pTex->numMipLevels || !stdD3D11_g.pContext ) {
        return;
    }

    uint32_t width  = pTex->width >> level;
    uint32_t height = pTex->height >> level;
    width  = width ? width : 1;
    height = height ? height : 1;

    if ( pTex->layout == STDGPU_TEXLAYOUT_RGBA )
    {
        ID3D11DeviceContext_UpdateSubresource(stdD3D11_g.pContext, (ID3D11Resource*)pTex->pTexture, level, NULL, pPixels, (UINT)rowSize, 0);
        return;
    }

    uint8_t* pRgba = (uint8_t*)STDMALLOC((size_t)width * height * 4);
    if ( !pRgba ) {
        return;
    }

    stdD3D11_ConvertTextureLevel(pRgba, pPixels, rowSize, width, height, pTex->layout);
    ID3D11DeviceContext_UpdateSubresource(stdD3D11_g.pContext, (ID3D11Resource*)pTex->pTexture, level, NULL, pRgba, width * 4, 0);
    stdMemory_Free(pRgba);
}

static void stdD3D11_FinishTextureUpload(void* pNativeTex)
{
    (void)pNativeTex; // Levels are uploaded directly
}

static void stdD3D11_DestroyTexture(void* pNativeTex)
{
    StdD3D11Texture* pTex = (StdD3D11Texture*)pNativeTex;
    if ( !pTex ) {
        return;
    }

    stdD3D11_Forget3DTexture(pTex);
    STDD3D11_RELEASE(pTex->pView);
    STDD3D11_RELEASE(pTex->pTexture);
    if ( stdD3D11_g.numLiveTextures ) {
        --stdD3D11_g.numLiveTextures;
    }

    stdMemory_Free(pTex);
}

const StdGpuBackend stdD3D11_g_backend = {
    .pApiName              = "Direct3D 11",
    .windowApi             = STDWINDOW_API_NATIVE,
    .pfStartup             = stdD3D11_Startup,
    .pfShutdown            = stdD3D11_Shutdown,
    .pfIsStarted           = stdD3D11_IsStarted,
    .pfGetDeviceName       = stdD3D11_GetDeviceName,
    .pfGetVideoMemory      = stdD3D11_GetVideoMemory,
    .pfGetMaxTextureSize   = stdD3D11_GetMaxTextureSize,
    .pfHasAnisotropy       = stdD3D11_HasAnisotropy,
    .pfCreateBackBuffer    = stdD3D11_CreateBackBuffer,
    .pfDestroyBackBuffer   = stdD3D11_DestroyBackBuffer,
    .pfHasBackBuffer       = stdD3D11_HasBackBuffer,
    .pfSetNoVSync          = stdD3D11_SetNoVSync,
    .pfPresent             = stdD3D11_Present,
    .pfGetPresentScale     = stdD3D11_GetPresentScale,
    .pfGetShadowPixels     = stdD3D11_GetShadowPixels,
    .pfSyncShadowForRead   = stdD3D11_SyncShadowForRead,
    .pfMarkShadowDirty     = stdD3D11_MarkShadowDirty,
    .pfClearColor          = stdD3D11_ClearColor,
    .pfClearDepth          = stdD3D11_ClearDepth,
    .pfOpen3D              = stdD3D11_Open3D,
    .pfClose3D             = stdD3D11_Close3D,
    .pfDraw                = stdD3D11_Draw,
    .pfCreateTexture       = stdD3D11_CreateTexture,
    .pfUploadTextureLevel  = stdD3D11_UploadTextureLevel,
    .pfFinishTextureUpload = stdD3D11_FinishTextureUpload,
    .pfDestroyTexture      = stdD3D11_DestroyTexture,
};
