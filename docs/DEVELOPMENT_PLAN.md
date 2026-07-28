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

## Phase 6 — Procedural animation (done)

- [x] Breathing (torso pulse)
- [x] Follow-the-leader chain for neck/tail (delayed segment-to-segment motion)
- [x] Head look-at a target

**Tangible result:** a stationary creature that reads as alive. Verified: neck/tail sway with a visible delay between segments, the spine visibly pulses, and the head leans toward the ImGui-controlled "Look-at target" point.

## Phase 7 — Leg IK + gait cycle (done)

- [x] FABRIK or CCD solver per leg
- [x] Walk/trot gait cycle, independent of leg count

**Tangible result:** the creature walks across a flat plane. Verified: diagonal trot with visible knee bending, looping around the fixed camera target on a visible ground plane. No terrain raycast/adaptation yet — that's Phase 8, this phase is flat ground only.

## Phase 8 — Terrain adaptation (done)

- [x] Per-leg raycast against terrain
- [x] Pelvis/spine/neck adjustment to match footing

**Tangible result:** the creature walks over uneven terrain without clipping or looking rigid. Verified over several iterations:
- Per-leg heightfield "raycast" (direct height sample — see `Terrain.h`) + body pitch/roll fit to the four contact points, with each leg's own IK reaching the residual the fitted plane doesn't explain (confirmed with a deliberately diagonal terrain wave that a single tilt plane can't fully capture).
- Switched leg IK from generic FABRIK to an analytic 2-bone solver (`SolveTwoBoneIK`) after finding FABRIK could flip the knee the wrong way on a 2-segment chain with no explicit bend-direction constraint.
- Gave the rest pose real standing crouch (leg segments reach further than the standing height, `kStandCrouchFactor` in `Skeleton.cpp`) after noticing legs looked penguin-stiff on flat ground — real legged animals never fully straighten their legs.
- Added mouse-follow steering (unprojected cursor → ground plane) and boundary walls, replacing the fixed circular walk path, to make terrain adaptation actually testable across the whole terrain instead of one small loop.

## Phase 9 — Visual variety (in progress, open-ended)

Every seed was reading as visually similar ("the same ugly giraffe") regardless of DNA. Crossbreeding two creatures that look the same has no demonstrative point, so this phase stays open — no fixed close date — until creatures look good on their own; Phase 10 (crossbreeding) is explicitly blocked until then (user decision, 2026-07-28).

- [x] Per-DNA color palette (`Palette.h/.cpp`): body + accent (horn/ear) + eye color from HSV, carried per-vertex (`MeshVertex::color`) and combined with the `uColor` uniform
- [x] Real horn/ear geometry — small bone chains reusing the leg/cylinder pipeline, not new mesh primitives
- [x] Real eye geometry — small contrasting spheres
- [x] Wider DNA proportion ranges (bodyLength/Height/neck/tail) so seeds read as distinct silhouettes, not just detail variation
- [x] Camera switched perspective → orthographic, fixed, sized to always show the whole map, no user zoom (`Camera::FitToGround`) — confirmed against reference screenshots showing zero perspective convergence
- [x] Terrain reworked from a smooth heightfield to stepped block terraces (`Terrain.cpp`)
- [x] `kCreatureScale` (`Skeleton.h`) — creature was several terrain blocks long vs. the reference's ~1 block; single global scale constant applied to skeleton + mesh radii, animation/gait constants scaled to match
- [x] Elliptical (non-circular) bone cross-sections for spine/neck/head (`CreatureMesh.cpp`'s `CrossSectionScale`) — legs stay circular since their near-vertical axis doesn't give `PerpendicularBasis` an anatomically meaningful side/up to flatten
- [x] 3-tone palette (`BellyColor` in `Palette.h/.cpp`) — lighter/less-saturated underside band on spine/neck/tail, split at the cross-section's equator with per-vertex color interpolation softening the seam
- [x] Live-editable DNA panel — proportions/details/color are ImGui sliders writing straight into `currentDNA`, skeleton rebuilt every frame so edits reshape the creature immediately (inspired by RujiK's own in-game editor, kept DNA-driven rather than per-segment manual sculpting)
- [x] Segmented spine (`Skeleton.cpp`'s `SpineProfile`) — 4 tapered sub-bones with a barrel width profile (fuller mid-body, narrower at both attachments, modulated by `bodyFat`) instead of one uniform-taper cylinder

**Tangible result so far:** two different seeds produce visibly distinct-colored, distinctly-shaped, non-tubular creatures with a paler belly band, at roughly the right scale relative to a blocky, fully-framed map, and any DNA field can be hand-tuned live from the panel. No further concrete items queued; stays open until the user confirms creatures look good on their own (see the phase intro above).

## Phase 10 — Crossbreeding (blocked until Phase 9 closes)

- [ ] Pick two creatures (DNA sets)
- [ ] Parameter interpolation + discrete trait inheritance + mutation
- [ ] Instant preview of the offspring

**Tangible result:** crossing two visually distinct creatures produces a third with blended traits.

## Phase 11 — Export & persistence

- [ ] Export to glTF/OBJ
- [ ] Save/load DNA to disk
- [ ] Genealogy history (optional)

**Tangible result:** an exported creature opens correctly in an external viewer (e.g. Blender); a saved DNA file reloads into the same creature.
