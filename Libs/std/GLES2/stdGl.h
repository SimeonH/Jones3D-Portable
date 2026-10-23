#ifndef STD_GLES2_STDGL_H
#define STD_GLES2_STDGL_H
#include <j3dcore/j3d.h>
#include <std/types.h>
#include <std/Gpu/stdGpu.h>

// GL functions are loaded at runtime through SDL (see STDGL_FUNCTIONS), no GLES library or headers are needed
#define SDL_USE_BUILTIN_OPENGL_DEFINITIONS 1
#define GL_GLES_PROTOTYPES 0
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_opengles2.h>

J3D_EXTERN_C_START

// Shared OpenGL ES 2.0 context of the GLES2 renderer backend (stdDisplay + std3D through stdGpu.h).
//
// The game's back buffer is a framebuffer object (color texture + depth renderbuffer) at the game resolution,
// drawn scaled to the window on present. Its rows are stored top-down (row 0 is the top of the game image),
// so pre-transformed vertices map to it with the same transform as in the Vulkan backend
// and glReadPixels/glTexSubImage2D rows match the rows of the CPU mirror.

#define STDGL_FUNCTIONS(X) \
    X(PFNGLACTIVETEXTUREPROC, glActiveTexture) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
    X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) \
    X(PFNGLBINDTEXTUREPROC, glBindTexture) \
    X(PFNGLBLENDFUNCPROC, glBlendFunc) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
    X(PFNGLCLEARPROC, glClear) \
    X(PFNGLCLEARCOLORPROC, glClearColor) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) \
    X(PFNGLDELETEPROGRAMPROC, glDeleteProgram) \
    X(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers) \
    X(PFNGLDELETESHADERPROC, glDeleteShader) \
    X(PFNGLDELETETEXTURESPROC, glDeleteTextures) \
    X(PFNGLDEPTHFUNCPROC, glDepthFunc) \
    X(PFNGLDEPTHMASKPROC, glDepthMask) \
    X(PFNGLDISABLEPROC, glDisable) \
    X(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) \
    X(PFNGLDRAWARRAYSPROC, glDrawArrays) \
    X(PFNGLDRAWELEMENTSPROC, glDrawElements) \
    X(PFNGLENABLEPROC, glEnable) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) \
    X(PFNGLGENTEXTURESPROC, glGenTextures) \
    X(PFNGLGETERRORPROC, glGetError) \
    X(PFNGLGETINTEGERVPROC, glGetIntegerv) \
    X(PFNGLGETFLOATVPROC, glGetFloatv) \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
    X(PFNGLGETSTRINGPROC, glGetString) \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLPIXELSTOREIPROC, glPixelStorei) \
    X(PFNGLREADPIXELSPROC, glReadPixels) \
    X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) \
    X(PFNGLSCISSORPROC, glScissor) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLTEXIMAGE2DPROC, glTexImage2D) \
    X(PFNGLTEXPARAMETERFPROC, glTexParameterf) \
    X(PFNGLTEXPARAMETERIPROC, glTexParameteri) \
    X(PFNGLTEXSUBIMAGE2DPROC, glTexSubImage2D) \
    X(PFNGLUNIFORM1FPROC, glUniform1f) \
    X(PFNGLUNIFORM1IPROC, glUniform1i) \
    X(PFNGLUNIFORM2FPROC, glUniform2f) \
    X(PFNGLUNIFORM3FPROC, glUniform3f) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
    X(PFNGLVIEWPORTPROC, glViewport)

#define STDGL_DECLARE_FUNCTION(type, name) extern type stdGl_##name;
STDGL_FUNCTIONS(STDGL_DECLARE_FUNCTION)
#undef STDGL_DECLARE_FUNCTION

// Calls go to the loaded function pointers
#define glActiveTexture            stdGl_glActiveTexture
#define glAttachShader             stdGl_glAttachShader
#define glBindAttribLocation       stdGl_glBindAttribLocation
#define glBindBuffer               stdGl_glBindBuffer
#define glBindFramebuffer          stdGl_glBindFramebuffer
#define glBindRenderbuffer         stdGl_glBindRenderbuffer
#define glBindTexture              stdGl_glBindTexture
#define glBlendFunc                stdGl_glBlendFunc
#define glCheckFramebufferStatus   stdGl_glCheckFramebufferStatus
#define glClear                    stdGl_glClear
#define glClearColor               stdGl_glClearColor
#define glCompileShader            stdGl_glCompileShader
#define glCreateProgram            stdGl_glCreateProgram
#define glCreateShader             stdGl_glCreateShader
#define glDeleteFramebuffers       stdGl_glDeleteFramebuffers
#define glDeleteProgram            stdGl_glDeleteProgram
#define glDeleteRenderbuffers      stdGl_glDeleteRenderbuffers
#define glDeleteShader             stdGl_glDeleteShader
#define glDeleteTextures           stdGl_glDeleteTextures
#define glDepthFunc                stdGl_glDepthFunc
#define glDepthMask                stdGl_glDepthMask
#define glDisable                  stdGl_glDisable
#define glDisableVertexAttribArray stdGl_glDisableVertexAttribArray
#define glDrawArrays               stdGl_glDrawArrays
#define glDrawElements             stdGl_glDrawElements
#define glEnable                   stdGl_glEnable
#define glEnableVertexAttribArray  stdGl_glEnableVertexAttribArray
#define glFramebufferRenderbuffer  stdGl_glFramebufferRenderbuffer
#define glFramebufferTexture2D     stdGl_glFramebufferTexture2D
#define glGenFramebuffers          stdGl_glGenFramebuffers
#define glGenRenderbuffers         stdGl_glGenRenderbuffers
#define glGenTextures              stdGl_glGenTextures
#define glGetError                 stdGl_glGetError
#define glGetIntegerv              stdGl_glGetIntegerv
#define glGetFloatv                stdGl_glGetFloatv
#define glGetProgramInfoLog        stdGl_glGetProgramInfoLog
#define glGetProgramiv             stdGl_glGetProgramiv
#define glGetShaderInfoLog         stdGl_glGetShaderInfoLog
#define glGetShaderiv              stdGl_glGetShaderiv
#define glGetString                stdGl_glGetString
#define glGetUniformLocation       stdGl_glGetUniformLocation
#define glLinkProgram              stdGl_glLinkProgram
#define glPixelStorei              stdGl_glPixelStorei
#define glReadPixels               stdGl_glReadPixels
#define glRenderbufferStorage      stdGl_glRenderbufferStorage
#define glScissor                  stdGl_glScissor
#define glShaderSource             stdGl_glShaderSource
#define glTexImage2D               stdGl_glTexImage2D
#define glTexParameterf            stdGl_glTexParameterf
#define glTexParameteri            stdGl_glTexParameteri
#define glTexSubImage2D            stdGl_glTexSubImage2D
#define glUniform1f                stdGl_glUniform1f
#define glUniform1i                stdGl_glUniform1i
#define glUniform2f                stdGl_glUniform2f
#define glUniform3f                stdGl_glUniform3f
#define glUseProgram               stdGl_glUseProgram
#define glVertexAttribPointer      stdGl_glVertexAttribPointer
#define glViewport                 stdGl_glViewport

// Vertex attribute locations of all programs
#define STDGL_ATTRIB_POSITION 0
#define STDGL_ATTRIB_COLOR    1
#define STDGL_ATTRIB_TEXCOORD 2

typedef struct sStdGlTexture
{
    GLuint tex;
    uint32_t width;
    uint32_t height;
    uint32_t numMipLevels;   // levels given by the game
    uint32_t numGlLevels;    // levels of the GL texture, a full chain down to 1x1 when mipmapped
    StdGpuTexLayout layout;
    bool bNpotLimited;       // NPOT texture without OES_texture_npot: clamped and not mipmapped
    uint8_t* pLastLevel;     // RGBA8 pixels of the last uploaded level until the upload is finished
    uint32_t lastLevel;

    // Sampler state set on the texture
    GLint minFilter;
    GLint magFilter;
    GLint wrapS;
    GLint wrapT;
    float anisotropy;
} StdGlTexture;

typedef struct sStdGlContext
{
    bool bLibrary; // SDL GL library loaded
    SDL_Window* pWindow;
    SDL_GLContext context;
    char aDeviceName[128];
    GLint maxTextureSize;
    float maxAnisotropy; // 0 if not supported
    bool bNpot;          // GL_OES_texture_npot
    bool bDesktop;       // Desktop OpenGL compatibility context instead of OpenGL ES (see stdSDL_SetGLAttributes)
    const char* pShaderHeader; // Prepended to the GLSL ES 1.00 shader sources
    GLenum depthFormat;
    size_t numLiveTextures;

    // Back buffer
    uint32_t width;
    uint32_t height;
    GLuint fbo;
    GLuint colorTex;
    GLuint depthRb;
    bool bNoVSync;
    int swapInterval; // -1 unset
    float presentScale;
    GLuint presentProgram;

    // CPU mirror of the back buffer (BGRX in memory)
    uint8_t* pShadowPixels;
    uint8_t* pStaging; // RGBA rows for upload & readback
    bool bShadowDirty; // CPU wrote to the mirror, upload before next GPU op
    bool bGpuNewer;    // GPU rendered to the color texture since last readback

    // GL state cache
    GLint boundFbo;     // -1 unknown
    GLuint boundProgram;
    GLuint boundTexture;
    int depthMask;      // -1 unknown
    int bDepthTest;     // -1 unknown
    int bBlend;         // -1 unknown
    int bScissor;       // -1 unknown
} StdGlContext;

extern StdGlContext stdGl_g;

// std3D rendering of the stdGpu backend (stdGl3D.c)
int stdGl_Open3D(void);
void stdGl_Close3D(void);
void stdGl_Draw(StdGpuPrimitive primitive, const StdGpuDrawState* pState, void* pNativeTex, const StdTLVertex* aVerts, size_t numVerts, const uint16_t* aIndices, size_t numIndices);

// State helpers keeping the cache in stdGl_g
void stdGl_BindFramebuffer(GLuint fbo);
void stdGl_UseProgram(GLuint program);
void stdGl_BindTexture(GLuint tex);
void stdGl_SetDepthMask(bool bEnable);
void stdGl_SetCap(GLenum cap, int* pState, bool bEnable);

void stdGl_BeginRendering(void); // Binds the back buffer for drawing, uploads the CPU mirror first if it's dirty
GLuint stdGl_CreateProgram(const char* pVertSource, const char* pFragSource);

J3D_EXTERN_C_END
#endif // STD_GLES2_STDGL_H
