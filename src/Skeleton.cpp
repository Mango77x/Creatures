#include "Skeleton.h"
#include "IK.h"

#include <glm/gtc/constants.hpp>

namespace {
    // Vertical offset at fraction t (0 = pelvis, 1 = chest), peaking at
    // mid-body like SpineProfile's bulge — positive archAmount arches the
    // spine up (cat/hyena-like), negative sags it (swayback). Both ends stay
    // put (sinf(0) = sinf(pi) = 0) since that's where the legs actually
    // attach; only the unsupported mid-span moves.
    float SpineArchOffset(float t, float archAmount) {
        return sinf(t * glm::pi<float>()) * archAmount;
    }

    // Spine width-profile multiplier at fraction t (0 = pelvis, 1 = chest):
    // narrowest at both attachments, fullest mid-body — a barrel ribcage
    // instead of a uniform-taper tube. bodyFat pushes the mid-body bulge
    // further out; a lean creature reads closer to a plain cylinder.
    float SpineProfile(float t, float bodyFat) {
        constexpr float kMinScale = 0.7f;
        float bulge = (1.0f - kMinScale) + bodyFat * 0.15f;
        return kMinScale + bulge * sinf(t * glm::pi<float>());
    }

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

    // Front/back leg height asymmetry (legHeightBias): pelvis and chestEnd no
    // longer sit at the same height, which is what made every seed stand
    // perfectly level regardless of DNA — no real quadruped does that.
    const float backLegLength = dna.bodyHeight * (1.0f - dna.legHeightBias);
    const float frontLegLength = dna.bodyHeight * (1.0f + dna.legHeightBias);
    const glm::vec3 pelvis(0.0f, backLegLength, 0.0f);
    glm::vec3 chestEnd = pelvis + forward * dna.bodyLength;
    chestEnd.y = frontLegLength;

    // Neck/tail take-off angle now comes from DNA instead of a fixed
    // constant, so seeds actually vary in silhouette (giraffe-like vertical
    // neck vs. boar-like horizontal one) and not just in length.
    const float neckPitchRad = glm::radians(dna.neckPitch);
    const glm::vec3 neckDir = glm::normalize(forward * cosf(neckPitchRad) + up * sinf(neckPitchRad));
    const glm::vec3 neckEnd = chestEnd + neckDir * dna.neckLength;
    const glm::vec3 headTip = neckEnd + neckDir * (dna.headLength * 0.3f);

    const float tailPitchRad = glm::radians(dna.tailPitch);
    const glm::vec3 tailDir = glm::normalize(-forward * cosf(tailPitchRad) + up * sinf(tailPitchRad));
    const glm::vec3 tailMid = pelvis + tailDir * (dna.tailLength * 0.5f);
    const glm::vec3 tailTip = pelvis + tailDir * dna.tailLength;

    j[Pelvis] = pelvis;
    j[ChestEnd] = chestEnd;
    j[NeckEnd] = neckEnd;
    j[HeadTip] = headTip;
    j[TailMid] = tailMid;
    j[TailTip] = tailTip;

    // Spine segmentation (Phase 9): three evenly-spaced points between
    // Pelvis and ChestEnd, straight-line interpolated then bowed vertically
    // by SpineArchOffset (spineArch was a flat line before that field
    // existed). See SpineProfile for the width each segment tapers to/from.
    j[SpineSeg1] = glm::mix(pelvis, chestEnd, 0.25f) + up * SpineArchOffset(0.25f, dna.spineArch * dna.bodyHeight);
    j[SpineSeg2] = glm::mix(pelvis, chestEnd, 0.5f) + up * SpineArchOffset(0.5f, dna.spineArch * dna.bodyHeight);
    j[SpineSeg3] = glm::mix(pelvis, chestEnd, 0.75f) + up * SpineArchOffset(0.75f, dna.spineArch * dna.bodyHeight);

    // Head appendages, offset from HeadTip. hornSize/earSize/eyeSize already
    // existed in DNA but only sized the head capsule before Phase 9 — here
    // they place actual small bone chains (horn/ears) and a point (eyes).
    //
    // The head's own joint-cap sphere (see CreatureMesh.cpp's BoneRadius for
    // BoneKind::Head) has radius ~0.17-0.30 — bigger than earLength/eyeOffset
    // were originally, so ears/eyes ended up entirely inside that sphere and
    // never poked through (invisible regardless of DNA). headRadiusApprox
    // mirrors that formula so appendages are placed relative to the actual
    // head surface instead of a fixed offset that happened to be too small.
    // Driven by headSize, not eyeSize — head size used to be a confusing
    // side effect of how big the eyes were; now it's its own DNA field.
    const float headRadiusApprox = 0.10f + dna.headSize * 0.11f;

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

    BuildLeg(j, FrontLeftHip, FrontLeftKnee, FrontLeftFoot, frontHipBase - right * sideOffset, frontLegLength, forward);
    BuildLeg(j, FrontRightHip, FrontRightKnee, FrontRightFoot, frontHipBase + right * sideOffset, frontLegLength, forward);
    BuildLeg(j, BackLeftHip, BackLeftKnee, BackLeftFoot, backHipBase - right * sideOffset, backLegLength, forward);
    BuildLeg(j, BackRightHip, BackRightKnee, BackRightFoot, backHipBase + right * sideOffset, backLegLength, forward);

    const float spineProfile0 = SpineProfile(0.0f, dna.bodyFat);
    const float spineProfile1 = SpineProfile(0.25f, dna.bodyFat);
    const float spineProfile2 = SpineProfile(0.5f, dna.bodyFat);
    const float spineProfile3 = SpineProfile(0.75f, dna.bodyFat);
    const float spineProfile4 = SpineProfile(1.0f, dna.bodyFat);

    skeleton.bones = {
        // Spine as 4 tapered segments instead of 1 uniform cylinder — see
        // SpineProfile. Reuses the existing per-end radiusScale mechanism
        // (same one the tail already uses to narrow to a point), just with
        // more segments in the chain.
        {Pelvis, SpineSeg1, BoneKind::Spine, spineProfile0, spineProfile1},
        {SpineSeg1, SpineSeg2, BoneKind::Spine, spineProfile1, spineProfile2},
        {SpineSeg2, SpineSeg3, BoneKind::Spine, spineProfile2, spineProfile3},
        {SpineSeg3, ChestEnd, BoneKind::Spine, spineProfile3, spineProfile4},
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
