#include "stdGl.h"

#include <std/General/std.h>
#include <std/General/stdMath.h>
#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>
#include <std/SDL/stdSDL.h>

#include <stdlib.h>
#include <string.h>

#define STDGL_VIDEO_MEMORY 0x40000000u // GLES has no memory query, report 1 GiB

#define STDGL_DEFINE_FUNCTION(type, name) type stdGl_##name = NULL;
STDGL_FUNCTIONS(STDGL_DEFINE_FUNCTION)
#undef STDGL_DEFINE_FUNCTION

StdGlContext stdGl_g = { 0 };

static const char stdGl_aPresentVertShader[] =
    "attribute vec2 a_pos;\n"
    "varying vec2 v_uv;\n"
    "void main()\n"
    "{\n"
    "    // Back buffer rows are top-down: the top of the window samples row 0\n"
    "    v_uv        = vec2(a_pos.x * 0.5 + 0.5, 0.5 - a_pos.y * 0.5);\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

static const char stdGl_aPresentFragShader[] =
    "precision mediump float;\n"
    "uniform sampler2D u_tex;\n"
    "varying vec2 v_uv;\n"
    "void main()\n"
    "{\n"
    "    gl_FragColor = vec4(texture2D(u_tex, v_uv).rgb, 1.0);\n"
    "}\n";

static void stdGl_ResetStateCache(void)
{
    stdGl_g.boundFbo     = -1;
    stdGl_g.boundProgram = 0;
    stdGl_g.boundTexture = 0;
    stdGl_g.depthMask    = -1;
    stdGl_g.bDepthTest   = -1;
    stdGl_g.bBlend       = -1;
    stdGl_g.bScissor     = -1;
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static bool stdGl_HasExtension(const char* pExtensions, const char* pName)
{
    size_t len = strlen(pName);
    for ( const char* pCur = pExtensions; pCur && (pCur = strstr(pCur, pName)) != NULL; pCur += len )
    {
        if ( (pCur == pExtensions || pCur[-1] == ' ') && (pCur[len] == ' ' || pCur[len] == '\0') ) {
            return true;
        }
    }

    return false;
}

static int stdGl_LoadFunctions(void)
{
#define STDGL_LOAD_FUNCTION(type, name) \
    stdGl_##name = (type)SDL_GL_GetProcAddress(#name); \
    if ( !stdGl_##name ) \
    { \
        STDLOG_ERROR("OpenGL ES function %s not found.\n", #name); \
        return 0; \
    }

    STDGL_FUNCTIONS(STDGL_LOAD_FUNCTION)
#undef STDGL_LOAD_FUNCTION
    return 1;
}

// Loads the GL library, the context is created with the back buffer (it needs an OpenGL game window)
static int stdGl_Startup(void)
{
    if ( stdGl_g.bLibrary ) {
        return 1;
    }

    stdSDL_SetGLAttributes();
    if ( !SDL_GL_LoadLibrary(NULL) )
    {
        STDLOG_ERROR("Error loading the OpenGL ES library: %s\n", SDL_GetError());
        return 0;
    }

    stdGl_g.bLibrary = true;
    return 1;
}

static int stdGl_CreateContext(SDL_Window* pWindow)
{
    if ( stdGl_g.context )
    {
        if ( stdGl_g.pWindow == pWindow ) {
            return 1;
        }

        STDLOG_ERROR("The OpenGL ES context belongs to another window.\n");
        return 0;
    }

    if ( !pWindow || !(SDL_GetWindowFlags(pWindow) & SDL_WINDOW_OPENGL) )
    {
        STDLOG_ERROR("The game window isn't an OpenGL window.\n");
        return 0;
    }

    SDL_GLContext context = SDL_GL_CreateContext(pWindow);
    if ( !context )
    {
        STDLOG_ERROR("Error creating OpenGL ES 2.0 context: %s\n", SDL_GetError());
        return 0;
    }

    stdGl_g.pWindow      = pWindow;
    stdGl_g.context      = context;
    stdGl_g.swapInterval = -1;

    if ( !SDL_GL_MakeCurrent(pWindow, context) || !stdGl_LoadFunctions() )
    {
        STDLOG_ERROR("Error initializing OpenGL ES context: %s\n", SDL_GetError());
        SDL_GL_DestroyContext(context);
        stdGl_g.pWindow = NULL;
        stdGl_g.context = NULL;
        return 0;
    }

    const char* pRenderer   = (const char*)glGetString(GL_RENDERER);
    const char* pVersion    = (const char*)glGetString(GL_VERSION);
    const char* pExtensions = (const char*)glGetString(GL_EXTENSIONS);

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &stdGl_g.maxTextureSize);
    stdGl_g.bNpot = stdGl_HasExtension(pExtensions, "GL_OES_texture_npot") || (pVersion && strncmp(pVersion, "OpenGL ES 3", 11) == 0);

    stdGl_g.maxAnisotropy = 0.0f;
    if ( stdGl_HasExtension(pExtensions, "GL_EXT_texture_filter_anisotropic") ) {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &stdGl_g.maxAnisotropy);
    }

    if ( stdGl_HasExtension(pExtensions, "GL_OES_depth24") ) {
        stdGl_g.depthFormat = GL_DEPTH_COMPONENT24_OES;
    }
    else if ( stdGl_HasExtension(pExtensions, "GL_OES_packed_depth_stencil") ) {
        stdGl_g.depthFormat = GL_DEPTH24_STENCIL8_OES;
    }
    else {
        stdGl_g.depthFormat = GL_DEPTH_COMPONENT16;
    }

    STDLOG_STATUS("OpenGL ES: %s, %s, max texture %d, anisotropy %g, npot %d, depth 0x%X.\n",
        pRenderer ? pRenderer : "?", pVersion ? pVersion : "?", stdGl_g.maxTextureSize, (double)stdGl_g.maxAnisotropy, stdGl_g.bNpot, stdGl_g.depthFormat);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glDepthFunc(GL_LEQUAL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DITHER);
    glActiveTexture(GL_TEXTURE0);
    stdGl_ResetStateCache();
    return 1;
}

static void stdGl_DestroyBackBuffer(void);

static void stdGl_Shutdown(void)
{
    stdGl_DestroyBackBuffer();

    // Materials may still hold textures (freed later), so the context is kept alive until all are gone
    if ( stdGl_g.numLiveTextures ) {
        return;
    }

    if ( stdGl_g.context )
    {
        if ( stdGl_g.presentProgram ) {
            glDeleteProgram(stdGl_g.presentProgram);
        }

        SDL_GL_MakeCurrent(stdGl_g.pWindow, NULL);
        SDL_GL_DestroyContext(stdGl_g.context);
    }

    if ( stdGl_g.bLibrary ) {
        SDL_GL_UnloadLibrary();
    }

    bool bNoVSync = stdGl_g.bNoVSync;
    memset(&stdGl_g, 0, sizeof(stdGl_g));
    stdGl_g.bNoVSync = bNoVSync;
}

// State cache

void stdGl_BindFramebuffer(GLuint fbo)
{
    if ( stdGl_g.boundFbo != (GLint)fbo )
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        stdGl_g.boundFbo = (GLint)fbo;
    }
}

void stdGl_UseProgram(GLuint program)
{
    if ( stdGl_g.boundProgram != program )
    {
        glUseProgram(program);
        stdGl_g.boundProgram = program;
    }
}

void stdGl_BindTexture(GLuint tex)
{
    if ( stdGl_g.boundTexture != tex )
    {
        glBindTexture(GL_TEXTURE_2D, tex);
        stdGl_g.boundTexture = tex;
    }
}

void stdGl_SetDepthMask(bool bEnable)
{
    if ( stdGl_g.depthMask != (int)bEnable )
    {
        glDepthMask(bEnable ? GL_TRUE : GL_FALSE);
        stdGl_g.depthMask = bEnable;
    }
}

void stdGl_SetCap(GLenum cap, int* pState, bool bEnable)
{
    if ( *pState != (int)bEnable )
    {
        if ( bEnable ) {
            glEnable(cap);
        }
        else {
            glDisable(cap);
        }
        *pState = bEnable;
    }
}

static GLuint stdGl_CompileShader(GLenum type, const char* pSource)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &pSource, NULL);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if ( !status )
    {
        char aLog[1024] = { 0 };
        glGetShaderInfoLog(shader, sizeof(aLog) - 1, NULL, aLog);
        STDLOG_ERROR("Error compiling OpenGL ES shader: %s\n", aLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint stdGl_CreateProgram(const char* pVertSource, const char* pFragSource)
{
    GLuint vert = stdGl_CompileShader(GL_VERTEX_SHADER, pVertSource);
    GLuint frag = stdGl_CompileShader(GL_FRAGMENT_SHADER, pFragSource);
    if ( !vert || !frag )
    {
        if ( vert ) {
            glDeleteShader(vert);
        }
        if ( frag ) {
            glDeleteShader(frag);
        }
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glBindAttribLocation(program, STDGL_ATTRIB_POSITION, "a_pos");
    glBindAttribLocation(program, STDGL_ATTRIB_COLOR, "a_color");
    glBindAttribLocation(program, STDGL_ATTRIB_TEXCOORD, "a_uv");
    glLinkProgram(program);
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if ( !status )
    {
        char aLog[1024] = { 0 };
        glGetProgramInfoLog(program, sizeof(aLog) - 1, NULL, aLog);
        STDLOG_ERROR("Error linking OpenGL ES program: %s\n", aLog);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

// Back buffer

static int stdGl_CreateBackBuffer(HWND hwnd, uint32_t width, uint32_t height)
{
    if ( !stdGl_Startup() || !stdGl_CreateContext((SDL_Window*)hwnd) ) {
        return 0;
    }

    stdGl_DestroyBackBuffer();

    stdGl_g.width  = width;
    stdGl_g.height = height;

    glGenTextures(1, &stdGl_g.colorTex);
    stdGl_BindTexture(stdGl_g.colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &stdGl_g.depthRb);
    glBindRenderbuffer(GL_RENDERBUFFER, stdGl_g.depthRb);
    glRenderbufferStorage(GL_RENDERBUFFER, stdGl_g.depthFormat, (GLsizei)width, (GLsizei)height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &stdGl_g.fbo);
    stdGl_BindFramebuffer(stdGl_g.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, stdGl_g.colorTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, stdGl_g.depthRb);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if ( status != GL_FRAMEBUFFER_COMPLETE )
    {
        STDLOG_ERROR("OpenGL ES back buffer framebuffer is incomplete (0x%X).\n", status);
        return 0;
    }

    stdGl_g.pShadowPixels = (uint8_t*)STDMALLOC((size_t)width * height * 4);
    stdGl_g.pStaging      = (uint8_t*)STDMALLOC((size_t)width * height * 4);
    if ( !stdGl_g.pShadowPixels || !stdGl_g.pStaging ) {
        return 0;
    }

    memset(stdGl_g.pShadowPixels, 0, (size_t)width * height * 4);

    // Start with a black frame and a cleared depth buffer
    stdGl_SetCap(GL_SCISSOR_TEST, &stdGl_g.bScissor, false);
    stdGl_SetDepthMask(true);
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    glClearDepthf(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);
    stdGl_g.bShadowDirty = true;
    stdGl_g.bGpuNewer    = false;
    return 1;
}

static void stdGl_DestroyBackBuffer(void)
{
    if ( !stdGl_g.context ) {
        return;
    }

    if ( stdGl_g.fbo )
    {
        stdGl_BindFramebuffer(0);
        glDeleteFramebuffers(1, &stdGl_g.fbo);
    }

    if ( stdGl_g.depthRb ) {
        glDeleteRenderbuffers(1, &stdGl_g.depthRb);
    }

    if ( stdGl_g.colorTex )
    {
        stdGl_BindTexture(0);
        glDeleteTextures(1, &stdGl_g.colorTex);
    }

    if ( stdGl_g.pShadowPixels ) {
        stdMemory_Free(stdGl_g.pShadowPixels);
    }

    if ( stdGl_g.pStaging ) {
        stdMemory_Free(stdGl_g.pStaging);
    }

    stdGl_g.fbo           = 0;
    stdGl_g.depthRb       = 0;
    stdGl_g.colorTex      = 0;
    stdGl_g.pShadowPixels = NULL;
    stdGl_g.pStaging      = NULL;
    stdGl_g.width         = 0;
    stdGl_g.height        = 0;
    stdGl_g.bShadowDirty  = false;
    stdGl_g.bGpuNewer     = false;
}

// Swaps the R and B bytes of 4-byte pixels (RGBA <-> BGRA)
static void stdGl_SwapRB(uint8_t* pDst, const uint8_t* pSrc, size_t numPixels)
{
    const uint32_t* pSrc32 = (const uint32_t*)pSrc;
    uint32_t* pDst32       = (uint32_t*)pDst;
    for ( size_t i = 0; i < numPixels; ++i )
    {
        uint32_t p = pSrc32[i];
        pDst32[i]  = (p & 0xFF00FF00u) | ((p >> 16) & 0xFFu) | ((p & 0xFFu) << 16);
    }
}

static void stdGl_UploadShadow(void)
{
    stdGl_SwapRB(stdGl_g.pStaging, stdGl_g.pShadowPixels, (size_t)stdGl_g.width * stdGl_g.height);
    stdGl_BindTexture(stdGl_g.colorTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)stdGl_g.width, (GLsizei)stdGl_g.height, GL_RGBA, GL_UNSIGNED_BYTE, stdGl_g.pStaging);

    stdGl_g.bShadowDirty = false;
    stdGl_g.bGpuNewer    = false;
}

void stdGl_BeginRendering(void)
{
    if ( stdGl_g.bShadowDirty ) {
        stdGl_UploadShadow();
    }

    if ( stdGl_g.boundFbo != (GLint)stdGl_g.fbo )
    {
        stdGl_BindFramebuffer(stdGl_g.fbo);
        glViewport(0, 0, (GLsizei)stdGl_g.width, (GLsizei)stdGl_g.height);
    }

    stdGl_SetCap(GL_SCISSOR_TEST, &stdGl_g.bScissor, false);
    stdGl_SetCap(GL_DEPTH_TEST, &stdGl_g.bDepthTest, true);
    stdGl_SetCap(GL_BLEND, &stdGl_g.bBlend, true);
    stdGl_g.bGpuNewer = true;
}

static void stdGl_SetNoVSync(bool bNoVSync)
{
    stdGl_g.bNoVSync = bNoVSync;
}

static int stdGl_CreatePresentProgram(void)
{
    if ( !stdGl_g.presentProgram )
    {
        stdGl_g.presentProgram = stdGl_CreateProgram(stdGl_aPresentVertShader, stdGl_aPresentFragShader);
        if ( !stdGl_g.presentProgram ) {
            return 0;
        }

        stdGl_UseProgram(stdGl_g.presentProgram);
        glUniform1i(glGetUniformLocation(stdGl_g.presentProgram, "u_tex"), 0);
    }

    return 1;
}

static int stdGl_Present(void)
{
    if ( !stdGl_g.fbo || !stdGl_CreatePresentProgram() ) {
        return 1;
    }

    if ( stdGl_g.bShadowDirty ) {
        stdGl_UploadShadow();
    }

    int swapInterval = stdGl_g.bNoVSync ? 0 : 1;
    if ( stdGl_g.swapInterval != swapInterval )
    {
        if ( !SDL_GL_SetSwapInterval(swapInterval) ) {
            STDLOG_ERROR("Error setting swap interval %d: %s\n", swapInterval, SDL_GetError());
        }
        stdGl_g.swapInterval = swapInterval;
    }

    int winWidth = 0, winHeight = 0;
    SDL_GetWindowSizeInPixels(stdGl_g.pWindow, &winWidth, &winHeight);
    if ( winWidth <= 0 || winHeight <= 0 ) {
        return 0; // Minimized
    }

    // Scale to the window keeping the aspect ratio
    uint32_t dstWidth     = (uint32_t)winWidth;
    uint32_t dstHeight    = (uint32_t)winHeight;
    uint32_t scaledWidth  = dstWidth;
    uint32_t scaledHeight = (uint32_t)((uint64_t)dstWidth * stdGl_g.height / stdGl_g.width);
    if ( scaledHeight > dstHeight )
    {
        scaledHeight = dstHeight;
        scaledWidth  = (uint32_t)((uint64_t)dstHeight * stdGl_g.width / stdGl_g.height);
    }

    stdGl_g.presentScale = (float)scaledHeight / (float)stdGl_g.height;

    stdGl_BindFramebuffer(0);
    stdGl_SetCap(GL_SCISSOR_TEST, &stdGl_g.bScissor, false);
    stdGl_SetCap(GL_DEPTH_TEST, &stdGl_g.bDepthTest, false);
    stdGl_SetCap(GL_BLEND, &stdGl_g.bBlend, false);
    glViewport(0, 0, winWidth, winHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // GL viewport origin is bottom-left
    GLint dstX = (GLint)(dstWidth - scaledWidth) / 2;
    GLint dstY = (GLint)(dstHeight - scaledHeight) - (GLint)(dstHeight - scaledHeight) / 2;
    glViewport(dstX, dstY, (GLsizei)scaledWidth, (GLsizei)scaledHeight);

    GLint filter = scaledWidth == stdGl_g.width && scaledHeight == stdGl_g.height ? GL_NEAREST : GL_LINEAR;
    stdGl_BindTexture(stdGl_g.colorTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    static const float aQuad[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    stdGl_UseProgram(stdGl_g.presentProgram);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(STDGL_ATTRIB_POSITION);
    glDisableVertexAttribArray(STDGL_ATTRIB_COLOR);
    glDisableVertexAttribArray(STDGL_ATTRIB_TEXCOORD);
    glVertexAttribPointer(STDGL_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, aQuad);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if ( !SDL_GL_SwapWindow(stdGl_g.pWindow) ) {
        STDLOG_ERROR("Error presenting: %s\n", SDL_GetError());
    }

    return 0;
}

static float stdGl_GetPresentScale(void)
{
    return stdGl_g.presentScale;
}

// CPU mirror

static uint8_t* stdGl_GetShadowPixels(void)
{
    return stdGl_g.pShadowPixels;
}

static void stdGl_SyncShadowForRead(void)
{
    if ( !stdGl_g.bGpuNewer || !stdGl_g.fbo ) {
        return;
    }

    stdGl_BindFramebuffer(stdGl_g.fbo);
    glReadPixels(0, 0, (GLsizei)stdGl_g.width, (GLsizei)stdGl_g.height, GL_RGBA, GL_UNSIGNED_BYTE, stdGl_g.pStaging);
    stdGl_SwapRB(stdGl_g.pShadowPixels, stdGl_g.pStaging, (size_t)stdGl_g.width * stdGl_g.height);
    stdGl_g.bGpuNewer = false;
}

static void stdGl_MarkShadowDirty(void)
{
    stdGl_g.bShadowDirty = true;
    stdGl_g.bGpuNewer    = false;
}

static void stdGl_ClearColor(uint32_t xrgb, const StdRect* pRect)
{
    if ( !stdGl_g.fbo ) {
        return;
    }

    stdGl_BeginRendering();
    if ( pRect )
    {
        // StdRect is (left, top, width, height) here, like in stdDisplay_VBufferFill
        int32_t x0 = STDMATH_CLAMP(pRect->left, 0, (int32_t)stdGl_g.width);
        int32_t y0 = STDMATH_CLAMP(pRect->top, 0, (int32_t)stdGl_g.height);
        int32_t x1 = STDMATH_CLAMP(pRect->left + pRect->right, 0, (int32_t)stdGl_g.width);
        int32_t y1 = STDMATH_CLAMP(pRect->top + pRect->bottom, 0, (int32_t)stdGl_g.height);
        if ( x1 <= x0 || y1 <= y0 ) {
            return;
        }

        // Rows are top-down, so scissor y is the top row
        stdGl_SetCap(GL_SCISSOR_TEST, &stdGl_g.bScissor, true);
        glScissor(x0, y0, x1 - x0, y1 - y0);
    }

    glClearColor((float)((xrgb >> 16) & 0xFF) / 255.0f, (float)((xrgb >> 8) & 0xFF) / 255.0f, (float)(xrgb & 0xFF) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    stdGl_SetCap(GL_SCISSOR_TEST, &stdGl_g.bScissor, false);
}

static void stdGl_ClearDepth(void)
{
    if ( !stdGl_g.fbo ) {
        return;
    }

    stdGl_BeginRendering();
    stdGl_SetDepthMask(true);
    glClearDepthf(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);
}

// Device

static bool stdGl_IsStarted(void)
{
    return stdGl_g.bLibrary;
}

static const char* stdGl_GetDeviceName(void)
{
    // The renderer is only known once the context exists, keep the name the same for the saved display settings
    return "OpenGL ES 2.0 device";
}

static size_t stdGl_GetVideoMemory(void)
{
    return STDGL_VIDEO_MEMORY;
}

static uint32_t stdGl_GetMaxTextureSize(void)
{
    return stdGl_g.maxTextureSize > 0 ? (uint32_t)stdGl_g.maxTextureSize : 4096; // Until the context exists
}

static bool stdGl_HasAnisotropy(void)
{
    return stdGl_g.maxAnisotropy > 1.0f;
}

static bool stdGl_HasBackBuffer(void)
{
    return stdGl_g.fbo != 0;
}

// Textures

static bool stdGl_IsPowerOfTwo(uint32_t value)
{
    return value && (value & (value - 1)) == 0;
}

static void* stdGl_CreateTexture(uint32_t width, uint32_t height, uint32_t numMipLevels, StdGpuTexLayout layout)
{
    if ( !stdGl_g.context ) {
        return NULL;
    }

    StdGlTexture* pTex = (StdGlTexture*)STDMALLOC(sizeof(StdGlTexture));
    if ( !pTex ) {
        return NULL;
    }

    memset(pTex, 0, sizeof(*pTex));
    pTex->width        = width;
    pTex->height       = height;
    pTex->numMipLevels = numMipLevels;
    pTex->layout       = layout;
    pTex->bNpotLimited = !stdGl_g.bNpot && (!stdGl_IsPowerOfTwo(width) || !stdGl_IsPowerOfTwo(height));

    // GLES 2.0 has no GL_TEXTURE_MAX_LEVEL, a mipmapped texture needs all levels down to 1x1
    pTex->numGlLevels = 1;
    if ( numMipLevels > 1 && !pTex->bNpotLimited )
    {
        uint32_t size = width > height ? width : height;
        while ( size > 1 )
        {
            size >>= 1;
            ++pTex->numGlLevels;
        }
    }

    glGenTextures(1, &pTex->tex);
    stdGl_BindTexture(pTex->tex);

    // Sampler state is set per draw, see stdGl3D.c
    pTex->minFilter = pTex->numGlLevels > 1 ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR;
    pTex->magFilter = GL_LINEAR;
    pTex->wrapS     = GL_REPEAT;
    pTex->wrapT     = GL_REPEAT;
    pTex->anisotropy = 1.0f;
    if ( pTex->bNpotLimited )
    {
        pTex->wrapS = GL_CLAMP_TO_EDGE;
        pTex->wrapT = GL_CLAMP_TO_EDGE;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if ( pTex->numGlLevels == 1 ) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    ++stdGl_g.numLiveTextures;
    return pTex;
}

// Converts a level to R8G8B8A8 rows
static void stdGl_ConvertTextureLevel(uint8_t* pDst, const uint8_t* pSrc, size_t rowSize, uint32_t width, uint32_t height, StdGpuTexLayout layout)
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

static void stdGl_LevelSize(const StdGlTexture* pTex, uint32_t level, uint32_t* pWidth, uint32_t* pHeight)
{
    uint32_t width  = pTex->width >> level;
    uint32_t height = pTex->height >> level;
    *pWidth  = width ? width : 1;
    *pHeight = height ? height : 1;
}

static void stdGl_UploadTextureLevel(void* pNativeTex, uint32_t level, const uint8_t* pPixels, size_t rowSize)
{
    StdGlTexture* pTex = (StdGlTexture*)pNativeTex;
    if ( !pTex || level >= pTex->numGlLevels ) {
        return;
    }

    uint32_t width, height;
    stdGl_LevelSize(pTex, level, &width, &height);

    uint8_t* pRgba = (uint8_t*)STDMALLOC((size_t)width * height * 4);
    if ( !pRgba ) {
        return;
    }

    stdGl_ConvertTextureLevel(pRgba, pPixels, rowSize, width, height, pTex->layout);

    stdGl_BindTexture(pTex->tex);
    glTexImage2D(GL_TEXTURE_2D, (GLint)level, GL_RGBA, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pRgba);

    // The last level is kept to generate the missing levels of the chain
    if ( pTex->pLastLevel ) {
        stdMemory_Free(pTex->pLastLevel);
    }
    pTex->pLastLevel = pRgba;
    pTex->lastLevel  = level;
}

static void stdGl_FinishTextureUpload(void* pNativeTex)
{
    StdGlTexture* pTex = (StdGlTexture*)pNativeTex;
    if ( !pTex ) {
        return;
    }

    // Box filter the levels the game didn't provide
    uint8_t* pSrc = pTex->pLastLevel;
    for ( uint32_t level = pTex->lastLevel + 1; pSrc && level < pTex->numGlLevels; ++level )
    {
        uint32_t srcWidth, srcHeight, width, height;
        stdGl_LevelSize(pTex, level - 1, &srcWidth, &srcHeight);
        stdGl_LevelSize(pTex, level, &width, &height);

        uint8_t* pDst = (uint8_t*)STDMALLOC((size_t)width * height * 4);
        if ( !pDst ) {
            break;
        }

        for ( uint32_t y = 0; y < height; ++y )
        {
            uint32_t y0 = y * 2 < srcHeight ? y * 2 : srcHeight - 1;
            uint32_t y1 = y * 2 + 1 < srcHeight ? y * 2 + 1 : y0;
            for ( uint32_t x = 0; x < width; ++x )
            {
                uint32_t x0 = x * 2 < srcWidth ? x * 2 : srcWidth - 1;
                uint32_t x1 = x * 2 + 1 < srcWidth ? x * 2 + 1 : x0;
                for ( uint32_t c = 0; c < 4; ++c )
                {
                    uint32_t sum = pSrc[((size_t)y0 * srcWidth + x0) * 4 + c] + pSrc[((size_t)y0 * srcWidth + x1) * 4 + c]
                        + pSrc[((size_t)y1 * srcWidth + x0) * 4 + c] + pSrc[((size_t)y1 * srcWidth + x1) * 4 + c];
                    pDst[((size_t)y * width + x) * 4 + c] = (uint8_t)((sum + 2) / 4);
                }
            }
        }

        stdGl_BindTexture(pTex->tex);
        glTexImage2D(GL_TEXTURE_2D, (GLint)level, GL_RGBA, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pDst);

        stdMemory_Free(pSrc);
        pSrc = pDst;
    }

    if ( pSrc ) {
        stdMemory_Free(pSrc);
    }

    pTex->pLastLevel = NULL;
}

static void stdGl_DestroyTexture(void* pNativeTex)
{
    StdGlTexture* pTex = (StdGlTexture*)pNativeTex;
    if ( !pTex ) {
        return;
    }

    if ( stdGl_g.context )
    {
        if ( stdGl_g.boundTexture == pTex->tex ) {
            stdGl_BindTexture(0);
        }
        glDeleteTextures(1, &pTex->tex);
        --stdGl_g.numLiveTextures;
    }

    if ( pTex->pLastLevel ) {
        stdMemory_Free(pTex->pLastLevel);
    }

    stdMemory_Free(pTex);
}

const StdGpuBackend stdGl_g_backend = {
    .pApiName              = "OpenGL ES 2.0",
    .windowApi             = STDWINDOW_API_OPENGL,
    .pfStartup             = stdGl_Startup,
    .pfShutdown            = stdGl_Shutdown,
    .pfIsStarted           = stdGl_IsStarted,
    .pfGetDeviceName       = stdGl_GetDeviceName,
    .pfGetVideoMemory      = stdGl_GetVideoMemory,
    .pfGetMaxTextureSize   = stdGl_GetMaxTextureSize,
    .pfHasAnisotropy       = stdGl_HasAnisotropy,
    .pfCreateBackBuffer    = stdGl_CreateBackBuffer,
    .pfDestroyBackBuffer   = stdGl_DestroyBackBuffer,
    .pfHasBackBuffer       = stdGl_HasBackBuffer,
    .pfSetNoVSync          = stdGl_SetNoVSync,
    .pfPresent             = stdGl_Present,
    .pfGetPresentScale     = stdGl_GetPresentScale,
    .pfGetShadowPixels     = stdGl_GetShadowPixels,
    .pfSyncShadowForRead   = stdGl_SyncShadowForRead,
    .pfMarkShadowDirty     = stdGl_MarkShadowDirty,
    .pfClearColor          = stdGl_ClearColor,
    .pfClearDepth          = stdGl_ClearDepth,
    .pfOpen3D              = stdGl_Open3D,
    .pfClose3D             = stdGl_Close3D,
    .pfDraw                = stdGl_Draw,
    .pfCreateTexture       = stdGl_CreateTexture,
    .pfUploadTextureLevel  = stdGl_UploadTextureLevel,
    .pfFinishTextureUpload = stdGl_FinishTextureUpload,
    .pfDestroyTexture      = stdGl_DestroyTexture,
};
