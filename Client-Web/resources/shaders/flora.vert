#version 300 es
precision highp float;
precision highp int;

layout (location = 0) in vec3 coord;
layout (location = 1) in vec2 pass_uv;
layout (location = 2) in uint texcoord_data;

uniform mat4 model;
uniform mat4 projection_view;

out vec2 uv;
flat out uvec2 texcoord;
out vec3 vWorldPos;

uint bf(uint value, int offset, int bits) {
    uint mask = (1u << uint(bits)) - 1u;
    return (value >> uint(offset)) & mask;
}

void main()
{
    texcoord.x = bf(texcoord_data, 0, 4);
    texcoord.y = bf(texcoord_data, 4, 4);
    texcoord.y = 15u - texcoord.y;
    vec4 worldPos = model * vec4(coord, 1.0);
    vWorldPos = worldPos.xyz;
    gl_Position = projection_view * worldPos;
    uv = pass_uv;
}