#include "Terrain.h"
#include "Skeleton.h" // kCreatureScale

#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

namespace {
    struct Hill {
        float x, z;      // center, world units
        float radius;    // falloff radius — bigger means gentler slope for the same height
        float height;    // peak height (negative = depression), pre-kCreatureScale
    };

    // Deliberately placed, smooth (Gaussian-falloff) hills and one
    // depression instead of a repeating wave field — a sculpted landscape
    // with real, clearly-visible elevation change, not Critter Crosser's
    // block terraces (moving away from copying its terrain style on
    // purpose) and not the earlier subtle ripples either. Every bump is
    // smooth everywhere (no seams, no vertical walls), so however steep two
    // bumps get by overlapping, there's always a continuous slope for the
    // leg IK to climb — unlike the block terraces' sharp risers, which had
    // no collision avoidance against the torso and clipped constantly.
    constexpr Hill kHills[] = {
        {-2.5f, -2.0f, 2.8f,  3.0f},
        { 3.0f,  2.5f, 2.4f,  2.2f},
        { 2.5f, -3.0f, 2.0f, -2.0f},
        {-3.0f,  3.0f, 2.2f,  1.6f},
    };

    float RawHeight(float worldX, float worldZ) {
        float height = 0.0f;
        for (const Hill& hill : kHills) {
            float dx = worldX - hill.x;
            float dz = worldZ - hill.z;
            float distSq = dx * dx + dz * dz;
            float falloff = expf(-distSq / (hill.radius * hill.radius));
            height += hill.height * falloff;
        }
        return height;
    }

    // Low ground reads as grass, higher ground fades toward a lighter,
    // rockier tone — with only 4 shading bands a subtle slope can be hard to
    // read from lighting alone, so elevation gets a second, color-based cue.
    glm::vec3 TerrainColorAt(float height) {
        const glm::vec3 lowColor(0.42f, 0.55f, 0.32f);
        const glm::vec3 highColor(0.64f, 0.60f, 0.47f);
        float t = std::clamp((height + 1.0f * kCreatureScale) / (2.5f * kCreatureScale), 0.0f, 1.0f);
        return glm::mix(lowColor, highColor, t);
    }
}

float TerrainHeight(float worldX, float worldZ) {
    return RawHeight(worldX, worldZ) * kCreatureScale;
}

std::vector<MeshVertex> BuildTerrainMesh(float halfSize, int resolution) {
    std::vector<MeshVertex> mesh;
    mesh.reserve(static_cast<size_t>(resolution) * resolution * 6);

    const float step = (halfSize * 2.0f) / static_cast<float>(resolution);
    const float eps = 0.05f;

    auto vertexAt = [&](float x, float z) {
        float y = TerrainHeight(x, z);
        // Analytic normal via central-difference slope, standard for a heightfield.
        float dhdx = (TerrainHeight(x + eps, z) - TerrainHeight(x - eps, z)) / (2.0f * eps);
        float dhdz = (TerrainHeight(x, z + eps) - TerrainHeight(x, z - eps)) / (2.0f * eps);
        glm::vec3 normal = glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
        return MeshVertex{glm::vec3(x, y, z), normal, TerrainColorAt(y)};
    };

    for (int i = 0; i < resolution; ++i) {
        for (int k = 0; k < resolution; ++k) {
            float x0 = -halfSize + i * step;
            float x1 = x0 + step;
            float z0 = -halfSize + k * step;
            float z1 = z0 + step;

            MeshVertex v00 = vertexAt(x0, z0);
            MeshVertex v10 = vertexAt(x1, z0);
            MeshVertex v11 = vertexAt(x1, z1);
            MeshVertex v01 = vertexAt(x0, z1);

            mesh.push_back(v00);
            mesh.push_back(v10);
            mesh.push_back(v11);
            mesh.push_back(v00);
            mesh.push_back(v11);
            mesh.push_back(v01);
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
