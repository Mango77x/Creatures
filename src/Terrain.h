#pragma once

#include <vector>

#include "CreatureMesh.h" // reuses MeshVertex (position + normal)

// A handful of smooth, deliberately-placed hills/depressions (Gaussian
// falloff bumps) instead of a repeating wave field or the Phase 9 block
// terraces — real, clearly-visible elevation change, but with a bounded
// slope everywhere so the leg IK can always adapt without the body clipping
// into a vertical wall (the block terraces' sharp risers had no collision
// avoidance against the torso, only per-foot height sampling, which is
// exactly what breaks on a cliff edge). Deterministic and analytic, so
// per-leg "raycasting" (see main.cpp) collapses to a direct height sample
// instead of a real ray/mesh intersection: for a heightfield, a vertical ray
// hit and evaluating the height function at that (x, z) are the same thing.
float TerrainHeight(float worldX, float worldZ);

std::vector<MeshVertex> BuildTerrainMesh(float halfSize, int resolution);

// Four inward-facing quads marking the terrain's boundary — paired with a
// position clamp in main.cpp so the creature can't walk (or be steered) off
// the edge of the world.
std::vector<MeshVertex> BuildBoundaryWalls(float halfSize, float yBottom, float yTop);
