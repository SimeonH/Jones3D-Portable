// Push constants shared by std3D.vert and std3D.frag, see StdVkPushConstants in std3D.c
layout(push_constant) uniform PushConstants
{
    vec2 pixelToNdc;  // 2 / back buffer size
    float alphaRef;   // fragments with alpha below are discarded (D3D alpha test GREATER ref)
    uint flags;       // STD3D_SHADER_*
    vec4 fogColor;
    float fogStart;   // linear fog range in eye depth (w)
    float fogEnd;
} pc;

#define STD3D_SHADER_TEXTURED 0x1u
#define STD3D_SHADER_FOG      0x2u
