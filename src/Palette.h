#pragma once

#include <glm/glm.hpp>

#include "DNA.h"

// Per-creature color palette derived from DNA — replaces the old single
// hardcoded uColor. Saturated but not neon (matches the Critter Crosser
// reference, see CLAUDE.md's "Referencia visual"): mid saturation/value HSV
// ranges, not fully saturated primaries.
glm::vec3 HsvToRgb(float h, float s, float v);

glm::vec3 BodyColor(const DNA& dna);   // spine/neck/tail/legs/head
glm::vec3 AccentColor(const DNA& dna); // horns/ears
glm::vec3 BellyColor(const DNA& dna);  // lighter, less saturated underside band (spine/neck/tail)
glm::vec3 EyeColor(const DNA& dna);    // high-contrast against BodyColor
