#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "DNA.h"

// Uniform scale applied to the whole generated creature (skeleton positions
// in Skeleton.cpp, mesh radii in CreatureMesh.cpp) — one shared knob instead
// of retuning every DNA range, so relative proportions/Phase-9 tuning (color,
// horn/ear/eye placement as fractions of head radius, etc.) all carry through
// unchanged. Everything computed from unscaled DNA values first, then scaled
// once at the end, keeps the two independently-scaled halves (skeleton here,
// radii in CreatureMesh.cpp) mathematically consistent with each other.
// Introduced when the creature's on-screen size turned out several terrain
// blocks long instead of the reference's ~1 block (see CLAUDE.md).
constexpr float kCreatureScale = 0.5f;

enum class BoneKind { Spine, Neck, Head, Tail, Leg, Horn, Ear };

// Indices into Skeleton::joints. A plain enum (not enum class) so it can be
// used directly as an array index — animation code needs to reach specific
// joints (e.g. HeadTip) by name, not just iterate bones generically.
enum SkeletonJoint {
    Pelvis, ChestEnd, NeckEnd, HeadTip, TailMid, TailTip,
    FrontLeftHip, FrontLeftKnee, FrontLeftFoot,
    FrontRightHip, FrontRightKnee, FrontRightFoot,
    BackLeftHip, BackLeftKnee, BackLeftFoot,
    BackRightHip, BackRightKnee, BackRightFoot,
    // Head appendages (Phase 9): horns/ears are small bone chains off
    // HeadTip reusing the same cylinder pipeline as legs/tail, not special
    // mesh cases — see the RujiK devlog note in CLAUDE.md about antennae
    // being "leg code" pointed a different direction. Eyes have no bone
    // (a point, not a segment) but still need a joint so they can be
    // carried along by head animation.
    HornTip, LeftEarTip, RightEarTip, LeftEye, RightEye,
    // Spine body segmentation (Phase 9): intermediate points between Pelvis
    // and ChestEnd so the torso is a short chain of tapered segments with a
    // width profile (fuller mid-body, narrower at both attachments) instead
    // of one uniform-taper cylinder — see BuildSkeleton's spine loop and
    // CLAUDE.md's note on why this is procedural (DNA-driven), not a
    // per-segment manual sculpt like Critter Crosser's own editor.
    SpineSeg1, SpineSeg2, SpineSeg3,
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
