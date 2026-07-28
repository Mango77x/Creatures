# Creatures — Project Overview

**Last updated:** Phase 2

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
- **Camera**: an orbital camera — no free-fly controls. It orbits a fixed target point; mouse drag changes yaw/pitch around it, scroll changes distance. Implemented with GLM's view/projection matrices.
- **UI**: Dear ImGui is initialized against the same GLFW window/OpenGL context (via its `imgui_impl_glfw` and `imgui_impl_opengl3` backends) and renders an empty panel, ready to host the DNA lab controls starting Phase 2.

## Phase 2 — DNA system

- **`DNA` struct** (`src/DNA.h`): a flat set of numeric parameters — no genome language, no morphology graph. `legCount` is present but hardcoded to 4 for now, matching the fixed-quadruped-skeleton decision; it becomes meaningfully variable once body plans generalize past Phase 8.
- **Generation** (`src/DNA.cpp`, `GenerateDNA(seed)`): seeds a `std::mt19937` with the given seed and draws each field in a fixed order via `std::uniform_real_distribution`. Same seed in → same DNA out, deterministically, because the RNG's sequence only depends on the seed and the fixed draw order.
- **UI**: the ImGui panel exposes a seed field, a "Generate" button (regenerate from the typed seed), and a "Random seed" button (draws a seed from `std::random_device`), followed by a read-only listing of the resulting parameters.

## Build & toolchain

Target platform is Windows with MSVC (see `HELP.md`). CMake is otherwise portable — nothing here prevents adding a Linux/WSL target later if that becomes useful, it's just not the active target today.
