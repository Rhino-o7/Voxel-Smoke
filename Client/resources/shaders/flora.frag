#version 440 core

out vec4 FragColor;

uniform sampler2D game_texture;
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

in vec2 uv;
flat in uvec2 texcoord;
in vec3 vWorldPos;

const float sprite_size = 1.0f / 16;

void main() {  
    vec2 coord = vec2(sprite_size * texcoord.x, sprite_size * texcoord.y);
    coord.x += (uv.x) * sprite_size;
    coord.y += (uv.y) * sprite_size;
    FragColor = texture(game_texture, coord);

    if (FragColor.w == 0) {
        discard;
    }

    const vec3 normal = vec3(0.0, 1.0, 0.0);
    vec3 lightDir = normalize(-uLighting.sunDirection);
    vec3 viewDir = normalize(uViewPos - vWorldPos);
    vec3 halfDir = normalize(lightDir + viewDir);

    float diffuse = max(dot(normal, lightDir), 0.0) * (uLighting.diffuseStrength * 0.5);
    float specular = pow(max(dot(normal, halfDir), 0.0), uLighting.shininess) * (uLighting.specularStrength * 0.25);
    float lightFactor = uLighting.ambientStrength + diffuse + specular;

    FragColor.rgb *= lightFactor;
}
