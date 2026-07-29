#pragma once

#include <glm/glm.hpp>

#include "Skeleton.h"

// Lagged ("follow the leader") state for the neck/tail/spine chains,
// persisted across frames — each segment chases the (already-updated this
// frame) segment before it with its own delay, instead of snapping straight
// to a target. The spine is a genuine chain from chest to hips, each link
// slower than the one in front of it, not one global bend angle applied
// all-at-once — see ApplyAnimation.
struct AnimationState {
    // neckEnd/headTip are recomputed fresh every frame from the (already
    // lagged) chest/headAngleLag state — NOT independently eased — so the
    // neck/head capsules always keep their correct rest-pose shape instead
    // of stretching/twisting when two independently-lagged endpoints chase
    // their targets at different rates (that was the actual bug behind
    // "the head capsule wobbles and lags").
    glm::vec3 neckEnd{0.0f};
    glm::vec3 headTip{0.0f};
    // Only the look-at glance gets its own small lag layer, on top of the
    // otherwise-rigid head — see ApplyAnimation.
    glm::vec3 headLean{0.0f};
    // tailMid/tailTip are driven by a real damped spring (see ApplyAnimation),
    // not a position lag — so they carry actual velocity, not just position,
    // and can overshoot/settle instead of only ever easing monotonically
    // toward a target.
    glm::vec3 tailMid{0.0f};
    glm::vec3 tailTip{0.0f};
    glm::vec3 tailMidVelocity{0.0f};
    glm::vec3 tailTipVelocity{0.0f};

    // rearYawLag: the hips'/global-transform orientation, slowest of all to
    // catch up to bodyYaw. chestAngleLag/spine3/spine2/spine1AngleLag: the
    // spine's bend chain, relative to rearYawLag, chest fastest (but still
    // not instant) down to spine1 (nearest the hips, slowest). tailSwingLag
    // gives the tail its own extra trailing whip, lagging even further
    // behind rearYawLag than the hips do — a tail isn't rigidly attached the
    // way legs are. headAngleLag is separate from (and faster than)
    // chestAngleLag: a real animal's head/neck lead a turn, deciding where
    // to go before the shoulders catch up, so the neck's own follow speed
    // shouldn't compound on top of the chest's — see ApplyAnimation.
    float rearYawLag = 0.0f;
    float chestAngleLag = 0.0f;
    float spine3AngleLag = 0.0f;
    float spine2AngleLag = 0.0f;
    float spine1AngleLag = 0.0f;
    float tailSwingLag = 0.0f;
    float headAngleLag = 0.0f;

    float breathScale = 0.0f;
    bool initialized = false;
};

// Returns a copy of `rest` with the neck/tail chain joints replaced by their
// animated (lagged) positions, the head biased toward lookAtTarget, and the
// spine (+ front hips) bent through a genuine front-to-back lag chain
// instead of the whole rigid body pivoting on the spot. Legs are not
// otherwise touched — this is still the "follow the leader" chain from
// CLAUDE.md, not a full IK/rig system.
Skeleton ApplyAnimation(AnimationState& state, const Skeleton& rest, float time, float dt,
                         const glm::vec3& lookAtTarget, float bodyYaw);
