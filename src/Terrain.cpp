#include "Terrain.h"

#include <glm/glm.hpp>

float TerrainHeight(float worldX, float worldZ) {
    // Short wavelengths on purpose: the creature only ever walks in a small
    // area around the origin, so the bumps need to be tight enough that its
    // own footprint crosses several of them.
    //
    // The last term is deliberately a *diagonal* wave with a wavelength
    // close to the creature's own stance size — a pure front/back + left/
    // right slope (the first three terms) is something a single body-tilt
    // plane can fully explain, but a diagonal wave isn't: it can put two
    // diagonally-opposite feet high and the other two low at the same time,
    // which pitch+roll alone can't capture. That residual is exactly what
    // forces each leg's IK to bend independently instead of the body tilt
    // doing all the work — see the Phase 8 note in main.cpp.
    return sinf(worldX * 1.5f) * 0.2f
         + cosf(worldZ * 1.3f) * 0.15f
         + sinf((worldX + worldZ) * 0.8f) * 0.1f
         + sinf(worldX * 3.7f + worldZ * 2.9f) * 0.14f;
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
        return MeshVertex{glm::vec3(x, y, z), normal};
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
