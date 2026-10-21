// OpenGL ES 2.0 3D rendering of the GPU std3D (see Gpu/std3D.c): programs, sampler state and draws.
// Same shading as the Vulkan backend (Vulkan/shaders), with a program variant per texture & fog combination.
#include "stdGl.h"

#include <std/General/std.h>
#include <std/General/stdUtil.h>

#include <string.h>

#define STDGL_PROGRAM_TEXTURED 0x1
#define STDGL_PROGRAM_FOG      0x2
#define STDGL_NUM_PROGRAMS     4

typedef struct sStdGlProgram
{
    GLuint program;
    GLint pixelToNdcLoc;
    GLint alphaRefLoc;
    GLint fogColorLoc;
    GLint fogRangeLoc;

    // Uniform values set on the program
    float pixelToNdc[2];
    float alphaRef;
    float fogColor[3];
    float fogRange[2];
} StdGlProgram;

static const char std3D_aVertShader[] =
    "// StdTLVertex (D3DTLVERTEX)\n"
    "attribute vec4 a_pos;   // sx, sy, sz, rhw\n"
    "attribute vec4 a_color; // diffuse, D3DCOLOR is ARGB (BGRA bytes)\n"
    "attribute vec2 a_uv;\n"
    "uniform vec2 u_pixelToNdc; // 2 / back buffer size\n"
    "varying vec4 v_color;\n"
    "varying vec2 v_uv;\n"
    "void main()\n"
    "{\n"
    "    // Pre-transformed vertex in pixels, D3D pixel centers are at integer coordinates.\n"
    "    // Multiply by w to get perspective correct interpolation (gl_FragCoord.w is then rhw).\n"
    "    // Vertices behind the camera have a negative rhw, their negative w gets them clipped like in D3D.\n"
    "    float w     = a_pos.w != 0.0 ? 1.0 / a_pos.w : 1.0;\n"
    "    vec2 ndc    = (a_pos.xy + 0.5) * u_pixelToNdc - 1.0;\n"
    "    gl_Position = vec4(ndc * w, (a_pos.z * 2.0 - 1.0) * w, w);\n"
    "    gl_PointSize = 1.0;\n"
    "    v_color = a_color.bgra;\n"
    "    v_uv    = a_uv;\n"
    "}\n";

static const char std3D_aFragShader[] =
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "uniform sampler2D u_tex;\n"
    "uniform float u_alphaRef; // fragments with alpha below are discarded (D3D alpha test GREATER ref)\n"
    "uniform vec3 u_fogColor;\n"
    "uniform vec2 u_fogRange;  // linear fog start & end in eye depth (w)\n"
    "varying vec4 v_color;\n"
    "varying vec2 v_uv;\n"
    "void main()\n"
    "{\n"
    "    // D3DTOP_MODULATE of texture and diffuse for both color and alpha\n"
    "    vec4 color = v_color;\n"
    "#ifdef TEXTURED\n"
    "    color *= texture2D(u_tex, v_uv);\n"
    "#endif\n"
    "    if ( color.a < u_alphaRef ) {\n"
    "        discard;\n"
    "    }\n"
    "#ifdef FOG\n"
    "    // Linear table fog on eye depth (w-fog)\n"
    "    float depth = 1.0 / gl_FragCoord.w;\n"
    "    float f     = clamp((u_fogRange.y - depth) / (u_fogRange.y - u_fogRange.x), 0.0, 1.0);\n"
    "    color.rgb   = mix(u_fogColor, color.rgb, f);\n"
    "#endif\n"
    "    gl_FragColor = color;\n"
    "}\n";

static StdGlProgram std3D_aPrograms[STDGL_NUM_PROGRAMS];

static void std3D_DestroyPrograms(void)
{
    for ( size_t i = 0; i < STDGL_NUM_PROGRAMS; ++i )
    {
        if ( std3D_aPrograms[i].program )
        {
            if ( stdGl_g.boundProgram == std3D_aPrograms[i].program ) {
                stdGl_UseProgram(0);
            }
            glDeleteProgram(std3D_aPrograms[i].program);
        }
    }

    memset(std3D_aPrograms, 0, sizeof(std3D_aPrograms));
}

int stdGl_Open3D(void)
{
    if ( !stdGl_g.fbo ) {
        return 0;
    }

    std3D_DestroyPrograms();
    for ( size_t i = 0; i < STDGL_NUM_PROGRAMS; ++i )
    {
        char aFragSource[sizeof(std3D_aFragShader) + 64];
        STD_FORMAT(aFragSource, "%s%s%s",
            (i & STDGL_PROGRAM_TEXTURED) ? "#define TEXTURED\n" : "",
            (i & STDGL_PROGRAM_FOG) ? "#define FOG\n" : "",
            std3D_aFragShader);

        StdGlProgram* pProgram = &std3D_aPrograms[i];
        pProgram->program = stdGl_CreateProgram(std3D_aVertShader, aFragSource);
        if ( !pProgram->program )
        {
            std3D_DestroyPrograms();
            return 0;
        }

        pProgram->pixelToNdcLoc = glGetUniformLocation(pProgram->program, "u_pixelToNdc");
        pProgram->alphaRefLoc   = glGetUniformLocation(pProgram->program, "u_alphaRef");
        pProgram->fogColorLoc   = glGetUniformLocation(pProgram->program, "u_fogColor");
        pProgram->fogRangeLoc   = glGetUniformLocation(pProgram->program, "u_fogRange");

        stdGl_UseProgram(pProgram->program);
        glUniform1i(glGetUniformLocation(pProgram->program, "u_tex"), 0);

        // Force the first update of each uniform
        pProgram->pixelToNdc[0] = -1.0f;
        pProgram->alphaRef      = -1.0f;
        pProgram->fogColor[0]   = -1.0f;
        pProgram->fogRange[0]   = -1.0f;
        pProgram->fogRange[1]   = -1.0f;
    }

    return 1;
}

void stdGl_Close3D(void)
{
    if ( stdGl_g.context ) {
        std3D_DestroyPrograms();
    }
}

static void std3D_SetUniforms(StdGlProgram* pProgram, const StdGpuDrawState* pState)
{
    float pixelToNdc[2] = { 2.0f / (float)stdGl_g.width, 2.0f / (float)stdGl_g.height };
    if ( pProgram->pixelToNdc[0] != pixelToNdc[0] || pProgram->pixelToNdc[1] != pixelToNdc[1] )
    {
        glUniform2f(pProgram->pixelToNdcLoc, pixelToNdc[0], pixelToNdc[1]);
        pProgram->pixelToNdc[0] = pixelToNdc[0];
        pProgram->pixelToNdc[1] = pixelToNdc[1];
    }

    if ( pProgram->alphaRef != pState->alphaRef )
    {
        glUniform1f(pProgram->alphaRefLoc, pState->alphaRef);
        pProgram->alphaRef = pState->alphaRef;
    }

    if ( pState->bFog )
    {
        if ( memcmp(pProgram->fogColor, pState->fogColor, sizeof(pProgram->fogColor)) != 0 )
        {
            glUniform3f(pProgram->fogColorLoc, pState->fogColor[0], pState->fogColor[1], pState->fogColor[2]);
            memcpy(pProgram->fogColor, pState->fogColor, sizeof(pProgram->fogColor));
        }

        if ( pProgram->fogRange[0] != pState->fogStart || pProgram->fogRange[1] != pState->fogEnd )
        {
            glUniform2f(pProgram->fogRangeLoc, pState->fogStart, pState->fogEnd);
            pProgram->fogRange[0] = pState->fogStart;
            pProgram->fogRange[1] = pState->fogEnd;
        }
    }
}

static void std3D_SetSamplerState(StdGlTexture* pTex, const StdGpuDrawState* pState)
{
    bool bMips = pTex->numGlLevels > 1 && pState->mipFilter != STD3D_MIPMAPFILTER_NONE;
    bool bLinear = pState->filter != STDGPU_TEXFILTER_POINT;

    GLint minFilter;
    if ( !bMips ) {
        minFilter = bLinear ? GL_LINEAR : GL_NEAREST;
    }
    else if ( pState->mipFilter == STD3D_MIPMAPFILTER_TRILINEAR ) {
        minFilter = bLinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
    }
    else {
        minFilter = bLinear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
    }

    GLint magFilter = bLinear ? GL_LINEAR : GL_NEAREST;
    GLint wrapS     = pState->bClampU || pTex->bNpotLimited ? GL_CLAMP_TO_EDGE : GL_REPEAT;
    GLint wrapT     = pState->bClampV || pTex->bNpotLimited ? GL_CLAMP_TO_EDGE : GL_REPEAT;

    float anisotropy = 1.0f;
    if ( pState->filter == STDGPU_TEXFILTER_ANISOTROPIC && stdGl_g.maxAnisotropy > 1.0f ) {
        anisotropy = stdGl_g.maxAnisotropy > 16.0f ? 16.0f : stdGl_g.maxAnisotropy;
    }

    if ( pTex->minFilter != minFilter )
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
        pTex->minFilter = minFilter;
    }

    if ( pTex->magFilter != magFilter )
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
        pTex->magFilter = magFilter;
    }

    if ( pTex->wrapS != wrapS )
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
        pTex->wrapS = wrapS;
    }

    if ( pTex->wrapT != wrapT )
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
        pTex->wrapT = wrapT;
    }

    if ( pTex->anisotropy != anisotropy && stdGl_g.maxAnisotropy > 1.0f )
    {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
        pTex->anisotropy = anisotropy;
    }
}

void stdGl_Draw(StdGpuPrimitive primitive, const StdGpuDrawState* pState, void* pNativeTex, const StdTLVertex* aVerts, size_t numVerts, const uint16_t* aIndices, size_t numIndices)
{
    StdGlTexture* pTexture = (StdGlTexture*)pNativeTex;
    size_t programIdx = (pTexture ? STDGL_PROGRAM_TEXTURED : 0) | (pState->bFog ? STDGL_PROGRAM_FOG : 0);
    StdGlProgram* pProgram = &std3D_aPrograms[programIdx];
    if ( !pProgram->program || !stdGl_g.fbo ) {
        return;
    }

    stdGl_BeginRendering();
    stdGl_SetDepthMask(pState->bDepthWrite);
    stdGl_UseProgram(pProgram->program);
    std3D_SetUniforms(pProgram, pState);

    if ( pTexture )
    {
        stdGl_BindTexture(pTexture->tex);
        std3D_SetSamplerState(pTexture, pState);
    }

    // Vertices are drawn from client memory
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(STDGL_ATTRIB_POSITION);
    glEnableVertexAttribArray(STDGL_ATTRIB_COLOR);
    glEnableVertexAttribArray(STDGL_ATTRIB_TEXCOORD);
    glVertexAttribPointer(STDGL_ATTRIB_POSITION, 4, GL_FLOAT, GL_FALSE, sizeof(StdTLVertex), &aVerts->sx);
    glVertexAttribPointer(STDGL_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(StdTLVertex), &aVerts->color);
    glVertexAttribPointer(STDGL_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(StdTLVertex), &aVerts->tu);

    static const GLenum aModes[STDGPU_NUM_PRIMITIVES] = { GL_TRIANGLES, GL_LINE_STRIP, GL_POINTS };
    if ( numIndices ) {
        glDrawElements(aModes[primitive], (GLsizei)numIndices, GL_UNSIGNED_SHORT, aIndices);
    }
    else {
        glDrawArrays(aModes[primitive], 0, (GLsizei)numVerts);
    }
}
