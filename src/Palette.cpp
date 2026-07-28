#include "Palette.h"

#include <cmath>
#include <algorithm>

glm::vec3 HsvToRgb(float h, float s, float v) {
    h = h - floorf(h); // wrap into [0, 1)
    float i = floorf(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);

    switch (static_cast<int>(i) % 6) {
        case 0:  return glm::vec3(v, t, p);
        case 1:  return glm::vec3(q, v, p);
        case 2:  return glm::vec3(p, v, t);
        case 3:  return glm::vec3(p, q, v);
        case 4:  return glm::vec3(t, p, v);
        default: return glm::vec3(v, p, q);
    }
}

glm::vec3 BodyColor(const DNA& dna) {
    return HsvToRgb(dna.bodyHue, dna.colorSaturation, dna.colorValue);
}

glm::vec3 AccentColor(const DNA& dna) {
    // Horn/ear "material" reads as darker and a bit more saturated than the
    // body at a shifted hue — like keratin/mane coloring differing from skin
    // — instead of just picking a second random hue at the same tone.
    float hue = dna.bodyHue + dna.accentHueShift;
    float saturation = std::min(dna.colorSaturation + 0.15f, 0.9f);
    float value = std::max(dna.colorValue - 0.2f, 0.25f);
    return HsvToRgb(hue, saturation, value);
}

glm::vec3 BellyColor(const DNA& dna) {
    // Lighter, less saturated than the body — the common real-animal pattern
    // (pale underside, darker back) that a single flat body tone can't show,
    // and the third of the reference's ~3-4 tones per creature that our old
    // 2-tone (body + accent) palette was missing.
    float saturation = std::max(dna.colorSaturation - 0.25f, 0.15f);
    float value = std::min(dna.colorValue + 0.18f, 0.95f);
    return HsvToRgb(dna.bodyHue, saturation, value);
}

glm::vec3 EyeColor(const DNA& dna) {
    // Push to whichever value extreme contrasts more with the body, so the
    // eye always reads against the head instead of risking a similar tone.
    float value = dna.colorValue > 0.6f ? 0.08f : 0.95f;
    float saturation = dna.colorValue > 0.6f ? 0.3f : 0.05f;
    return HsvToRgb(dna.bodyHue + 0.5f, saturation, value);
}
