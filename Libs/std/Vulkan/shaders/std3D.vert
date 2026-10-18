#version 450
#extension GL_GOOGLE_include_directive : require
#include "std3D_common.glsl"

// StdTLVertex (D3DTLVERTEX)
layout(location = 0) in vec4 inPos;   // sx, sy, sz, rhw
layout(location = 1) in vec4 inColor; // diffuse
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;

void main()
{
    // Pre-transformed vertex in pixels, D3D pixel centers are at integer coordinates.
    // Multiply by w to get perspective correct interpolation (gl_FragCoord.w is then rhw).
    // Vertices behind the camera have a negative rhw, their negative w gets them clipped like in D3D.
    float w   = inPos.w != 0.0 ? 1.0 / inPos.w : 1.0;
    vec2 ndc  = (inPos.xy + 0.5) * pc.pixelToNdc - 1.0;
    gl_Position = vec4(ndc * w, inPos.z * w, w);

    outColor = inColor;
    outUV    = inUV;
}
