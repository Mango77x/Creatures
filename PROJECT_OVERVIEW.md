# Creatures — Project Overview

**Last updated:** Phase 7

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

- **Knee joints** (`src/Skeleton.h/.cpp`): each leg is now 3 bones (`Hip→Knee→Foot`) instead of a single segment — a straight-line leg gives an IK solver nothing to bend, so the knee (with a small forward bend-hint offset in the rest pose) is a prerequisite for any real IK.
- **`SolveFABRIK`** (`src/IK.h/.cpp`): a standard FABRIK solver (backward pass pulls the end effector to the target and walks back fixing segment lengths, forward pass re-pins the root and walks forward again fixing lengths, repeated a few iterations). The hip is pinned as the chain root; the rest-pose knee position is reused each frame purely as the starting guess, which is enough to keep the bend direction stable frame to frame for a 2-segment chain.
- **`ComputeFootTarget`** (`src/Gait.h/.cpp`): stateless procedural gait — during "stance" the foot's body-local position drifts backward at the same rate the body walks forward (so it stays roughly planted in world space without tracking a touch-down point), and during "swing" it arcs forward and up (`sin` lift) to the next stance position. `GaitParams` (speed/stride/lift) are live ImGui sliders.
- **Gait pattern**: diagonal trot — front-left + back-right share phase 0.0, front-right + back-left share phase 0.5 — expressed as a small `LegDescriptor` array in `main.cpp` rather than hardcoded per-leg logic, so it isn't tied to exactly 4 legs even though the skeleton is fixed-quadruped for now.
- **Body movement**: the creature loops on a small circle around the fixed camera target (`bodyTransform` = translate + yaw-toward-tangent, computed from `time`) — a straight walk path would leave the Phase 5 fixed-angle view almost immediately. All leg-IK/gait math happens in the skeleton's own local space; `bodyTransform` only affects the render-time `uModel`, so the solver never needs to know the body is moving.
- **Ground plane**: a simple static quad at `y = 0`, drawn with the same lit `basic` shader but its own muted color and an identity model matrix (it doesn't move with the body).

## Build & toolchain

Target platform is Windows with MSVC (see `HELP.md`). CMake is otherwise portable — nothing here prevents adding a Linux/WSL target later if that becomes useful, it's just not the active target today.
