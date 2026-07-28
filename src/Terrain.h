#pragma once

#include <vector>

#include "CreatureMesh.h" // reuses MeshVertex (position + normal)

// A handful of overlapping sine waves — enough bumpy, non-flat terrain to
// demonstrate adaptation without pulling in a noise library. Deterministic
// and analytic, so per-leg "raycasting" (see main.cpp) collapses to a direct
// height sample instead of a real ray/mesh intersection: for a heightfield,
// a vertical ray hit and evaluating the height function at that (x, z) are
// the same thing. True ray-mesh intersection only earns its cost once the
// terrain can overhang itself, which this can't.
float TerrainHeight(float worldX, float worldZ);

std::vector<MeshVertex> BuildTerrainMesh(float halfSize, int resolution);

// Four inward-facing quads marking the terrain's boundary — paired with a
// position clamp in main.cpp so the creature can't walk (or be steered) off
// the edge of the world.
std::vector<MeshVertex> BuildBoundaryWalls(float halfSize, float yBottom, float yTop);
