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
