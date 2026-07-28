# Creatures — Project Overview

**Last updated:** Phase 9 (in progress)

## Purpose

A C++/OpenGL lab (no engine) for procedurally generating, animating, and simulating quadruped creatures from a seeded DNA parameter set. This doc explains where things live and why, for anyone (including future-me) picking the codebase back up.

## Architecture decisions (locked, see `CLAUDE.md` for the full rationale)

- **DNA = flat parameter struct**, not an interpreted genome language or a morphology graph with sockets.
- **Template variation**: new creatures mutate a base/parent's DNA rather than generating from nothing.
- **Fixed quadruped skeleton** (Pelvis → Spine → Neck/Head, Tail, 4 legs) until that pipeline works end-to-end — no arbitrary body plans yet.
- **Body/neck/tail animation** is a follow-the-leader delayed chain, not full per-segment IK.
- **Legs**: raycast → foot placement → IK (FABRIK/CCD) → pelvis/spine adjustment.
- **Rendering**: simple per-bone geometry (capsules/cylinders/spheres) + deliberate low-poly + pixel-art shader. No PBR, no dynamic shadows.

## Repo layout (high level)

```
CMakeLists.txt        Root build config, FetchContent dependencies
src/                  Engine source (window, camera, renderer, DNA, skeleton, animation, IK)
shaders/              GLSL shader sources
docs/DEVELOPMENT_PLAN.md   Phase-by-phase execution tracker
```

This section grows as each phase lands real subsystems (DNA struct, skeleton generator, mesh generator, IK solver, etc.) — right now only Phase 1's window/camera/ImGui scaffold exists.

## Phase 1 — Window, context, and camera

- **Windowing/context**: GLFW creates the OS window and the OpenGL context (the connection between your code and the GPU driver).
- **Function loading**: OpenGL's modern functions aren't declared by the OS headers on Windows (which only ship OpenGL 1.1). GLAD generates the loader that resolves the real function pointers from the driver at runtime.
- **Camera**: orbits a fixed target point using spherical coordinates (yaw/pitch/distance), implemented with GLM's view/projection matrices. Through Phase 4 this was fully mouse-controllable (a dev/debug convenience for inspecting the generated skeleton/mesh from any angle); **as of Phase 5, yaw and pitch are locked** to a fixed oblique/dimetric angle (see `CLAUDE.md`'s camera decision and the Phase 5 section below) — only scroll-to-zoom remains.
- **UI**: Dear ImGui is initialized against the same GLFW window/OpenGL context (via its `imgui_impl_glfw` and `imgui_impl_opengl3` backends) and renders an empty panel, ready to host the DNA lab controls starting Phase 2.

## Phase 2 — DNA system

- **`DNA` struct** (`src/DNA.h`): a flat set of numeric parameters — no genome language, no morphology graph. `legCount` is present but hardcoded to 4 for now, matching the fixed-quadruped-skeleton decision; it becomes meaningfully variable once body plans generalize past Phase 8.
- **Generation** (`src/DNA.cpp`, `GenerateDNA(seed)`): seeds a `std::mt19937` with the given seed and draws each field in a fixed order via `std::uniform_real_distribution`. Same seed in → same DNA out, deterministically, because the RNG's sequence only depends on the seed and the fixed draw order.
- **UI**: the ImGui panel exposes a seed field, a "Generate" button (regenerate from the typed seed), and a "Random seed" button (draws a seed from `std::random_device`), followed by a read-only listing of the resulting parameters.

## Phase 3 — Skeleton generator

- **`Skeleton` struct** (`src/Skeleton.h`): a flat list of joint positions (`glm::vec3`) plus a list of `Bone` entries (start joint, end joint, a `BoneKind` tag: Spine/Neck/Head/Tail/Leg) — no generic bone hierarchy/transform tree yet, since the skeleton is a fixed quadruped shape, not a variable rig. The `BoneKind` tag exists so the Phase 4 mesh generator knows how thick to make each bone without hardcoding joint indices.
- **`BuildSkeleton(dna)`** (`src/Skeleton.cpp`): places joints procedurally from DNA fields — pelvis height from `bodyHeight`, spine/neck/tail directions and lengths from `bodyLength`/`neckLength`/`tailLength`, leg spacing from `bodyFat`. Feet are always projected to `y = 0` (ground level). `hornSize`/`eyeSize`/`earSize`/`muscle`/`aggressiveness` don't affect the skeleton — they're mesh/animation/behavior concerns for later phases.
- **Debug rendering**: no mesh yet, so the skeleton draws as raw `GL_LINES` (bones) and `GL_POINTS` (joints) via a new unlit `line.vert`/`line.frag` shader pair (`Shader` class reused, just without the lighting normal that `basic.vert`/`basic.frag` expect). Re-uploaded to the GPU (`glBufferData`) every time DNA regenerates. The Phase 1 placeholder cube and `basic.vert`/`basic.frag` are retired from `main.cpp` for now — Phase 4 reintroduces lit shading once there's real mesh geometry to shade.

## Phase 4 — Mesh generator

- **`BuildCreatureMesh(skeleton, dna)`** (`src/CreatureMesh.h/.cpp`): walks every `Bone` and appends a tapered cylinder (`AppendCylinder`) between its two joints, radius picked by `BoneKind` and DNA (e.g. spine thickness from `bodyFat`/`muscle`, head size from `eyeSize`). The tail bone specifically tapers from full radius down to a near-zero tip for a pointed look, instead of the constant radius every other bone uses.
- **Joint spheres**: every joint also gets an `AppendSphere` cap sized to the thickest bone touching it, so segments meet without gaps (e.g. the pelvis, where spine/tail/both back legs all connect) and leaf ends (feet, head tip) get a rounded cap instead of an open tube.
- **No index buffers**: both the cylinder and sphere generators emit a flat `MeshVertex{position, normal}` triangle list (same pattern as the Phase 1 cube) — simplest thing that works at this scale, not worth an EBO yet.
- **Rendering**: `basic.vert`/`basic.frag` (the lit shader retired in Phase 3) comes back for the solid mesh. The Phase 3 debug skeleton (lines + joint points) is now an optional overlay toggled from the panel ("Show skeleton (debug)"), drawn with `glDisable(GL_DEPTH_TEST)` so it stays visible on top of the solid mesh instead of being hidden inside it.

## Phase 5 — Shaders

- **Fixed camera** (`src/Camera.h/.cpp`): `Camera`'s constructor now takes explicit `yawRadians`/`pitchRadians`, defaulting to Creatures' final oblique/dimetric angle (45°/32°). `ProcessMouseDrag` and its callback wiring in `main.cpp` are gone — only `ProcessScroll` (zoom) remains.
- **Banded (cel-shaded) lighting** (`shaders/basic.frag`): the diffuse term is quantized into 4 discrete steps (`floor(diffuse * bands) / bands`) instead of a smooth falloff, matching the reference's flat tone bands instead of a gradient.
- **Real pixel-art via a low-res render pass**, not a post-process filter — this is the actual technique the Critter Crosser research confirmed (low-poly 3D + a fixed-angle pixelation shader, see `CLAUDE.md`'s cited sources):
  - The 3D scene (mesh + optional debug skeleton) renders into an offscreen framebuffer (`lowResFbo`/`lowResColorTex`/`lowResDepthRbo`) sized to `window size / pixelScale`, recreated on resize or when `pixelScale` changes.
  - That low-res color texture is then blitted to the real window via a fullscreen quad (`shaders/screen.vert/.frag`) with `GL_NEAREST` filtering — the nearest-neighbor upscale is what turns smooth 3D edges into chunky pixel blocks.
  - `pixelScale` (1-10) is a live ImGui slider, so the pixelation amount is tunable without recompiling.
  - ImGui itself renders after this blit, directly at full window resolution — the lab UI is never pixelated, only the 3D viewport.
- **Known gaps, intentionally out of Phase 5's scope**: the creature is a single flat hue (no multi-tone palette like the reference's per-part coloring) and has no distinct horn/eye/ear geometry (DNA carries `hornSize`/`eyeSize`/`earSize`, but Phase 4's mesh generator only uses them to size the head capsule). Both are mesh/coloring work for a later phase or polish pass, not a Phase 5 shader concern.

## Phase 6 — Procedural animation

- **`SkeletonJoint` made public** (`src/Skeleton.h`): the joint-name enum moved out of `Skeleton.cpp`'s anonymous namespace so animation code can address specific joints (`NeckEnd`, `HeadTip`, `TailMid`, `TailTip`, `ChestEnd`, `Pelvis`) by name instead of only iterating bones generically.
- **Tail split into two bones** (`Pelvis→TailMid→TailTip`, both `BoneKind::Tail`) instead of one, each with explicit `startRadiusScale`/`endRadiusScale` on `Bone` so it tapers continuously to a point across both segments. This generalized the old Tail-only special case in `CreatureMesh.cpp` into a per-bone radius multiplier any bone could use.
- **`AnimationState` + `ApplyAnimation`** (`src/Animation.h/.cpp`): the spine and legs stay exactly at their DNA rest-pose positions (no IK/foot-planting yet — that's Phase 7); only the neck (`NeckEnd`, `HeadTip`) and tail (`TailMid`, `TailTip`) chains are animated. Each frame, a small sine-wave "leader" offset (an idle bob) is applied at the chain's fixed anchor (`ChestEnd` for the neck, `Pelvis` for the tail), and each joint in the chain exponentially chases (previous joint's already-lagged position + its rest-pose offset) — this is literally CLAUDE.md's "each segment follows the previous one with a small delay," not a spring/IK solver.
- **Head look-at**: `HeadTip`'s target position is nudged toward `lookAtTarget` (clamped to a max lean distance so it can't detach from the neck), then smoothed the same way as the rest of the chain. Since the head has no directional mesh feature yet (no eyes), "looking" reads as the head/neck leaning toward the target rather than a true rotation — a deliberate simplification until there's an asymmetric head shape to actually orient.
- **Breathing**: `AnimationState::breathScale` is a sine wave multiplied into the Spine bone's radius only (`CreatureMesh`'s new `breathScale` parameter), inflating/deflating the torso — independent of the neck/tail sway.
- **Mesh/skeleton now rebuild every frame**, not just on Generate/Random seed: `main.cpp`'s render loop calls `ApplyAnimation` then re-uploads both the debug skeleton and the mesh each frame, since joint positions are no longer static between DNA regenerations. DNA regeneration resets `AnimationState` to avoid stale lag state from a differently-proportioned creature.
- **UI**: an "Animation" panel section exposes `lookAtTarget` as a live `SliderFloat3`, so pointing it around and watching the head/neck respond is directly testable without recompiling.

## Phase 7 — Leg IK + gait cycle

- **Knee joints** (`src/Skeleton.h/.cpp`): each leg is now 3 bones (`Hip→Knee→Foot`) instead of a single segment — a straight-line leg gives an IK solver nothing to bend, so the knee is a prerequisite for any real IK.
- **`SolveFABRIK`** (`src/IK.h/.cpp`): a standard FABRIK solver, introduced here for legs. **Superseded for legs in Phase 8** by an analytic 2-bone solver — see below — after it turned out to flip the knee the wrong way sometimes; `SolveFABRIK` itself is still in the codebase for any future chain with more than 2 segments (e.g. neck/tail IK).
- **`ComputeFootTarget`** (`src/Gait.h/.cpp`): stateless procedural gait — during "stance" the foot's body-local position drifts backward at the same rate the body walks forward (so it stays roughly planted in world space without tracking a touch-down point), and during "swing" it arcs forward and up (`sin` lift) to the next stance position. `GaitParams` (speed/stride/lift) are live ImGui sliders.
- **Gait pattern**: diagonal trot — front-left + back-right share phase 0.0, front-right + back-left share phase 0.5 — expressed as a small `LegDescriptor` array in `main.cpp` rather than hardcoded per-leg logic, so it isn't tied to exactly 4 legs even though the skeleton is fixed-quadruped for now.
- **Body movement**: the creature loops on a small circle around the fixed camera target (`bodyTransform` = translate + yaw-toward-tangent, computed from `time`) — a straight walk path would leave the Phase 5 fixed-angle view almost immediately. All leg-IK/gait math happens in the skeleton's own local space; `bodyTransform` only affects the render-time `uModel`, so the solver never needs to know the body is moving.
- **Ground plane**: a simple static quad at `y = 0`, drawn with the same lit `basic` shader but its own muted color and an identity model matrix (it doesn't move with the body). **Replaced by a real heightfield in Phase 8** — see below.

## Phase 8 — Terrain adaptation

- **`TerrainHeight`/`BuildTerrainMesh`** (`src/Terrain.h/.cpp`): a handful of overlapping sine waves (deterministic, no noise library) instead of the Phase 7 flat quad. Because it's an analytic heightfield, per-leg "raycasting" collapses to a direct `TerrainHeight(x, z)` sample — a vertical ray hit and evaluating the height function at that point are the same thing for a heightfield; true ray/mesh intersection only earns its cost once terrain can overhang itself, which this can't. One term is a deliberately *diagonal* wave (wavelength close to the creature's stance width) — a plain front/back + left/right slope is something a single body-tilt plane can fully explain, but a diagonal wave isn't, which is what actually exercises independent per-leg adjustment (see below).
- **Pelvis/spine adjustment**: each frame, all four feet's ground heights are sampled, then averaged into a body pitch (front vs. back) and roll (left vs. right), clamped to `kMaxTilt`. This becomes part of `bodyTransform` (translate at the average height, yaw, then pitch, then roll). Whatever the fitted plane *doesn't* explain (e.g. two diagonally-opposite feet on different local bumps, which cancel out in a front/back+left/right average) is left as a residual that each leg's own IK has to resolve independently — confirmed by testing against a deliberately diagonal terrain feature.
- **Leg IK switched to analytic 2-bone** (`SolveTwoBoneIK` in `src/IK.h/.cpp`): law-of-cosines solve for an exact hip→knee→foot chain, always bending toward a fixed `poleDir` (local forward). Replaces the Phase 7 FABRIK-per-leg call after FABRIK — having no explicit bend-direction constraint on a 2-segment chain — was observed flipping the knee the wrong way as the target moved. `SolveFABRIK` remains for chains with more than 2 segments; it was simply the wrong tool for this specific case.
- **Rest-pose standing crouch** (`BuildLeg` helper + `kStandCrouchFactor` in `Skeleton.cpp`): the rest pose's knee is now built with the *same* `SolveTwoBoneIK` call the runtime uses, with each leg's total reach (upper + lower segment) set to standing-height ÷ 0.82 rather than ≈ standing-height. That slack is what gives the leg a permanent, natural knee bend even standing still on flat ground — real legged animals never fully straighten their legs; the earlier version had almost no slack and looked stiff/penguin-like.
- **Movement**: replaced the Phase 7 fixed circular walk path with mouse-follow steering — the cursor is unprojected into a world-space ray, intersected with the `y = 0` plane, clamped inside the terrain bounds, and the body seeks toward that point at a constant speed (`kWalkSpeed`), only rotating/advancing the gait clock while actually moving (so legs don't march in place when idle). Four inward-facing wall quads (`BuildBoundaryWalls`) mark the terrain edge and pair with a position clamp so steering can't walk the creature off the world.

## Phase 9 — Visual variety (in progress)

- **Per-DNA palette** (`src/Palette.h/.cpp`): `BodyColor`/`AccentColor`/`EyeColor` convert new DNA fields (`bodyHue`, `accentHueShift`, `colorSaturation`, `colorValue`) via HSV→RGB. `MeshVertex` gained a `color` field, set per-bone in `CreatureMesh.cpp`'s `BoneColor` (accent for Horn/Ear, body for everything else) and multiplied with the `uColor` uniform in `basic.frag` (`shaded = uColor * vColor * banding`) — non-creature meshes (terrain, walls) pass white vertex color so their existing `uColor` tint is unaffected.
- **Horn/ear/eye geometry** (`Skeleton.h/.cpp`): new joints (`HornTip`, `LeftEarTip`, `RightEarTip`, `LeftEye`, `RightEye`) placed as offsets from `HeadTip`. Horns/ears are ordinary tapered-cylinder bones (`BoneKind::Horn`/`Ear`) reusing the existing leg/tail cylinder pipeline — no new mesh code. Eyes are explicit spheres (no bone, since a point has no segment). Placement is relative to the head's own mesh radius (`headRadiusApprox`, mirroring `CreatureMesh.cpp`'s Head `BoneRadius` formula) so ears/eyes clear the head's surface instead of being rendered fully inside it (an actual bug hit and fixed during this phase — the original fixed offsets were smaller than the head sphere's own radius). `Animation.cpp` carries these joints along with the head's look-at lean via a translation delta from `HeadTip`'s rest→animated offset.
- **Camera: perspective → orthographic, no zoom** (`src/Camera.h/.cpp`): confirmed against the reference screenshots (sidewalk tiles/building edges never converge to a vanishing point) that Critter Crosser uses parallel, not perspective, projection. `GetProjectionMatrix` now calls `glm::ortho`. `FitToGround(groundHalfSize, extraHeight)` computes the view-space extents of the terrain's corners (projected onto the camera's actual right/up basis) once at startup, so the whole map is always framed — ground extent and vertical clearance (walls/creature height) are combined as independent additive terms rather than checked per corner-height combination, which had been wildly over-conservative (checking a tall object at the map's most extreme diagonal corner, which nothing in the scene actually does). `ProcessScroll`/zoom is gone entirely.
- **Terrain: heightfield → block terraces** (`src/Terrain.cpp`): `TerrainHeight` quantizes the same overlapping-sine shape into flat steps (`kTerraceStep`) instead of a continuous surface. `BuildTerrainMesh` emits a flat top quad per grid cell plus vertical riser quads only where neighboring cells differ in height (checked from the higher cell's side, so each boundary gets exactly one wall) — a voxel/terrace mesh instead of a smoothly interpolated grid. `TerrainHeight`'s signature is unchanged, so the Phase 8 per-leg raycast/IK code didn't need to change, only the terrain it samples.
- **`kCreatureScale`** (`Skeleton.h`): the generated creature turned out several terrain blocks long instead of the reference's ~1 block. Rather than retuning every DNA range, one global scale constant is applied as a final pass over `BuildSkeleton`'s joint positions and multiplied into `CreatureMesh.cpp`'s radius formulas — since skeleton generation and the two-bone leg IK are all linear in the input lengths, scaling the final joint positions by a constant is mathematically identical to having scaled every DNA length input from the start, so all Phase 3-8 proportional tuning carries through unchanged. Animation offsets (`kBobAmount`, `kMaxHeadLean`), gait defaults (`strideLength`, `liftHeight`), walk speed, and terrain step/amplitude are separately multiplied by the same constant so they stay proportional to the smaller creature instead of looking relatively oversized.
- **Elliptical cross-sections** (`CreatureMesh.cpp`): `AppendCylinder` gained a `glm::vec2 crossSection` (side scale, up scale) instead of assuming a perfect circle — a uniform tube read as "capsules glued together" regardless of DNA. Only applied to spine/neck/head, whose `PerpendicularBasis` side/up vectors happen to line up with the body's actual left-right/up axes for a roughly horizontal bone; legs keep circular cross-sections since their near-vertical axis picks an arbitrary horizontal reference that wouldn't consistently mean anything anatomically. The ellipse's outward normal isn't the same direction as its surface offset (unlike a circle) — normals are computed with the perpendicular (inverted) scale so lighting stays correct.
- **3-tone palette** (`Palette.h/.cpp`'s `BellyColor`, `CreatureMesh.cpp`'s `BoneBellyColor`): `AppendCylinder`/`AppendTriangle` now carry a top and bottom color instead of one flat color per bone, split at the cross-section's equator (`sin(t) >= 0`) — applied to spine/neck/tail only. The GPU interpolating each straddling triangle's vertex colors turns the hard split into a one-segment-wide gradient band instead of a razor edge, without needing extra geometry.

## Build & toolchain

Target platform is Windows with MSVC (see `HELP.md`). CMake is otherwise portable — nothing here prevents adding a Linux/WSL target later if that becomes useful, it's just not the active target today.
