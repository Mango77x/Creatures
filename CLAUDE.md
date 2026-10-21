# Project Context — Procedural Creature Lab

## What this is

It is not a game. It is a C++ technical lab for generating, animating and simulating procedural creatures, without relying on any engine (Unity/Unreal/Godot). The final deliverable is an application with a 3D viewer + ImGui panel where you can:

- generate a creature from a seed,
- tweak its parameters in real time and watch it regenerate,
- watch it walk and adapt to uneven terrain,
- watch it interact physically with other creatures (shoves, grabs, losing balance because of another one's weight/strength) — see the physics simulation decision in "Architecture decisions",
- cross it with another creature to produce offspring (crossbreeding).

## Inspiration and real technical reference (verified, do not assume otherwise)

Inspired by *Critter Crosser* (dev: RujiK). Facts confirmed by direct research — **do not assume Unity/C#** or architectures more complex than what it actually uses:

- Made in **GameMaker (GML) + hand-written GLSL shaders**. It does not use a large engine.
- Each creature's DNA is a **flat array/struct of numbers** (body length, width, leg count, eyes, ears, etc.) — **not** a genome language, nor an abstract morphology tree/graph with sockets.
- The whole generator is **~300 lines of code**.
- Each creature is generated **"loosely" from a single template/parent**, not fully from scratch at random. It is variation on a template, not free generation.
- Body/neck/tail animation is a **"follow the leader" chain**: each segment follows the position/rotation of the previous one with a small delay (like a centipede), not a full per-segment IK solver.
- Terrain adaptation uses the standard industry pattern: per-leg raycast → foothold → IK → spine/hip/neck adjustment.
- The "2.5D" look comes from **real 3D geometry with deliberately reduced polygons + pixel-art shading**, not from sprites. **Confirmed with citable primary sources (2026-07-28)**, after a correction and counter-correction of our own — see the methodology note below:
  - Developer interview on the official GameMaker blog: *"GameMaker is primarily a 2D game engine so 3D requires you to do most of the heavy lifting yourself"* — [gamemaker.io/en/blog/socket-beast-rujik](https://gamemaker.io/en/blog/socket-beast-rujik). The same interview also confirms, with a direct quote, the DNA as a flat array of numbers and crossbreeding as a lerp between two arrays.
  - Official devlog #5, titled **"Rendering Organic Monsters"**, description (translated): *"by removing polygons and updating procedural animations"* — [youtube.com/watch?v=T2oUOWNNnx4](https://www.youtube.com/watch?v=T2oUOWNNnx4). "Removing polygons" only makes sense if there is a real 3D mesh.
  - **Methodology note — why this matters**: an earlier visual review (looking only at 7 static Steam screenshots) wrongly concluded it was 2D sprite art, because a low-poly 3D mesh rendered with a fixed camera/angle + a pixelation shader can look indistinguishable from hand-drawn 2D sprites in a still image — that is exactly the intended effect. **Lesson: to claim how something is built (3D vs 2D, render technique), looking at screenshots is not enough — look for direct statements from the developer (interviews, devlogs, code) before treating it as "confirmed".**

## Visual reference (Critter Crosser)

Real screenshots downloaded from the Steam store page (`store.steampowered.com/app/2792320/Critter_Crosser/`) on 2026-07-28, stored in `reference/critter-crosser/` (folder ignored by git — for local consultation only, never committed). They serve for mood/palette/lighting/composition — not for inferring the render technique (see the methodology note above: that is confirmed with sources, not by looking at screenshots):

- **Perspective**: fixed oblique/dimetric, Pokémon/Stardew Valley style (buildings show roof + walls at the same time), not a free 3D camera.
- **Palette**: saturated but not neon; each surface (grass, water, brick) uses ~3-4 flat tone bands, no smooth gradients.
- **Pixel density**: deliberately "chunky"/low-resolution pixel art, consistent pixel size across all elements (no mixed resolutions).
- **Creatures/characters**: small on screen (~1-1.5 tiles tall), flat color shapes with simple internal shading (light/mid-tone/shadow), no thick cartoon black outline — the silhouette reads through shading, not through an outline.
- **Lighting**: consistent light direction (top-left) baked into each sprite; dynamic shader effects are layered on top (rain + a warm light cone following the player at night, water with animated ripple/dither, splash particles) — consistent with "GameMaker + hand-written GLSL shaders".
- **Tone/setting**: a mundane, recognizable world (brick houses, sidewalks, cars, a police station, a shop called "The CompPost") populated by fantastical creatures — the "monsters in everyday life" contrast is the visual hook, not a stylized fantasy world.
- **UI**: panels with a thick pixel font, thick light-blue borders, segmented health bars, text with shadow — a direct heir of the Pokémon GBA style but cleaner.

**Working rule**: before implementing or generating any visual element (color palette, shader, creature geometry/silhouette, ImGui UI with aesthetic intent), review the screenshots in `reference/critter-crosser/` against this analysis instead of improvising from memory.

## Architecture decisions already made (do not reopen without a strong reason)

- **DNA = struct/list of flat parameters.** No genome-as-program and no node graph with sockets. Approximate shape:
  `seed, bodyLength, bodyHeight, neckLength, tailLength, legCount, hornSize, eyeSize, earSize, bodyFat, muscle, aggressiveness, ...`
- **Template variation**: new creatures start from a base/parent creature and mutate parameters; no free generation from scratch.
- **Body/tail/neck animation**: "follow the leader" chain with delay, before considering full per-segment IK.
- **Legs**: raycast + IK (FABRIK or CCD) + pelvis/spine adjustment for terrain adaptation.
- **Initial skeleton**: fixed quadruped hierarchy (Pelvis → Spine → Neck → Head / Tail; 4 legs). Do not generalize to an arbitrary graph or to other body plans until the quadruped works end to end.
- **Rendering**: simple geometry per bone (capsules/cylinders/spheres) + deliberate detail reduction + pixel-art shader. No PBR and no complex dynamic shadows.
- **Free orbital camera, with zoom, perspective projection** (final decision revised 2026-07-28, Phase 9): the camera went through several iterations — fixed/oblique (Phase 5) → orthographic with no zoom (first pass of Phase 9, to imitate the parallel projection confirmed in the Critter Crosser screenshots) → **back to free + perspective + zoom**, by explicit decision of the user to move away from Critter Crosser's camera style and treat this as a lab tool, not a 1:1 visual recreation. Dragging with the **left** button orbits (yaw/pitch), scroll zooms (distance). The right button is reserved for moving the creature (hold = follows the mouse; when not held, the creature does not move), so they do not clash.
- **Terrain with smooth, pronounced hills, not block terraces** (final decision revised 2026-07-28, Phase 9): block terraces (with abrupt vertical walls) caused constant clipping — the body does not avoid walls, it only follows the average height of the 4 legs, so a sharp height jump between neighboring cells made the IK stretch beyond its reach. Replaced by a continuous heightfield of deliberate hills/depressions (a sum of Gaussian falloffs in `Terrain.cpp`, not terraces or repeating waves) — slope is always smooth at any point, but elevation is clearly visible, so terrain adaptation can be tested properly. `TerrainHeight` keeps the same signature, so the per-leg raycast did not change.
- **Global creature scale** (`kCreatureScale` in `Skeleton.h`, Phase 9): after noticing the creature spanned several terrain blocks instead of ~1 as in the reference, a single scale factor was introduced, applied to the skeleton (joint positions) and to mesh radii, instead of retouching each DNA range separately — it keeps all variation/relative proportions intact and only changes the global size. Walking speed, stride and idle animation offsets are scaled with the same constant so they remain proportional.
- **Spine kinematics as a chain of lagged angles, not a single global angle** (Phase 9, explicit user goal: "simulate the kinematics of a body as realistically as possible"): the spine, neck and tail genuinely bend when turning (they do not just change thickness). Rules learned while iterating with the user, all in `Animation.cpp`:
  - The real body turn (`bodyYaw` in `main.cpp`) has a maximum angular velocity (`kMaxTurnRate`) — without it, if the target changes direction faster than the chain can follow, the gap between chest and hips grows without bound and the spine curls over itself. An additional hard limit per joint (`kMaxJointBendAngle`) acts as a safety net.
  - Each spine link chases the one in front of it (not the chest directly) at its own rate, progressively slower toward the hips — a propagating wave, not an angle applied all at once.
  - Neck and head use **their own, faster angle** than the chest (not chained to its rate) — in a real animal the head decides where to go before the body follows; chaining the neck's lag to the chest's made it doubly slow.
  - The `NeckEnd`/`HeadTip` positions are recomputed **fresh every frame** anchored to the lagged angle (which already has its own smoothness) — never with their own independent position `ExpLerp`. Two ends of the same capsule chasing targets at different rates makes the capsule stretch/twist instead of behaving as a rigid piece; that was the real bug behind "the head wobbles".
  - Horns/ears/eyes rotate (not just translate) with the same angle as the head — a pure translation only approximates a small rotation well; with turns of tens of degrees they visibly detach from the head surface.
  - The tail inherits "weight": constant droop (not just oscillation) and its own sway, slower than the hips, since it is a continuation of the spine, not a rigid stub.
  - The front legs hang from the already-bent chest (same angle), and their gait target is computed on that SAME bent base — if the target is computed without the bend, the IK stretches beyond its reach and the leg "flies" without touching the ground. A single transform (`bodyTransform`, based on the lagged rear orientation) serves everything — a second parallel transform for the front half was never needed; only the reference position of each leg has to account for the bend before applying it.
  - The leg IK itself (`SolveTwoBoneIK`, `main.cpp`) was never touched during this work.
- **Head: size and length as their own DNA fields**, decoupled from `eyeSize` (Phase 9): `headSize` controls the head capsule radius, `headLength` its length — previously the length was a fixed constant (0.3, not adjustable) and the radius depended indirectly on `eyeSize`.
- **Round cranium + independent snout** (Phase 9, 2026-07-29): the head stopped being a single uniform capsule (very few real quadrupeds have that shape — at most a horse/crocodile) and became two sections: a short, untapered cranium (`NeckEnd→SnoutBase`, length tied to the head radius, not to `headLength`, so it always reads as a round "ball") and a tapering snout (`SnoutBase→HeadTip`, which is where `headLength` really acts, with the new `snoutTaper` field controlling how much the tip narrows). Horns/ears/eyes were relocated from `HeadTip` (the muzzle tip) to `SnoutBase` (the cranium, where they sit on a real animal), with the matching adjustment in `Animation.cpp` so `SnoutBase` is also recomputed rigidly during head turns.
- **Legs fused with the torso surface, not a separate "stick"** (Phase 9, 2026-07-29): there was a straight bone (`ChestEnd/Pelvis → Hip`) connecting the center of the spine to the start of each leg, at a distance (`sideOffset`) that was fixed and independent of the actual body size — it read as a rigid rod coming out of the torso, not as a real shoulder/hip. `sideOffset` now replicates the real spine radius at that point (same computation as `BoneRadius(Spine)*CrossSectionScale(Spine).x` in `CreatureMesh.cpp`, with the exact 0.7 of `SpineProfile` at the ends), and the bridge bone was removed from the mesh — the hip joint falls right on the torso surface and its own (already elliptical) cap blends it into the body, just as eyes/ears already did with the head (`headRadiusApprox`).
- **Spring-damper physics for the tail, not just positional lag** (Phase 9, 2026-07-29): `tailMid`/`tailTip` stopped using `ExpLerp` (which only approaches a target, never overshoots it) and moved to a real mass-spring-damper system (acceleration = stiffness×(target−position) − damping×velocity − constant gravity, integrated with real velocity via semi-implicit Euler). The tail droop is no longer a fixed offset (`kTailDroopAmount`, removed) but the real equilibrium between gravity and spring stiffness. It is, on a small scale, a prototype of the full-body physics simulation planned for Phase 10 (see below) — same principle (mass + force + damping instead of a formula that forces the exact position), applied for now to only 2 bones.
- **Particle-and-constraint physics simulation for the body — decision made 2026-07-29, planned for Phase 10, not yet implemented.** It will replace the analytic IK (`IK.cpp`, for live posing) and the lag chains of `Animation.cpp` as the live-posing mechanism — it does not touch DNA, `BuildSkeleton` (hierarchy + rest pose) or `Gait.cpp` (gait target), which are reused as they are. Reason: the user wants creatures to genuinely interact physically with each other (a tiger grabbing/knocking down a deer, with the weight of one affecting the balance of the other) — that is a categorically different problem from "walking believably on its own", and a purely kinematic system cannot express it: IK has no notion of force, mass, or "who wins a struggle", it only places an exact position without resistance. Technical reference: Rain World (bodies = chains of particles with mass connected by distance/angle constraints, solved by Verlet integration + iterative relaxation; procedural "muscles" that apply a force toward a target pose, instead of fixing the position). Proposed design:
  - **Particles**: each existing joint gets a position + previous position (Verlet, no separate velocity variable) + mass.
  - **Distance constraints**: each existing bone (`Skeleton::bones`) becomes a length constraint, with the rest length taken from `BuildSkeleton` unchanged.
  - **Angle constraints**: bend limits between consecutive bones sharing a joint, derived generically by walking the bone graph (no species-specific code needed).
  - **Muscles**: a small force pulling each joint toward its target (DNA rest pose + `Gait.cpp` target + movement direction) — same principle as the tail spring above, applied to the whole body.
  - **Ground contact**: the foot cannot go below `TerrainHeight(x,z)` — one more constraint, solved in the same pass as the rest.
  - **Per-frame loop**: integrate Verlet (forces → predicted position) and then several passes (4-8) solving all constraints.
  - It applies to any skeleton topology (the solver does not know joint names, only a generic graph of particles+constraints) — unlike the current `Animation.cpp`, which references joints by concrete name and would not generalize to a new body plan without a considerable rewrite. This makes it a more solid foundation for future body plans (snakes, flyers) than the current kinematic system.

## Technical stack

- C++20/23
- OpenGL (context via GLFW)
- GLM (math)
- Dear ImGui (debug/lab UI)
- stb_image (texture loading if needed)
- Build: CMake

## Developer profile (to calibrate explanations)

- Solid prior experience in Java, GDScript, C#, Ruby on Rails and React; comfortable with Linux, Git and CI/CD — there is no need to explain version control or general programming concepts.
- Works with Claude Code as a constant copilot.
- Prefers clear explanations the first time a new graphics or 3D-math concept is introduced (VAOs/VBOs, shaders, projection matrices, etc.), without being longer than necessary.

## Non-goals (explicitly out of scope)

- It is not a game with levels, objectives or a story.
- Do not use Unity, Unreal, Godot or any external engine.
- Do not implement an interpreted "genome language" or abstract body plans with sockets — it was evaluated and discarded after researching the real reference project.
- Do not pursue PBR realism in rendering.
- Do not generalize to arbitrary anatomies (spiders, birds, insects) before having a quadruped working end to end.

## Roadmap (phases)

1. OpenGL window + context + basic orbital camera (development tool, see camera decision) + ImGui initialized.
2. DNA system: parameter struct/array + seed + reproducible pseudo-random generation.
3. Skeleton generator: fixed quadruped hierarchy from the DNA.
4. Mesh generator: simple procedural geometry per bone (capsules/cylinders), joining the pieces.
5. Shaders: pixel shading + detail reduction for the "organic low-poly" look + lock the camera to the final fixed oblique/dimetric angle (end of the free orbital camera).
6. Procedural animation: breathing, follow-the-leader chain for neck/tail, head looking toward a target.
7. Leg Inverse Kinematics (FABRIK or CCD) + gait cycle (step/trot) independent of the number of legs.
8. Terrain adaptation: per-leg raycast + pelvis/spine/neck adjustment.
9. Visual variety + realistic kinematics: DNA-driven color, real horn/ear/eye geometry, wider proportion range, terrain with its own hills, spine/neck/tail that genuinely bend when turning, real quadruped anatomy (spine arch, leg height asymmetry, neck/tail angle, cranium+snout, legs fused with the torso). Deliberately open phase — do not advance to crossbreeding until creatures look and move well, and that includes real physical reaction (see Phase 10) — explicit user decision, 2026-07-28/29.
10. Physical body simulation (particles + constraints, replaces IK/kinematic animation for live posing): body as a chain of mass particles connected by distance/angle constraints (Rain World style, Verlet integration), procedural "muscles" that pull toward a pose/gait target instead of fixing the exact position, ground contact as one more constraint. Enables real physical interaction between creatures (grabbing, pushing, losing balance because of another one's weight/strength) — something analytic IK cannot express. Reuses DNA/`BuildSkeleton`/`Gait.cpp` unchanged; replaces `IK.cpp` (live posing) and `Animation.cpp` (lag chains). Decision made 2026-07-29, see "Architecture decisions". **Blocks crossbreeding just like Phase 9 — it is not crossbreeding territory, it is a prerequisite.**
11. Save/load DNA to disk (creature library): without this, the crossbreeding of Phase 12 has nowhere to get "two distinct creatures" other than typing loose seeds by hand — it was moved forward from the old export phase because crossbreeding needs it, not the other way around (explicit user decision, 2026-07-29).
12. Genetic crossbreeding: blending two DNAs (parameter interpolation + discrete trait inheritance) + random mutation, choosing among saved creatures (Phase 11). **Blocked until Phases 9, 10 and 11 are closed.**
13. Export (glTF/OBJ) + genealogy history (optional) — the rest of the old export phase, with no dependency on crossbreeding.

## Current phase

**Phase 1 — completed and merged.** OpenGL window + context, orbital camera (drag rotates, scroll zooms), empty ImGui panel, placeholder cube with basic shading.

**Phase 2 — completed.** Flat DNA struct + reproducible seed-based generation (`std::mt19937`) + ImGui panel with seed/generate/random seed and a parameter listing. Verified: the same seed reproduces the same values.

**Phase 3 — completed.** Quadruped skeleton generated from the DNA, drawn as a debug stick figure (lines + points) with the free development orbital camera. Verified: proportions change with each seed.

**Phase 4 — completed.** Procedural mesh (tapered cylinders per bone + spheres at the joints) generated from the skeleton, with a basic lighting shader. The Phase 3 debug skeleton became an optional overlay ("Show skeleton (debug)" checkbox), drawn without depth testing so it is not hidden inside the mesh. Verified: a solid, recognizable creature that changes shape with the seed.

**Phase 5 — completed.** Camera locked to the fixed oblique angle (yaw 45°/pitch 32°, scroll zoom only), flat-band shading (cel-shading, 4 steps) instead of a continuous gradient, and real pixel art via low-resolution rendering + `GL_NEAREST` upscaling (live adjustable "Pixel scale" slider). Verified against the real screenshots in `reference/critter-crosser/`: no black outline (matches), banded shading (matches), real pixelation (matches in spirit). Left pending for later (not in the scope of this phase): per-creature multi-tone palette and distinct geometry for horns/eyes/ears (the DNA already carries those fields, but the Phase 4 mesh generator only uses them for the head size).

**Phase 6 — completed.** "Follow the leader" chain with delay for neck and tail (each joint chases the previous, already lagged one, no direct snap), breathing (the spine radius pulses with a sine wave), and a head that tilts toward a "look-at target" controllable live from the panel. Spine and legs stay fixed in their rest pose (leg IK is Phase 7). Verified: the still creature looks alive.

**Phase 7 — completed.** Each leg gained a knee (Hip→Knee→Foot, 3 bones) solved with a real FABRIK solver; a stateless procedural gait cycle (`ComputeFootTarget`) with a diagonal trot pattern; the creature walks in a circle around the camera's fixed target on a visible ground plane. All IK/gait computation happens in the skeleton's local space; body movement only affects the render `uModel`. No raycast/terrain adaptation yet (that is Phase 8, flat ground for now). Verified: bent knees visible, trot-like gait.

**Phase 8 — completed.** Terrain with real relief (analytic heightfield, "raycast" = direct height sampling), pelvis/spine tilt adjusted to the 4 support points, and the leg IK changed from generic FABRIK to an analytic 2-bone solver (`SolveTwoBoneIK`) after discovering that FABRIK could bend the knee to the wrong side on a chain of only 2 segments. The legs' rest pose now has a permanent natural knee flexion (more total reach than the foot height, `kStandCrouchFactor`), avoiding the rigid penguin look. The fixed circular path was replaced by mouse-follow steering (projected onto the ground) with boundary walls at the terrain edges. Verified over several iterations with the user, including an intentionally diagonal terrain to confirm that each leg adjusts independently beyond the global body tilt.

**Phase 9 — in progress, open on purpose.** Goal: visual variety + realistic kinematics, not crossbreeding (that moved to Phase 12 and is blocked until this one, the Phase 10 physics and the Phase 11 saving are closed). Done so far:
- Per-DNA palette (`Palette.h/.cpp`, HSV): base color + accent (horns/ears) + eye color, all derived from `bodyHue`/`accentHueShift`/`colorSaturation`/`colorValue`, carried per vertex (`MeshVertex::color`) and combined with `uColor` in the shader.
- Horns/ears as mini bone chains reusing the leg cylinder pipeline (no new meshes); eyes as contrasting-color spheres. Fixed a bug where eyes/ears ended up buried inside the head sphere because of an offset smaller than their own radius.
- Wider DNA proportion range (before, all seeds gave similar silhouettes).
- Camera switched from perspective to fixed orthographic, with no zoom, framing the whole map (`Camera::FitToGround`).
- Terrain redone as block terraces instead of a smooth heightfield (`Terrain.cpp`).
- `kCreatureScale` so the creature spans ~1 terrain block, as in the reference, instead of several.
- Elliptical (not circular) body sections for spine/neck/head: `AppendCylinder` now accepts an ellipse (independent width and height) instead of a single radius, with the normal correctly recomputed for the ellipse (it is not the same direction as the offset, unlike a circle). Only applied where the side/up basis of `PerpendicularBasis` matches the real left-right/up-down axes of the body (nearly horizontal bones); the legs stay circular because their perpendicular basis has no anatomical meaning.
- 3-tone palette: `BellyColor` (lighter, less saturated) is applied to the lower half of spine/neck/tail, with a smooth one-segment transition (per-vertex color interpolation), imitating the paler belly typical of the reference.
- Live-editable ImGui panel: the DNA fields (proportions, details, color) went from read-only text to sliders writing directly into `currentDNA`; the skeleton is rebuilt every frame from the current DNA, so dragging a slider reshapes the creature instantly. Inspired by RujiK's editor (screenshots sent by the user: Body/Colr/Limb/Detl tabs), but keeping the project's procedural approach — you edit DNA, you do not sculpt each segment by hand.
- Segmented spine: instead of a single Pelvis→ChestEnd cylinder, there are now 4 sub-bones with a thickness profile (`SpineProfile` in `Skeleton.cpp`) fuller in the middle and narrower at the ends — modulated by `bodyFat` — instead of a uniform-thickness tube. It reuses the existing per-bone-end `radiusScale` mechanism (the same one that already tapered the tail), just with more segments in the chain.
- Click-based movement kept: the creature only moves while the **right** mouse button is held (and the cursor is inside the 3D window, not over the panel), instead of constantly chasing the mouse.
- Camera returned to free + perspective + zoom, and terrain returned to smooth hills (not terraces) — see "Architecture decisions" above, camera and terrain section.
- Realistic spine/neck/tail kinematics — see "Architecture decisions" above; it was the longest work of this phase, iterated several times with the user until the wobble/stretching and the "flying" of the front legs in sharp turns were eliminated.
- `headSize`/`headLength`: their own DNA fields for the head capsule, decoupled from `eyeSize`.
- `spineArch`/`legHeightBias`/`neckPitch`/`tailPitch` (2026-07-29): the spine was a perfectly straight line, the 4 legs were always exactly the same length, and the neck/tail insertion angle were fixed constants regardless of the DNA — all four now vary per seed, closing the "generic/regular quadruped" feel the user pointed out.
- Flattened joint caps (`AppendEllipsoid` in `CreatureMesh.cpp`, generalizes the old `AppendSphere`) — the internal joints of the segmented spine had no real seam to cover, so an unflattened spherical cap stuck out of the elliptical spine outline like a visible "bead".
- Legs fused with the torso surface instead of a floating stick — `sideOffset` now replicates the real spine radius at that point, and the bridge bone `ChestEnd/Pelvis→Hip` was removed from the mesh.
- Head split into a short round cranium (`SnoutBase`) + a tapering snout (`HeadTip`, controlled by the new `snoutTaper` field) instead of a uniform capsule — horns/ears/eyes relocated from the muzzle tip to the cranium, where they really sit on a skull.
- Spring-damper physics for the tail (mass + stiffness + damping + gravity, with real integrated velocity) replacing the earlier positional lag (`ExpLerp`) — a small-scale prototype of the full-body physics simulation planned for Phase 10.
- DNA panel converted to ImGui tabs (Body/Details/Color/Animation) instead of a long scrolling list.

Still open for the visual/DNA side — there are no more concrete pending items noted there. But the phase is not formally closed until the user confirms that the creatures look and move well, and that now explicitly includes real physical reaction (Phase 10 below), not just the visual/kinematic scope of this phase. See `docs/DEVELOPMENT_PLAN.md`.

## Phase 10 — Physical body simulation (planned, not started)

Replaces the analytic IK (`IK.cpp`, for live posing) and the lag chains of `Animation.cpp` with a particle-and-constraint physics solver — DNA, `BuildSkeleton` (hierarchy + rest pose) and `Gait.cpp` (gait target) are reused unchanged. Decided 2026-07-29: the user wants creatures to genuinely interact physically with each other (a tiger grabbing/knocking down a deer, with the weight/strength of one really affecting the balance of the other) — a purely kinematic system cannot express that, because IK has no notion of force, mass, or "who wins a struggle", it only places an exact position without resistance. Technical reference: Rain World (bodies = chains of mass particles connected by distance/angle constraints, solved with Verlet integration + iterative relaxation; procedural "muscles" that apply a force toward a target pose instead of fixing the position directly). See the detailed design in "Architecture decisions" above.

- [ ] Particles: each existing joint gains a previous position (Verlet, no separate velocity variable) and a mass
- [ ] Distance constraints: each existing bone (`Skeleton::bones`) becomes a length constraint, with the rest length taken from `BuildSkeleton` unchanged
- [ ] Angle constraints: bend limits between consecutive bones sharing a joint, derived generically by walking the bone graph
- [ ] Muscles: a small force pulling each joint toward its target (DNA rest pose + `Gait.cpp` target + movement direction) — same principle as the Phase 9 tail spring
- [ ] Ground contact: the foot cannot go below `TerrainHeight(x,z)`, solved in the same relaxation pass as the rest
- [ ] Per-frame loop: Verlet integration (forces → predicted position) + several (4-8) relaxation passes over all constraints

Applicable to any skeleton topology by construction — the solver does not reference joints by name, only a generic graph of particles+constraints — unlike the current `Animation.cpp`, which does reference joints by concrete name and would not generalize to a new body plan without a considerable rewrite. It is a more solid foundation for future body plans (snakes, flyers) in addition to being the only route to real physical interaction between creatures. **Blocks crossbreeding just like Phase 9 — it is not work adjacent to crossbreeding, it is a prerequisite.**

## Phase 11 — Save/load DNA (planned, not started)

Moved forward from the old single "Export" phase (user decision, 2026-07-29): without being able to save and load creatures, Phase 12 (Crossbreeding) has nowhere to get "two distinct creatures" other than typing two loose seeds by hand — saving/loading DNA is in practice a requirement for crossbreeding to make sense to use, not a later extra. The rest of the old export phase (glTF/OBJ, genealogy history) does not depend on this and stays in Phase 13, at the end.

- [ ] Serialize `DNA` to disk (simple format, JSON or binary) and load it back, rebuilding the same creature
- [ ] ImGui panel: list/choose among saved creatures, save the current one under a name

**Tangible result:** close the app, reopen it, load a creature saved earlier, and get exactly the same one (same DNA, same shape/color).

## Working conventions with Claude Code

- Advance phase by phase, without getting ahead of later phases' work.
- Each phase must end in something visible/runnable, not just untested code.
- Briefly explain any 3D graphics/OpenGL concept the first time it appears in a phase.
- Ask before introducing new dependencies that are not in this document's technical stack.
- Before touching anything visual (palette, shaders, creature shape/silhouette, UI look), review the reference screenshots in `reference/critter-crosser/` and the "Visual reference" section of this document — do not improvise the style from memory.
- The screenshots only serve for mood/palette/composition. To claim how something is technically built (3D vs 2D, engine, shaders, pipeline), looking at images is not enough — look for interviews/devlogs/direct statements from the developer before treating it as "confirmed" (see the methodology note in "Inspiration and real technical reference").
