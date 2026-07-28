#pragma once

#include <glm/glm.hpp>

// Fixed oblique/dimetric camera (see CLAUDE.md's camera decision). Uses a
// pure parallel/orthographic projection, not perspective — confirmed against
// the Critter Crosser reference screenshots, where sidewalk tiles and
// building edges never converge toward a vanishing point as they recede, the
// signature of an orthographic (or hand-painted non-perspective) view rather
// than a real-camera perspective one.
//
// There is no user-controllable zoom: the orthographic view volume is sized
// once, via FitToGround, to always show the entire map — matching the
// reference's fixed, fully-visible "diorama" framing instead of a
// player-scrollable camera.
class Camera {
public:
    Camera(glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f), float distance = 10.0f,
           float yawRadians = glm::radians(45.0f), float pitchRadians = glm::radians(30.0f));

    // Sizes the orthographic view volume so a (groundHalfSize * 2)-wide
    // square footprint centered on the target — plus extraHeight of vertical
    // clearance above it, for boundary walls and tall creatures — is fully
    // visible regardless of the aspect ratio GetProjectionMatrix is later
    // asked for. Call once after construction (the map doesn't change size
    // at runtime).
    void FitToGround(float groundHalfSize, float extraHeight);

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

private:
    glm::vec3 m_Target;
    float m_Distance;
    float m_YawRadians;
    float m_PitchRadians;

    // Half-extents (in view-space right/up units) required to frame the
    // ground, computed by FitToGround. GetProjectionMatrix expands whichever
    // one the current aspect ratio makes too tight, so the fit never clips.
    float m_OrthoHalfWidth = 8.0f;
    float m_OrthoHalfHeight = 5.0f;
};
