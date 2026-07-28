#include "Terrain.h"
#include "Skeleton.h" // kCreatureScale

#include <glm/glm.hpp>
#include <cmath>

namespace {
    // Chunky, stepped "staircase" terrain instead of a smoothly curved
    // surface — matches the blocky stone-terrace look of the Critter Crosser
    // reference (see CLAUDE.md's Referencia visual) far better than a curved
    // heightfield does under a flat-banded pixel-art shader, which reads
    // gradients as smears rather than the reference's crisp tone steps.
    // Scaled with kCreatureScale so a step stays a believable fraction of the
    // (now smaller) creature's leg reach instead of becoming relatively taller.
    constexpr float kTerraceStep = 0.4f * kCreatureScale;

    // Same overlapping-sine shape as before quantization — see the original
    // note this replaces: short wavelengths so the creature's own footprint
    // crosses several bumps, with a deliberately diagonal term so a single
    // front/back + left/right body tilt can't fully explain the terrain
    // (forcing each leg's IK to bend independently — see main.cpp). Amplitude
    // scaled with kCreatureScale for the same reason as kTerraceStep.
    float RawHeight(float worldX, float worldZ) {
        return kCreatureScale * (sinf(worldX * 1.5f) * 0.2f
             + cosf(worldZ * 1.3f) * 0.15f
             + sinf((worldX + worldZ) * 0.8f) * 0.1f
             + sinf(worldX * 3.7f + worldZ * 2.9f) * 0.14f);
    }
}

float TerrainHeight(float worldX, float worldZ) {
    return floorf(RawHeight(worldX, worldZ) / kTerraceStep) * kTerraceStep;
}

std::vector<MeshVertex> BuildTerrainMesh(float halfSize, int resolution) {
    std::vector<MeshVertex> mesh;
    mesh.reserve(static_cast<size_t>(resolution) * resolution * 6 * 3);

    const float step = (halfSize * 2.0f) / static_cast<float>(resolution);
    const glm::vec3 topColor(0.45f, 0.58f, 0.34f);
    const glm::vec3 riserColor(0.58f, 0.52f, 0.44f);

    auto cellHeight = [&](int i, int k) {
        float cx = -halfSize + (i + 0.5f) * step;
        float cz = -halfSize + (k + 0.5f) * step;
        return TerrainHeight(cx, cz);
    };

    auto addQuad = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                        const glm::vec3& normal, const glm::vec3& color) {
        mesh.push_back({a, normal, color});
        mesh.push_back({b, normal, color});
        mesh.push_back({c, normal, color});
        mesh.push_back({a, normal, color});
        mesh.push_back({c, normal, color});
        mesh.push_back({d, normal, color});
    };

    for (int i = 0; i < resolution; ++i) {
        for (int k = 0; k < resolution; ++k) {
            float x0 = -halfSize + i * step;
            float x1 = x0 + step;
            float z0 = -halfSize + k * step;
            float z1 = z0 + step;
            float h = cellHeight(i, k);

            // Flat top — every cell is a single plateau, not an interpolated
            // slope, which is what actually reads as "blocks" instead of
            // "hills".
            addQuad({x0, h, z0}, {x1, h, z0}, {x1, h, z1}, {x0, h, z1},
                    glm::vec3(0.0f, 1.0f, 0.0f), topColor);

            // Vertical risers down to lower neighbors — checking "am I higher
            // than this neighbor" on all four sides means each boundary
            // between two cells gets exactly one wall, built from the higher
            // cell's side, with no gap and no duplicate.
            float hEast = (i + 1 < resolution) ? cellHeight(i + 1, k) : h;
            float hWest = (i - 1 >= 0) ? cellHeight(i - 1, k) : h;
            float hNorth = (k + 1 < resolution) ? cellHeight(i, k + 1) : h;
            float hSouth = (k - 1 >= 0) ? cellHeight(i, k - 1) : h;

            if (h > hEast) {
                addQuad({x1, h, z0}, {x1, h, z1}, {x1, hEast, z1}, {x1, hEast, z0},
                        glm::vec3(1.0f, 0.0f, 0.0f), riserColor);
            }
            if (h > hWest) {
                addQuad({x0, h, z1}, {x0, h, z0}, {x0, hWest, z0}, {x0, hWest, z1},
                        glm::vec3(-1.0f, 0.0f, 0.0f), riserColor);
            }
            if (h > hNorth) {
                addQuad({x1, h, z1}, {x0, h, z1}, {x0, hNorth, z1}, {x1, hNorth, z1},
                        glm::vec3(0.0f, 0.0f, 1.0f), riserColor);
            }
            if (h > hSouth) {
                addQuad({x0, h, z0}, {x1, h, z0}, {x1, hSouth, z0}, {x0, hSouth, z0},
                        glm::vec3(0.0f, 0.0f, -1.0f), riserColor);
            }
        }
    }

    return mesh;
}

std::vector<MeshVertex> BuildBoundaryWalls(float halfSize, float yBottom, float yTop) {
    std::vector<MeshVertex> mesh;

    auto addQuad = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                        const glm::vec3& normal) {
        mesh.push_back({a, normal});
        mesh.push_back({b, normal});
        mesh.push_back({c, normal});
        mesh.push_back({a, normal});
        mesh.push_back({c, normal});
        mesh.push_back({d, normal});
    };

    // North (+Z), facing inward.
    addQuad({-halfSize, yBottom, halfSize}, {halfSize, yBottom, halfSize},
            {halfSize, yTop, halfSize}, {-halfSize, yTop, halfSize}, {0.0f, 0.0f, -1.0f});
    // South (-Z), facing inward.
    addQuad({halfSize, yBottom, -halfSize}, {-halfSize, yBottom, -halfSize},
            {-halfSize, yTop, -halfSize}, {halfSize, yTop, -halfSize}, {0.0f, 0.0f, 1.0f});
    // East (+X), facing inward.
    addQuad({halfSize, yBottom, halfSize}, {halfSize, yBottom, -halfSize},
            {halfSize, yTop, -halfSize}, {halfSize, yTop, halfSize}, {-1.0f, 0.0f, 0.0f});
    // West (-X), facing inward.
    addQuad({-halfSize, yBottom, -halfSize}, {-halfSize, yBottom, halfSize},
            {-halfSize, yTop, halfSize}, {-halfSize, yTop, -halfSize}, {1.0f, 0.0f, 0.0f});

    return mesh;
}
