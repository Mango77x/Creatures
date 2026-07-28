#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

Camera::Camera(glm::vec3 target, float distance, float yawRadians, float pitchRadians)
    : m_Target(target), m_Distance(distance), m_YawRadians(yawRadians), m_PitchRadians(pitchRadians) {}

void Camera::FitToGround(float groundHalfSize, float extraHeight) {
    // Same forward/right/up basis GetViewMatrix's lookAt implies, computed
    // directly here so ground corners can be projected onto it without
    // building a full view matrix.
    glm::vec3 forward = glm::normalize(glm::vec3(
        -cosf(m_PitchRadians) * sinf(m_YawRadians),
        -sinf(m_PitchRadians),
        -cosf(m_PitchRadians) * cosf(m_YawRadians)));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 camUp = glm::cross(right, forward);

    float maxRight = 0.0f;
    float maxUp = 0.0f;
    const glm::vec3 corners[4] = {
        {groundHalfSize, 0.0f, groundHalfSize}, {groundHalfSize, 0.0f, -groundHalfSize},
        {-groundHalfSize, 0.0f, groundHalfSize}, {-groundHalfSize, 0.0f, -groundHalfSize},
    };
    for (const glm::vec3& corner : corners) {
        glm::vec3 delta = corner - m_Target;
        maxRight = std::max(maxRight, fabsf(glm::dot(delta, right)));
        maxUp = std::max(maxUp, fabsf(glm::dot(delta, camUp)));
    }

    // Vertical clearance (boundary walls, a tall creature) is added as its
    // own term rather than paired with the worst-case ground corner — a
    // tall object doesn't actually stand at the map's extreme diagonal
    // corner, so combining the two independently (instead of checking every
    // corner+height combination) avoids planning for a worst case nothing in
    // the scene produces, which was leaving far more empty margin than the
    // small kMargin below suggests.
    maxUp += extraHeight * camUp.y;

    constexpr float kMargin = 1.05f; // small safety margin so edges don't touch the viewport border
    m_OrthoHalfWidth = maxRight * kMargin;
    m_OrthoHalfHeight = maxUp * kMargin;
}

glm::mat4 Camera::GetViewMatrix() const {
    glm::vec3 position;
    position.x = m_Target.x + m_Distance * cosf(m_PitchRadians) * sinf(m_YawRadians);
    position.y = m_Target.y + m_Distance * sinf(m_PitchRadians);
    position.z = m_Target.z + m_Distance * cosf(m_PitchRadians) * cosf(m_YawRadians);

    return glm::lookAt(position, m_Target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const {
    // Fit-to-contain: whichever dimension the current aspect ratio makes
    // tighter drives the box size, so the FitToGround extent stays fully
    // visible at any window shape, not just the one it happened to be
    // computed at.
    float halfHeight = std::max(m_OrthoHalfHeight, m_OrthoHalfWidth / aspectRatio);
    float halfWidth = halfHeight * aspectRatio;
    return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, 0.1f, 100.0f);
}
