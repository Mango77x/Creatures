#include "Gait.h"

#include <glm/gtc/constants.hpp>
#include <cmath>

glm::vec3 ComputeFootTarget(const glm::vec3& restFootLocal, float time, float phaseOffset, const GaitParams& params) {
    float phase = time * params.speed + phaseOffset;
    phase -= floorf(phase); // wrap to 0..1

    float forwardOffset;
    float height;

    if (phase < params.stanceFraction) {
        float t = phase / params.stanceFraction;
        forwardOffset = glm::mix(params.strideLength * 0.5f, -params.strideLength * 0.5f, t);
        height = 0.0f;
    } else {
        float t = (phase - params.stanceFraction) / (1.0f - params.stanceFraction);
        forwardOffset = glm::mix(-params.strideLength * 0.5f, params.strideLength * 0.5f, t);
        height = params.liftHeight * sinf(glm::pi<float>() * t);
    }

    return restFootLocal + glm::vec3(0.0f, height, 0.0f) + glm::vec3(0.0f, 0.0f, forwardOffset);
}
