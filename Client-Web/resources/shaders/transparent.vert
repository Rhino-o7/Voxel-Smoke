#version 300 es
precision highp float;
precision highp int;

layout (location = 0) in uint vert_data;
layout (location = 1) in float exposure;

uniform mat4 model;
uniform mat4 projection_view;

flat out uvec2 tex_coord;
out vec2 vert_pos;
flat out float vExposure;
flat out uint vFaceIndex;
out vec3 vLocalPos;
out vec3 vWorldPos;

uint bf(uint value, int offset, int bits) {
    uint mask = (1u << uint(bits)) - 1u;
    return (value >> uint(offset)) & mask;
}

void main() {
    uint x = bf(vert_data, 0, 5);
    uint y = bf(vert_data, 10, 9);
    uint z = bf(vert_data, 5, 5);
    uint uv_x = bf(vert_data, 19, 1);
    uint uv_y = bf(vert_data, 20, 1);
    uint tex_x = bf(vert_data, 21, 4);
    uint tex_y = bf(vert_data, 25, 4);
    uint face_index = bf(vert_data, 29, 3);

    tex_coord = uvec2(tex_x, 15u - tex_y);
    vert_pos = vec2(float(uv_x), float(uv_y));
    vExposure = exposure;
    vFaceIndex = face_index;
    vLocalPos = vec3(float(x), float(y), float(z));

    vec4 worldPos = model * vec4(float(x), float(y), float(z), 1.0);
    vWorldPos = worldPos.xyz;

    gl_Position = projection_view * worldPos;
}