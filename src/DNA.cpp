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
    dna.bodyLength = RandRange(rng, 0.6f, 1.8f);
    dna.bodyHeight = RandRange(rng, 0.4f, 1.2f);
    dna.neckLength = RandRange(rng, 0.2f, 1.5f);
    dna.tailLength = RandRange(rng, 0.2f, 2.0f);
    dna.legCount = 4; // fixed quadruped skeleton for now, see CLAUDE.md
    dna.hornSize = RandRange(rng, 0.0f, 0.6f);
    dna.eyeSize = RandRange(rng, 0.05f, 0.3f);
    dna.earSize = RandRange(rng, 0.05f, 0.4f);
    dna.bodyFat = RandRange(rng, 0.0f, 1.0f);
    dna.muscle = RandRange(rng, 0.0f, 1.0f);
    dna.aggressiveness = RandRange(rng, 0.0f, 1.0f);
    return dna;
}
