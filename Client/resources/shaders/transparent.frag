#version 440 core

uniform sampler2D game_texture;
uniform float zFar = 5000.0f;
uniform float zNear = 0.1f;
uniform float uExposureScale;
uniform vec3 uViewPos;
uniform vec3 uWaterTint;
uniform float uWaterDiffuseMul;
uniform float uWaterSpecularMul;
uniform float uWaterMinAlpha;

struct LightingData {
    vec3 sunDirection;
    float ambientStrength;
    float diffuseStrength;
    float specularStrength;
    float shininess;
    float daylight;
};

uniform LightingData uLighting;

flat in uvec2 tex_coord;
in vec2 vert_pos;
flat in float vExposure;
flat in uint vFaceIndex;
in vec3 vWorldPos;

layout (location = 0) out vec4 accum;
layout (location = 1) out float reveal;

const float sprite_size = 1.0f / 16;

bool IsWaterTile(uvec2 atlasCoord) {
    return atlasCoord.y == 0u && atlasCoord.x <= 2u;
}

float d(float z) {
    return ((zNear * zFar) / z - zFar) / (zNear - zFar);
}

float weight(float z, float a) {
    float b = 1 - d(z);
    return a * max(0.01f, b * b * b * 0.003);
}

void main() {  
    vec2 coord = vec2(sprite_size * tex_coord.x, sprite_size * tex_coord.y);
    coord.x += (vert_pos.x) * sprite_size;
    coord.y += (vert_pos.y) * sprite_size;

    vec4 color = texture(game_texture, coord);

    if (color.w == 0) {
        discard;
    }

    const vec3 normals[6] = vec3[6](
        vec3(0.0, 0.0, 1.0),
        vec3(0.0, 0.0, -1.0),
        vec3(1.0, 0.0, 0.0),
        vec3(-1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, -1.0, 0.0)
    );

    vec3 normal = normals[min(vFaceIndex, 5u)];

    vec3 lightDir = normalize(-uLighting.sunDirection);
    vec3 viewDir = normalize(uViewPos - vWorldPos);
    vec3 halfDir = normalize(lightDir + viewDir);

    float diffuse = max(dot(normal, lightDir), 0.0) * uLighting.diffuseStrength;
    float specular = pow(max(dot(normal, halfDir), 0.0), uLighting.shininess) * uLighting.specularStrength;

    if (IsWaterTile(tex_coord)) {
        color.rgb *= uWaterTint;
        diffuse *= uWaterDiffuseMul;
        specular *= uWaterSpecularMul;
        color.a = max(color.a, uWaterMinAlpha);
    }

    float lightFactor = uLighting.ambientStrength + diffuse + specular;

    color.rgb *= lightFactor;

    float darken = clamp(vExposure * uExposureScale, 0.0, 0.8);
    color.rgb *= (1.0 - darken);

    float w = weight(gl_FragCoord.z, color.w);

    if (color.w == 1) {
        w *= 10;
    }

    accum = vec4(color.xyz, color.w) * w;
    reveal = color.a;
}