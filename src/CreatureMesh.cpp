#include "CreatureMesh.h"
#include "Palette.h"

#include <glm/gtc/constants.hpp>
#include <algorithm>

namespace {
    constexpr int kCylinderSegments = 10;
    constexpr int kSphereSlices = 8;
    constexpr int kSphereStacks = 6;

    // Two vectors perpendicular to axis and to each other, so points around a
    // circle can be built as center + cos(t)*side + sin(t)*up.
    void PerpendicularBasis(const glm::vec3& axis, glm::vec3& side, glm::vec3& up) {
        glm::vec3 reference = std::abs(axis.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        side = glm::normalize(glm::cross(reference, axis));
        up = glm::cross(axis, side);
    }

    void AppendTriangle(std::vector<MeshVertex>& out, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                         const glm::vec3& na, const glm::vec3& nb, const glm::vec3& nc,
                         const glm::vec3& ca, const glm::vec3& cb, const glm::vec3& cc) {
        out.push_back({a, na, ca});
        out.push_back({b, nb, cb});
        out.push_back({c, nc, cc});
    }

    // crossSection.x/y scale the ellipse along `side`/`up` relative to the
    // plain circular radius — (1,1) is the old perfect circle. A real torso
    // isn't round in cross-section, so a uniform tube read as "capsules glued
    // together" no matter what the DNA said; this is the single biggest
    // lever for that (see CLAUDE.md's Phase 9 notes).
    //
    // topColor/bottomColor split at the equator (sin(t) >= 0) instead of a
    // single flat color — the reference's common pale-belly/darker-back
    // pattern, and the third of its ~3-4 tones per creature our old 2-tone
    // (body + accent) palette was missing. The GPU interpolating each
    // straddling triangle's vertex colors turns the hard split into a
    // narrow (one segment wide) gradient band rather than a razor edge.
    void AppendCylinder(std::vector<MeshVertex>& out, const glm::vec3& start, const glm::vec3& end,
                         float startRadius, float endRadius, const glm::vec2& crossSection,
                         const glm::vec3& topColor, const glm::vec3& bottomColor) {
        glm::vec3 axis = end - start;
        float length = glm::length(axis);
        if (length < 1e-5f) return;
        axis /= length;

        glm::vec3 side, up;
        PerpendicularBasis(axis, side, up);

        for (int i = 0; i < kCylinderSegments; ++i) {
            float t0 = glm::two_pi<float>() * static_cast<float>(i) / kCylinderSegments;
            float t1 = glm::two_pi<float>() * static_cast<float>(i + 1) / kCylinderSegments;

            // Ellipse offset: semi-axis lengths are radius*crossSection.x
            // (along side) and radius*crossSection.y (along up).
            glm::vec3 dir0 = cosf(t0) * crossSection.x * side + sinf(t0) * crossSection.y * up;
            glm::vec3 dir1 = cosf(t1) * crossSection.x * side + sinf(t1) * crossSection.y * up;

            // An ellipse's outward normal isn't the same direction as its
            // surface offset (unlike a circle) — the perpendicular scales
            // invert relative to the radius scale.
            glm::vec3 normal0 = glm::normalize(cosf(t0) / crossSection.x * side + sinf(t0) / crossSection.y * up);
            glm::vec3 normal1 = glm::normalize(cosf(t1) / crossSection.x * side + sinf(t1) / crossSection.y * up);

            glm::vec3 color0 = sinf(t0) >= 0.0f ? topColor : bottomColor;
            glm::vec3 color1 = sinf(t1) >= 0.0f ? topColor : bottomColor;

            glm::vec3 startA = start + dir0 * startRadius;
            glm::vec3 startB = start + dir1 * startRadius;
            glm::vec3 endA = end + dir0 * endRadius;
            glm::vec3 endB = end + dir1 * endRadius;

            AppendTriangle(out, startA, endA, endB, normal0, normal0, normal1, color0, color0, color1);
            AppendTriangle(out, startA, endB, startB, normal0, normal1, normal1, color0, color1, color1);
        }
    }

    // General joint cap: axis is the "pole" direction (pointing along
    // whichever bone it caps) and crossSection flattens the equatorial plane
    // exactly like AppendCylinder's ellipse, in the same side/up basis. A
    // sphere is just the degenerate case crossSection=(1,1) (any axis works,
    // since that's rotationally symmetric) — this replaced a plain sphere
    // cap after segmenting the spine into 4 sub-bones made internal joints
    // (SpineSeg1/2/3) sprout visibly round "beads": those joints already
    // meet at a matched radius in the rest pose (no seam to hide), so an
    // unflattened sphere there just poked out past the spine's elliptical
    // silhouette instead of blending into it.
    void AppendEllipsoid(std::vector<MeshVertex>& out, const glm::vec3& center, float radius,
                          const glm::vec3& axis, const glm::vec2& crossSection, const glm::vec3& color) {
        if (radius < 1e-5f) return;

        glm::vec3 side, up;
        PerpendicularBasis(axis, side, up);

        for (int stack = 0; stack < kSphereStacks; ++stack) {
            float phi0 = glm::pi<float>() * static_cast<float>(stack) / kSphereStacks;
            float phi1 = glm::pi<float>() * static_cast<float>(stack + 1) / kSphereStacks;

            for (int slice = 0; slice < kSphereSlices; ++slice) {
                float theta0 = glm::two_pi<float>() * static_cast<float>(slice) / kSphereSlices;
                float theta1 = glm::two_pi<float>() * static_cast<float>(slice + 1) / kSphereSlices;

                auto pointOnCap = [&](float phi, float theta) {
                    float polar = cosf(phi);
                    float equatorial = sinf(phi);
                    return axis * polar
                         + side * (equatorial * cosf(theta) * crossSection.x)
                         + up   * (equatorial * sinf(theta) * crossSection.y);
                };
                // Same inverse-scale trick AppendCylinder uses: an ellipse's
                // outward normal isn't the same direction as its surface
                // offset once the two axes are scaled differently.
                auto normalOnCap = [&](float phi, float theta) {
                    float polar = cosf(phi);
                    float equatorial = sinf(phi);
                    return glm::normalize(axis * polar
                         + side * (equatorial * cosf(theta) / crossSection.x)
                         + up   * (equatorial * sinf(theta) / crossSection.y));
                };

                glm::vec3 n00 = normalOnCap(phi0, theta0);
                glm::vec3 n01 = normalOnCap(phi0, theta1);
                glm::vec3 n10 = normalOnCap(phi1, theta0);
                glm::vec3 n11 = normalOnCap(phi1, theta1);

                glm::vec3 p00 = center + pointOnCap(phi0, theta0) * radius;
                glm::vec3 p01 = center + pointOnCap(phi0, theta1) * radius;
                glm::vec3 p10 = center + pointOnCap(phi1, theta0) * radius;
                glm::vec3 p11 = center + pointOnCap(phi1, theta1) * radius;

                AppendTriangle(out, p00, p10, p11, n00, n10, n11, color, color, color);
                AppendTriangle(out, p00, p11, p01, n00, n11, n01, color, color, color);
            }
        }
    }

    void AppendSphere(std::vector<MeshVertex>& out, const glm::vec3& center, float radius, const glm::vec3& color) {
        AppendEllipsoid(out, center, radius, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f), color);
    }

    float BoneRadius(BoneKind kind, const DNA& dna) {
        switch (kind) {
            case BoneKind::Spine: return 0.22f + dna.bodyFat * 0.18f + dna.muscle * 0.08f;
            case BoneKind::Neck:  return 0.12f + dna.muscle * 0.05f;
            case BoneKind::Head:  return 0.10f + dna.headSize * 0.11f; // mirrors Skeleton.cpp's headRadiusApprox
            case BoneKind::Tail:  return 0.08f + dna.muscle * 0.04f;
            case BoneKind::Leg:   return 0.09f + dna.muscle * 0.06f;
            case BoneKind::Horn:  return 0.05f + dna.hornSize * 0.06f;
            case BoneKind::Ear:   return 0.04f + dna.earSize * 0.05f;
        }
        return 0.1f;
    }

    // Width (side) vs. height (up) ratio for AppendCylinder's ellipse, per
    // bone kind. Only applied to bones whose PerpendicularBasis side/up
    // actually line up with the body's real left-right/up axes (a roughly
    // horizontal bone picks reference=(0,1,0), giving side=body-left-right
    // and up=world-up) — spine/neck/head qualify; a leg's near-vertical axis
    // picks an arbitrary horizontal reference instead, so flattening it
    // wouldn't consistently mean anything anatomically and is left circular.
    glm::vec2 CrossSectionScale(BoneKind kind) {
        switch (kind) {
            case BoneKind::Spine: return glm::vec2(1.2f, 0.82f);  // ribcage: wider than tall, not a tube
            case BoneKind::Neck:  return glm::vec2(0.88f, 1.05f); // narrower, slightly taller
            case BoneKind::Head:  return glm::vec2(1.08f, 0.94f); // slightly wider than tall
            default: return glm::vec2(1.0f, 1.0f);
        }
    }

    // Body vs. accent material per bone (Phase 9's per-DNA palette, see
    // Palette.h): horns/ears read as a different "material" (like keratin
    // vs. skin), everything else is the base body color.
    glm::vec3 BoneColor(BoneKind kind, const DNA& dna) {
        switch (kind) {
            case BoneKind::Horn:
            case BoneKind::Ear:
                return AccentColor(dna);
            default:
                return BodyColor(dna);
        }
    }

    // Underside color for AppendCylinder's top/bottom split. Only the main
    // body chain gets a belly band — horns/ears/head/legs stay a single flat
    // color (returning the same value for top and bottom is what makes the
    // split invisible there).
    glm::vec3 BoneBellyColor(BoneKind kind, const DNA& dna) {
        switch (kind) {
            case BoneKind::Spine:
            case BoneKind::Neck:
            case BoneKind::Tail:
                return BellyColor(dna);
            default:
                return BoneColor(kind, dna);
        }
    }
}

std::vector<MeshVertex> BuildCreatureMesh(const Skeleton& skeleton, const DNA& dna, float breathScale) {
    std::vector<MeshVertex> mesh;

    auto effectiveRadius = [&](const Bone& bone, float radiusScale) {
        float radius = BoneRadius(bone.kind, dna) * radiusScale;
        if (bone.kind == BoneKind::Spine) radius *= (1.0f + breathScale);
        // kCreatureScale shrinks skeleton joint positions (Skeleton.cpp) but
        // BoneRadius's constants are unscaled — apply the same factor here
        // so thickness shrinks in lockstep with length instead of the
        // creature ending up relatively fatter than before.
        return radius * kCreatureScale;
    };

    // Each joint's cap is sized, colored, and shaped (axis + cross-section)
    // to whichever connected bone is thickest there — e.g. the pelvis picks
    // up the spine's color and flattened ellipse over the thinner legs/tail,
    // an ear tip picks up its own ear color/axis since nothing else touches
    // it. The axis is that bone's own direction so the cap's "poles" point
    // along the bone instead of a fixed world axis, and blends into its
    // taper instead of bulging past it.
    std::vector<float> jointRadius(skeleton.joints.size(), 0.0f);
    std::vector<glm::vec3> jointColor(skeleton.joints.size(), glm::vec3(1.0f));
    std::vector<glm::vec3> jointAxis(skeleton.joints.size(), glm::vec3(0.0f, 1.0f, 0.0f));
    std::vector<glm::vec2> jointCrossSection(skeleton.joints.size(), glm::vec2(1.0f));
    for (const Bone& bone : skeleton.bones) {
        float startRadius = effectiveRadius(bone, bone.startRadiusScale);
        float endRadius = effectiveRadius(bone, bone.endRadiusScale);
        glm::vec3 color = BoneColor(bone.kind, dna);
        glm::vec3 boneVec = skeleton.joints[bone.endJoint] - skeleton.joints[bone.startJoint];
        float boneLen = glm::length(boneVec);
        glm::vec3 boneAxis = boneLen > 1e-5f ? boneVec / boneLen : glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec2 crossSection = CrossSectionScale(bone.kind);
        if (startRadius > jointRadius[bone.startJoint]) {
            jointRadius[bone.startJoint] = startRadius;
            jointColor[bone.startJoint] = color;
            jointAxis[bone.startJoint] = boneAxis;
            jointCrossSection[bone.startJoint] = crossSection;
        }
        if (endRadius > jointRadius[bone.endJoint]) {
            jointRadius[bone.endJoint] = endRadius;
            jointColor[bone.endJoint] = color;
            jointAxis[bone.endJoint] = boneAxis;
            jointCrossSection[bone.endJoint] = crossSection;
        }
    }

    for (const Bone& bone : skeleton.bones) {
        float startRadius = effectiveRadius(bone, bone.startRadiusScale);
        float endRadius = effectiveRadius(bone, bone.endRadiusScale);
        AppendCylinder(mesh, skeleton.joints[bone.startJoint], skeleton.joints[bone.endJoint], startRadius, endRadius,
                        CrossSectionScale(bone.kind), BoneColor(bone.kind, dna), BoneBellyColor(bone.kind, dna));
    }

    for (size_t i = 0; i < skeleton.joints.size(); ++i) {
        AppendEllipsoid(mesh, skeleton.joints[i], jointRadius[i], jointAxis[i], jointCrossSection[i], jointColor[i]);
    }

    // Eyes are a point, not a bone (see Skeleton.h), so they get an explicit
    // sphere instead of going through the joint-cap loop above.
    float eyeRadius = (0.04f + dna.eyeSize * 0.12f) * kCreatureScale;
    glm::vec3 eyeColor = EyeColor(dna);
    AppendSphere(mesh, skeleton.joints[LeftEye], eyeRadius, eyeColor);
    AppendSphere(mesh, skeleton.joints[RightEye], eyeRadius, eyeColor);

    return mesh;
}
