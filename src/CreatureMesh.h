#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "Skeleton.h"
#include "DNA.h"

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

// Procedural geometry per bone (tapered cylinders) + spheres at joints so
// segments meet without gaps. Simple shapes only — see CLAUDE.md.
std::vector<MeshVertex> BuildCreatureMesh(const Skeleton& skeleton, const DNA& dna);
