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
in vec3 vWorldPos;

const float sprite_size = 1.0f / 16;

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
    float lightFactor = uLighting.ambientStrength + diffuse + specular;

    color.rgb *= lightFactor;

    float darken = clamp(vExposure * uExposureScale, 0.0, 1.0);
    color.rgb *= (1.0 - darken);
    FragColor = color;
}