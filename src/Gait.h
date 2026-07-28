#pragma once

#include <glm/glm.hpp>

#include "Skeleton.h" // kCreatureScale

struct GaitParams {
    float speed = 1.0f;          // gait cycles per second
    // strideLength/liftHeight are absolute world-unit distances, tuned for
    // the pre-kCreatureScale skeleton size — scaled down to match (see
    // Skeleton.h's kCreatureScale) so a step still looks like a fraction of
    // a leg length rather than becoming relatively oversized.
    float strideLength = 0.5f * kCreatureScale;  // world units the foot travels during stance
    float liftHeight = 0.18f * kCreatureScale;   // swing arc height
    float stanceFraction = 0.6f;                 // fraction of the cycle spent planted
};

// Body-local target position for a leg's foot at a given time, given its
// phase offset (0..1) within the shared gait cycle. Stateless: during stance
// the foot drifts backward (in body space) at the same rate the body itself
// moves forward, so it stays roughly planted in world space without needing
// to remember where it touched down. See CLAUDE.md's Phase 7 decision
// (procedural gait, not raycast/terrain adaptation yet — that's Phase 8).
glm::vec3 ComputeFootTarget(const glm::vec3& restFootLocal, float time, float phaseOffset, const GaitParams& params);
