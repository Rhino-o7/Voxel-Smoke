#version 440 core

layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 projection_view;

out vec3 vWorldPos;

void main() {
    vec4 world = model * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    gl_Position = projection_view * world;
}