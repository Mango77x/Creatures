#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace {
    constexpr float kScrollSensitivity = 0.5f;
    constexpr float kMinDistance = 1.5f;
    constexpr float kMaxDistance = 40.0f;
}

Camera::Camera(glm::vec3 target, float distance, float yawRadians, float pitchRadians)
    : m_Target(target), m_Distance(distance), m_YawRadians(yawRadians), m_PitchRadians(pitchRadians) {}

void Camera::ProcessScroll(float yOffset) {
    m_Distance -= yOffset * kScrollSensitivity;
    m_Distance = std::clamp(m_Distance, kMinDistance, kMaxDistance);
}

glm::mat4 Camera::GetViewMatrix() const {
    glm::vec3 position;
    position.x = m_Target.x + m_Distance * cosf(m_PitchRadians) * sinf(m_YawRadians);
    position.y = m_Target.y + m_Distance * sinf(m_PitchRadians);
    position.z = m_Target.z + m_Distance * cosf(m_PitchRadians) * cosf(m_YawRadians);

    return glm::lookAt(position, m_Target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
}
