# Make3D Portfolio Notes

This document is written for portfolio reviewers and interviewers.

## One-line summary

Make3D is a deterministic C++ image-to-3D proxy asset generator. It extracts foreground regions, estimates or consumes depth, reconstructs relief / volume / hybrid meshes, exports OBJ and glTF, and writes debug artifacts and reports so the generation process can be inspected.

## What to run first

On Windows, build with CMake:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Then run:

```text
build/Release/Make3DAdvancedGui.exe
```

or:

```bash
build/Release/Make3DAdvancedCLI.exe --input <image> --output output --mode hybrid --quality detailed
```

## What the reviewer should inspect

After running the advanced GUI or CLI, inspect:

```text
make3d_advanced.obj
make3d_advanced.gltf
make3d_advanced.bin
debug_mask.ppm
debug_depth.ppm
make3d_report.md
make3d_report.json
```

The important files are not only the mesh exports. The debug mask, debug depth, and report show how the model was generated.

## Why this is not just image extrusion

The advanced backend performs several steps before geometry export:

1. Foreground extraction from alpha or background color estimation.
2. Morphological cleanup of the mask.
3. Depth preparation from either a depth PNG or single-image pseudo-depth estimation.
4. Reconstruction mode selection.
5. Relief surface, silhouette volume, or hybrid reconstruction.
6. Normal recomputation and mesh normalization.
7. OBJ / glTF export and report generation.

The `HybridVolume` mode is the main portfolio mode because it combines a volumetric proxy with front-surface depth detail.

## What is intentionally not claimed

Make3D does not claim to generate perfect production models from arbitrary images.

The defensible claim is:

> Make3D generates inspectable proxy 3D assets from 2D inputs using deterministic C++ image processing and mesh reconstruction.

This is stronger and more technically honest than claiming universal 3D reconstruction.

## Current proof points

- Advanced backend is separated from the GUI.
- CLI path makes the algorithm reproducible.
- Smoke test validates mask/depth/mesh/export pipeline.
- CI builds and tests on Windows.
- CI generates sample outputs and uploads them as artifacts.
- Advanced GUI allows non-technical reviewers to run the backend.

## Interview talking points

### 1. Why deterministic reconstruction?

The goal is to make each processing stage inspectable. Unlike a black-box generator, Make3D can output the mask, depth preview, mesh statistics, and warnings.

### 2. Why proxy assets?

Single-image reconstruction cannot infer hidden geometry. By positioning the output as game-development proxy assets, the tool remains useful while keeping its claim realistic.

### 3. Why glTF in addition to OBJ?

OBJ is simple and widely inspectable. glTF is more suitable for modern runtime/game asset workflows and makes the output easier to test in contemporary viewers and engines.

### 4. What would be improved next?

- Real sample set with screenshots and generated outputs.
- glTF material and texture export.
- Mesh cleanup: duplicate merge, manifold checks, and decimation.
- Better segmentation for real-world photos.
- Multi-frame video sampling.
- More geometry validation tests.

## Suggested portfolio wording

Japanese:

> 画像・深度画像からゲーム開発向けのプロキシ3Dアセットを生成するC++/Win32ツールを開発。前景抽出、疑似depth推定、relief/volume/hybrid mesh reconstruction、OBJ/glTF出力、debug mask/depth、Markdown/JSON report出力までを実装し、生成過程を検証可能な形にした。

English:

> Developed a C++/Win32 image-to-3D proxy asset generator for game-development workflows. The tool extracts foreground masks, estimates or consumes depth, reconstructs relief/volume/hybrid meshes, exports OBJ/glTF, and writes debug artifacts and Markdown/JSON reports for inspectable validation.
