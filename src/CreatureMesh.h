#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "Skeleton.h"
#include "DNA.h"

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    // Multiplied with the uColor uniform in the shader (Phase 9's per-DNA
    // palette). Non-creature meshes (terrain, walls) leave this at white so
    // their existing uColor tint keeps working unchanged.
    glm::vec3 color{1.0f, 1.0f, 1.0f};
};

// Procedural geometry per bone (tapered cylinders) + spheres at joints so
// segments meet without gaps. Simple shapes only — see CLAUDE.md.
// breathScale (-1..1-ish, 0 = rest) inflates/deflates the spine radius only —
// the Phase 6 "torso pulse" idle animation.
std::vector<MeshVertex> BuildCreatureMesh(const Skeleton& skeleton, const DNA& dna, float breathScale = 0.0f);
