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

## Phase 2 — DNA system (done)

- [x] Flat DNA struct (bodyLength, bodyHeight, neckLength, tailLength, legCount, hornSize, eyeSize, earSize, bodyFat, muscle, aggressiveness, ...)
- [x] Seeded, reproducible pseudo-random generation
- [x] ImGui panel: seed field + generate button, showing the resulting parameters

**Tangible result:** the same seed always reproduces the same DNA values; a different seed gives different values. Verified: same seed re-generates identical parameters, different seeds diverge.

## Phase 3 — Skeleton generator (done)

- [x] Fixed quadruped hierarchy: Pelvis → Spine → Neck → Head / Tail, 4 legs
- [x] Skeleton built procedurally from DNA parameters

**Tangible result:** changing a DNA parameter in the panel reshapes the skeleton's proportions live. Verified: drawn as a debug stick figure (green bone lines + yellow joint points), reshapes on Generate/Random seed. Rendered with the free orbital dev camera — see the Phase 5 note about locking the camera to a fixed angle later.

## Phase 4 — Mesh generator (done)

- [x] Procedural geometry per bone (capsules/cylinders)
- [x] Pieces joined into one creature mesh

**Tangible result:** a solid, recognizable 3D creature on screen, generated from a seed. Verified: solid lit creature (body, tapered neck/tail, rounded head and feet) reshapes with each seed. Debug skeleton overlay (toggle in the panel) draws with depth testing disabled so it stays visible on top of the mesh instead of being hidden inside it.

## Phase 5 — Shaders (done)

- [x] Pixel-art / low-poly shading
- [x] Deliberate detail reduction for the organic look
- [x] Lock the camera to a fixed oblique/dimetric angle (matching the Critter Crosser reference) — retires the free orbital camera from Phase 1, which was a development/debug convenience only

**Tangible result:** visible jump from generic gray geometry to the project's final visual style, viewed from the final fixed camera angle instead of free orbit. Verified against real Critter Crosser screenshots (`reference/critter-crosser/`): no cartoon outline (matches), flat-banded shading instead of a smooth gradient (matches), real pixel-art blockiness from the low-res render pass (matches in spirit). Known gaps intentionally deferred, not Phase 5 scope: single-hue coloring vs. the reference's multi-tone palettes, and no distinct horn/eye/ear geometry yet (DNA already carries `hornSize`/`eyeSize`/`earSize`, but the Phase 4 mesh generator only uses them to size the head capsule, not as separate shapes).

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
