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
    float bodyFat = 0.5f;
    float muscle = 0.5f;
    float aggressiveness = 0.5f;
};

// Same seed always produces the same DNA; a different seed produces different DNA.
DNA GenerateDNA(uint32_t seed);
