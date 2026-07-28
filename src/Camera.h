#pragma once

#include <glm/glm.hpp>

class Camera {
public:
    Camera(glm::vec3 target = glm::vec3(0.0f), float distance = 6.0f);

    void ProcessMouseDrag(float dxPixels, float dyPixels);
    void ProcessScroll(float yOffset);

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

private:
    glm::vec3 m_Target;
    float m_Distance;
    float m_YawRadians;
    float m_PitchRadians;
};
