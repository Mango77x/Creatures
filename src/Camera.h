#pragma once

#include <glm/glm.hpp>

// Free orbital camera — perspective projection, drag to rotate, scroll to
// zoom. Revision (2026-07-28, Phase 9): this project no longer tries to
// match Critter Crosser's fixed dimetric/orthographic presentation — that
// was explicitly dropped in favor of a "proper lab tool" camera the user can
// freely move, same spirit as the terrain no longer copying its block-terace
// look. See CLAUDE.md's camera decision history for the prior fixed/ortho
// phase this replaces.
class Camera {
public:
    Camera(glm::vec3 target = glm::vec3(0.0f, 0.3f, 0.0f), float distance = 8.0f,
           float yawRadians = 0.0f, float pitchRadians = 0.3f);

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
