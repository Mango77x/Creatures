#pragma once

#include <cstdint>

// Flat parameter struct — the whole "genome" is just numbers, no genome
// language or morphology graph. See CLAUDE.md's architecture decisions.
struct DNA {
    uint32_t seed = 0;

    float bodyLength = 1.0f;
    float bodyHeight = 1.0f;
    float neckLength = 1.0f;
    float tailLength = 1.0f;
    int legCount = 4;
    float hornSize = 0.0f;
    float eyeSize = 0.15f;
    float earSize = 0.15f;
    // Head capsule radius (see Skeleton.cpp/CreatureMesh.cpp) — its own
    // field so head size isn't a confusing side effect of eyeSize.
    float headSize = 1.0f;
    // Head capsule length, independent of headSize — a narrow-long snout
    // and a short-wide head should be reachable separately, not locked
    // together as one "size" knob.
    float headLength = 1.0f;
    // Snout tip radius as a fraction of the cranium's (base) radius — low
    // values taper sharply toward a point (fox/greyhound), high values stay
    // almost as thick as the cranium (bear/hippo, barely tapers).
    float snoutTaper = 0.5f;
    float bodyFat = 0.5f;
    float muscle = 0.5f;
    float aggressiveness = 0.5f;

    // Fraction of bodyHeight the spine arches up (positive, cat/hyena-like)
    // or sags down (negative, swayback) at its mid-point — 0 is a dead-straight
    // line, which is what every seed produced before this field existed.
    float spineArch = 0.0f;
    // Front/back leg length asymmetry, as a fraction of bodyHeight applied in
    // opposite directions (front = bodyHeight*(1+bias), back = bodyHeight*
    // (1-bias)) — 0 keeps front and back the same height, which no real
    // quadruped actually has (hyenas/giraffes taller at the shoulder, rabbits/
    // kangaroos taller at the hip).
    float legHeightBias = 0.0f;
    // Neck take-off angle in degrees, measured up from horizontal (forward).
    // 45 matches the old fixed constant (normalize(up+forward)); low values
    // read as boar/lizard-like (near-horizontal), high values as giraffe-like
    // (near-vertical).
    float neckPitch = 45.0f;
    // Tail take-off angle in degrees, measured up from horizontal-backward.
    // 16.7 matches the old fixed constant (normalize(up*0.3 - forward)). A
    // creature can carry its neck high and its tail low (or vice versa), so
    // this is its own field rather than sharing neckPitch.
    float tailPitch = 16.7f;

    // Color (see Palette.h): bodyHue/accentHueShift pick the two hues, the
    // saturation/value pair sets how rich vs. how light the whole palette
    // reads — so two creatures with the same hue can still look distinct.
    float bodyHue = 0.0f;
    float accentHueShift = 0.3f;
    float colorSaturation = 0.6f;
    float colorValue = 0.7f;
};

// Same seed always produces the same DNA; a different seed produces different DNA.
DNA GenerateDNA(uint32_t seed);
