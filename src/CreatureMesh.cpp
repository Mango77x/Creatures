#include "CreatureMesh.h"

#include <glm/gtc/constants.hpp>
#include <algorithm>

namespace {
    constexpr int kCylinderSegments = 10;
    constexpr int kSphereSlices = 8;
    constexpr int kSphereStacks = 6;

    // Two vectors perpendicular to axis and to each other, so points around a
    // circle can be built as center + cos(t)*side + sin(t)*up.
    void PerpendicularBasis(const glm::vec3& axis, glm::vec3& side, glm::vec3& up) {
        glm::vec3 reference = std::abs(axis.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        side = glm::normalize(glm::cross(reference, axis));
        up = glm::cross(axis, side);
    }

    void AppendTriangle(std::vector<MeshVertex>& out, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                         const glm::vec3& na, const glm::vec3& nb, const glm::vec3& nc) {
        out.push_back({a, na});
        out.push_back({b, nb});
        out.push_back({c, nc});
    }

    void AppendCylinder(std::vector<MeshVertex>& out, const glm::vec3& start, const glm::vec3& end,
                         float startRadius, float endRadius) {
        glm::vec3 axis = end - start;
        float length = glm::length(axis);
        if (length < 1e-5f) return;
        axis /= length;

        glm::vec3 side, up;
        PerpendicularBasis(axis, side, up);

        for (int i = 0; i < kCylinderSegments; ++i) {
            float t0 = glm::two_pi<float>() * static_cast<float>(i) / kCylinderSegments;
            float t1 = glm::two_pi<float>() * static_cast<float>(i + 1) / kCylinderSegments;

            glm::vec3 dir0 = cosf(t0) * side + sinf(t0) * up;
            glm::vec3 dir1 = cosf(t1) * side + sinf(t1) * up;

            glm::vec3 startA = start + dir0 * startRadius;
            glm::vec3 startB = start + dir1 * startRadius;
            glm::vec3 endA = end + dir0 * endRadius;
            glm::vec3 endB = end + dir1 * endRadius;

            AppendTriangle(out, startA, endA, endB, dir0, dir0, dir1);
            AppendTriangle(out, startA, endB, startB, dir0, dir1, dir1);
        }
    }

    void AppendSphere(std::vector<MeshVertex>& out, const glm::vec3& center, float radius) {
        if (radius < 1e-5f) return;

        for (int stack = 0; stack < kSphereStacks; ++stack) {
            float phi0 = glm::pi<float>() * static_cast<float>(stack) / kSphereStacks;
            float phi1 = glm::pi<float>() * static_cast<float>(stack + 1) / kSphereStacks;

            for (int slice = 0; slice < kSphereSlices; ++slice) {
                float theta0 = glm::two_pi<float>() * static_cast<float>(slice) / kSphereSlices;
                float theta1 = glm::two_pi<float>() * static_cast<float>(slice + 1) / kSphereSlices;

                auto pointOnSphere = [](float phi, float theta) {
                    return glm::vec3(sinf(phi) * cosf(theta), cosf(phi), sinf(phi) * sinf(theta));
                };

                glm::vec3 n00 = pointOnSphere(phi0, theta0);
                glm::vec3 n01 = pointOnSphere(phi0, theta1);
                glm::vec3 n10 = pointOnSphere(phi1, theta0);
                glm::vec3 n11 = pointOnSphere(phi1, theta1);

                glm::vec3 p00 = center + n00 * radius;
                glm::vec3 p01 = center + n01 * radius;
                glm::vec3 p10 = center + n10 * radius;
                glm::vec3 p11 = center + n11 * radius;

                AppendTriangle(out, p00, p10, p11, n00, n10, n11);
                AppendTriangle(out, p00, p11, p01, n00, n11, n01);
            }
        }
    }

    float BoneRadius(BoneKind kind, const DNA& dna) {
        switch (kind) {
            case BoneKind::Spine: return 0.22f + dna.bodyFat * 0.18f + dna.muscle * 0.08f;
            case BoneKind::Neck:  return 0.12f + dna.muscle * 0.05f;
            case BoneKind::Head:  return 0.16f + dna.eyeSize * 0.2f;
            case BoneKind::Tail:  return 0.08f + dna.muscle * 0.04f;
            case BoneKind::Leg:   return 0.09f + dna.muscle * 0.06f;
        }
        return 0.1f;
    }
}

std::vector<MeshVertex> BuildCreatureMesh(const Skeleton& skeleton, const DNA& dna, float breathScale) {
    std::vector<MeshVertex> mesh;

    auto effectiveRadius = [&](const Bone& bone, float radiusScale) {
        float radius = BoneRadius(bone.kind, dna) * radiusScale;
        if (bone.kind == BoneKind::Spine) radius *= (1.0f + breathScale);
        return radius;
    };

    std::vector<float> jointRadius(skeleton.joints.size(), 0.0f);
    for (const Bone& bone : skeleton.bones) {
        float startRadius = effectiveRadius(bone, bone.startRadiusScale);
        float endRadius = effectiveRadius(bone, bone.endRadiusScale);
        jointRadius[bone.startJoint] = std::max(jointRadius[bone.startJoint], startRadius);
        jointRadius[bone.endJoint] = std::max(jointRadius[bone.endJoint], endRadius);
    }

    for (const Bone& bone : skeleton.bones) {
        float startRadius = effectiveRadius(bone, bone.startRadiusScale);
        float endRadius = effectiveRadius(bone, bone.endRadiusScale);
        AppendCylinder(mesh, skeleton.joints[bone.startJoint], skeleton.joints[bone.endJoint], startRadius, endRadius);
    }

    for (size_t i = 0; i < skeleton.joints.size(); ++i) {
        AppendSphere(mesh, skeleton.joints[i], jointRadius[i]);
    }

    return mesh;
}
