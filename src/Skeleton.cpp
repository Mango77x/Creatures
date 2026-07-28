#include "Skeleton.h"

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

    const float sideOffset = 0.3f + dna.bodyFat * 0.3f;
    const glm::vec3 frontHipBase = chestEnd - forward * 0.15f;
    const glm::vec3 backHipBase = pelvis + forward * 0.15f;

    j[FrontLeftHip] = frontHipBase - right * sideOffset;
    j[FrontLeftFoot] = glm::vec3(j[FrontLeftHip].x, 0.0f, j[FrontLeftHip].z);
    j[FrontRightHip] = frontHipBase + right * sideOffset;
    j[FrontRightFoot] = glm::vec3(j[FrontRightHip].x, 0.0f, j[FrontRightHip].z);
    j[BackLeftHip] = backHipBase - right * sideOffset;
    j[BackLeftFoot] = glm::vec3(j[BackLeftHip].x, 0.0f, j[BackLeftHip].z);
    j[BackRightHip] = backHipBase + right * sideOffset;
    j[BackRightFoot] = glm::vec3(j[BackRightHip].x, 0.0f, j[BackRightHip].z);

    skeleton.bones = {
        {Pelvis, ChestEnd, BoneKind::Spine},
        {ChestEnd, NeckEnd, BoneKind::Neck},
        {NeckEnd, HeadTip, BoneKind::Head},
        {Pelvis, TailMid, BoneKind::Tail, 1.0f, 0.55f},
        {TailMid, TailTip, BoneKind::Tail, 0.55f, 0.12f},
        {ChestEnd, FrontLeftHip, BoneKind::Leg}, {FrontLeftHip, FrontLeftFoot, BoneKind::Leg},
        {ChestEnd, FrontRightHip, BoneKind::Leg}, {FrontRightHip, FrontRightFoot, BoneKind::Leg},
        {Pelvis, BackLeftHip, BoneKind::Leg}, {BackLeftHip, BackLeftFoot, BoneKind::Leg},
        {Pelvis, BackRightHip, BoneKind::Leg}, {BackRightHip, BackRightFoot, BoneKind::Leg},
    };

    return skeleton;
}
