#pragma once

#include <vector>
#include <glm/glm.hpp>

// FABRIK (Forward And Backward Reaching Inverse Kinematics): solves a chain
// of joints with fixed segment lengths so the last joint reaches `target` as
// closely as possible, keeping the first joint (the root/hip) pinned in
// place. `chain` is both the starting guess (its shape hints which way to
// bend) and the solved output.
void SolveFABRIK(std::vector<glm::vec3>& chain, const glm::vec3& target, int iterations = 10, float tolerance = 0.001f);

// Analytic 2-bone IK (law of cosines) for an exactly hip->knee->foot chain.
// Unlike generic FABRIK, this always bends toward `poleDir` (a fixed
// direction, e.g. the body's local forward axis) instead of whatever
// direction the iterative solve happens to converge to — for a 2-segment
// chain FABRIK has no explicit bend-direction constraint and can flip the
// knee the wrong way when the target moves; this can't, by construction.
// Returns the solved knee position and writes the solved foot position
// (== target, or the closest reachable point if target is out of range)
// to outFoot.
glm::vec3 SolveTwoBoneIK(const glm::vec3& hip, const glm::vec3& target, float upperLength, float lowerLength,
                          const glm::vec3& poleDir, glm::vec3& outFoot);
