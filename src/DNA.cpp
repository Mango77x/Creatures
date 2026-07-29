#include "DNA.h"

#include <random>

namespace {
    float RandRange(std::mt19937& rng, float min, float max) {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(rng);
    }
}

DNA GenerateDNA(uint32_t seed) {
    std::mt19937 rng(seed);

    DNA dna;
    dna.seed = seed;
    // Widened from the original 0.6-1.8/0.4-1.2/... ranges (Phase 9): the
    // narrower ranges kept every seed close to the same proportions, so
    // creatures only differed in fine detail instead of overall silhouette
    // (skinny vs. stocky, long-necked vs. squat). See CLAUDE.md Phase 9.
    dna.bodyLength = RandRange(rng, 0.5f, 2.0f);
    dna.bodyHeight = RandRange(rng, 0.35f, 1.35f);
    dna.neckLength = RandRange(rng, 0.15f, 1.8f);
    dna.tailLength = RandRange(rng, 0.15f, 2.2f);
    dna.legCount = 4; // fixed quadruped skeleton for now, see CLAUDE.md
    dna.hornSize = RandRange(rng, 0.0f, 0.6f);
    dna.eyeSize = RandRange(rng, 0.05f, 0.3f);
    dna.earSize = RandRange(rng, 0.05f, 0.4f);
    dna.bodyFat = RandRange(rng, 0.0f, 1.0f);
    dna.muscle = RandRange(rng, 0.0f, 1.0f);
    dna.aggressiveness = RandRange(rng, 0.0f, 1.0f);

    dna.spineArch = RandRange(rng, -0.15f, 0.3f);
    dna.legHeightBias = RandRange(rng, -0.25f, 0.25f);
    dna.neckPitch = RandRange(rng, 20.0f, 75.0f);
    dna.tailPitch = RandRange(rng, -10.0f, 45.0f);

    dna.bodyHue = RandRange(rng, 0.0f, 1.0f);
    float accentShiftMagnitude = RandRange(rng, 0.15f, 0.45f);
    dna.accentHueShift = RandRange(rng, 0.0f, 1.0f) < 0.5f ? accentShiftMagnitude : -accentShiftMagnitude;
    dna.colorSaturation = RandRange(rng, 0.45f, 0.75f);
    dna.colorValue = RandRange(rng, 0.5f, 0.85f);

    dna.headSize = RandRange(rng, 0.6f, 1.8f);
    dna.headLength = RandRange(rng, 0.5f, 2.0f);
    dna.snoutTaper = RandRange(rng, 0.15f, 0.85f);
    return dna;
}
