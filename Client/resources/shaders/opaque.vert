#version 440 core

layout (location = 0) in uint vert_data;
layout (location = 1) in float exposure;

uniform mat4 model;
uniform mat4 projection_view;

flat out uvec2 tex_coord;
out vec2 vert_pos;
flat out float vExposure;
flat out uint vFaceIndex;
out vec3 vWorldPos;

void main() {
    uint x = bitfieldExtract(vert_data, 0, 5);
    uint y = bitfieldExtract(vert_data, 10, 9);
    uint z = bitfieldExtract(vert_data, 5, 5);
    uint uv_x = bitfieldExtract(vert_data, 19, 1);
    uint uv_y = bitfieldExtract(vert_data, 20, 1);
    uint tex_x = bitfieldExtract(vert_data, 21, 4);
    uint tex_y = bitfieldExtract(vert_data, 25, 4);
    uint face_index = bitfieldExtract(vert_data, 29, 3);

    tex_coord = uvec2(tex_x, 15 - tex_y);
    vert_pos = vec2(uv_x, uv_y);
    vExposure = exposure;
    vFaceIndex = face_index;

    vec4 worldPos = model * vec4(x, y, z, 1.0f);
    vWorldPos = worldPos.xyz;

    gl_Position = projection_view * worldPos;
}