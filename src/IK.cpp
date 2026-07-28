#include "IK.h"

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
