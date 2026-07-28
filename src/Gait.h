#pragma once

#include <glm/glm.hpp>

struct GaitParams {
    float speed = 1.0f;          // gait cycles per second
    float strideLength = 0.5f;   // world units the foot travels during stance
    float liftHeight = 0.18f;    // swing arc height
    float stanceFraction = 0.6f; // fraction of the cycle spent planted
};

// Body-local target position for a leg's foot at a given time, given its
// phase offset (0..1) within the shared gait cycle. Stateless: during stance
// the foot drifts backward (in body space) at the same rate the body itself
// moves forward, so it stays roughly planted in world space without needing
// to remember where it touched down. See CLAUDE.md's Phase 7 decision
// (procedural gait, not raycast/terrain adaptation yet — that's Phase 8).
glm::vec3 ComputeFootTarget(const glm::vec3& restFootLocal, float time, float phaseOffset, const GaitParams& params);
