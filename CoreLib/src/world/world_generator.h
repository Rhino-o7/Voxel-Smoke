#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <FastNoiseLite.h>
#include "world/chunk.h"

namespace yc::world {

class World;

class WorldGenerator {

public:
    struct NoiseParams {
        FastNoiseLite::NoiseType noiseType = FastNoiseLite::NoiseType_OpenSimplex2;
        FastNoiseLite::FractalType fractalType = FastNoiseLite::FractalType_FBm;
        int32_t octaves = 4;
        float frequency = 0.002f;
        float lacunarity = 2.0f;
        float gain = 0.5f;
    };

    struct WorldGenTuning {
        // Terrain height
        int32_t baseHeight = 30;
        int32_t heightAmplitude = 120;
        float mountainShapePower = 3.0f;

        // Biome/material thresholds
        int32_t waterLevel = 45;
        int32_t beachMaxY = 46;

        int32_t stoneStartY = 100;
        int32_t snowStartY = 125;
        float stoneSnowNoiseInfluence = 10.0f;

        // Sampling scales (larger = higher frequency / smaller features)
        float stoneSnowScale = 16.0f;
        float mountainBlendScale1 = 1.0f;
        float mountainBlendScale2 = 2.0f;
        float mountainBlendScale4 = 4.0f;

        float forestScale = 4.0f;
        float treeScale = 1024.0f;
        float grassScale = 256.0f;
        float flowerScale = 128.0f;

        // Feature shaping
        float treeShapePower = 4.0f;
        float grassShapePower = 2.0f;
        float flowerShapePower = 2.0f;

        // Feature thresholds
        int32_t featuresMinHeight = 50;
        int32_t featuresMaxHeight = 100;

        float forestTreeThreshold = 0.50f;
        float treeThreshold = 0.45f;

        float forestGrassThreshold = 0.45f;
        float grassThreshold = 0.60f;

        float flowerThreshold = 0.70f;
        float redFlowerThreshold = 0.73f;
        float yellowFlowerThreshold = 0.72f;
        float blueFlowerThreshold = 0.70f;
    };

    struct TerrainConfig {
        NoiseParams noise;
        WorldGenTuning tuning;
    };

public:
    // Presets
    static TerrainConfig MakeMountainsConfig();
    static TerrainConfig MakeHilllyConfig();

    WorldGenerator(int32_t seed);
    WorldGenerator(int32_t seed, const TerrainConfig& config);

    int32_t getSeed() const { return seed; }
    void setSeed(int32_t value);

    void setTerrainConfig(const TerrainConfig& config);

    std::shared_ptr<Chunk> generateChunk(World* world, const glm::ivec2& chunkCoord);

    BlockData getBlockData(int32_t height, int32_t maxHeight, float noise);

    void generateTreeAt(World* world, const glm::ivec3& coord, float noise);
    void generateTreeAt(std::shared_ptr<Chunk> chunk, const glm::ivec3& coord, float noise);

	void generateChimneyAt(World* world, const glm::ivec3& coord, int height, int radius);

private:
    static void ConfigureNoise(FastNoiseLite& noise, int32_t seed, const NoiseParams& p);

    static float Sample01(const FastNoiseLite& noise, float x, float z);
    static float PowShaper(float x01, float power);
    static float WeightedOctaves3(const FastNoiseLite& noise, float x, float z,
        float s1, float w1, float s2, float w2, float s3, float w3);

private:
    int32_t seed;
    FastNoiseLite mountainNoise;

    TerrainConfig config;
};

}