# Make3D Portfolio Notes

This document is written for portfolio reviewers and interviewers.

## One-line summary

Make3D is a C++ / Win32 character-oriented image-to-3D asset generation pipeline. It converts a 2D character image into an inspectable hero-style glTF asset using foreground refinement, pseudo-depth and local learned-shape inference, character-specific geometry priors, hair/clothing/detail volumes, semantic vertex coloring, and debug/report artifacts.

## What to open first

From the GitHub Actions artifact:

```text
make3d-production-pipeline-sample
```

Open:

```text
production_pipeline/output/hero/make3d_hero_character_vertex_color.gltf
```

Then inspect:

```text
production_pipeline/output/hero/make3d_hero_character_material.gltf
production_pipeline/output/production_report.md
production_pipeline/output/debug_mask_refined.ppm
production_pipeline/output/debug_depth_inferred.ppm
production_pipeline/output/debug_depth_learned.ppm
```

## Why the hero output matters

The older generic reconstruction path produces raw/polished/voxel outputs. Those are still included, but the current portfolio direction is the hero character path.

The hero path adds:

```text
head / torso / arms / legs
hair volume
clothing shell
neck / shoulder connectors
hands / feet
eyes / nose / mouth hints
hair strand hints
clothing fold hints
finger hints
shoe soles
semantic vertex colors
```

This is intentionally less generic and more focused on character/figure-like output.

## How to build

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## How to generate the portfolio sample

```bash
build/Release/Make3DGenerateProductionPipelineSample.exe output/production_pipeline
```

With locally trained weights:

```bash
build/Release/Make3DTrainLearnedShapeModel.exe output/learned_shape_training
build/Release/Make3DGenerateProductionPipelineSample.exe output/production_pipeline output/learned_shape_training/learned_shape.weights
```

## Current proof points

- Advanced backend is separated from the GUI.
- Production pipeline generates a hero character output, generic polished output, voxel output, debug images, and reports.
- Local learned-shape model supports built-in weights and external weight files.
- Synthetic trainer generates `learned_shape.weights` and training reports.
- Hero character model creates character-specific base geometry.
- Hero detail enhancer adds hair, clothing, neck/shoulder connectors, hands, and feet.
- Hero fine detail pass adds eyes, nose, mouth, hair strand hints, clothing folds, finger hints, and shoe soles.
- Hero semantic glTF exporter writes `COLOR_0` vertex-color glTF with skin/hair/face/clothing/shoe regions.
- CI builds, tests, trains, generates samples, packages outputs, and uploads artifacts.

## What is intentionally not claimed

Make3D does not claim to be a perfect one-click Blender/ZBrush replacement.

The defensible claim is:

> Make3D generates inspectable hero-style character 3D assets from 2D inputs using deterministic C++ image processing, local learned-shape inference, character-specific geometry priors, semantic vertex coloring, glTF export, debug images, and Markdown/JSON reports.

This is stronger and more technically honest than claiming universal single-image 3D reconstruction.

## Interview talking points

### 1. Why specialize in character output?

Generic single-image 3D reconstruction is extremely hard because hidden geometry is not observable. The project therefore focuses on a narrower, portfolio-visible target: character/figure-style assets where a strong prior can produce more convincing shapes.

### 2. Why deterministic and inspectable?

Every major stage produces debug artifacts or reports: mask, depth, shape inference, learned-shape summary, hero reconstruction statistics, mesh quality reports, and glTF outputs. This makes the pipeline reviewable rather than a black box.

### 3. Why local learned-shape support?

The project avoids relying on external generation services. The local learned-shape stage supports built-in weights, external weight loading, and a small synthetic trainer so the pipeline has a real training/inference boundary.

### 4. Why semantic vertex colors?

Texture projection and UV unwrapping are still future work. Semantic `COLOR_0` glTF coloring gives reviewers an immediate visual distinction between skin, hair, face, clothing, lower clothing, and shoes in modern glTF viewers.

### 5. What would be improved next?

- True UV unwrapping.
- Texture projection.
- Retopology / remeshing.
- Better image-dependent pose and part detection.
- Stronger learned model trained on real character data.
- Blender screenshot or turntable artifact generation in CI.

## Suggested portfolio wording

Japanese:

> 2Dキャラクター画像から、ゲーム開発向けのhero-style 3Dアセットを生成するC++/Win32ツールを開発。前景抽出、mask refinement、疑似depth推定、local learned-shape inference、キャラクター専用の頭・胴体・腕・脚・髪・服・手足・顔パーツ生成、semantic vertex-color glTF出力、debug mask/depth、Markdown/JSON report、CI artifact生成までを実装し、生成過程と出力品質を検証可能な形にした。

English:

> Developed a C++/Win32 hero-style character asset generation pipeline from 2D inputs. The tool performs foreground refinement, pseudo-depth estimation, local learned-shape inference, character-specific geometry generation, hair/clothing/face/detail passes, semantic vertex-color glTF export, debug mask/depth outputs, Markdown/JSON reports, and CI artifact generation for inspectable technical review.
