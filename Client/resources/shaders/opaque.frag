#version 440 core

out vec4 FragColor;

uniform sampler2D game_texture;
uniform float uExposureScale;

flat in uvec2 tex_coord;
in vec2 vert_pos;
flat in float vExposure;

const float sprite_size = 1.0f / 16;

void main() {  
    vec2 coord = vec2(sprite_size * tex_coord.x, sprite_size * tex_coord.y);
    coord.x += (vert_pos.x) * sprite_size;
    coord.y += (vert_pos.y) * sprite_size;

    vec4 color = texture(game_texture, coord);
    if (color.w == 0) {
        discard;
    }

    float darken = clamp(vExposure * uExposureScale, 0.0, 1.0);
    color.rgb *= (1.0 - darken);
    FragColor = color;
}