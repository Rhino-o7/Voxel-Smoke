#version 440 core

layout (location = 0) out vec4 accum;
layout (location = 1) out float reveal;

in vec3 vWorldPos;

uniform vec3 uCameraPos;
uniform vec3 uBoxMin;
uniform vec3 uBoxMax;

uniform float uWindSpeed;
uniform vec2 uWindDirXZ;

uniform int uSourceCount;
uniform vec4 uSourcePosH[32];
uniform vec4 uSourceParams[32];

uniform int uStepCount;
uniform float uDensityScale;
uniform vec3 uSmokeColor;

uniform float zFar = 5000.0f;
uniform float zNear = 0.1f;

const float PI = 3.14159265359;

float d(float z) {
    return ((zNear * zFar) / z - zFar) / (zNear - zFar);
}

float weight(float z, float a) {
    float b = 1.0 - d(z);
    return a * max(0.01, b * b * b * 0.003);
}

float sampleGaussian(vec3 p) {
    if (uWindSpeed <= 1e-6) return 0.0;

    float total = 0.0;

    for (int i = 0; i < uSourceCount; ++i) {
        vec3 srcPos = uSourcePosH[i].xyz;
        float srcH = uSourcePosH[i].w;
        float exitVelocity = uSourceParams[i].x;
        float radius = uSourceParams[i].y;

        vec3 rel = p - srcPos;

        vec2 w = normalize(uWindDirXZ);
        vec2 r = vec2(rel.x, rel.z);

        float downwind = dot(r, w);
        if (downwind <= 0.0) continue;

        vec2 perp = vec2(-w.y, w.x);
        float crosswind = dot(r, perp);

        float x = max(downwind, 0.1);
        float vRatio = exitVelocity / max(1.0, uWindSpeed);
        float widen = 1.0 + 0.12 * clamp(vRatio, 0.0, 8.0);

        float sigmaY = 0.6 * sqrt(x) * widen;
        float sigmaZ = 0.3 * sqrt(x) * widen;

        float area = PI * radius * radius;
        float Q = area * exitVelocity;

        float gy = exp(-(crosswind * crosswind) / (2.0 * sigmaY * sigmaY));
        float z = rel.y;
        float gz = exp(-((z - srcH) * (z - srcH)) / (2.0 * sigmaZ * sigmaZ));

        float dilution = (uWindSpeed * sigmaY * sigmaZ);
        float C = (Q / (2.0 * PI * dilution)) * gy * gz;

        total += C;
    }

    return total;
}

bool intersectAABB(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax, out float tNear, out float tFar) {
    vec3 inv = 1.0 / rd;
    vec3 t0 = (bmin - ro) * inv;
    vec3 t1 = (bmax - ro) * inv;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    tNear = max(max(tmin.x, tmin.y), tmin.z);
    tFar = min(min(tmax.x, tmax.y), tmax.z);
    return tFar >= max(tNear, 0.0);
}

void main() {
    vec3 ro = uCameraPos;
    vec3 rd = normalize(vWorldPos - uCameraPos);

    float tNear, tFar;
    if (!intersectAABB(ro, rd, uBoxMin, uBoxMax, tNear, tFar)) {
        discard;
    }

    tNear = max(tNear, 0.0);
    float distance = tFar - tNear;
    if (distance <= 0.0) discard;

    float jitter = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    int steps = max(uStepCount, 1);
    float stepSize = distance / float(steps);

    float transmittance = 1.0;
    vec3 color = vec3(0.0);

    for (int i = 0; i < steps; ++i) {
        float t = tNear + (float(i) + jitter) * stepSize;
        vec3 pos = ro + rd * t;

        float density = sampleGaussian(pos);
        float alpha = 1.0 - exp(-density * uDensityScale * stepSize);

        color += transmittance * alpha * uSmokeColor;
        transmittance *= (1.0 - alpha);

        if (transmittance < 0.02) break;
    }

    float finalAlpha = 1.0 - transmittance;
    if (finalAlpha <= 0.001) discard;

    float w = weight(gl_FragCoord.z, finalAlpha);
    accum = vec4(color, finalAlpha) * w;
    reveal = finalAlpha;
}