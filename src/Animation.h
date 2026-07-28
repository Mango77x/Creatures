#pragma once

#include <glm/glm.hpp>

#include "Skeleton.h"

// Lagged ("follow the leader") state for the neck/tail chains, persisted
// across frames — each segment chases the previous frame's position of the
// segment before it, with a delay, instead of snapping straight to a target.
struct AnimationState {
    glm::vec3 neckEnd{0.0f};
    glm::vec3 headTip{0.0f};
    glm::vec3 tailMid{0.0f};
    glm::vec3 tailTip{0.0f};
    float breathScale = 0.0f;
    bool initialized = false;
};

// Returns a copy of `rest` with the neck/tail chain joints replaced by their
// animated (lagged) positions and the head biased toward lookAtTarget.
// Spine/legs stay exactly at their rest-pose positions — this is the
// "follow the leader" chain from CLAUDE.md, not a full IK/rig system.
Skeleton ApplyAnimation(AnimationState& state, const Skeleton& rest, float time, float dt,
                         const glm::vec3& lookAtTarget);
