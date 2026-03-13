#version 300 es
precision highp float;
precision highp int;

uniform sampler2D game_texture;
uniform float zFar;
uniform float zNear;
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
in vec3 vLocalPos;
in vec3 vWorldPos;

layout (location = 0) out vec4 accum;
layout (location = 1) out float reveal;

const float sprite_size = 1.0 / 16.0;

vec2 GetFaceUv(uint faceIndex, vec3 localPos) {
    vec3 f = fract(localPos);

    if (faceIndex == 0u) return vec2(f.x, f.y);
    if (faceIndex == 1u) return vec2(1.0 - f.x, f.y);
    if (faceIndex == 2u) return vec2(1.0 - f.z, f.y);
    if (faceIndex == 3u) return vec2(f.z, f.y);
    if (faceIndex == 4u) return vec2(f.x, 1.0 - f.z);
    return vec2(1.0 - f.x, 1.0 - f.z);
}

bool IsWaterTile(uvec2 atlasCoord) {
    return atlasCoord.y == 0u && atlasCoord.x <= 2u;
}

float d(float z) {
    return ((zNear * zFar) / z - zFar) / (zNear - zFar);
}

float weight(float z, float a) {
    float b = 1.0 - d(z);
    return a * max(0.01, b * b * b * 0.003);
}

void main() {
    vec2 faceUv = GetFaceUv(vFaceIndex, vLocalPos);
    vec2 coord = vec2(sprite_size * float(tex_coord.x), sprite_size * float(tex_coord.y));
    coord.x += faceUv.x * sprite_size;
    coord.y += faceUv.y * sprite_size;

    vec4 color = texture(game_texture, coord);

    if (color.a == 0.0) {
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

    vec3 normal = normals[int(min(vFaceIndex, 5u))];

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

    float lightFactor = max(uLighting.ambientStrength + diffuse + specular, 0.15);

    color.rgb *= lightFactor;

    float darken = clamp(vExposure * uExposureScale, 0.0, 0.8);
    color.rgb *= (1.0 - darken);

    float w = weight(gl_FragCoord.z, color.a);

    if (color.a == 1.0) {
        w *= 10.0;
    }

    accum = vec4(color.rgb, color.a) * w;
    reveal = color.a;
}