#pragma once

#include <glm/glm.hpp>

#include "Skeleton.h"
#include "Physics.h"

// Physics-driven state for the spine/neck/head/tail chains, persisted across
// frames — see ApplyAnimation and Physics.h.
struct AnimationState {
    // Only the look-at glance gets its own small lag layer, on top of the
    // otherwise-rigid head — see ApplyAnimation.
    glm::vec3 headLean{0.0f};
    // Phase 10, step 2: the tail is driven by the generic particle+
    // constraint physics solver (Physics.h) instead of a hand-rolled spring
    // — see ApplyAnimation. 5 particles: Pelvis (pinned, index 0), then
    // TailSeg1-3 (indices 1-3), then TailTip (index 4). Built once in
    // ApplyAnimation's init block; distance constraints/muscle targets are
    // refreshed every frame after so live DNA edits (tailLength, tailPitch)
    // reshape it immediately instead of fighting a stale rest length.
    PhysicsBody tailBody;
    // Phase 10, step 3: the spine/neck chain is now driven by the same
    // generic solver as the tail, replacing the old hand-chained
    // ExpLerpAngle sequence (chestAngleLag->spine3->spine2->spine1AngleLag,
    // and a separately-timed headAngleLag) — see ApplyAnimation. 6
    // particles: Pelvis (pinned, index 0), SpineSeg1-3 (1-3), ChestEnd (4),
    // NeckEnd (5) — NeckEnd is the last one, the true tip of the flexible
    // neck. SnoutBase/HeadTip (and horns/ears/eyes) are deliberately NOT
    // their own particles — the whole skull is one rigid piece starting
    // exactly where the neck ends, it doesn't have its own extra flex joint
    // in between. They're derived rigidly from wherever NeckEnd ends up,
    // via a full 3D shortest-arc rotation (Animation.cpp's
    // RotateVectorShortestArc) matching how much the ChestEnd->NeckEnd bone
    // itself has rotated — not a tracked angle. (Three earlier attempts got
    // this wrong: giving HeadTip its own particle with a flexible angle
    // constraint to SnoutBase; then giving SnoutBase itself its own particle
    // with a flexible angle constraint to NeckEnd — both left a "floppy"
    // extra joint a real skull doesn't have, visible as the head bending/
    // wobbling independently of the neck's own tip; then a hand-eased
    // Y-axis-only angle, which can't represent a segment that also pitches
    // up/down, e.g. from a steep neckPitch or the body's own height.)
    // Each particle's own muscle rate (declining from NeckEnd, fastest, down
    // to SpineSeg1 nearest the pelvis, slowest) reproduces the old "wave
    // propagating back through the body" behaviour, but now emerges from the
    // solver's own inertia plus the rigid distance/angle constraints holding
    // neighbours together, instead of each link's target being hand-set to
    // the previous link's already-lagged value.
    PhysicsBody spineBody;

    // rearYawLag: the hips'/global-transform orientation (see main.cpp's
    // bodyTransform), still hand-eased with ExpLerpAngle — nothing in
    // spineBody drives the pelvis itself. chestAngleLag is no longer eased
    // state of its own: it's DERIVED every frame from spineBody's simulated
    // ChestEnd position (see ApplyAnimation), purely so main.cpp's front-hip
    // /gait-target rotation can keep reading a single scalar angle, same as
    // before. The head's own rigid attachments (HeadTip, horns, ears, eyes)
    // don't use an angle at all anymore — see RotateVectorShortestArc in
    // Animation.cpp.
    float rearYawLag = 0.0f;
    float chestAngleLag = 0.0f;

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
