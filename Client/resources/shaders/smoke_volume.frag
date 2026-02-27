#version 440 core

layout (location = 0) out vec4 accum;
layout (location = 1) out float reveal;

in vec3 vWorldPos;

uniform vec3 uCameraPos;
uniform vec3 uBoxMin;
uniform vec3 uBoxMax;
uniform mat4 projection_view;

uniform float uWindSpeed;
uniform vec2 uWindDirXZ;
uniform float uPrevWindSpeed;
uniform vec2 uPrevWindDirXZ;
uniform float uWindChangeTimeSec;
uniform float uWindTransitionSec;
uniform float uSimTimeSec;
uniform float uVoxelSize;
uniform float uVoxelThreshold;
uniform float uDissipationHalfLifeSec;
uniform float uMaxDownwind;
uniform float uDownwindFade;
uniform float uWindVariationScale;
uniform float uWindSpeedVariation;
uniform float uWindDirVariationDeg;

uniform int uSourceCount;
uniform vec4 uSourcePosH[32];
uniform vec4 uSourceParams[32];

uniform int uStepCount;
uniform float uDensityScale;
uniform vec3 uSmokeColor;

uniform int uUseWeightedOIT = 1;

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
    float windSpeed = max(uWindSpeed, 1e-6);

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
        float fadeWidth = max(uDownwindFade, 0.01);
        float downwindFade = 1.0 - smoothstep(uMaxDownwind, uMaxDownwind + fadeWidth, downwind);
        if (downwindFade <= 0.0) continue;

        float age = downwind / windSpeed;
        float tSinceChange = uSimTimeSec - uWindChangeTimeSec;
        float transition = max(uWindTransitionSec, 0.01);
        float blend = smoothstep(-transition, transition, age - tSinceChange);
        vec2 wPrev = normalize(uPrevWindDirXZ);
        vec2 wBlendRaw = mix(uWindDirXZ, wPrev, blend);
        vec2 wBlend = length(wBlendRaw) > 1e-6 ? normalize(wBlendRaw) : normalize(uWindDirXZ);
        float speedBlend = mix(uWindSpeed, uPrevWindSpeed, blend);

        w = wBlend;
        windSpeed = max(speedBlend, 1e-6);

        downwind = dot(r, w);
        if (downwind <= 0.0) continue;
        downwindFade = 1.0 - smoothstep(uMaxDownwind, uMaxDownwind + fadeWidth, downwind);
        if (downwindFade <= 0.0) continue;

        age = downwind / windSpeed;
        float halfLife = max(uDissipationHalfLifeSec, 0.01);
        float decay = pow(0.5, max(age, 0.0) / halfLife);

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

        float dilution = (windSpeed * sigmaY * sigmaZ);
        float phase = uSimTimeSec * max(uWindVariationScale, 0.01);
        float dirVar = uWindDirVariationDeg * 0.01745329252;
        float turb = 0.5 + 0.5 * sin(dot(p.xz, vec2(0.08, 0.11)) + p.y * 0.03 + phase + dirVar);
        float jitter = mix(1.0 - uWindSpeedVariation, 1.0 + uWindSpeedVariation, turb);
        float C = (Q / (2.0 * PI * dilution)) * gy * gz * jitter;

        total += C * decay * downwindFade;
    }

    return total;
}

bool intersectAABB(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax, out float tNear, out float tFar) {
    vec3 safeRd = vec3(
        abs(rd.x) < 1e-6 ? (rd.x < 0.0 ? -1e-6 : 1e-6) : rd.x,
        abs(rd.y) < 1e-6 ? (rd.y < 0.0 ? -1e-6 : 1e-6) : rd.y,
        abs(rd.z) < 1e-6 ? (rd.z < 0.0 ? -1e-6 : 1e-6) : rd.z);
    vec3 inv = 1.0 / safeRd;
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

    float minStep = max(uVoxelSize, 0.01);
    // Align a stable world grid origin to the voxel size so voxels do not shift
    vec3 worldOrigin = floor(uBoxMin / minStep) * minStep;
    vec3 boxSize = max(uBoxMax - worldOrigin, vec3(minStep));
    ivec3 gridDim = ivec3(max(ivec3(1), ivec3(ceil(boxSize / minStep))));
    int maxSteps = max(int(ceil(distance / minStep)) + 1, max(uStepCount, 1));

    vec3 pos = ro + rd * tNear;
    vec3 gridPos = (pos - worldOrigin) / minStep;
    ivec3 voxel = ivec3(floor(gridPos));
    ivec3 stepDir = ivec3(sign(rd));
    vec3 safeRd = vec3(
        abs(rd.x) < 1e-6 ? (rd.x < 0.0 ? -1e-6 : 1e-6) : rd.x,
        abs(rd.y) < 1e-6 ? (rd.y < 0.0 ? -1e-6 : 1e-6) : rd.y,
        abs(rd.z) < 1e-6 ? (rd.z < 0.0 ? -1e-6 : 1e-6) : rd.z);
    vec3 invRd = 1.0 / safeRd;
    vec3 tDelta = abs(vec3(minStep) * invRd);

    vec3 nextBoundary;
    nextBoundary.x = (stepDir.x > 0 ? (float(voxel.x) + 1.0) : float(voxel.x)) * minStep + worldOrigin.x;
    nextBoundary.y = (stepDir.y > 0 ? (float(voxel.y) + 1.0) : float(voxel.y)) * minStep + worldOrigin.y;
    nextBoundary.z = (stepDir.z > 0 ? (float(voxel.z) + 1.0) : float(voxel.z)) * minStep + worldOrigin.z;

    vec3 tMax = (nextBoundary - ro) * invRd;
    tMax = max(tMax, vec3(tNear));
    voxel = clamp(voxel, ivec3(0), gridDim - ivec3(1));

    bool hit = false;
    vec3 color = vec3(0.0);
    float tHit = tNear;
    float t = tNear;

    for (int i = 0; i < maxSteps; ++i) {
        if (any(lessThan(voxel, ivec3(0))) || any(greaterThanEqual(voxel, gridDim))) {
            break;
        }
        vec3 voxelPos = (vec3(voxel) + vec3(0.5)) * minStep + uBoxMin;

        float density = sampleGaussian(voxelPos);
        if (density >= uVoxelThreshold) {
            float intensity = clamp(density * uDensityScale, 0.0, 1.0);
            float gray = dot(uSmokeColor, vec3(0.3333));
            vec3 lowColor = mix(vec3(gray), vec3(0.0), 0.5);
            vec3 baseColor = mix(lowColor, uSmokeColor, intensity);

            // Add a slight deterministic per-voxel tint so adjacent voxels are easier to
            // distinguish. This uses a simple hash of the integer voxel coordinates.
            vec3 vp = vec3(voxel);
            float h1 = fract(sin(dot(vp, vec3(127.1, 311.7, 74.7))) * 43758.5453);
            float h2 = fract(sin(dot(vp, vec3(269.5, 183.3, 246.1))) * 43758.5453);
            vec3 tint = vec3(h1, h2, fract(h1 * h2));
            float variationStrength = 0.12; // small variation amount
            vec3 varied = baseColor * (1.0 + (tint - 0.5) * variationStrength);
            color = clamp(varied, 0.0, 1.0);

            hit = true;
            tHit = t;
            break;
        }

        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                t = tMax.x;
                voxel.x += stepDir.x;
                tMax.x += tDelta.x;
            } else {
                t = tMax.z;
                voxel.z += stepDir.z;
                tMax.z += tDelta.z;
            }
        } else {
            if (tMax.y < tMax.z) {
                t = tMax.y;
                voxel.y += stepDir.y;
                tMax.y += tDelta.y;
            } else {
                t = tMax.z;
                voxel.z += stepDir.z;
                tMax.z += tDelta.z;
            }
        }

        if (t > tFar) {
            break;
        }
    }

    if (!hit) discard;

    vec3 hitPos = ro + rd * tHit;
    vec4 clip = projection_view * vec4(hitPos, 1.0);
    float ndcDepth = clip.z / clip.w;
    gl_FragDepth = ndcDepth * 0.5 + 0.5;

    if (uUseWeightedOIT != 0) {
        float w = weight(gl_FragDepth, 1.0);
        accum = vec4(color, 1.0) * w;
        reveal = 1.0;
    } else {
        accum = vec4(color, 1.0);
        reveal = 1.0;
    }
}