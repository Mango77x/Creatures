#include "Physics.h"

#include <algorithm>
#include <cmath>

namespace {
    void IntegrateVerlet(std::vector<Particle>& particles, float dt, const glm::vec3& gravity, float damping) {
        for (Particle& p : particles) {
            if (p.inverseMass <= 0.0f) continue; // pinned
            glm::vec3 velocity = (p.position - p.previousPosition) * damping;
            glm::vec3 newPosition = p.position + velocity + gravity * (dt * dt);
            p.previousPosition = p.position;
            p.position = newPosition;
        }
    }

    void SolveDistanceConstraint(std::vector<Particle>& particles, const DistanceConstraint& c) {
        Particle& pa = particles[c.a];
        Particle& pb = particles[c.b];
        float invMassSum = pa.inverseMass + pb.inverseMass;
        if (invMassSum <= 0.0f) return; // both pinned, nothing to correct

        glm::vec3 delta = pb.position - pa.position;
        float dist = glm::length(delta);
        if (dist < 1e-6f) return;
        float diff = (dist - c.restLength) / dist;
        glm::vec3 correction = delta * diff;

        pa.position += correction * (pa.inverseMass / invMassSum);
        pb.position -= correction * (pb.inverseMass / invMassSum);
    }

    // Rotates `v` around the unit axis `axis` by `angleRad` (Rodrigues'
    // rotation formula).
    glm::vec3 RotateAroundAxis(const glm::vec3& v, const glm::vec3& axis, float angleRad) {
        float c = cosf(angleRad);
        float s = sinf(angleRad);
        return v * c + glm::cross(axis, v) * s + axis * glm::dot(axis, v) * (1.0f - c);
    }

    // Below this length, a-pivot/c-pivot's direction is numerically
    // unreliable (dividing an already-small, noisy vector by its own small
    // magnitude amplifies that noise into the unit direction) — much larger
    // than the old 1e-6f "truly coincident points" threshold, which never
    // caught this. A short-but-legitimate bone (e.g. a creature with
    // neckLength or bodyLength dialled down in the live DNA panel) can land
    // exactly in that unstable gap: not degenerate enough to hit the old
    // early-return, but short enough that dirA/dirC swing wildly frame to
    // frame, computing a near-random rotation axis and a spurious large
    // `error`. That correction gets applied to BOTH arms at the pivot,
    // including a potentially long/stable one on the other side — this was
    // the actual mechanism behind short-neck/short-body edits making the
    // whole creature spin out ("se vuelve loco").
    constexpr float kMinStableArmLength = 1e-3f;
    // Hard ceiling on how much a single relaxation iteration may rotate
    // either arm. Normal, well-conditioned corrections are already much
    // smaller than this (they converge gradually over StepPhysics's several
    // iterations); this only ever engages as a safety valve against a bad
    // direction estimate (see kMinStableArmLength above) turning into an
    // explosive single-step correction that compounds via Verlet's
    // velocity carry-over into the next frame.
    constexpr float kMaxAngleCorrectionPerIteration = 0.26f; // ~15 degrees

    void SolveAngleConstraint(std::vector<Particle>& particles, const AngleConstraint& c) {
        Particle& pa = particles[c.a];
        Particle& pp = particles[c.pivot];
        Particle& pc = particles[c.c];

        glm::vec3 toA = pa.position - pp.position;
        glm::vec3 toC = pc.position - pp.position;
        float lenA = glm::length(toA);
        float lenC = glm::length(toC);
        if (lenA < kMinStableArmLength || lenC < kMinStableArmLength) return;
        glm::vec3 dirA = toA / lenA;
        glm::vec3 dirC = toC / lenC;

        float cosAngle = glm::clamp(glm::dot(dirA, dirC), -1.0f, 1.0f);
        float angle = acosf(cosAngle);
        float clamped = glm::clamp(angle, c.minAngle, c.maxAngle);
        float error = clamped - angle;
        if (fabsf(error) < 1e-5f) return; // already within limits
        error = glm::clamp(error, -kMaxAngleCorrectionPerIteration, kMaxAngleCorrectionPerIteration);

        glm::vec3 axis = glm::cross(dirA, dirC);
        float axisLen = glm::length(axis);
        if (axisLen < 1e-6f) return; // degenerate (perfectly straight/folded) — no stable axis to correct around

        axis /= axisLen;

        float invMassSum = pa.inverseMass + pc.inverseMass;
        if (invMassSum <= 0.0f) return;
        // Rotating `c` by +angle around cross(dirA, dirC) increases the
        // angle between dirA/dirC (verified numerically); `a` needs the
        // opposite sign to widen/narrow the same angle from its side.
        float rotA = -error * (pa.inverseMass / invMassSum);
        float rotC = error * (pc.inverseMass / invMassSum);

        pa.position = pp.position + RotateAroundAxis(toA, axis, rotA);
        pc.position = pp.position + RotateAroundAxis(toC, axis, rotC);
    }

    // Applied once per frame (see MuscleTarget's comment on why this can't
    // live inside the relaxation loop below), with the same frame-rate-
    // independent exponential ease Animation.cpp's ExpLerp uses.
    void ApplyMuscleTarget(std::vector<Particle>& particles, const MuscleTarget& m, float dt) {
        Particle& p = particles[m.particle];
        if (p.inverseMass <= 0.0f) return; // pinned particles aren't pulled by muscles
        float t = 1.0f - expf(-m.rate * dt);
        p.position = glm::mix(p.position, m.target, t);
    }

    void SolveGroundConstraint(std::vector<Particle>& particles, const GroundConstraint& g) {
        Particle& p = particles[g.particle];
        if (p.position.y >= g.groundHeight) return;

        glm::vec3 velocity = p.position - p.previousPosition;
        p.position.y = g.groundHeight;
        // Zero the vertical component (stop falling through) and damp the
        // horizontal component by `friction` — Verlet has no explicit
        // velocity to scale directly, so this is done by placing
        // previousPosition accordingly.
        velocity.y = 0.0f;
        velocity.x *= (1.0f - g.friction);
        velocity.z *= (1.0f - g.friction);
        p.previousPosition = p.position - velocity;
    }
}

void StepPhysics(PhysicsBody& body, float dt, const glm::vec3& gravity, int iterations, float damping) {
    IntegrateVerlet(body.particles, dt, gravity, damping);

    // Muscle pulls happen once per frame, time-scaled — see MuscleTarget's
    // comment. The relaxation loop below is for hard structural constraints
    // only (distance/angle/ground), which are meant to be fully enforced
    // every step regardless of dt, unlike the soft muscle pull.
    for (const MuscleTarget& m : body.muscleTargets) {
        ApplyMuscleTarget(body.particles, m, dt);
    }

    for (int iter = 0; iter < iterations; ++iter) {
        for (const DistanceConstraint& c : body.distanceConstraints) {
            SolveDistanceConstraint(body.particles, c);
        }
        for (const AngleConstraint& c : body.angleConstraints) {
            SolveAngleConstraint(body.particles, c);
        }
        for (const GroundConstraint& g : body.groundConstraints) {
            SolveGroundConstraint(body.particles, g);
        }
    }
}
