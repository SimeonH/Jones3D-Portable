#ifndef STD_D3D11_STDD3D11_H
#define STD_D3D11_STDD3D11_H
#include <j3dcore/j3d.h>
#include <std/types.h>
#include <std/Gpu/stdGpu.h>

// D3DCOLORVALUE is already defined by the Direct3D 6 headers of the original renderer interface (std/types.h)
#define D3DCOLORVALUE_DEFINED
#define COBJMACROS
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>

J3D_EXTERN_C_START

// Shared Direct3D 11 device of the D3D11 renderer backend (stdDisplay + std3D through stdGpu.h).
//
// The game's back buffer is a color texture with a depth buffer at the game resolution, drawn scaled to the window
// on present. Its rows are top-down like the rows of the CPU mirror (BGRX), so the color texture is B8G8R8A8
// and uploads & readbacks copy rows as they are.
// Shaders are HLSL compiled at runtime with d3dcompiler_47.dll (part of Windows since 8.1, and of Wine).

typedef struct sStdD3D11Texture
{
    ID3D11Texture2D* pTexture;
    ID3D11ShaderResourceView* pView;
    uint32_t width;
    uint32_t height;
    uint32_t numMipLevels;
    StdGpuTexLayout layout;
} StdD3D11Texture;

// Constant buffer of the std3D shaders, see stdD3D113D.c
typedef struct sStdD3D11Constants
{
    float pixelToNdc[2]; // 2 / back buffer size
    float alphaRef;
    float pad0;
    float fogColor[4];
    float fogRange[2]; // linear fog start & end in eye depth (w)
    float pad1[2];
} StdD3D11Constants;

typedef struct sStdD3D11Context
{
    HMODULE hCompiler; // d3dcompiler_47.dll
    pD3DCompile pfCompile;

    ID3D11Device* pDevice;
    ID3D11DeviceContext* pContext;
    ID3D11DeviceContext1* pContext1; // NULL before D3D 11.1, for ClearView
    IDXGIFactory1* pFactory;
    D3D_FEATURE_LEVEL featureLevel;
    char aDeviceName[128];
    size_t videoMemory;
    size_t numLiveTextures;

    // Swap chain of the game window
    HWND hwnd;
    IDXGISwapChain* pSwapChain;
    ID3D11RenderTargetView* pSwapView;
    uint32_t swapWidth;
    uint32_t swapHeight;
    bool bFlipModel;
    bool bNoVSync;
    float presentScale;
    ID3D11VertexShader* pPresentVS;
    ID3D11PixelShader* pPresentPS;
    ID3D11SamplerState* apPresentSamplers[2]; // point, linear

    // Back buffer
    uint32_t width;
    uint32_t height;
    ID3D11Texture2D* pColorTex;
    ID3D11RenderTargetView* pColorView;
    ID3D11ShaderResourceView* pColorSrv;
    ID3D11Texture2D* pDepthTex;
    ID3D11DepthStencilView* pDepthView;
    ID3D11Texture2D* pStagingTex; // readback

    // CPU mirror of the back buffer (BGRX in memory)
    uint8_t* pShadowPixels;
    bool bShadowDirty; // CPU wrote to the mirror, upload before next GPU op
    bool bGpuNewer;    // GPU rendered to the color texture since last readback
    bool bTargetBound; // back buffer is the bound render target

    // Pipeline state
    ID3D11RasterizerState* pRasterState;
    ID3D11BlendState* pBlendState;
    ID3D11BlendState* pNoBlendState;
    ID3D11DepthStencilState* apDepthStates[2]; // depth write off, on
    ID3D11DepthStencilState* pNoDepthState;
} StdD3D11Context;

extern StdD3D11Context stdD3D11_g;

// std3D rendering of the stdGpu backend (stdD3D113D.c)
int stdD3D11_Open3D(void);
void stdD3D11_Close3D(void);
void stdD3D11_Draw(StdGpuPrimitive primitive, const StdGpuDrawState* pState, void* pNativeTex, const StdTLVertex* aVerts, size_t numVerts, const uint16_t* aIndices, size_t numIndices);
void stdD3D11_Release3D(void); // Releases the 3D resources with the device
void stdD3D11_Invalidate3DState(void); // The pipeline state was changed outside of std3D, e.g. by the present
void stdD3D11_Forget3DTexture(const StdD3D11Texture* pTex); // The texture is destroyed

void stdD3D11_BeginRendering(void); // Binds the back buffer for drawing, uploads the CPU mirror first if it's dirty

// Compiles HLSL source, returns the bytecode blob or NULL
ID3DBlob* stdD3D11_CompileShader(const char* pSource, const char* pEntryPoint, const char* pTarget, const D3D_SHADER_MACRO* aDefines);

#define STDD3D11_RELEASE(p) \
    do { \
        if ( (p) ) { \
            ((IUnknown*)(p))->lpVtbl->Release((IUnknown*)(p)); \
            (p) = NULL; \
        } \
    } while ( 0 )

J3D_EXTERN_C_END
#endif // STD_D3D11_STDD3D11_H
