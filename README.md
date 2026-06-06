# Make3D

**Make3D** is a C++17 / Win32 image-to-3D asset generation pipeline for producing inspectable game-development proxy / hero assets from 2D character or prop images.

This repository is positioned as a **Technical Artist / Tools Programmer portfolio project**. The focus is not to claim perfect single-image reconstruction. The focus is an explainable local pipeline: foreground extraction, mask refinement, pseudo-depth / learned-shape style inference, geometry generation, semantic coloring, reports, debug images, regression tests, and reproducible build artifacts.

> Scope note: Make3D is not a Blender, ZBrush, photogrammetry, or neural reconstruction replacement. The defensible claim is an inspectable C++ game-asset generation pipeline with deterministic outputs and reviewable intermediate data.

---

## Portfolio summary

```text
2D character / prop image
  -> foreground mask extraction
  -> mask refinement
  -> pseudo-depth preparation
  -> shape inference
  -> local learned-shape inference
  -> hero / proxy geometry generation
  -> semantic palette extraction
  -> OBJ / material glTF / vertex-color glTF export
  -> debug images + Markdown/JSON reports
  -> CI-generated review artifact
```

The strongest current path is the **hero character / game asset review pipeline**. It produces reviewable OBJ / glTF outputs, debug masks, depth images, reports, and artifacts rather than hiding the result behind an opaque model.

---

## Problem / user / output

| Item | Description |
|---|---|
| Problem | Early game-asset exploration often needs quick, reviewable 3D proxy output from 2D concept inputs. |
| Primary user | Technical Artist, Tools Programmer, prototype developer, asset pipeline reviewer. |
| Input | 2D character / prop image, optional depth input, optional learned-shape weights. |
| Output | Hero OBJ, material glTF, semantic vertex-color glTF, debug masks/depth images, Markdown/JSON reports, CI artifacts. |
| Safety goal | Show intermediate data and limitations clearly instead of claiming perfect hidden-geometry reconstruction. |

---

## Reviewer path

For the shortest review path, open:

```text
docs/REVIEWER_BRIEF.md
```

If using GitHub Actions artifacts, inspect:

```text
make3d-production-pipeline-sample
```

Recommended artifact review order:

```text
1. production_pipeline/OPEN_THIS_FIRST.txt
2. production_pipeline/output/hero/make3d_hero_character_vertex_color.gltf
3. production_pipeline/output/hero/make3d_hero_character_material.gltf
4. production_pipeline/output/hero/make3d_hero_character.obj
5. production_pipeline/output/production_report.md
6. production_pipeline/output/debug_mask_refined.ppm
7. production_pipeline/output/debug_depth_inferred.ppm
8. production_pipeline/output/debug_depth_learned.ppm
```

The standard production sample is intentionally focused on the hero path. Raw / polished / voxel fallback outputs are secondary and should not be the first reviewer experience.

---

## Current hero pipeline

```text
input character image
  -> foreground mask
  -> mask refinement
  -> pseudo-depth preparation
  -> shape inference
  -> local learned shape model
  -> hero character base mesh
      - head
      - torso
      - arms
      - legs
  -> hero detail enhancer
      - hair volume
      - clothing shell
      - neck / shoulder connectors
      - hands
      - feet
  -> hero fine detail pass
      - eyes
      - nose
      - mouth
      - hair strand hints
      - clothing folds
      - finger hints
      - shoe soles
  -> semantic palette extraction
      - skin
      - hair
      - face
      - clothing
      - lower clothing
      - shoes
  -> semantic vertex-color glTF
```

---

## Main outputs

Standard production sample output:

```text
OPEN_THIS_FIRST.txt
input_noisy_character.tga
output/hero/make3d_hero_character.obj
output/hero/make3d_hero_character_material.gltf
output/hero/make3d_hero_character_vertex_color.gltf
output/hero/make3d_hero_character_vertex_color.bin
output/debug_mask_refined.ppm
output/debug_depth_refined.ppm
output/debug_depth_inferred.ppm
output/debug_depth_learned.ppm
output/production_report.md
output/production_report.json
```

Recommended review order:

```text
1. OPEN_THIS_FIRST.txt
2. output/hero/make3d_hero_character_vertex_color.gltf
3. output/hero/make3d_hero_character_material.gltf
4. output/production_report.md
5. output/debug_depth_learned.ppm
```

---

## Implemented feature status

| Area | Status | Notes |
|---|---:|---|
| C++17 reconstruction backend | Implemented | `Make3DAdvancedCore`. |
| Win32 advanced GUI | Implemented | `Make3DAdvancedGui`. |
| CLI path | Implemented | `Make3DAdvancedCLI`. |
| Production pipeline | Implemented | Hero-only review sample / artifact path. |
| Foreground mask extraction | Implemented | Alpha / background estimation. |
| Mask refinement | Implemented | Component removal, hole fill, smoothing. |
| Pseudo-depth generation | Implemented | Silhouette / luminance / depth bias. |
| Shape inference | Implemented | Character / prop / flat classification and depth adjustment. |
| Local learned shape model | Implemented | Built-in weights plus load/save external weights. |
| Synthetic trainer | Implemented | Generates `learned_shape.weights` and training reports. |
| Hero character base mesh | Implemented | Head, torso, arms, legs. |
| Hero detail enhancer | Implemented | Hair volume, clothing shell, neck/shoulder, hands, feet. |
| Hero fine detail pass | Implemented | Eyes, nose, mouth, hair strand hints, clothing folds, finger hints, shoe soles. |
| Hero semantic glTF exporter | Implemented | `COLOR_0` semantic vertex colors for body-part coloring. |
| Hero-only regression guard | Implemented | Prevents fallback meshes from becoming the standard review output. |
| OBJ / glTF export | Implemented | Geometry, material, vertex-color, semantic hero glTF. |
| Markdown / JSON reports | Implemented | Production, reconstruction, mesh quality, and training reports. |
| CI workflow | Implemented | Build / test / package / sample / training artifact upload. |
| Preview/final mesh separation | Active work | Stage profiler branch separates interactive preview mesh from final export mesh. |
| Stage benchmark output | Active work | Planned output: `make3d_benchmark.json` / `make3d_benchmark.md`. |
| Blender/ZBrush-grade final quality | Not claimed | Needs true UVs, texture projection, retopology, and stronger models. |

---

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Important targets:

```text
Make3DAdvancedGui
Make3DAdvancedCLI
Make3DGenerateProductionPipelineSample
Make3DTrainLearnedShapeModel
Make3DHeroOnlyProductionTest
Make3DHeroCharacterModelTest
Make3DHeroSemanticGltfExporterTest
Make3DLearnedShapeModelTest
Make3DLearnedShapeTrainerTest
Make3DMeshQualityGateTest
```

---

## Generate the portfolio sample

```bash
Make3DGenerateProductionPipelineSample output/production_pipeline
```

With trained weights:

```bash
Make3DTrainLearnedShapeModel output/learned_shape_training
Make3DGenerateProductionPipelineSample output/production_pipeline output/learned_shape_training/learned_shape.weights
```

---

## GitHub Actions artifacts

The workflow builds, tests, generates training weights, generates samples, stages a portable package, and uploads artifacts.

Artifacts:

```text
make3d-advanced-portable-package
make3d-smoke-test-output
make3d-generated-advanced-samples
make3d-mesh-quality-report
make3d-learned-shape-training
make3d-production-pipeline-sample
```

The most important artifact for review is:

```text
make3d-production-pipeline-sample
```

---

## Technical highlights

- C++17 standalone reconstruction and character-generation backend.
- Deterministic, inspectable pipeline rather than opaque external service calls.
- Local learned-shape inference stage with weight save/load.
- Synthetic trainer for local `learned_shape.weights` generation.
- Character-specialized hero mesh path for stronger review output.
- Fine detail pass for face, hair strand hints, clothing folds, fingers, and shoe soles.
- Semantic hero glTF exporter with `COLOR_0` body-part coloring.
- Hero-only regression test to keep fallback meshes out of the main review artifact.
- Debug mask/depth images and Markdown/JSON reports.
- CI-generated artifacts for reproducible evaluation.

---

## Active performance work

The active engineering target is a profiled build path that separates preview mesh generation from final export mesh generation.

Target design:

```text
Preview button / interactive view
  -> lower grid resolution
  -> fewer radial segments
  -> fast previewMesh
  -> UI display only

Save / export
  -> full grid resolution
  -> full radial segments
  -> finalMesh
  -> OBJ / glTF export
  -> benchmark JSON / Markdown
```

Planned profiler stages:

```text
load_color
load_depth
foreground_mask
depth_prepare
preview_mesh
final_mesh
export_obj
export_gltf
write_reports
debug_images
total
```

Planned benchmark output:

```text
make3d_benchmark.json
make3d_benchmark.md
```

This exists to make the tool reviewable as an engineering project, not only as a visual demo.

---

## Current limitations

Make3D intentionally does **not** claim to perfectly reconstruct hidden 3D geometry from a single image.

Remaining high-end work:

```text
true UV unwrapping
texture projection
retopology / remeshing
better learned models
image-dependent pose and part detection
higher-quality hair and clothing geometry
Blender screenshot / turntable artifact generation
```

---

## Next improvements

### Short term

- Merge stage profiler / preview-final separation branch.
- Register and run benchmark smoke test in CMake.
- Route GUI preview through preview mesh generation.
- Route save/export through final mesh generation.
- Commit sample benchmark reports under `docs/reports/`.
- Add turntable screenshots or animated GIFs.

### Mid term

- Improve texture projection path.
- Add true UV unwrapping or export-friendly UV generation.
- Add more typed samples: prop, vehicle, machine, furniture.
- Add mesh quality thresholds to CI.
- Generate reviewer-friendly comparison pages.

### Long term

- Better local learned model quality.
- Character part detection.
- Retopology / remeshing pass.
- Integration with AssetUtility and Unity validation workflow.

---

## Portfolio wording

> Make3D is an inspectable C++17 image-to-3D asset generation pipeline. It converts a 2D character image into a hero-style OBJ / glTF asset using foreground extraction, mask refinement, pseudo-depth and local learned-shape style inference, character-specific geometric priors, semantic vertex coloring, debug images, reports, CI artifacts, and regression tests. The goal is not perfect reconstruction, but a reproducible tool pipeline for generating editable game-development proxy assets.

Japanese resume wording:

> C++17 / Win32で、2Dキャラクター画像からhero-style 3Dアセットを生成するMake3Dを開発。前景抽出、mask refinement、疑似depth、shape inference、local learned-shape inference、キャラクター専用の頭・胴体・腕・脚・髪・服・手足・顔パーツ生成、semantic vertex-color glTF出力、debug image、Markdown/JSON report、CI artifact生成、hero-only regression testまで実装し、生成過程と出力品質を検証可能にした。

Avoid these claims:

```text
perfect single-image 3D reconstruction
Blender/ZBrush replacement
production-ready automatic character sculpting
accurate hidden geometry recovery
```

Use these claims:

```text
Inspectable C++ image-to-3D pipeline
Game asset proxy generator
Hero character glTF generation path
Semantic vertex-color exporter
Debuggable reconstruction reports
Stage-profiler-ready asset pipeline
```
