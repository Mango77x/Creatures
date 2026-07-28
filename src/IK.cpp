#include "IK.h"

#include <cmath>
#include <algorithm>

void SolveFABRIK(std::vector<glm::vec3>& chain, const glm::vec3& target, int iterations, float tolerance) {
    int n = static_cast<int>(chain.size());
    if (n < 2) return;

    std::vector<float> segmentLength(n - 1);
    float totalLength = 0.0f;
    for (int i = 0; i < n - 1; ++i) {
        segmentLength[i] = glm::length(chain[i + 1] - chain[i]);
        totalLength += segmentLength[i];
    }

    const glm::vec3 root = chain[0];
    const float distToTarget = glm::length(target - root);

    if (distToTarget >= totalLength) {
        // Target out of reach: stretch the chain straight toward it.
        glm::vec3 dir = glm::normalize(target - root);
        for (int i = 1; i < n; ++i) {
            chain[i] = chain[i - 1] + dir * segmentLength[i - 1];
        }
        return;
    }

    for (int iter = 0; iter < iterations; ++iter) {
        if (glm::length(chain[n - 1] - target) < tolerance) break;

        // Backward pass: snap the end effector to the target, then walk back
        // toward the root, keeping each segment's length fixed.
        chain[n - 1] = target;
        for (int i = n - 2; i >= 0; --i) {
            glm::vec3 dir = glm::normalize(chain[i] - chain[i + 1]);
            chain[i] = chain[i + 1] + dir * segmentLength[i];
        }

        // Forward pass: pin the root back at its real position, then walk
        // forward again, re-fixing each segment's length.
        chain[0] = root;
        for (int i = 0; i < n - 1; ++i) {
            glm::vec3 dir = glm::normalize(chain[i + 1] - chain[i]);
            chain[i + 1] = chain[i] + dir * segmentLength[i];
        }
    }
}

glm::vec3 SolveTwoBoneIK(const glm::vec3& hip, const glm::vec3& target, float upperLength, float lowerLength,
                          const glm::vec3& poleDir, glm::vec3& outFoot) {
    glm::vec3 toTarget = target - hip;
    float rawDist = glm::length(toTarget);

    const float maxReach = upperLength + lowerLength - 1e-4f;
    const float minReach = std::abs(upperLength - lowerLength) + 1e-4f;
    float d = std::clamp(rawDist, minReach, maxReach);

    glm::vec3 dir = rawDist > 1e-5f ? toTarget / rawDist : glm::vec3(0.0f, 0.0f, 1.0f);
    outFoot = hip + dir * d;

    // Law of cosines: angle at the hip between the hip->target line and the
    // hip->knee segment.
    float cosHipAngle = (upperLength * upperLength + d * d - lowerLength * lowerLength) / (2.0f * upperLength * d);
    cosHipAngle = std::clamp(cosHipAngle, -1.0f, 1.0f);
    float hipAngle = acosf(cosHipAngle);

    // Bend axis: perpendicular to both the hip->target line and the pole
    // direction, so rotating `dir` by hipAngle around it always folds the
    // knee toward the pole side, never the opposite way.
    glm::vec3 poleOnPlane = poleDir - dir * glm::dot(poleDir, dir);
    if (glm::length(poleOnPlane) < 1e-5f) poleOnPlane = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 bendDir = glm::normalize(poleOnPlane);
    glm::vec3 bendAxis = glm::normalize(glm::cross(dir, bendDir));

    // Rodrigues' rotation formula (bendAxis is already perpendicular to dir,
    // so the parallel/(k.v) term drops out).
    glm::vec3 rotatedDir = dir * cosf(hipAngle) + glm::cross(bendAxis, dir) * sinf(hipAngle);

    return hip + rotatedDir * upperLength;
}
