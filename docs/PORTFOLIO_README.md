# Make3D Portfolio Notes

This document is written for portfolio reviewers and interviewers.

## One-line summary

Make3D is a deterministic C++ image-to-3D proxy asset generator. It extracts foreground regions, estimates or consumes depth, reconstructs relief / volume / hybrid meshes, exports OBJ, geometry glTF, and material glTF, and writes debug artifacts plus mesh quality reports so the generation process can be inspected.

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

For generated proof artifacts, run:

```bash
build/Release/Make3DGenerateAdvancedSamples.exe output/generated_samples
build/Release/Make3DGenerateMeshQualityReport.exe output/mesh_quality
```

## What the reviewer should inspect

After running the advanced GUI or CLI, inspect:

```text
make3d_advanced.obj
make3d_advanced.gltf
make3d_advanced.bin
make3d_advanced_material.gltf
make3d_advanced_material.bin
debug_mask.ppm
debug_depth.ppm
make3d_report.md
make3d_report.json
```

The preferred modern asset output is `make3d_advanced_material.gltf` because it includes material metadata in addition to mesh geometry.

For output details, see:

```text
docs/OUTPUT_ARTIFACTS.md
```

## Mesh quality proof

The mesh quality generator creates raw and cleaned outputs:

```text
mesh_quality_report.md
mesh_quality_report.json
raw/make3d_advanced.obj
raw/make3d_advanced.gltf
cleaned/make3d_cleaned.obj
cleaned/make3d_cleaned.gltf
cleaned/make3d_cleaned_material.gltf
```

This demonstrates that Make3D does not only export a model; it also checks invalid triangles, degenerate triangles, boundary edges, non-manifold edges, and export validity.

## Why this is not just image extrusion

The advanced backend performs several steps before geometry export:

1. Foreground extraction from alpha or background color estimation.
2. Morphological cleanup of the mask.
3. Depth preparation from either a depth PNG or single-image pseudo-depth estimation.
4. Reconstruction mode selection.
5. Relief surface, silhouette volume, or hybrid reconstruction.
6. Normal recomputation and mesh normalization.
7. OBJ / geometry glTF / material glTF export.
8. Debug artifact and report generation.
9. Mesh quality validation and cleanup through dedicated tools.

The `HybridVolume` mode is the main portfolio mode because it combines a volumetric proxy with front-surface depth detail.

## What is intentionally not claimed

Make3D does not claim to generate perfect production models from arbitrary images.

The defensible claim is:

> Make3D generates inspectable proxy 3D assets from 2D inputs using deterministic C++ image processing, mesh reconstruction, material glTF export, and mesh validation reports.

This is stronger and more technically honest than claiming universal 3D reconstruction.

## Current proof points

- Advanced backend is separated from the GUI.
- CLI path makes the algorithm reproducible.
- Advanced GUI allows non-technical reviewers to run the backend.
- Smoke test validates mask/depth/mesh/export pipeline.
- Mesh tools test validates cleanup and mesh inspection logic.
- glTF material exporter test verifies material sections are written.
- CI builds, tests, generates samples, generates mesh quality reports, and uploads artifacts.

## Interview talking points

### 1. Why deterministic reconstruction?

The goal is to make each processing stage inspectable. Unlike a black-box generator, Make3D can output the mask, depth preview, mesh statistics, warnings, and quality reports.

### 2. Why proxy assets?

Single-image reconstruction cannot infer hidden geometry. By positioning the output as game-development proxy assets, the tool remains useful while keeping its claim realistic.

### 3. Why glTF in addition to OBJ?

OBJ is simple and widely inspectable. glTF is more suitable for modern runtime/game asset workflows. The material glTF path adds PBR metadata such as base color, roughness, metallic factor, and double-sided rendering.

### 4. Why mesh validation?

Generated geometry can contain invalid or degenerate data. The validation path makes the output safer to inspect by reporting export validity, boundary edges, non-manifold edges, and cleaned mesh statistics.

### 5. What would be improved next?

- Real sample set with screenshots and generated outputs.
- Texture image copying into the material glTF output folder.
- Stronger mesh cleanup: duplicate merge, manifold repair, and decimation.
- Better segmentation for real-world photos.
- Multi-frame video sampling.
- More geometry validation tests.

## Suggested portfolio wording

Japanese:

> 画像・深度画像からゲーム開発向けのプロキシ3Dアセットを生成するC++/Win32ツールを開発。前景抽出、疑似depth推定、relief/volume/hybrid mesh reconstruction、OBJ/glTF/material glTF出力、debug mask/depth、Markdown/JSON report、mesh quality report出力までを実装し、生成過程と出力品質を検証可能な形にした。

English:

> Developed a C++/Win32 image-to-3D proxy asset generator for game-development workflows. The tool extracts foreground masks, estimates or consumes depth, reconstructs relief/volume/hybrid meshes, exports OBJ, geometry glTF, and material glTF, and writes debug artifacts plus Markdown/JSON quality reports for inspectable validation.
