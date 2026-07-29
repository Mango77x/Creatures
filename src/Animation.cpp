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

    constexpr float kRearFollowSpeed = 2.0f;   // hips / global transform — slowest of all, still hand-eased

    // Spine/neck/head physics (Phase 10, step 3): replaces the old hand-
    // chained ExpLerpAngle sequence (chestAngleLag->spine3->spine2->
    // spine1AngleLag, and a separately-timed headAngleLag) with a PhysicsBody
    // (state.spineBody) — same generic solver the tail already uses. Each
    // particle gets its OWN muscle rate pulling toward the SAME kinematic
    // target (the rest offset rotated by the full, immediate bodyYaw — see
    // ApplyAnimation), rather than each link's target being hand-set to the
    // previous link's already-lagged value. The "wave propagating back
    // through the body" still happens — HeadTip reacts fastest, SpineSeg1
    // (nearest the pelvis) slowest — but now it emerges from the solver's
    // own inertia plus the rigid distance/angle constraints holding
    // neighbours together, not from literally chaining targets. Rates keep
    // the same relative ordering/values the old follow-speeds used.
    constexpr float kSpine1MuscleRate = 2.6f;
    constexpr float kSpine2MuscleRate = 3.2f;
    constexpr float kSpine3MuscleRate = 4.0f;
    constexpr float kChestMuscleRate = 5.0f;
    // NeckEnd's rate steps up sharply past the chest's — a real animal's
    // head/neck lead a turn (looking where it's going before the shoulders
    // catch up). NeckEnd is the fastest/last particle in the chain — it
    // stands in for the whole rigid skull's inertia too, since nothing past
    // NeckEnd (SnoutBase, HeadTip, horns/ears/eyes) is separately simulated
    // (see spineBody's comment in Animation.h).
    constexpr float kNeckMuscleRate = 14.0f;
    // Bend limits at each spine/neck joint. Two tiers: the spine itself
    // (pivots SpineSeg1-3) stays fairly rigid per-joint — real vertebrae
    // don't fold much individually, even though the whole spine's cumulative
    // curve can be considerable. The chest/neck joint (pivot ChestEnd, i.e.
    // how much the neck bone itself can angle away from the spine's own
    // direction) gets a much more permissive limit, matching how flexible a
    // real neck is compared to the torso.
    constexpr float kSpineMinBendAngle = glm::radians(150.0f);
    constexpr float kNeckMinBendAngle = glm::radians(90.0f);
    constexpr float kSpineNeckMaxBendAngle = glm::pi<float>();
    // Much weaker than the tail's kTailGravityPerUnitLength: a real spine/
    // neck is actively held up by strong back/neck muscles (modelled here as
    // the muscle targets above), not passively hanging — gravity should only
    // read as a subtle droop, not the dominant force the way it is for the
    // tail.
    constexpr float kSpineGravityPerUnitLength = 6.0f;

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

    // Rotates `v` around an arbitrary unit `axis` by `angleRad` (Rodrigues'
    // rotation formula) — the general 3D version of RotateVectorAroundY
    // above, needed for the head's rigid attachments (see
    // RotateVectorShortestArc): a real skull can pitch up/down as well as
    // yaw, so a Y-only rotation can't correctly carry it.
    glm::vec3 RotateAroundAxisGeneric(const glm::vec3& v, const glm::vec3& axis, float angleRad) {
        float c = cosf(angleRad);
        float s = sinf(angleRad);
        return v * c + glm::cross(axis, v) * s + axis * glm::dot(axis, v) * (1.0f - c);
    }

    // Rotates `v` by whatever rotation takes unit vector `fromDir` to unit
    // vector `toDir` (shortest arc) — used to carry the head's rigid
    // attachments (HeadTip, horns, ears, eyes) along with wherever the
    // NeckEnd->SnoutBase segment is ACTUALLY pointing after physics, in full
    // 3D. Two earlier attempts tracked a single hand-eased Y-axis angle
    // instead (headAngleLag) — that can only ever represent a turn, not a
    // segment that also pitches up/down, which is exactly what happens with
    // a steep neckPitch or a taller/shorter body: the snout would tip the
    // wrong way or wobble because a 1D angle was being asked to stand in for
    // a genuinely 3D rotation. This has no separate state or hand-tuned lag
    // of its own — NeckEnd/SnoutBase already carry the physics chain's own
    // lag/inertia, so deriving directly from their real positions inherits
    // that for free instead of re-approximating it.
    glm::vec3 RotateVectorShortestArc(const glm::vec3& v, const glm::vec3& fromDir, const glm::vec3& toDir) {
        glm::vec3 axis = glm::cross(fromDir, toDir);
        float axisLen = glm::length(axis);
        float cosAngle = glm::clamp(glm::dot(fromDir, toDir), -1.0f, 1.0f);
        if (axisLen < 1e-6f) {
            if (cosAngle > 0.0f) return v; // already aligned, no rotation needed
            // fromDir/toDir point exactly opposite (180 degrees) — cross
            // product gives no usable axis, so pick any vector not parallel
            // to fromDir and use the perpendicular between them instead.
            glm::vec3 fallback = (fabsf(fromDir.x) < 0.9f) ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 perp = glm::normalize(glm::cross(fromDir, fallback));
            return RotateAroundAxisGeneric(v, perp, glm::pi<float>());
        }
        axis /= axisLen;
        float angle = acosf(cosAngle);
        return RotateAroundAxisGeneric(v, axis, angle);
    }

    // Signed angle (radians) around Y such that RotateVectorAroundY(from,
    // angle) ~= to (both projected onto XZ, ignoring any Y component) — used
    // to turn spineBody's simulated ChestEnd position back into the single
    // scalar angle main.cpp's front-hip/gait-target rotation still expects
    // (chestAngleLag). Only safe for a mostly-horizontal segment (like the
    // spine) — see RotateVectorShortestArc for why the head needed a full 3D
    // rotation instead of this.
    float SignedYAngleBetween(const glm::vec3& from, const glm::vec3& to) {
        float crossY = from.z * to.x - from.x * to.z;
        float dotXZ = from.x * to.x + from.z * to.z;
        return atan2f(crossY, dotXZ);
    }
}

Skeleton ApplyAnimation(AnimationState& state, const Skeleton& rest, float time, float dt,
                         const glm::vec3& lookAtTarget, float bodyYaw) {
    Skeleton animated = rest;

    const glm::vec3 restSpineSeg1 = rest.joints[SpineSeg1];
    const glm::vec3 restSpineSeg2 = rest.joints[SpineSeg2];
    const glm::vec3 restSpineSeg3 = rest.joints[SpineSeg3];
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
        state.headLean = glm::vec3(0.0f);
        state.rearYawLag = bodyYaw;
        state.chestAngleLag = 0.0f;

        // Spine/neck PhysicsBody: particle 0 = Pelvis (pinned, inverseMass
        // 0), 1-3 = SpineSeg1-3, 4 = ChestEnd, 5 = NeckEnd. No SnoutBase/
        // HeadTip particle — see spineBody's comment in Animation.h. Rest
        // lengths/positions come straight from the rest skeleton — no new
        // authoring.
        state.spineBody.particles = {
            {pelvis, pelvis, 0.0f},
            {restSpineSeg1, restSpineSeg1, 1.0f},
            {restSpineSeg2, restSpineSeg2, 1.0f},
            {restSpineSeg3, restSpineSeg3, 1.0f},
            {restChestEnd, restChestEnd, 1.0f},
            {restNeckEnd, restNeckEnd, 1.0f},
        };
        state.spineBody.distanceConstraints = {
            {0, 1, glm::length(restSpineSeg1 - pelvis)},
            {1, 2, glm::length(restSpineSeg2 - restSpineSeg1)},
            {2, 3, glm::length(restSpineSeg3 - restSpineSeg2)},
            {3, 4, glm::length(restChestEnd - restSpineSeg3)},
            {4, 5, glm::length(restNeckEnd - restChestEnd)},
        };
        // Pivot ChestEnd (between the spine's own last segment and the neck
        // bone) gets the permissive neck limit — that's the joint that
        // represents how much the neck itself can angle away from the
        // spine, which is where real neck flexibility actually lives.
        state.spineBody.angleConstraints = {
            {0, 1, 2, kSpineMinBendAngle, kSpineNeckMaxBendAngle},
            {1, 2, 3, kSpineMinBendAngle, kSpineNeckMaxBendAngle},
            {2, 3, 4, kSpineMinBendAngle, kSpineNeckMaxBendAngle},
            {3, 4, 5, kNeckMinBendAngle, kSpineNeckMaxBendAngle},
        };
        state.spineBody.muscleTargets = {
            {1, restSpineSeg1, kSpine1MuscleRate},
            {2, restSpineSeg2, kSpine2MuscleRate},
            {3, restSpineSeg3, kSpine3MuscleRate},
            {4, restChestEnd, kChestMuscleRate},
            {5, restNeckEnd, kNeckMuscleRate},
        };

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
    // slowest of the whole chain, still hand-eased (nothing in spineBody
    // drives the pelvis itself).
    state.rearYawLag = ExpLerpAngle(state.rearYawLag, bodyYaw, kRearFollowSpeed, dt);

    // Refresh rest lengths every frame — same reasoning as tailBody just
    // below: the skeleton is rebuilt from currentDNA every frame, so live-
    // editing e.g. bodyLength/neckLength/headLength must reshape the chain
    // immediately instead of fighting stale rest lengths from whenever
    // spineBody was first built.
    state.spineBody.distanceConstraints[0].restLength = glm::length(restSpineSeg1 - pelvis);
    state.spineBody.distanceConstraints[1].restLength = glm::length(restSpineSeg2 - restSpineSeg1);
    state.spineBody.distanceConstraints[2].restLength = glm::length(restSpineSeg3 - restSpineSeg2);
    state.spineBody.distanceConstraints[3].restLength = glm::length(restChestEnd - restSpineSeg3);
    state.spineBody.distanceConstraints[4].restLength = glm::length(restNeckEnd - restChestEnd);

    // A small idle bob/sway "leader" signal the neck/tail chains chase with a
    // delay — this is what makes a creature that never moves still read as
    // breathing/alive, per CLAUDE.md's Phase 6 goal. Only the neck/head
    // targets get it below (matching the old neckLeader), not the spine
    // segments/chest. The tail's own downward droop isn't baked in here at
    // all — it comes from kTailGravityPerUnitLength acting on the tailBody
    // physics below.
    glm::vec3 neckBob(0.0f, sinf(time * kBobSpeed) * kBobAmount, sinf(time * kBobSpeed * 0.5f) * kBobAmount * 0.5f);
    glm::vec3 tailBob(0.0f, sinf(time * kBobSpeed + 1.0f) * kBobAmount, 0.0f);
    glm::vec3 tailLeader = tailBase + tailBob;

    // Muscle targets: rotate each particle's rest offset from the pelvis by
    // the FULL immediate bodyYaw — NOT rearYawLag, and NOT chained off a
    // neighbour's own (already-lagged) target. This is what gives the
    // solver a genuinely moving target to react to every frame (same
    // reasoning as the tail's rearYawLag-rotated targets below), and it's
    // what lets a fast muscle rate (NeckEnd) visibly lead a slow one
    // (SpineSeg1) despite both chasing the "same" underlying turn. Results
    // are un-rotated by rearYawLag (not bodyYaw) below — the gap left
    // between the two reproduces the old kinematic system's equilibrium
    // (chest/head lead the hips by bodyYaw-rearYawLag, not by the whole
    // turn) while still letting the physics feel the turn happen in real
    // time, exactly like the tail's own target-rotate/result-unrotate
    // pattern just uses a different pair of angles.
    state.spineBody.particles[0].position = pelvis; // pelvis pin, kept explicit
    state.spineBody.muscleTargets[0].target = pelvis + RotateVectorAroundY(restSpineSeg1 - pelvis, bodyYaw);
    state.spineBody.muscleTargets[1].target = pelvis + RotateVectorAroundY(restSpineSeg2 - pelvis, bodyYaw);
    state.spineBody.muscleTargets[2].target = pelvis + RotateVectorAroundY(restSpineSeg3 - pelvis, bodyYaw);
    state.spineBody.muscleTargets[3].target = pelvis + RotateVectorAroundY(restChestEnd - pelvis, bodyYaw);
    state.spineBody.muscleTargets[4].target = pelvis + RotateVectorAroundY(restNeckEnd - pelvis, bodyYaw) + neckBob;

    float spineTotalLength = glm::length(restSpineSeg1 - pelvis) + glm::length(restSpineSeg2 - restSpineSeg1) +
                              glm::length(restSpineSeg3 - restSpineSeg2) + glm::length(restChestEnd - restSpineSeg3) +
                              glm::length(restNeckEnd - restChestEnd);
    float spineGravity = kSpineGravityPerUnitLength * spineTotalLength;
    StepPhysics(state.spineBody, dt, glm::vec3(0.0f, -spineGravity, 0.0f));

    // Un-rotate by rearYawLag before use — see the muscle-target comment
    // above and the tail's identical pattern below. main.cpp's outer
    // bodyTransform re-applies rearYawLag once at render time.
    glm::vec3 spineSeg1Final = RotateAroundY(state.spineBody.particles[1].position, pelvis, -state.rearYawLag);
    glm::vec3 spineSeg2Final = RotateAroundY(state.spineBody.particles[2].position, pelvis, -state.rearYawLag);
    glm::vec3 spineSeg3Final = RotateAroundY(state.spineBody.particles[3].position, pelvis, -state.rearYawLag);
    glm::vec3 chestEndFinal = RotateAroundY(state.spineBody.particles[4].position, pelvis, -state.rearYawLag);
    glm::vec3 neckEndFinal = RotateAroundY(state.spineBody.particles[5].position, pelvis, -state.rearYawLag);

    animated.joints[SpineSeg1] = spineSeg1Final;
    animated.joints[SpineSeg2] = spineSeg2Final;
    animated.joints[SpineSeg3] = spineSeg3Final;
    animated.joints[ChestEnd] = chestEndFinal;

    // chestAngleLag: still derived from spineBody's actual simulated
    // ChestEnd position (relative to the pelvis) — see the fields' comment
    // in Animation.h. Has to track the real physics result, not a hand-eased
    // approximation, because main.cpp's front-hip/gait-target rotation below
    // needs the front legs to stay glued to wherever the chest ACTUALLY is;
    // a mismatch there is exactly the "floating leg" bug Phase 9 fixed.
    state.chestAngleLag = SignedYAngleBetween(restChestEnd - pelvis, chestEndFinal - pelvis);

    // The whole skull (SnoutBase, HeadTip, horns, ears, eyes) is carried
    // rigidly by the ACTUAL 3D rotation the neck bone itself (ChestEnd->
    // NeckEnd) underwent — see RotateVectorShortestArc's comment above for
    // why a single Y-axis angle (two earlier attempts) couldn't work, and
    // spineBody's comment in Animation.h for why the skull starts exactly at
    // NeckEnd instead of having its own extra flex joint. restNeckSegDir/
    // finalNeckSegDir already fold in whatever lag/inertia the physics gave
    // NeckEnd, so nothing extra needs to be tracked or eased by hand. Guarded
    // against a near-zero-length neck (neckLength pushed to its minimum in
    // the live DNA panel) — normalizing a near-zero vector would produce NaN.
    glm::vec3 neckSegRest = restNeckEnd - restChestEnd;
    glm::vec3 neckSegFinal = neckEndFinal - chestEndFinal;
    glm::vec3 restNeckSegDir = glm::length(neckSegRest) > 1e-4f ? glm::normalize(neckSegRest) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 finalNeckSegDir = glm::length(neckSegFinal) > 1e-4f ? glm::normalize(neckSegFinal) : restNeckSegDir;
    glm::vec3 snoutBaseFinal = neckEndFinal + RotateVectorShortestArc(restSnoutBase - restNeckEnd, restNeckSegDir, finalNeckSegDir);
    glm::vec3 headTipFinal = snoutBaseFinal + RotateVectorShortestArc(restHeadTip - restSnoutBase, restNeckSegDir, finalNeckSegDir);

    animated.joints[FrontLeftHip] = RotateAroundY(rest.joints[FrontLeftHip], pelvis, state.chestAngleLag);
    animated.joints[FrontRightHip] = RotateAroundY(rest.joints[FrontRightHip], pelvis, state.chestAngleLag);

    // Only the look-at glance gets its own lag, as a small offset layered on
    // top of the otherwise-rigid head — lagging a small nudge can't stretch
    // anything, unlike lagging the head's whole position could. Applied on
    // top of headTipFinal only for the render position (below); horns/ears
    // /eyes above are rotated from headTipFinal's own un-leaned direction,
    // so a glance can't subtly rotate them.
    glm::vec3 leanTarget = lookAtTarget - headTipFinal;
    float leanLen = glm::length(leanTarget);
    if (leanLen > kMaxHeadLean && leanLen > 1e-5f) {
        leanTarget *= (kMaxHeadLean / leanLen);
    }
    state.headLean = ExpLerp(state.headLean, leanTarget, kLookAtSpeed, dt);

    // Tail: muscle targets are where each segment would rigidly be if the
    // tail followed the pelvis's current (already-lagged) orientation
    // instantly — rearYawLag directly, no separate hand-tuned extra lag
    // angle needed anymore. Each segment's target chains off the previous
    // segment's own simulated (already physically lagging) position from
    // the previous solve — one-frame-delayed, imperceptible at frame rate.
    // (Unlike the tail, the spine/neck/head chain above targets everyone
    // off the pelvis directly instead of chaining — see its own comment.)
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

    // The tail's physics runs with muscle targets rotated by rearYawLag so
    // inertia has something real to react to (like the rope test's moving
    // anchor) — but main.cpp's outer bodyTransform ALSO rotates the whole
    // local skeleton by rearYawLag when rendering. Storing the raw physics
    // result would rotate by rearYawLag twice (a 90° turn spinning the tail
    // by 180° — the "se da la vuelta" bug), so undo that same rotation here;
    // the outer transform re-applies it once, correctly, recovering the
    // genuine (lagging) world result.
    animated.joints[NeckEnd] = neckEndFinal;
    animated.joints[SnoutBase] = snoutBaseFinal;
    animated.joints[HeadTip] = headTipFinal + state.headLean;
    animated.joints[TailSeg1] = RotateAroundY(state.tailBody.particles[1].position, tailBase, -state.rearYawLag);
    animated.joints[TailSeg2] = RotateAroundY(state.tailBody.particles[2].position, tailBase, -state.rearYawLag);
    animated.joints[TailSeg3] = RotateAroundY(state.tailBody.particles[3].position, tailBase, -state.rearYawLag);
    animated.joints[TailTip] = RotateAroundY(state.tailBody.particles[4].position, tailBase, -state.rearYawLag);

    // Horns/ears/eyes: rotate their rest-pose offset from SnoutBase (the
    // cranium, where they actually attach — see Skeleton.cpp) by the same
    // real 3D rotation used for HeadTip above, THEN anchor at wherever the
    // cranium currently is (snoutBaseFinal). A pure translation delta (the
    // old approach, pre-Phase-10) only approximates a small rotation — once
    // turns bend the head by tens of degrees, translating alone visibly
    // detaches them from the skull's actual surface.
    animated.joints[HornTip] = snoutBaseFinal + RotateVectorShortestArc(rest.joints[HornTip] - restSnoutBase, restNeckSegDir, finalNeckSegDir);
    animated.joints[LeftEarTip] = snoutBaseFinal + RotateVectorShortestArc(rest.joints[LeftEarTip] - restSnoutBase, restNeckSegDir, finalNeckSegDir);
    animated.joints[RightEarTip] = snoutBaseFinal + RotateVectorShortestArc(rest.joints[RightEarTip] - restSnoutBase, restNeckSegDir, finalNeckSegDir);
    animated.joints[LeftEye] = snoutBaseFinal + RotateVectorShortestArc(rest.joints[LeftEye] - restSnoutBase, restNeckSegDir, finalNeckSegDir);
    animated.joints[RightEye] = snoutBaseFinal + RotateVectorShortestArc(rest.joints[RightEye] - restSnoutBase, restNeckSegDir, finalNeckSegDir);

    state.breathScale = sinf(time * kBreathSpeed) * kBreathAmount;

    return animated;
}
