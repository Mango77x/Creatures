#pragma once

#include <glm/glm.hpp>

class Camera {
public:
    // yaw/pitch default to Creatures' fixed oblique/dimetric viewing angle
    // (see CLAUDE.md's camera decision) — free rotation was a Phase 1-4
    // development convenience, not the final look.
    Camera(glm::vec3 target = glm::vec3(0.0f, 1.0f, 1.0f), float distance = 6.0f,
           float yawRadians = glm::radians(45.0f), float pitchRadians = glm::radians(32.0f));

    void ProcessScroll(float yOffset);

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

private:
    glm::vec3 m_Target;
    float m_Distance;
    float m_YawRadians;
    float m_PitchRadians;
};
