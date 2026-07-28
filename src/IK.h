#pragma once

#include <vector>
#include <glm/glm.hpp>

// FABRIK (Forward And Backward Reaching Inverse Kinematics): solves a chain
// of joints with fixed segment lengths so the last joint reaches `target` as
// closely as possible, keeping the first joint (the root/hip) pinned in
// place. `chain` is both the starting guess (its shape hints which way to
// bend) and the solved output.
void SolveFABRIK(std::vector<glm::vec3>& chain, const glm::vec3& target, int iterations = 10, float tolerance = 0.001f);
