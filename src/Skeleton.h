#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "DNA.h"

enum class BoneKind { Spine, Neck, Head, Tail, Leg };

struct Bone {
    int startJoint;
    int endJoint;
    BoneKind kind;
};

// A fixed quadruped hierarchy (Pelvis -> Spine -> Neck -> Head / Tail, 4 legs),
// generated procedurally from DNA. No arbitrary body plans yet — see CLAUDE.md.
struct Skeleton {
    std::vector<glm::vec3> joints;
    std::vector<Bone> bones;
};

Skeleton BuildSkeleton(const DNA& dna);
