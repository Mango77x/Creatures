# Creatures — Project Overview

**Last updated:** Phase 4

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
- **Camera**: an orbital camera — no free-fly controls. It orbits a fixed target point; mouse drag changes yaw/pitch around it, scroll changes distance. Implemented with GLM's view/projection matrices. **This is a development/debug camera**, useful for inspecting the generated skeleton/mesh from any angle while building Phases 3-4. The final look locks the camera to a fixed oblique angle in Phase 5 (see `CLAUDE.md`'s architecture decisions) — the free orbit goes away then.
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

## Build & toolchain

Target platform is Windows with MSVC (see `HELP.md`). CMake is otherwise portable — nothing here prevents adding a Linux/WSL target later if that becomes useful, it's just not the active target today.
