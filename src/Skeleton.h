#pragma once

#include <vector>
#include <utility>
#include <glm/glm.hpp>

#include "DNA.h"

// A fixed quadruped hierarchy (Pelvis -> Spine -> Neck -> Head / Tail, 4 legs),
// generated procedurally from DNA. No arbitrary body plans yet — see CLAUDE.md.
struct Skeleton {
    std::vector<glm::vec3> joints;
    std::vector<std::pair<int, int>> bones; // pairs of indices into joints
};

Skeleton BuildSkeleton(const DNA& dna);
