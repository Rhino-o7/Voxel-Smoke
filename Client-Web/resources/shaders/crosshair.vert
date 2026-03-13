#version 300 es
precision highp float;
precision highp int;

layout (location = 0) in vec2 coord;
layout (location = 1) in vec2 pass_texcoord;

uniform int screen_width;
uniform int screen_height;
uniform int width;
uniform int height;

out vec2 texcoord;

void main() {
    float xsize = float(width) / float(screen_width);
    float ysize = float(height) / float(screen_height);

    gl_Position = vec4(vec2(xsize * coord.x, ysize * coord.y), 0.0, 1.0);
    texcoord = pass_texcoord;
}