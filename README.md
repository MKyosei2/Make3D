# Make3D

**Make3D** is a C++17 / Win32 image-to-3D asset generation project focused on producing inspectable game-development 3D assets from 2D character images.

The current main direction is no longer generic reconstruction first. The primary portfolio path is now a **hero character generation pipeline**: foreground extraction, mask refinement, pseudo-depth / learned-depth style inference, character-specific body prior, hair / clothing / hand / foot detail volumes, fine facial and clothing detail hints, semantic vertex coloring, and glTF export.

> Honest scope: Make3D is not yet a Blender/ZBrush-grade fully automatic sculpting system. It is an inspectable C++ character-asset generation pipeline with a growing local shape inference / training path, deterministic reports, debug images, and CI-generated review artifacts.

---

## Reviewer: open this first

GitHub Actions artifact:

```text
make3d-production-pipeline-sample
```

Open this file first:

```text
production_pipeline/output/hero/make3d_hero_character_vertex_color.gltf
```

Then inspect:

```text
production_pipeline/output/hero/make3d_hero_character_material.gltf
production_pipeline/output/hero/make3d_hero_character.obj
production_pipeline/output/production_report.md
production_pipeline/output/debug_mask_refined.ppm
production_pipeline/output/debug_depth_inferred.ppm
production_pipeline/output/debug_depth_learned.ppm
```

The hero glTF is the strongest current output. It is generated through the character-specialized path, not the older generic polished/voxel path.

---

## Current hero pipeline

```text
input character image
  ↓
foreground mask
  ↓
mask refinement
  ↓
pseudo-depth preparation
  ↓
shape inference
  ↓
local learned shape model
  ↓
hero character base mesh
  ├─ head
  ├─ torso
  ├─ arms
  └─ legs
  ↓
hero detail enhancer
  ├─ hair volume
  ├─ clothing shell
  ├─ neck / shoulder connectors
  ├─ hands
  └─ feet
  ↓
hero fine detail pass
  ├─ eyes
  ├─ nose
  ├─ mouth
  ├─ hair strand hints
  ├─ clothing folds
  ├─ finger hints
  └─ shoe soles
  ↓
semantic palette extraction
  ├─ skin
  ├─ hair
  ├─ face
  ├─ clothing
  ├─ lower clothing
  └─ shoes
  ↓
semantic vertex-color glTF
```

---

## Main outputs

Production sample output:

```text
input_noisy_character.tga
output/hero/make3d_hero_character.obj
output/hero/make3d_hero_character_material.gltf
output/hero/make3d_hero_character_vertex_color.gltf
output/hero/make3d_hero_character_vertex_color.bin
output/raw/make3d_raw.obj
output/raw/make3d_raw_material.gltf
output/polished/make3d_polished.obj
output/polished/make3d_polished_material.gltf
output/polished/make3d_polished_vertex_color.gltf
output/voxel/make3d_voxel_volume.obj
output/voxel/make3d_voxel_volume_material.gltf
output/voxel/make3d_voxel_volume_vertex_color.gltf
output/debug_mask_refined.ppm
output/debug_depth_refined.ppm
output/debug_depth_inferred.ppm
output/debug_depth_learned.ppm
output/production_report.md
output/production_report.json
```

Recommended review order:

1. `output/hero/make3d_hero_character_vertex_color.gltf`
2. `output/hero/make3d_hero_character_material.gltf`
3. `output/production_report.md`
4. `output/debug_mask_refined.ppm`
5. `output/debug_depth_learned.ppm`

---

## Feature status

| Area | Status | Notes |
|---|---:|---|
| C++17 reconstruction backend | Implemented | `Make3DAdvancedCore` |
| Win32 advanced GUI | Implemented | `Make3DAdvancedGui` |
| CLI path | Implemented | `Make3DAdvancedCLI` |
| Production pipeline | Implemented | Refined sample/artifact path |
| Foreground mask extraction | Implemented | Alpha/background estimation |
| Mask refinement | Implemented | Component removal, hole fill, smoothing |
| Pseudo-depth generation | Implemented | Silhouette/luminance/depth bias |
| Shape inference | Implemented | Character/prop/flat classification and depth adjustment |
| Local learned shape model | Implemented | Built-in weights plus load/save external weights |
| Synthetic trainer | Implemented | Generates `learned_shape.weights` and training reports |
| Hero character base mesh | Implemented | Head, torso, arms, legs |
| Hero detail enhancer | Implemented | Hair volume, clothing shell, neck/shoulder, hands, feet |
| Hero fine detail pass | Implemented | Eyes, nose, mouth, hair strand hints, clothing folds, finger hints, shoe soles |
| Hero semantic glTF exporter | Implemented | `COLOR_0` semantic vertex colors for skin/hair/face/clothing/shoes |
| Voxel closed-volume output | Implemented | Secondary review path |
| Mesh polish | Implemented | Component filtering and smoothing |
| OBJ / glTF export | Implemented | Geometry, material, vertex-color, semantic hero glTF |
| Markdown / JSON reports | Implemented | Production, reconstruction, mesh quality, training reports |
| CI workflow | Implemented | Build/test/package/sample/training artifact upload |
| Blender/ZBrush-grade final quality | Not claimed | Still needs true UV, texture projection, retopology, higher-quality learned models |

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
Make3DHeroCharacterModelTest
Make3DHeroSemanticGltfExporterTest
Make3DLearnedShapeModelTest
Make3DLearnedShapeTrainerTest
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

The workflow at `.github/workflows/make3d-advanced.yml` builds, tests, generates training weights, generates samples, stages a portable package, and uploads artifacts.

Artifacts:

```text
make3d-advanced-portable-package
make3d-smoke-test-output
make3d-generated-advanced-samples
make3d-mesh-quality-report
make3d-learned-shape-training
make3d-production-pipeline-sample
```

The most important artifact is:

```text
make3d-production-pipeline-sample
```

---

## Technical highlights

- C++17 standalone reconstruction and character-generation backend.
- Deterministic, inspectable pipeline rather than opaque external AI calls.
- Local learned-shape inference stage with weight save/load.
- Synthetic trainer that can generate `learned_shape.weights` without an external service.
- Character-specialized hero mesh path for stronger portfolio output.
- Fine detail pass for face, hair strand hints, clothing folds, fingers, and shoe soles.
- Semantic hero glTF exporter with `COLOR_0` for body-part coloring.
- Debug mask/depth images and Markdown/JSON reports for review.
- CI-generated artifacts for reproducible evaluation.

---

## What is intentionally not claimed

Make3D does **not** claim to perfectly reconstruct arbitrary hidden geometry from a single image.

The defensible current claim is:

> Make3D is an inspectable C++ character-asset generation pipeline that converts a 2D character image into a hero-style 3D glTF asset with mask/depth inference, local learned-shape support, character-specific geometric priors, fine detail hints, semantic vertex coloring, and CI-generated review artifacts.

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
