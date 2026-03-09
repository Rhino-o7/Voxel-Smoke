#include "world/world_generator.h"
#include <iostream>
#include <cmath>
#include "world/world.h"

namespace yc::world {

static BlockData dirtBlock { BlockType::DIRT };
static BlockData grassBlock { BlockType::GRASS_BLOCK };
static BlockData stoneBlock { BlockType::STONE };
static BlockData sandBlock { BlockType::SAND };
static BlockData snowBlock { BlockType::SNOW };
static BlockData woodBlock { BlockType::WOOD };
static BlockData leafBlock { BlockType::LEAF };
static BlockData chimneyBlock { BlockType::CHIMNEY };

WorldGenerator::TerrainConfig WorldGenerator::MakeMountainsConfig() {
    TerrainConfig c{};
    // Matches your current "mountain" feel
    c.noise.octaves = 4;
    c.noise.frequency = 0.002f;
    c.noise.lacunarity = 2.0f;
    c.noise.gain = 0.5f;

    c.tuning.baseHeight = 30;
    c.tuning.heightAmplitude = 120;
    c.tuning.mountainShapePower = 3.0f;

    c.tuning.mountainBlendScale1 = 1.0f;
    c.tuning.mountainBlendScale2 = 2.0f;
    c.tuning.mountainBlendScale4 = 4.0f;

    return c;
}

WorldGenerator::TerrainConfig WorldGenerator::MakeHilllyConfig() {
    TerrainConfig c{};

    // Smoother, broader features
    c.noise.octaves = 2;
    c.noise.frequency = 0.01f;
    c.noise.lacunarity = 2.0f;
    c.noise.gain = 0.35f;

    // Mostly flat with some hills
    c.tuning.baseHeight = 30;
    c.tuning.heightAmplitude = 75;
    c.tuning.mountainShapePower = 1.5f;

    // Broader hill shapes (smaller scale => larger features)
    c.tuning.mountainBlendScale1 = 0.20f;
    c.tuning.mountainBlendScale2 = 0.45f;
    c.tuning.mountainBlendScale4 = 0.90f;

    // Keep the rest defaults (vegetation thresholds, etc.)
    return c;
}

void WorldGenerator::ConfigureNoise(FastNoiseLite& noise, int32_t seed, const NoiseParams& p) {
    noise.SetSeed(seed);
    noise.SetNoiseType(p.noiseType);
    noise.SetFractalType(p.fractalType);
    noise.SetFractalOctaves(p.octaves);
    noise.SetFrequency(p.frequency);
    noise.SetFractalLacunarity(p.lacunarity);
    noise.SetFractalGain(p.gain);
}

float WorldGenerator::Sample01(const FastNoiseLite& noise, float x, float z) {
    // FastNoiseLite returns roughly [-1..1]
    return (noise.GetNoise(x, z) + 1.0f) * 0.5f;
}

float WorldGenerator::PowShaper(float x01, float power) {
    return std::pow(x01, power);
}

float WorldGenerator::WeightedOctaves3(const FastNoiseLite& noise, float x, float z,
    float s1, float w1, float s2, float w2, float s3, float w3) {

    // Note: these are raw [-1..1] samples, matching your original approach
    return w1 * noise.GetNoise(x * s1, z * s1)
         + w2 * noise.GetNoise(x * s2, z * s2)
         + w3 * noise.GetNoise(x * s3, z * s3);
}

WorldGenerator::WorldGenerator(int32_t seed)
    : WorldGenerator(seed, MakeHilllyConfig()) { // Set NoiseConfig here
}

WorldGenerator::WorldGenerator(int32_t seed, const TerrainConfig& config)
    : seed(seed),
      mountainNoise(seed),
      config(config) {
    ConfigureNoise(mountainNoise, seed, this->config.noise);
}

void WorldGenerator::setSeed(int32_t value) {
    seed = value;
    ConfigureNoise(mountainNoise, seed, config.noise);
}

void WorldGenerator::setTerrainConfig(const TerrainConfig& config) {
    this->config = config;
    ConfigureNoise(mountainNoise, seed, this->config.noise);
}

BlockData WorldGenerator::getBlockData(int32_t height, int32_t maxHeight, float noise01) {
    const auto& tuning = config.tuning;

    if (height == 0) return stoneBlock;

    if (height <= tuning.beachMaxY) return sandBlock;

    if (height >= tuning.snowStartY + static_cast<int32_t>(noise01 * tuning.stoneSnowNoiseInfluence)) return snowBlock;

    if (height >= tuning.stoneStartY + static_cast<int32_t>(noise01 * tuning.stoneSnowNoiseInfluence)) return stoneBlock;

    if (height == maxHeight) return grassBlock;

    return dirtBlock;
}

std::shared_ptr<Chunk> WorldGenerator::generateChunk(World* world, const glm::ivec2& chunkCoord) {
    const auto& tuning = config.tuning;

    auto chunk = std::make_shared<Chunk>();
    chunk->setCoordinate(world, chunkCoord);

    const glm::vec3 worldCoord = chunk->getWorldCoord();

    for (int32_t x = 0; x < Chunk::Length; ++x)
    for (int32_t z = 0; z < Chunk::Width; ++z) {
        const float wx = worldCoord.x + static_cast<float>(x);
        const float wz = worldCoord.z + static_cast<float>(z);

        // --- Terrain height ---
        const float mountainRaw = WeightedOctaves3(
            mountainNoise, wx, wz,
            tuning.mountainBlendScale1, 0.50f,
            tuning.mountainBlendScale2, 0.35f,
            tuning.mountainBlendScale4, 0.15f);

        float mountain01 = (mountainRaw + 1.0f) * 0.5f;
        mountain01 = PowShaper(mountain01, tuning.mountainShapePower);

        const int32_t terrainHeight = static_cast<int32_t>(mountain01 * tuning.heightAmplitude) + tuning.baseHeight;

        // Used by getBlockData() for snow/stone variation
        const float stoneSnowNoise01 = Sample01(mountainNoise, wx * tuning.stoneSnowScale, wz * tuning.stoneSnowScale);

        // --- Feature noise fields ---
        float tree01 = Sample01(mountainNoise, wx * tuning.treeScale, wz * tuning.treeScale);
        tree01 = PowShaper(tree01, tuning.treeShapePower);

        float forest01 = Sample01(mountainNoise, wx * tuning.forestScale, wz * tuning.forestScale);

        float grass01 = Sample01(mountainNoise, wx * tuning.grassScale, wz * tuning.grassScale);
        grass01 = PowShaper(grass01, tuning.grassShapePower);

        float flower01 = Sample01(mountainNoise, wx * tuning.flowerScale, wz * tuning.flowerScale);
        flower01 = PowShaper(flower01, tuning.flowerShapePower);

        // --- Fill terrain column ---
        glm::ivec3 coord { x, 0, z };
        for (; coord.y <= terrainHeight; ++coord.y) {
            chunk->setBlockData(coord, getBlockData(coord.y, terrainHeight, stoneSnowNoise01));
        }

        // --- Vegetation/details ---
        if (terrainHeight > tuning.featuresMinHeight && terrainHeight < tuning.featuresMaxHeight) {
            if (forest01 > tuning.forestTreeThreshold && tree01 > tuning.treeThreshold) {
                if (0 < x && x < 15 && 0 < z && z < 15) {
                    generateTreeAt(chunk, coord, tree01);
                }
            } else if (forest01 > tuning.forestGrassThreshold && grass01 > tuning.grassThreshold) {
                chunk->setBlockData(coord, { BlockType::GRASS });
            } else if (flower01 > tuning.flowerThreshold) {
                if (flower01 > tuning.redFlowerThreshold) {
                    chunk->setBlockData(coord, { BlockType::RED_FLOWER });
                } else if (flower01 > tuning.yellowFlowerThreshold) {
                    chunk->setBlockData(coord, { BlockType::YELLOW_FLOWER });
                } else if (flower01 > tuning.blueFlowerThreshold) {
                    chunk->setBlockData(coord, { BlockType::BLUE_FLOWER });
                }
            }
        }

        // --- Water fill to sea level ---
        for (; coord.y <= tuning.waterLevel; ++coord.y) {
            chunk->setBlockData(coord, { BlockType::WATER });
        }
    }

    chunk->prepareToBuildMesh();
    return chunk;
}

void WorldGenerator::generateTreeAt(std::shared_ptr<Chunk> chunk, const glm::ivec3& coord, float noise) {
    int32_t height = noise > 6 ? 6 : 4;
    int32_t leafHeight = noise > 6 ? 5 : 4;

    // build leafs
    for (int y = height - (leafHeight + 1) / 2; y <= height + leafHeight / 2; ++y) {
        for (int x = -1; x <= 1; ++x) {
            for (int z = -1; z <= 1; ++z) {
                if ((y == height + leafHeight / 2 || y == height - (leafHeight + 1) / 2) &&
                    ((x == -1 && z == -1) || (x == -1 && z == 1) || (x == 1 && z == -1) || (x == 1 && z == 1)))
                    continue;
                chunk->setBlockData({ coord.x + x, coord.y + y, coord.z + z }, leafBlock);
            }
        }
    }

    // build woods
    for (int i = 0; i < height; ++i) {
        chunk->setBlockData({ coord.x, coord.y + i, coord.z }, woodBlock);
    }
}



void WorldGenerator::generateTreeAt(World* world, const glm::ivec3& coord, float noise) {
    generateTreeAt(world->getChunkIfLoadedAt(World::GetChunkCoordOf(coord)), {
        coord.x & 15,
        coord.y,
        coord.z & 15
    }, noise);
}

void WorldGenerator::generateChimneyAt(World* world, const glm::ivec3& coord, int height, int radius)
{
    for (int y = 0; y < height; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            for (int z = -radius; z <= radius; ++z) {
                if (std::sqrt(x * x + z * z) <= radius) {
                    world->setBlockDataIfLoadedAt({ coord.x + x, coord.y + y, coord.z + z }, chimneyBlock);
                }
            }
        }
    }
}


}