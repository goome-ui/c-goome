#version 450

layout(set = 0, binding = 0) uniform sampler2D tex;
layout(location = 0) out vec4 color;

void main()
{
    color = texelFetch(tex, ivec2(gl_FragCoord.xy), 0);
}
