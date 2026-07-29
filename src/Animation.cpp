#include "Animation.h"

#include <glm/gtc/constants.hpp>
#include <cmath>

namespace {
    // Tail physics (Phase 10, step 2): the tail is now a 5-particle
    // PhysicsBody (Pelvis pinned, TailSeg1-3, TailTip) solved by Physics.h's
    // generic particle+constraint solver, instead of the hand-rolled
    // force-spring this replaced. A single TailMid could only bend at one
    // point — never more than a single V-shaped kink — so it never read as
    // a real hanging curve the way the (5-particle) rope test did; 3
    // intermediate joints let the sag distribute across several bends. The
    // "muscle" targets below are soft pulls toward where the tail would
    // rigidly be if it followed the body instantly — the solver's own
    // inertia (Verlet integration carrying velocity between frames) is what
    // makes the actual simulated position trail behind and overshoot/settle,
    // not a hand-authored extra lag angle (the old tailSwingLag is gone;
    // rearYawLag drives the target directly). Muscle rates decrease toward
    // the tip, same declining relationship kChestFollowSpeed->kSpine1FollowSpeed
    // has for the spine — the very end of the tail is the whippiest part.
    constexpr float kTailSeg1MuscleRate = 8.0f;
    constexpr float kTailSeg2MuscleRate = 6.0f;
    constexpr float kTailSeg3MuscleRate = 4.0f;
    constexpr float kTailTipMuscleRate = 2.5f;
    // Bend limits at each tail joint: never fully doubles back on itself
    // (min), but free to go fully straight (max = pi, acos's own natural
    // ceiling).
    constexpr float kTailMinBendAngle = glm::radians(70.0f);
    constexpr float kTailMaxBendAngle = glm::pi<float>();
    // Gravity per unit of the tail's own (rest-pose, already kCreatureScale-
    // scaled) length, NOT a flat acceleration — a fixed absolute value made
    // a short tail wiggle (the same gravity is a much bigger fraction of a
    // short segment's own length, pushing it against the angle constraint's
    // limit repeatedly instead of settling) while a long tail stayed stable,
    // since the same push was proportionally tiny for it. Scaling by the
    // tail's actual length keeps the relative sag consistent regardless of
    // how long this particular creature's tail is. Much higher than it
    // looks like it should need to be: unlike the free-hanging rope test
    // (nothing opposes gravity there but the distance constraints), the
    // tail's muscle pull is actively fighting gravity to hold a rigid
    // posture, so gravity has to be strong enough to visibly win some of
    // that fight, not just exist.
    constexpr float kTailGravityPerUnitLength = 40.0f;

    // Spine bend chain, chest (front) to spine1 (nearest the hips): each
    // link chases the one in front of it, progressively slower — a genuine
    // wave propagating back through the body instead of one global angle
    // reflected instantly at the chest. kChestFollowSpeed used to effectively
    // be "infinite" (raw, unsmoothed bodyYaw) — even the most responsive
    // part of a real animal can't redirect in zero time, which is what made
    // sharp turns look like the chest teleporting toward the mouse.
    constexpr float kRearFollowSpeed = 2.0f;   // hips / global transform — slowest
    constexpr float kChestFollowSpeed = 5.0f;
    constexpr float kSpine3FollowSpeed = 4.0f;
    constexpr float kSpine2FollowSpeed = 3.2f;
    constexpr float kSpine1FollowSpeed = 2.6f;
    // Faster than kChestFollowSpeed on purpose, and NOT chained off it — a
    // real animal's head/neck lead a turn (looking where it's going before
    // the shoulders catch up), so the neck shouldn't inherit the chest's own
    // lag on top of its own; that compounding was what made long necks read
    // as heavy and sluggish instead of alert.
    constexpr float kHeadFollowSpeed = 14.0f;
    // Hard safety clamp on each link's bend angle, independent of how it got
    // there — main.cpp bounds bodyYaw's own turn rate so this should rarely
    // bind, but it guarantees the spine can never wind past what's
    // anatomically plausible (e.g. sustained fast circling) no matter what
    // input drives it.
    constexpr float kMaxJointBendAngle = glm::radians(40.0f);

    // kBobAmount/kMaxHeadLean are absolute world-unit offsets tuned for the
    // pre-kCreatureScale skeleton size — scaled down to match so they stay
    // proportional instead of becoming relatively oversized on the smaller
    // creature (see Skeleton.h's kCreatureScale).
    constexpr float kBobAmount = 0.05f * kCreatureScale;
    constexpr float kBobSpeed = 1.6f;
    constexpr float kMaxHeadLean = 0.35f * kCreatureScale; // world units the head may lean toward the target
    constexpr float kLookAtSpeed = 4.0f;
    constexpr float kBreathSpeed = 1.2f;
    constexpr float kBreathAmount = 0.06f; // a fractional radius multiplier, not a length — scale-invariant

    // Exponential smoothing: closes the gap to `target` at a rate independent
    // of frame rate, which is exactly "follows with a small delay" rather than
    // snapping straight there.
    glm::vec3 ExpLerp(const glm::vec3& current, const glm::vec3& target, float speed, float dt) {
        float t = 1.0f - expf(-speed * dt);
        return glm::mix(current, target, t);
    }

    // Wraps to (-pi, pi] so angle interpolation always takes the shortest way
    // around instead of spinning the long way when crossing the +-pi seam.
    float WrapAngle(float angle) {
        angle = fmodf(angle + glm::pi<float>(), glm::two_pi<float>());
        if (angle < 0.0f) angle += glm::two_pi<float>();
        return angle - glm::pi<float>();
    }

    float ExpLerpAngle(float current, float target, float speed, float dt) {
        float delta = WrapAngle(target - current);
        float t = 1.0f - expf(-speed * dt);
        return current + delta * t;
    }

    // Rotates a direction/offset vector around the Y (up) axis — the
    // building block both the spine's point-around-pivot rotation and the
    // tail's offset-vector rotation reduce to.
    glm::vec3 RotateVectorAroundY(const glm::vec3& v, float angle) {
        float c = cosf(angle);
        float s = sinf(angle);
        return glm::vec3(v.x * c + v.z * s, v.y, -v.x * s + v.z * c);
    }

    glm::vec3 RotateAroundY(const glm::vec3& point, const glm::vec3& pivot, float angle) {
        return pivot + RotateVectorAroundY(point - pivot, angle);
    }
}

Skeleton ApplyAnimation(AnimationState& state, const Skeleton& rest, float time, float dt,
                         const glm::vec3& lookAtTarget, float bodyYaw) {
    Skeleton animated = rest;

    const glm::vec3 restNeckEnd = rest.joints[NeckEnd];
    const glm::vec3 restSnoutBase = rest.joints[SnoutBase];
    const glm::vec3 restHeadTip = rest.joints[HeadTip];
    const glm::vec3 restTailSeg1 = rest.joints[TailSeg1];
    const glm::vec3 restTailSeg2 = rest.joints[TailSeg2];
    const glm::vec3 restTailSeg3 = rest.joints[TailSeg3];
    const glm::vec3 restTailTip = rest.joints[TailTip];
    const glm::vec3 restChestEnd = rest.joints[ChestEnd];
    const glm::vec3 pelvis = rest.joints[Pelvis]; // spine bend pivot — never itself moves
    const glm::vec3 tailBase = pelvis;

    if (!state.initialized) {
        // neckEnd/headTip need no init of their own — they're computed
        // fresh every frame below, not eased from a starting value.
        state.headLean = glm::vec3(0.0f);
        state.rearYawLag = bodyYaw;
        state.chestAngleLag = 0.0f;
        state.spine3AngleLag = 0.0f;
        state.spine2AngleLag = 0.0f;
        state.spine1AngleLag = 0.0f;
        state.headAngleLag = 0.0f;

        // Tail PhysicsBody: particle 0 = Pelvis (pinned, inverseMass 0),
        // 1-3 = TailSeg1-3, 4 = TailTip. Rest lengths/positions come
        // straight from the rest skeleton — no new authoring.
        state.tailBody.particles = {
            {pelvis, pelvis, 0.0f},
            {restTailSeg1, restTailSeg1, 1.0f},
            {restTailSeg2, restTailSeg2, 1.0f},
            {restTailSeg3, restTailSeg3, 1.0f},
            {restTailTip, restTailTip, 1.0f},
        };
        state.tailBody.distanceConstraints = {
            {0, 1, glm::length(restTailSeg1 - pelvis)},
            {1, 2, glm::length(restTailSeg2 - restTailSeg1)},
            {2, 3, glm::length(restTailSeg3 - restTailSeg2)},
            {3, 4, glm::length(restTailTip - restTailSeg3)},
        };
        state.tailBody.angleConstraints = {
            {0, 1, 2, kTailMinBendAngle, kTailMaxBendAngle},
            {1, 2, 3, kTailMinBendAngle, kTailMaxBendAngle},
            {2, 3, 4, kTailMinBendAngle, kTailMaxBendAngle},
        };
        state.tailBody.muscleTargets = {
            {1, restTailSeg1, kTailSeg1MuscleRate},
            {2, restTailSeg2, kTailSeg2MuscleRate},
            {3, restTailSeg3, kTailSeg3MuscleRate},
            {4, restTailTip, kTailTipMuscleRate},
        };

        state.initialized = true;
    }

    // Hips / global transform (see main.cpp's bodyTransform) — lags bodyYaw
    // slowest of the whole chain.
    state.rearYawLag = ExpLerpAngle(state.rearYawLag, bodyYaw, kRearFollowSpeed, dt);

    // Spine bend chain: each link chases the (already updated this frame)
    // link in front of it, at its own speed, instead of one global angle
    // applied fully at the chest. This is what fixes sharp turns looking
    // like the chest teleporting toward the mouse — the chest itself now
    // has real inertia (kChestFollowSpeed), it just has less than the rest
    // of the spine.
    float chestBendTarget = WrapAngle(bodyYaw - state.rearYawLag);
    state.chestAngleLag = ExpLerpAngle(state.chestAngleLag, chestBendTarget, kChestFollowSpeed, dt);
    state.spine3AngleLag = ExpLerpAngle(state.spine3AngleLag, state.chestAngleLag, kSpine3FollowSpeed, dt);
    state.spine2AngleLag = ExpLerpAngle(state.spine2AngleLag, state.spine3AngleLag, kSpine2FollowSpeed, dt);
    state.spine1AngleLag = ExpLerpAngle(state.spine1AngleLag, state.spine2AngleLag, kSpine1FollowSpeed, dt);

    state.chestAngleLag = glm::clamp(state.chestAngleLag, -kMaxJointBendAngle, kMaxJointBendAngle);
    state.spine3AngleLag = glm::clamp(state.spine3AngleLag, -kMaxJointBendAngle, kMaxJointBendAngle);
    state.spine2AngleLag = glm::clamp(state.spine2AngleLag, -kMaxJointBendAngle, kMaxJointBendAngle);
    state.spine1AngleLag = glm::clamp(state.spine1AngleLag, -kMaxJointBendAngle, kMaxJointBendAngle);

    glm::vec3 bentChestEnd = RotateAroundY(restChestEnd, pelvis, state.chestAngleLag);
    animated.joints[SpineSeg3] = RotateAroundY(rest.joints[SpineSeg3], pelvis, state.spine3AngleLag);
    animated.joints[SpineSeg2] = RotateAroundY(rest.joints[SpineSeg2], pelvis, state.spine2AngleLag);
    animated.joints[SpineSeg1] = RotateAroundY(rest.joints[SpineSeg1], pelvis, state.spine1AngleLag);
    animated.joints[ChestEnd] = bentChestEnd;
    animated.joints[FrontLeftHip] = RotateAroundY(rest.joints[FrontLeftHip], pelvis, state.chestAngleLag);
    animated.joints[FrontRightHip] = RotateAroundY(rest.joints[FrontRightHip], pelvis, state.chestAngleLag);

    // A small idle bob/sway "leader" signal the neck/tail chains chase with a
    // delay — this is what makes a creature that never moves still read as
    // breathing/alive, per CLAUDE.md's Phase 6 goal. The tail's own downward
    // droop isn't baked in here at all — it comes from kTailGravityPerUnitLength
    // acting on the tailBody physics below.
    glm::vec3 neckBob(0.0f, sinf(time * kBobSpeed) * kBobAmount, sinf(time * kBobSpeed * 0.5f) * kBobAmount * 0.5f);
    glm::vec3 tailBob(0.0f, sinf(time * kBobSpeed + 1.0f) * kBobAmount, 0.0f);

    glm::vec3 neckLeader = bentChestEnd + neckBob; // rooted at the chest's actual (slower) position...
    glm::vec3 tailLeader = tailBase + tailBob;

    // ...but the direction the neck extends in tracks the turn on its own,
    // faster than the chest — see kHeadFollowSpeed.
    state.headAngleLag = ExpLerpAngle(state.headAngleLag, chestBendTarget, kHeadFollowSpeed, dt);
    state.headAngleLag = glm::clamp(state.headAngleLag, -kMaxJointBendAngle, kMaxJointBendAngle);
    glm::vec3 bentNeckOffset = RotateVectorAroundY(restNeckEnd - restChestEnd, state.headAngleLag);

    // neckEnd is computed fresh every frame — rigidly attached to the chest
    // via headAngleLag, which already carries its own lag — instead of
    // being independently eased toward a (constantly moving) target. That
    // independent easing was the actual bug: two separately-lagged
    // endpoints (neckEnd and headTip) chasing their targets at different
    // rates meant the capsule between them stretched/twisted instead of
    // staying a rigid shape.
    state.neckEnd = neckLeader + bentNeckOffset;

    // Same fast headAngleLag, not a separate follow speed — otherwise the
    // head would visibly kink back toward the unbent rest direction right
    // where it meets the already-bent neck. Rigidly attached to neckEnd for
    // the same reason neckEnd is rigidly attached to the chest. SnoutBase
    // (cranium/snout boundary) chains in the same way before HeadTip — if it
    // stayed at its rest position while NeckEnd/HeadTip rotated around it,
    // the snout would visibly stretch/twist during a head turn exactly like
    // the old neckEnd/headTip bug this same pattern already fixed.
    glm::vec3 snoutBaseRigid = state.neckEnd + RotateVectorAroundY(restSnoutBase - restNeckEnd, state.headAngleLag);
    glm::vec3 headTipRigid = snoutBaseRigid + RotateVectorAroundY(restHeadTip - restSnoutBase, state.headAngleLag);

    // Only the look-at glance gets its own lag, as a small offset layered on
    // top of the otherwise-rigid head — lagging a small nudge can't stretch
    // anything, unlike lagging the head's whole position could.
    glm::vec3 leanTarget = lookAtTarget - headTipRigid;
    float leanLen = glm::length(leanTarget);
    if (leanLen > kMaxHeadLean && leanLen > 1e-5f) {
        leanTarget *= (kMaxHeadLean / leanLen);
    }
    state.headLean = ExpLerp(state.headLean, leanTarget, kLookAtSpeed, dt);
    state.headTip = headTipRigid + state.headLean;

    // Tail: muscle targets are where each segment would rigidly be if the
    // tail followed the pelvis's current (already-lagged) orientation
    // instantly — rearYawLag directly, no separate hand-tuned extra lag
    // angle needed anymore. Each segment's target chains off the previous
    // segment's own simulated (already physically lagging) position from
    // the previous solve, same "each link reacts to the one before it"
    // pattern the spine bend chain above uses — one-frame-delayed,
    // imperceptible at frame rate.
    // Rest lengths refreshed every frame, not just at init — the skeleton
    // is rebuilt from currentDNA every frame (main.cpp) so live-editing
    // tailLength reshapes the rest pose immediately, but the distance
    // constraints wouldn't know that on their own: they'd keep enforcing
    // whatever length existed when tailBody was first built, fighting the
    // (correctly updated) muscle targets every frame instead of tracking
    // the new length — that fight is what made shortening the tail look
    // like it was "going crazy" instead of just getting shorter.
    state.tailBody.distanceConstraints[0].restLength = glm::length(restTailSeg1 - tailBase);
    state.tailBody.distanceConstraints[1].restLength = glm::length(restTailSeg2 - restTailSeg1);
    state.tailBody.distanceConstraints[2].restLength = glm::length(restTailSeg3 - restTailSeg2);
    state.tailBody.distanceConstraints[3].restLength = glm::length(restTailTip - restTailSeg3);

    state.tailBody.particles[0].position = tailBase; // pelvis pin, kept explicit
    state.tailBody.muscleTargets[0].target = tailLeader + RotateVectorAroundY(restTailSeg1 - tailBase, state.rearYawLag);
    state.tailBody.muscleTargets[1].target = state.tailBody.particles[1].position +
                                              RotateVectorAroundY(restTailSeg2 - restTailSeg1, state.rearYawLag);
    state.tailBody.muscleTargets[2].target = state.tailBody.particles[2].position +
                                              RotateVectorAroundY(restTailSeg3 - restTailSeg2, state.rearYawLag);
    state.tailBody.muscleTargets[3].target = state.tailBody.particles[3].position +
                                              RotateVectorAroundY(restTailTip - restTailSeg3, state.rearYawLag);

    float tailTotalLength = glm::length(restTailSeg1 - tailBase) + glm::length(restTailSeg2 - restTailSeg1) +
                             glm::length(restTailSeg3 - restTailSeg2) + glm::length(restTailTip - restTailSeg3);
    float tailGravity = kTailGravityPerUnitLength * tailTotalLength;
    StepPhysics(state.tailBody, dt, glm::vec3(0.0f, -tailGravity, 0.0f));

    // The physics above runs with muscle targets rotated by rearYawLag so
    // inertia has something real to react to (like the rope test's moving
    // anchor) — but main.cpp's outer bodyTransform ALSO rotates the whole
    // local skeleton by rearYawLag when rendering. Storing the raw physics
    // result would rotate by rearYawLag twice (a 90° turn spinning the tail
    // by 180° — the "se da la vuelta" bug), so undo that same rotation here;
    // the outer transform re-applies it once, correctly, recovering the
    // genuine (lagging) world result.
    animated.joints[NeckEnd] = state.neckEnd;
    animated.joints[SnoutBase] = snoutBaseRigid;
    animated.joints[HeadTip] = state.headTip;
    animated.joints[TailSeg1] = RotateAroundY(state.tailBody.particles[1].position, tailBase, -state.rearYawLag);
    animated.joints[TailSeg2] = RotateAroundY(state.tailBody.particles[2].position, tailBase, -state.rearYawLag);
    animated.joints[TailSeg3] = RotateAroundY(state.tailBody.particles[3].position, tailBase, -state.rearYawLag);
    animated.joints[TailTip] = RotateAroundY(state.tailBody.particles[4].position, tailBase, -state.rearYawLag);

    // Horns/ears/eyes: rotate their rest-pose offset from SnoutBase (the
    // cranium, where they actually attach — see Skeleton.cpp) by however
    // much the head itself has bent (headAngleLag), THEN anchor at wherever
    // the cranium currently is (snoutBaseRigid). A pure translation delta
    // (the old approach) only approximates a small rotation — once turns
    // bend the head by tens of degrees, translating alone visibly detaches
    // them from the skull's actual surface, e.g. an eye still pointing the
    // old direction while the head mesh underneath it has turned.
    animated.joints[HornTip] = snoutBaseRigid + RotateVectorAroundY(rest.joints[HornTip] - restSnoutBase, state.headAngleLag);
    animated.joints[LeftEarTip] = snoutBaseRigid + RotateVectorAroundY(rest.joints[LeftEarTip] - restSnoutBase, state.headAngleLag);
    animated.joints[RightEarTip] = snoutBaseRigid + RotateVectorAroundY(rest.joints[RightEarTip] - restSnoutBase, state.headAngleLag);
    animated.joints[LeftEye] = snoutBaseRigid + RotateVectorAroundY(rest.joints[LeftEye] - restSnoutBase, state.headAngleLag);
    animated.joints[RightEye] = snoutBaseRigid + RotateVectorAroundY(rest.joints[RightEye] - restSnoutBase, state.headAngleLag);

    state.breathScale = sinf(time * kBreathSpeed) * kBreathAmount;

    return animated;
}
