#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "DNA.h"

enum class BoneKind { Spine, Neck, Head, Tail, Leg };

// Indices into Skeleton::joints. A plain enum (not enum class) so it can be
// used directly as an array index — animation code needs to reach specific
// joints (e.g. HeadTip) by name, not just iterate bones generically.
enum SkeletonJoint {
    Pelvis, ChestEnd, NeckEnd, HeadTip, TailMid, TailTip,
    FrontLeftHip, FrontLeftKnee, FrontLeftFoot,
    FrontRightHip, FrontRightKnee, FrontRightFoot,
    BackLeftHip, BackLeftKnee, BackLeftFoot,
    BackRightHip, BackRightKnee, BackRightFoot,
    SkeletonJointCount
};

struct Bone {
    int startJoint;
    int endJoint;
    BoneKind kind;
    // Multiplies BoneRadius(kind) at each end — lets a bone taper along its
    // length (e.g. the tail chain narrowing to a point) without a special
    // case per BoneKind. 1.0 (no taper) unless set otherwise.
    float startRadiusScale = 1.0f;
    float endRadiusScale = 1.0f;
};

// A fixed quadruped hierarchy (Pelvis -> Spine -> Neck -> Head / Tail, 4 legs),
// generated procedurally from DNA. No arbitrary body plans yet — see CLAUDE.md.
struct Skeleton {
    std::vector<glm::vec3> joints;
    std::vector<Bone> bones;
};

Skeleton BuildSkeleton(const DNA& dna);
