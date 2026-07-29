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

Every seed was reading as visually similar ("the same ugly giraffe") regardless of DNA. Crossbreeding two creatures that look the same has no demonstrative point, so this phase stays open — no fixed close date — until creatures look and move well; crossbreeding (now Phase 12) is explicitly blocked until this phase, Phase 10 (physical body simulation), and Phase 11 (save/load) all close (user decision, 2026-07-28, sharpened 2026-07-29 to explicitly include physical realism and a save/load prerequisite, not just visuals).

- [x] Per-DNA color palette (`Palette.h/.cpp`): body + accent (horn/ear) + eye color from HSV, carried per-vertex (`MeshVertex::color`) and combined with the `uColor` uniform
- [x] Real horn/ear geometry — small bone chains reusing the leg/cylinder pipeline, not new mesh primitives
- [x] Real eye geometry — small contrasting spheres
- [x] Wider DNA proportion ranges (bodyLength/Height/neck/tail) so seeds read as distinct silhouettes, not just detail variation
- [x] Camera: perspective → orthographic/fixed/no-zoom (to match the reference) → reverted back to free orbital perspective + zoom (user decision: treat this as a lab tool, not a 1:1 visual recreation of Critter Crosser). Left mouse drags to orbit, scroll zooms; right mouse is reserved for click-and-hold creature movement.
- [x] Terrain: smooth heightfield → stepped block terraces (to match the reference) → reverted to a smooth heightfield of deliberate large hills/depressions (`Terrain.cpp`) — the block terraces' sharp vertical risers had no collision avoidance against the torso, so legs constantly over-stretched/clipped on height jumps between neighboring cells
- [x] `kCreatureScale` (`Skeleton.h`) — creature was several terrain blocks long vs. the reference's ~1 block; single global scale constant applied to skeleton + mesh radii, animation/gait constants scaled to match
- [x] Elliptical (non-circular) bone cross-sections for spine/neck/head (`CreatureMesh.cpp`'s `CrossSectionScale`) — legs stay circular since their near-vertical axis doesn't give `PerpendicularBasis` an anatomically meaningful side/up to flatten
- [x] 3-tone palette (`BellyColor` in `Palette.h/.cpp`) — lighter/less-saturated underside band on spine/neck/tail, split at the cross-section's equator with per-vertex color interpolation softening the seam
- [x] Live-editable DNA panel — proportions/details/color are ImGui sliders writing straight into `currentDNA`, skeleton rebuilt every frame so edits reshape the creature immediately (inspired by RujiK's own in-game editor, kept DNA-driven rather than per-segment manual sculpting)
- [x] Segmented spine (`Skeleton.cpp`'s `SpineProfile`) — 4 tapered sub-bones with a barrel width profile (fuller mid-body, narrower at both attachments, modulated by `bodyFat`) instead of one uniform-taper cylinder
- [x] Click-and-hold movement — the creature only moves while the right mouse button is held over the 3D view, instead of endlessly chasing the cursor
- [x] Realistic body kinematics (explicit user goal: "simular las kinemáticas de un cuerpo de la manera más realista posible") — the spine/neck/tail actually bend through a turn instead of the whole rigid body pivoting on the spot. See `Animation.cpp` and `PROJECT_OVERVIEW.md` for the full design: a front-to-back lag chain (each spine link chases the one in front of it, progressively slower toward the hips), a max turn-rate clamp on `bodyYaw` plus a hard per-joint bend-angle clamp (both needed — smoothing alone doesn't bound how far a sustained fast turn can wind the spine), a neck/head that leads on its own faster angle instead of inheriting the chest's lag, `NeckEnd`/`HeadTip` recomputed rigidly every frame instead of independently eased (two independently-lagged ends of one capsule was what caused visible stretching/wobble), horns/ears/eyes rotating (not just translating) with the head, a tail with constant gravity droop plus its own trailing swing lag, and front legs' gait targets bent by the same angle as the front hips so the IK doesn't over-reach and "fly" off the ground during a sharp turn.
- [x] `headSize`/`headLength` DNA fields — head capsule radius and length, previously a fixed constant (length) and a confusing side effect of `eyeSize` (radius)
- [x] `spineArch`/`legHeightBias`/`neckPitch`/`tailPitch` DNA fields (2026-07-29) — the spine was a dead-straight line, all 4 legs stood exactly level, and neck/tail take-off angle were fixed constants regardless of DNA; all four now vary per seed
- [x] Flattened joint caps (`CreatureMesh.cpp`'s `AppendEllipsoid`) — fixes visible round "bead" bulges at the segmented spine's internal joints
- [x] Legs fused to the torso surface (`sideOffset` mirrors the spine's real rendered radius) instead of a floating strut bone
- [x] Head split into a short round cranium (`SnoutBase`) + tapering snout (`HeadTip`, new `snoutTaper` DNA field); horns/ears/eyes relocated from the nose tip to the cranium
- [x] Tail spring-damper physics (mass + stiffness + damping + gravity, real velocity integration) replacing the old position-lag — a small-scale prototype of Phase 10's full body-physics simulation
- [x] DNA panel converted to ImGui tabs (Body/Details/Color/Animation) instead of one long scrolling list

**Tangible result so far:** two different seeds produce visibly distinct-colored, distinctly-shaped, non-tubular creatures with a paler belly band and real quadruped anatomy (arched/level spine, asymmetric leg height, angled neck/tail, a proper cranium+snout head), walking over a hilly (not blocky) terrain with a free-orbit camera, whose spine/neck/tail genuinely bend through turns instead of pivoting rigidly, whose tail reacts with real physical inertia, and any DNA field can be hand-tuned live from a tabbed panel. No further concrete items queued for the visual/DNA side; stays open until the user confirms creatures look and move well — which now explicitly includes Phase 10's physical-reaction work below, not just this phase's scope.

## Phase 10 — Physical body simulation (in progress, `phase-10` branch)

Replaces analytic IK (`IK.cpp`, for live posing) and `Animation.cpp`'s lag chains with a particle + constraint physics solver — DNA, `BuildSkeleton` (hierarchy + rest pose), and `Gait.cpp` (foot target function) are all reused unchanged. Decided 2026-07-29: the user wants genuine physical interaction between creatures (a tiger grabbing/knocking down a deer, with real weight/force affecting balance) — a purely kinematic system can't express that, since IK has no notion of force, mass, or "who wins a contested pull," it only ever places an exact position with no resistance. Technical reference: Rain World (bodies as chains of mass particles connected by distance/angle constraints, solved via Verlet integration + iterative relaxation; procedural "muscles" applying a force toward a target pose instead of fixing position directly). Full design writeup in `CLAUDE.md`'s architecture decisions. `main` keeps the current kinematic system untouched until this branch proves itself better, not just different.

- [x] Step 1 — isolated solver (`Physics.h`/`.cpp`): particles/distance/angle/muscle/ground constraints, verified with a standalone 6-particle hanging chain (debug toggle "Show rope physics test") before any creature code used it.
- [x] Step 2 — tail as a PhysicsBody (`Animation.cpp`): replaced Phase 9's hand-rolled spring-damper with a 5-particle body (pinned pelvis + 4 tail vertebrae, `TailSeg1-3`/`TailTip`, up from the original 2-segment `TailMid`). Took 6 real bugs found only through the user running the app to get right — inverted angle-constraint sign, muscle pull applied per-relaxation-iteration instead of per-frame (looked rigid), double-counted body rotation (90° turn looked like 180°), no velocity damping (perpetual oscillation), gravity as a flat constant instead of scaled to the tail's own length (short vs. long tails behaved inconsistently), and distance constraints frozen at init instead of refreshed per frame (broke live `tailLength` editing). User-confirmed as of this write-up.
- [ ] Step 3 — spine/neck/head as a physics chain (not started)
- [ ] Step 4 — legs with ground contact (not started)
- [ ] Step 5 — full integration + side-by-side comparison against `main` (not started)

Design status:
- [x] Particles: every existing joint gains a previous-position (Verlet, no separate velocity variable) and a mass
- [x] Distance constraints: every existing bone (`Skeleton::bones`) becomes a length constraint, rest length taken from `BuildSkeleton` unchanged
- [x] Angle constraints: bend limits between consecutive bones sharing a joint, derived generically by walking the bone graph
- [x] Muscles: a small force pulling each joint toward its target (DNA rest pose + `Gait.cpp` target + movement direction) — proven on the tail, still pending for spine/neck/head (Step 3) and legs (Step 4)
- [ ] Ground contact: `GroundConstraint` exists in `Physics.h` and is exercised by the Step 1 rope test, but no creature body uses it yet — that's Step 4 (legs)
- [x] Per-frame solve loop: Verlet integration (forces → predicted position) + several (4-8) relaxation passes over all constraints

**Tangible result so far:** the tail reacts with genuine physical inertia (gravity-driven droop, turn-lag, settling instead of oscillating) via a topology-agnostic solver, confirmed by the user as an improvement over the Phase 9 hand-tuned spring. Full "two creatures physically colliding/grabbing each other" payoff still requires Steps 3-5. **Blocks crossbreeding exactly like Phase 9 does — a prerequisite, not adjacent work.**

## Phase 11 — Save/load DNA (planned, not started)

Pulled forward from the old single "Export" phase (user decision, 2026-07-29): without saving/loading creatures, Phase 12 (crossbreeding) has no way to get "two distinct creatures" other than typing two seeds by hand — save/load is a practical prerequisite for crossbreeding to be usable, not a later nice-to-have. The rest of the old export phase (glTF/OBJ, genealogy history) doesn't depend on this and stays in Phase 13, at the end.

- [ ] Serialize `DNA` to disk (simple format, JSON or binary) and reload it back into the same creature
- [ ] ImGui panel: list/pick among saved creatures, save the current one under a name

**Tangible result:** close the app, reopen it, load a previously-saved creature, and get exactly the same one back (same DNA, same shape/color).

## Phase 12 — Crossbreeding (blocked until Phases 9, 10, and 11 close)

- [ ] Pick two creatures (DNA sets, from Phase 11's saved library)
- [ ] Parameter interpolation + discrete trait inheritance + mutation
- [ ] Instant preview of the offspring

**Tangible result:** crossing two visually distinct creatures produces a third with blended traits.

## Phase 13 — Export

- [ ] Export to glTF/OBJ
- [ ] Genealogy history (optional)

**Tangible result:** an exported creature opens correctly in an external viewer (e.g. Blender).
