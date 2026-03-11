#version 440 core

out vec4 FragColor;

uniform sampler2D game_texture;
uniform float uExposureScale;
uniform vec3 uViewPos;

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

const float sprite_size = 1.0f / 16;

vec2 GetFaceUv(uint faceIndex, vec3 localPos) {
    vec3 f = fract(localPos);

    if (faceIndex == 0u) return vec2(f.x, f.y);
    if (faceIndex == 1u) return vec2(1.0 - f.x, f.y);
    if (faceIndex == 2u) return vec2(1.0 - f.z, f.y);
    if (faceIndex == 3u) return vec2(f.z, f.y);
    if (faceIndex == 4u) return vec2(f.x, 1.0 - f.z);
    return vec2(1.0 - f.x, 1.0 - f.z);
}

void main() {  
    vec2 faceUv = GetFaceUv(vFaceIndex, vLocalPos);
    vec2 coord = vec2(sprite_size * tex_coord.x, sprite_size * tex_coord.y);
    coord.x += faceUv.x * sprite_size;
    coord.y += faceUv.y * sprite_size;

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
    float lightFactor = uLighting.ambientStrength + diffuse + specular;

    color.rgb *= lightFactor;

    float darken = clamp(vExposure * uExposureScale, 0.0, 1.0);
    color.rgb *= (1.0 - darken);
    FragColor = color;
}