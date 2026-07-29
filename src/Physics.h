#pragma once

#include <vector>
#include <glm/glm.hpp>

// Generic particle + constraint physics solver (Phase 10) — position-based
// dynamics, Jakobsen-style: Verlet integration (no explicit velocity field,
// it's implicit in the position delta) followed by several passes directly
// correcting positions to satisfy constraints, rather than integrating
// spring forces. More stable than a force/spring approach once many
// constraints interact (a whole body), unlike the tail's existing
// force-spring in Animation.cpp which is fine at its own small 2-particle
// scale but isn't the pattern to scale up — see CLAUDE.md's Phase 10 notes.
//
// Nothing here references a joint by name — only particles and generic
// constraints between them — so the same solver works for any skeleton
// topology, not just the current quadruped.

struct Particle {
    glm::vec3 position{0.0f};
    glm::vec3 previousPosition{0.0f};
    // 0 pins the particle in place — constraints/gravity can't move it, only
    // direct assignment can (e.g. the pelvis, driven by bodyPos/bodyYaw).
    float inverseMass = 1.0f;
};

// Rigid-length connection between two particles (mirrors a Skeleton::Bone).
struct DistanceConstraint {
    int a, b;
    float restLength;
};

// Bend limit at `pivot`, between arms pivot->a and pivot->c, clamping the
// angle a-pivot-c to [minAngle, maxAngle] — keeps a chain from folding past
// what's anatomically plausible (e.g. a knee bending backward).
struct AngleConstraint {
    int a, pivot, c;
    float minAngle;
    float maxAngle;
};

// A soft pull toward `target`, applied ONCE per StepPhysics call with a
// frame-rate-independent exponential ease (`1 - exp(-rate*dt)`, same
// formula Animation.cpp's ExpLerp already uses) — NOT repeated every
// relaxation iteration below. Repeating a fixed fraction every iteration
// made the pull's effective per-frame convergence depend on the iteration
// count instead of on time, converging to `target` almost instantly
// regardless of dt or how fast the target itself was moving — the bug
// behind an early build of the tail looking like a rigid spinning needle
// instead of a lagging pendulum. `rate` is in 1/seconds, same units as the
// old ExpLerp-based follow speeds (kChestFollowSpeed etc.) — higher pulls
// harder/faster, so this is how DNA rest-pose directions and Gait.cpp's
// foot targets drive the body, replacing what Animation.cpp's angle-lag
// chain and IK.cpp's exact solve used to do.
struct MuscleTarget {
    int particle;
    glm::vec3 target{0.0f};
    float rate = 0.0f; // 1/seconds
};

// Keeps a particle's Y from going below groundHeight and damps horizontal
// motion on contact — the physics equivalent of the per-leg terrain raycast
// in main.cpp.
struct GroundConstraint {
    int particle;
    float groundHeight = 0.0f;
    float friction = 0.6f; // 0 = frictionless slide, 1 = stops dead on contact
};

struct PhysicsBody {
    std::vector<Particle> particles;
    std::vector<DistanceConstraint> distanceConstraints;
    std::vector<AngleConstraint> angleConstraints;
    std::vector<MuscleTarget> muscleTargets;
    std::vector<GroundConstraint> groundConstraints;
};

// Verlet-integrates every non-pinned particle under `gravity` (velocity
// scaled by `damping` each step — without this, a chain driven by both
// gravity and a muscle pull has nothing dissipating energy and can ring/
// oscillate almost indefinitely instead of settling), then runs
// `iterations` relaxation passes satisfying every constraint (distance,
// angle, muscle pull, ground) in order. Call once per frame.
void StepPhysics(PhysicsBody& body, float dt, const glm::vec3& gravity, int iterations = 8, float damping = 0.9f);
