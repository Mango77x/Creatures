# Creatures — Help

## Requirements

- Windows 10/11
- Visual Studio Build Tools (or Visual Studio) with the "Desktop development with C++" workload — provides `cl.exe` (MSVC compiler)
- CMake 3.20+ (bundled with Visual Studio under `Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`, or install standalone)
- A GPU driver supporting OpenGL 3.3+

## Build

From a Developer Command Prompt / PowerShell with the MSVC toolchain on `PATH` (or via `vcvarsall.bat x64`):

```
cmake -B build -S . -G Ninja
cmake --build build
```

The first configure will download GLFW, GLM, Dear ImGui, stb_image, and GLAD via CMake `FetchContent` — this needs an internet connection the first time, then everything is cached under `build/_deps`.

## Run

```
./build/Creatures.exe
```

A window should open with an orbital camera (drag the mouse to rotate, scroll to zoom) and an empty ImGui panel.

## Troubleshooting

- **`cl.exe` not found**: you're not in an MSVC developer environment. Launch "Developer PowerShell for VS" from the Start menu, or run `vcvarsall.bat x64` from your VS Build Tools install before calling `cmake`.
- **Black window / nothing renders**: check your GPU driver supports OpenGL 3.3 core profile; update graphics drivers if unsure.
- **FetchContent fails to download**: check your network/proxy; CMake needs to reach GitHub to fetch GLFW/GLM/ImGui/GLAD/stb_image sources.

## Known tricky bugs

- **GLAD configure fails with `ModuleNotFoundError: No module named 'jinja2'`**: GLAD2's CMake integration (`glad_add_library`) shells out to its own Python generator at build time, which needs `jinja2` installed for whatever Python interpreter CMake found. Fix: `python -m pip install jinja2` (installs `MarkupSafe` as a dependency too), then re-run the build. This is a one-time machine setup step, not a CMakeLists issue.
