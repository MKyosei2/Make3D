# Advanced Reconstruction Engine

This document describes the new reconstruction core added to Make3D.

The original GUI application already supported alpha/background-based foreground masks, pseudo-depth, primitive box, humanoid proxy, and OBJ export. The new core is a separate implementation layer designed to move the project away from a simple image extrusion prototype and toward a more inspectable 3D asset generation pipeline.

## Goals

- Generate meshes that are more useful than flat push-out surfaces.
- Support both relief and volumetric reconstruction modes.
- Export both OBJ and glTF 2.0 style assets.
- Produce debug artifacts that show how the mesh was created.
- Produce a report that can be attached to a portfolio or validation log.
- Keep the engine deterministic and explainable.

## New files

```text
src/Make3DAdvancedCore.h
src/Make3DAdvancedCore.cpp
src/Make3DAdvancedCLI.cpp
CMakeLists.txt
docs/ADVANCED_RECONSTRUCTION.md
```

## Pipeline

```text
Color PNG
  ↓
LoadImageRGBA
  ↓
BuildForegroundMask
  ↓
PrepareDepth
  ├─ provided depth PNG
  └─ single-image depth estimation
  ↓
ReconstructMesh
  ├─ ReliefSurface
  ├─ SilhouetteVolume
  └─ HybridVolume
  ↓
RecomputeNormals
  ↓
NormalizeMesh
  ↓
OBJ / glTF export
  ↓
Markdown / JSON report
  ↓
debug_mask.ppm / debug_depth.ppm
```

## Reconstruction modes

### ReliefSurface

Builds a front/back depth surface from a foreground mask and depth map. Unlike a simple flat push-out, the front surface is shaped by the estimated depth field and capped with boundary walls.

Use this when:

- the source is an illustration or decal-like object
- a front-facing relief model is acceptable
- texture details should become surface relief

### SilhouetteVolume

Builds rings along the silhouette height and turns row widths into 3D radial cross sections. This gives the model an actual volume instead of only a front/back extrusion.

Use this when:

- the object has a clear silhouette
- the target is a proxy asset
- the result should be viewable from multiple angles

### HybridVolume

Combines the silhouette volume with a relief surface. This is the default direction for making the output less flat while still retaining depth details from the source image.

Use this when:

- the input is a single image
- no real depth map is available
- you want a model that has both volume and front-surface detail

## Depth estimation

When a depth PNG is not provided, the advanced core estimates pseudo-depth from:

- distance from silhouette boundary
- local luminance
- vertical body/object bias
- edge-aware smoothing

This is still not true multi-view 3D reconstruction. It is a deterministic heuristic designed for game-ready proxy generation.

## Output artifacts

The CLI writes:

```text
make3d_advanced.obj
make3d_advanced.mtl
make3d_advanced.gltf
make3d_advanced.bin
make3d_report.md
make3d_report.json
debug_mask.ppm
debug_depth.ppm
```

The report includes:

- image size
- foreground pixel count
- foreground coverage
- depth min/max/mean
- reconstruction mode
- vertex count
- triangle count
- whether provided depth was used
- whether the mesh is a watertight candidate
- warnings

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

## CLI usage

```bash
Make3DAdvancedCLI --input samples/input.png --output output --mode hybrid --quality detailed
```

With a depth PNG:

```bash
Make3DAdvancedCLI --input samples/input.png --depth samples/depth.png --output output --mode hybrid --quality detailed
```

Modes:

```text
auto
relief
volume
hybrid
```

Quality presets:

```text
draft
standard
detailed
```

## Portfolio value

The important change is not just that the tool writes another mesh. The important change is that the generation process is now inspectable:

- `debug_mask.ppm` proves what region was reconstructed.
- `debug_depth.ppm` proves the generated or loaded depth field.
- `make3d_report.md` proves mesh statistics and warnings.
- OBJ and glTF prove the result can be consumed by DCC/game tools.

This turns Make3D from a black-box demo into a small asset generation pipeline.

## Current limitations

- This is not neural 3D generation.
- Single-image reconstruction cannot infer hidden back-side details.
- glTF export currently writes geometry and buffers only; texture/material export should be expanded next.
- The Win32 GUI is not yet wired to the advanced core.
- The CLI path should be validated on Windows with the repository's actual build environment.

## Next implementation steps

1. Wire the advanced core into the Win32 GUI as a new quality/reconstruction backend.
2. Add texture export to glTF.
3. Add sample inputs and generated outputs.
4. Add automated geometry validation tests.
5. Add screenshots/GIFs for README.
6. Add mesh cleanup operations: duplicate vertex merge, UV seam handling, manifold check, and decimation.
