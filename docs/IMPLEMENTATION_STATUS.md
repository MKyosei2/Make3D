# Make3D Implementation Status

## 2026-06 Advanced Reconstruction Upgrade

This update adds a new standalone reconstruction backend to Make3D.

The goal is to move the project away from a simple 2D image push-out tool and toward a more serious image-to-3D proxy asset generator.

## Implemented in this update

### 1. Advanced reconstruction core

Added:

```text
src/Make3DAdvancedCore.h
src/Make3DAdvancedCore.cpp
```

The core implements:

- RGBA image loading
- optional depth image loading
- alpha/background-based foreground mask generation
- morphological mask cleanup
- single-image pseudo-depth estimation
- depth smoothing
- shape/mask analysis
- relief surface reconstruction
- silhouette-volume reconstruction
- hybrid reconstruction
- normal recomputation
- mesh normalization
- OBJ export
- glTF + external binary buffer export
- debug mask image export
- debug depth image export
- Markdown report generation
- JSON report generation

### 2. CLI validation path

Added:

```text
src/Make3DAdvancedCLI.cpp
```

Example:

```bash
Make3DAdvancedCLI --input samples/input.png --output output --mode hybrid --quality detailed
```

This makes the algorithm testable without opening the Win32 GUI.

### 3. CMake target

Added:

```text
CMakeLists.txt
```

Build:

```bash
cmake -S . -B build
cmake --build build --config Release
```

### 4. Documentation

Added:

```text
docs/ADVANCED_RECONSTRUCTION.md
```

This explains the new pipeline, reconstruction modes, outputs, limitations, and next implementation steps.

## Why this matters

Before this upgrade, Make3D was mainly a simple image/depth-to-OBJ tool.

After this upgrade, Make3D has a separate reconstruction backend that can produce and prove:

- the foreground mask used for reconstruction
- the depth field used for reconstruction
- the generated geometry statistics
- OBJ output
- glTF output
- Markdown/JSON validation reports

This is important for portfolio use because the output is no longer just a claim. It produces inspectable artifacts.

## Current limitation

The advanced backend is currently added as a CLI/core path. The existing Win32 GUI in `MainApp.cpp` is not yet fully rewired to call this backend.

This was intentional for safety:

1. keep the existing GUI working
2. isolate the new algorithm
3. make the new backend buildable and testable
4. wire it into the GUI after validation

## Next required work

To make this production-level, the next tasks are:

1. Wire `Make3DAdvancedCore` into `MainApp.cpp`.
2. Add sample inputs and generated outputs.
3. Add screenshots/GIFs to README.
4. Add automated mesh validation tests.
5. Expand glTF material/texture export.
6. Add mesh cleanup: duplicate merge, manifold check, decimation.
7. Add multi-frame video sampling instead of one representative frame.
8. Add CI build verification.

## Portfolio positioning

Use this wording:

> Make3D is a deterministic C++ image-to-3D proxy asset generator. It extracts foreground regions, estimates or consumes depth, reconstructs relief/volume/hybrid meshes, exports OBJ and glTF, and writes debug artifacts and reports so that the generation process can be inspected.

Avoid this wording:

> Make3D can generate perfect production 3D models from any image.

That claim is too broad and not technically defensible for a deterministic single-image tool.
