#include "Skeleton.h"
#include "IK.h"

namespace {
    // Real legged animals never fully straighten their legs, even standing
    // still on flat ground — a dog, cat, or human keeps a small permanent
    // knee bend. Giving each leg more total reach (upper + lower segment)
    // than the actual standing height leaves slack that forces that bend,
    // instead of the segments summing to almost exactly the hip-to-ground
    // distance (which reads as stiff, penguin-straight legs).
    constexpr float kStandCrouchFactor = 0.82f;

    // Builds a leg's hip/knee/foot joints with a natural resting bend, using
    // the same analytic 2-bone solver the runtime IK uses — so the rest pose
    // is self-consistent with how the leg actually articulates later.
    void BuildLeg(std::vector<glm::vec3>& joints, int hipIdx, int kneeIdx, int footIdx,
                  const glm::vec3& hipPos, float standHeight, const glm::vec3& forward) {
        glm::vec3 footPos(hipPos.x, 0.0f, hipPos.z);
        float totalReach = standHeight / kStandCrouchFactor;
        float upperLength = totalReach * 0.5f;
        float lowerLength = totalReach * 0.5f;

        glm::vec3 solvedFoot;
        glm::vec3 kneePos = SolveTwoBoneIK(hipPos, footPos, upperLength, lowerLength, forward, solvedFoot);

        joints[hipIdx] = hipPos;
        joints[kneeIdx] = kneePos;
        joints[footIdx] = footPos;
    }
}

Skeleton BuildSkeleton(const DNA& dna) {
    Skeleton skeleton;
    skeleton.joints.resize(SkeletonJointCount);
    auto& j = skeleton.joints;

    const glm::vec3 forward(0.0f, 0.0f, 1.0f);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const glm::vec3 right = glm::normalize(glm::cross(up, forward));

    const float legLength = dna.bodyHeight;
    const glm::vec3 pelvis(0.0f, legLength, 0.0f);
    const glm::vec3 chestEnd = pelvis + forward * dna.bodyLength;

    const glm::vec3 neckDir = glm::normalize(up + forward);
    const glm::vec3 neckEnd = chestEnd + neckDir * dna.neckLength;
    const glm::vec3 headTip = neckEnd + neckDir * 0.3f;

    const glm::vec3 tailDir = glm::normalize(up * 0.3f - forward);
    const glm::vec3 tailMid = pelvis + tailDir * (dna.tailLength * 0.5f);
    const glm::vec3 tailTip = pelvis + tailDir * dna.tailLength;

    j[Pelvis] = pelvis;
    j[ChestEnd] = chestEnd;
    j[NeckEnd] = neckEnd;
    j[HeadTip] = headTip;
    j[TailMid] = tailMid;
    j[TailTip] = tailTip;

    // Head appendages, offset from HeadTip. hornSize/earSize/eyeSize already
    // existed in DNA but only sized the head capsule before Phase 9 — here
    // they place actual small bone chains (horn/ears) and a point (eyes).
    //
    // The head's own joint-cap sphere (see CreatureMesh.cpp's BoneRadius for
    // BoneKind::Head) has radius ~0.16-0.22 — bigger than earLength/eyeOffset
    // were originally, so ears/eyes ended up entirely inside that sphere and
    // never poked through (invisible regardless of DNA). headRadiusApprox
    // mirrors that formula so appendages are placed relative to the actual
    // head surface instead of a fixed offset that happened to be too small.
    const float headRadiusApprox = 0.16f + dna.eyeSize * 0.2f;

    // hornSize can be 0, so the horn cylinder can end up zero-length —
    // AppendCylinder already skips those, giving hornless creatures for free
    // instead of needing a separate "has horn" flag. Unlike ears/eyes, the
    // horn deliberately does NOT get a head-radius clearance added: a small
    // horn merging into the head silhouette (only clearing the surface once
    // hornSize is large enough on its own) reads as "small horn", not a bug.
    const glm::vec3 hornDir = glm::normalize(neckDir + up * 0.6f);
    j[HornTip] = headTip + hornDir * (dna.hornSize * 1.1f);

    const glm::vec3 earDirLeft = glm::normalize(up - right * 0.7f);
    const glm::vec3 earDirRight = glm::normalize(up + right * 0.7f);
    const float earReach = headRadiusApprox + 0.03f + dna.earSize * 0.35f;
    j[LeftEarTip] = headTip + earDirLeft * earReach;
    j[RightEarTip] = headTip + earDirRight * earReach;

    // Eye center sits exactly on the head surface (distance headRadiusApprox
    // from HeadTip) so roughly half the eye sphere pokes out — visible
    // without floating detached off the head.
    const glm::vec3 eyeDirLeft = glm::normalize(neckDir * 0.35f - right);
    const glm::vec3 eyeDirRight = glm::normalize(neckDir * 0.35f + right);
    j[LeftEye] = headTip + eyeDirLeft * headRadiusApprox;
    j[RightEye] = headTip + eyeDirRight * headRadiusApprox;

    const float sideOffset = 0.3f + dna.bodyFat * 0.3f;
    const glm::vec3 frontHipBase = chestEnd - forward * 0.15f;
    const glm::vec3 backHipBase = pelvis + forward * 0.15f;

    BuildLeg(j, FrontLeftHip, FrontLeftKnee, FrontLeftFoot, frontHipBase - right * sideOffset, legLength, forward);
    BuildLeg(j, FrontRightHip, FrontRightKnee, FrontRightFoot, frontHipBase + right * sideOffset, legLength, forward);
    BuildLeg(j, BackLeftHip, BackLeftKnee, BackLeftFoot, backHipBase - right * sideOffset, legLength, forward);
    BuildLeg(j, BackRightHip, BackRightKnee, BackRightFoot, backHipBase + right * sideOffset, legLength, forward);

    skeleton.bones = {
        {Pelvis, ChestEnd, BoneKind::Spine},
        {ChestEnd, NeckEnd, BoneKind::Neck},
        {NeckEnd, HeadTip, BoneKind::Head},
        {HeadTip, HornTip, BoneKind::Horn, 1.0f, 0.05f},
        {HeadTip, LeftEarTip, BoneKind::Ear, 1.0f, 0.6f},
        {HeadTip, RightEarTip, BoneKind::Ear, 1.0f, 0.6f},
        {Pelvis, TailMid, BoneKind::Tail, 1.0f, 0.55f},
        {TailMid, TailTip, BoneKind::Tail, 0.55f, 0.12f},

        {ChestEnd, FrontLeftHip, BoneKind::Leg},
        {FrontLeftHip, FrontLeftKnee, BoneKind::Leg},
        {FrontLeftKnee, FrontLeftFoot, BoneKind::Leg},

        {ChestEnd, FrontRightHip, BoneKind::Leg},
        {FrontRightHip, FrontRightKnee, BoneKind::Leg},
        {FrontRightKnee, FrontRightFoot, BoneKind::Leg},

        {Pelvis, BackLeftHip, BoneKind::Leg},
        {BackLeftHip, BackLeftKnee, BoneKind::Leg},
        {BackLeftKnee, BackLeftFoot, BoneKind::Leg},

        {Pelvis, BackRightHip, BoneKind::Leg},
        {BackRightHip, BackRightKnee, BoneKind::Leg},
        {BackRightKnee, BackRightFoot, BoneKind::Leg},
    };

    // Uniform final scale (see kCreatureScale) — applied last, after every
    // joint (including IK-solved legs) is already correctly proportioned
    // relative to the others, so this only changes overall size, not shape.
    for (glm::vec3& joint : skeleton.joints) {
        joint *= kCreatureScale;
    }

    return skeleton;
}
