#include "Animation.h"

namespace {
    constexpr float kNeckFollowSpeed = 8.0f;  // higher = catches up faster = shorter delay
    constexpr float kTailFollowSpeed = 5.0f;
    constexpr float kBobAmount = 0.05f;
    constexpr float kBobSpeed = 1.6f;
    constexpr float kMaxHeadLean = 0.35f;     // world units the head may lean toward the target
    constexpr float kLookAtSpeed = 4.0f;
    constexpr float kBreathSpeed = 1.2f;
    constexpr float kBreathAmount = 0.06f;

    // Exponential smoothing: closes the gap to `target` at a rate independent
    // of frame rate, which is exactly "follows with a small delay" rather than
    // snapping straight there.
    glm::vec3 ExpLerp(const glm::vec3& current, const glm::vec3& target, float speed, float dt) {
        float t = 1.0f - expf(-speed * dt);
        return glm::mix(current, target, t);
    }
}

Skeleton ApplyAnimation(AnimationState& state, const Skeleton& rest, float time, float dt,
                         const glm::vec3& lookAtTarget) {
    Skeleton animated = rest;

    const glm::vec3 restNeckEnd = rest.joints[NeckEnd];
    const glm::vec3 restHeadTip = rest.joints[HeadTip];
    const glm::vec3 restTailMid = rest.joints[TailMid];
    const glm::vec3 restTailTip = rest.joints[TailTip];
    const glm::vec3 neckBase = rest.joints[ChestEnd];  // fixed anchor, not itself animated
    const glm::vec3 tailBase = rest.joints[Pelvis];

    if (!state.initialized) {
        state.neckEnd = restNeckEnd;
        state.headTip = restHeadTip;
        state.tailMid = restTailMid;
        state.tailTip = restTailTip;
        state.initialized = true;
    }

    // A small idle bob/sway "leader" signal the neck/tail chains chase with a
    // delay — this is what makes a creature that never moves still read as
    // breathing/alive, per CLAUDE.md's Phase 6 goal.
    glm::vec3 neckBob(0.0f, sinf(time * kBobSpeed) * kBobAmount, sinf(time * kBobSpeed * 0.5f) * kBobAmount * 0.5f);
    glm::vec3 tailBob(0.0f, sinf(time * kBobSpeed + 1.0f) * kBobAmount, 0.0f);

    glm::vec3 neckLeader = neckBase + neckBob;
    glm::vec3 tailLeader = tailBase + tailBob;

    glm::vec3 neckEndTarget = neckLeader + (restNeckEnd - neckBase);
    state.neckEnd = ExpLerp(state.neckEnd, neckEndTarget, kNeckFollowSpeed, dt);

    glm::vec3 headTipTarget = state.neckEnd + (restHeadTip - restNeckEnd);
    glm::vec3 toTarget = lookAtTarget - headTipTarget;
    float toTargetLen = glm::length(toTarget);
    if (toTargetLen > kMaxHeadLean && toTargetLen > 1e-5f) {
        toTarget *= (kMaxHeadLean / toTargetLen);
    }
    headTipTarget += toTarget;
    state.headTip = ExpLerp(state.headTip, headTipTarget, kLookAtSpeed, dt);

    glm::vec3 tailMidTarget = tailLeader + (restTailMid - tailBase);
    state.tailMid = ExpLerp(state.tailMid, tailMidTarget, kTailFollowSpeed, dt);

    glm::vec3 tailTipTarget = state.tailMid + (restTailTip - restTailMid);
    state.tailTip = ExpLerp(state.tailTip, tailTipTarget, kTailFollowSpeed * 0.8f, dt);

    animated.joints[NeckEnd] = state.neckEnd;
    animated.joints[HeadTip] = state.headTip;
    animated.joints[TailMid] = state.tailMid;
    animated.joints[TailTip] = state.tailTip;

    state.breathScale = sinf(time * kBreathSpeed) * kBreathAmount;

    return animated;
}
