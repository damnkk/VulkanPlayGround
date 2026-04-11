# Repository Guidelines

## Project Structure & Module Organization
`code/` contains the application and engine sources. Key areas are `code/renderer/` for renderers and passes, `code/resourceManagement/` for shaders, pipelines, scene data, and RDG helpers, `code/core/` for shared utilities, and `code/debugger/` for Nsight/diagnostics hooks. Entry points live in `code/main.cpp` and `code/PlayApp.*`.

Runtime assets are split by purpose: `resource/` holds models and skyboxes, `content/volumeData/` holds volume datasets, and `shaders/` contains GLSL and Slang sources such as `shaders/newShaders/gaussian/`. Third-party code is vendored under `External/`; avoid modifying it unless the change is an intentional dependency update.

## Build, Test, and Development Commands
This repo uses CMake with Ninja and currently builds in `build/` with `CMAKE_BUILD_TYPE=Debug`.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
.\_bin\Debug\VulkanPlayGround.exe --validation --rendermode defer
```

Use `--rendermode gaussian` when working on the Gaussian path. Build outputs and runtime shader artifacts are copied under `_bin/Debug/`.

## Coding Style & Naming Conventions
Formatting is defined in `.clang-format`: 4-space indentation, no tabs, Allman braces, left-aligned pointers, and a 150-column limit. Run `clang-format -i` on touched C++ files before opening a PR.

Match existing naming in the area you edit: PascalCase for types and most engine files (`PlayApp.h`, `GBufferPass.cpp`), descriptive shader names, and existing member-prefix style within each class (`_renderer`, `m_app`, etc.). Keep header/source pairs aligned by name.

## Testing Guidelines
There is no top-level automated test suite for the application. Validate changes with:
- a clean `cmake --build build`
- a local smoke run of `_bin/Debug/VulkanPlayGround.exe`
- targeted checks for the feature you changed, especially shader compilation and viewport/render-path startup

If you add tests, keep them outside `External/` and wire them into the root CMake project.

## Commit & Pull Request Guidelines
Recent commits use short lowercase prefixes such as `fixup:` and `add:` followed by an imperative summary. Keep subjects concise and scoped, for example `fixup: resolve gaussian depth ordering`.

PRs should include a short problem/solution summary, linked issue if applicable, build/test notes, and screenshots for rendering or UI changes. Call out dependency, shader, or asset changes explicitly.
