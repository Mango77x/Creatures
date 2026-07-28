# Creatures — Development Plan

## Workflow

One `phase-N` branch per phase, opened as a PR against `main`, merged by the maintainer (Mango77x). No AI/Claude co-author trailer on any commit. Each phase closes only once it produces something visible/executable — not just code that compiles. This doc is checked off phase by phase, alongside `PROJECT_OVERVIEW.md`'s architecture notes.

Full context, architecture decisions, and non-goals live in the project's `CLAUDE.md` — this doc tracks execution status only.

## Phase 1 — Window + orbital camera (done)

- [x] CMake project (C++20/23) with GLFW, GLAD, GLM, Dear ImGui, stb_image via FetchContent
- [x] Window with OpenGL context, render loop, clear color
- [x] Orbital camera controllable with the mouse (drag to rotate, scroll to zoom)
- [x] ImGui initialized, empty panel rendered
- [x] Placeholder object on screen to confirm the camera visually

**Tangible result:** an executable window with a navigable 3D scene. Verified: window opens, cube renders, drag-to-orbit and scroll-to-zoom both work.

## Phase 2 — DNA system

- [ ] Flat DNA struct (bodyLength, bodyHeight, neckLength, tailLength, legCount, hornSize, eyeSize, earSize, bodyFat, muscle, aggressiveness, ...)
- [ ] Seeded, reproducible pseudo-random generation
- [ ] ImGui panel: seed field + generate button, showing the resulting parameters

**Tangible result:** the same seed always reproduces the same DNA values; a different seed gives different values.

## Phase 3 — Skeleton generator

- [ ] Fixed quadruped hierarchy: Pelvis → Spine → Neck → Head / Tail, 4 legs
- [ ] Skeleton built procedurally from DNA parameters

**Tangible result:** changing a DNA parameter in the panel reshapes the skeleton's proportions live.

## Phase 4 — Mesh generator

- [ ] Procedural geometry per bone (capsules/cylinders)
- [ ] Pieces joined into one creature mesh

**Tangible result:** a solid, recognizable 3D creature on screen, generated from a seed.

## Phase 5 — Shaders

- [ ] Pixel-art / low-poly shading
- [ ] Deliberate detail reduction for the organic look

**Tangible result:** visible jump from generic gray geometry to the project's final visual style.

## Phase 6 — Procedural animation

- [ ] Breathing (torso pulse)
- [ ] Follow-the-leader chain for neck/tail (delayed segment-to-segment motion)
- [ ] Head look-at a target

**Tangible result:** a stationary creature that reads as alive.

## Phase 7 — Leg IK + gait cycle

- [ ] FABRIK or CCD solver per leg
- [ ] Walk/trot gait cycle, independent of leg count

**Tangible result:** the creature walks across a flat plane.

## Phase 8 — Terrain adaptation

- [ ] Per-leg raycast against terrain
- [ ] Pelvis/spine/neck adjustment to match footing

**Tangible result:** the creature walks over uneven terrain without clipping or looking rigid.

## Phase 9 — Crossbreeding

- [ ] Pick two creatures (DNA sets)
- [ ] Parameter interpolation + discrete trait inheritance + mutation
- [ ] Instant preview of the offspring

**Tangible result:** crossing two visually distinct creatures produces a third with blended traits.

## Phase 10 — Export & persistence

- [ ] Export to glTF/OBJ
- [ ] Save/load DNA to disk
- [ ] Genealogy history (optional)

**Tangible result:** an exported creature opens correctly in an external viewer (e.g. Blender); a saved DNA file reloads into the same creature.
