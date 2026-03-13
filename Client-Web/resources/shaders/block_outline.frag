#version 300 es
precision highp float;
precision highp int;

in vec2 uv;
out vec4 FragColor;

const float outline_size = 0.015;

void main() {
    if (outline_size < uv.x && uv.x < 1.0 - outline_size &&
        outline_size < uv.y && uv.y < 1.0 - outline_size) {
        discard;
    }

    gl_FragDepth = gl_FragCoord.z - 0.00001;
    FragColor = vec4(236.0 / 255.0, 240.0 / 255.0, 240.0 / 255.0, 1.0);
}
