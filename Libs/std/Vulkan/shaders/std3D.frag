#version 450
#extension GL_GOOGLE_include_directive : require
#include "std3D_common.glsl"

layout(set = 0, binding = 0) uniform sampler texSampler;
layout(set = 1, binding = 0) uniform texture2D tex;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main()
{
    // D3DTOP_MODULATE of texture and diffuse for both color and alpha
    vec4 color = inColor;
    if ( (pc.flags & STD3D_SHADER_TEXTURED) != 0u ) {
        color *= texture(sampler2D(tex, texSampler), inUV);
    }

    if ( color.a < pc.alphaRef ) {
        discard;
    }

    // Linear table fog on eye depth (w-fog)
    if ( (pc.flags & STD3D_SHADER_FOG) != 0u )
    {
        float depth = 1.0 / gl_FragCoord.w;
        float f     = clamp((pc.fogEnd - depth) / (pc.fogEnd - pc.fogStart), 0.0, 1.0);
        color.rgb   = mix(pc.fogColor.rgb, color.rgb, f);
    }

    outColor = color;
}
