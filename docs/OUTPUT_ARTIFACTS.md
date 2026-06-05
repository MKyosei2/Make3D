# Make3D Output Artifacts

This document explains the files produced by the Make3D production pipeline.

The current recommended review path is **hero-only review mode**. This mode intentionally skips the older raw / polished / voxel fallback meshes so reviewers do not accidentally open a broken generic reconstruction first.

---

## Recommended review path

For portfolio review, inspect this artifact first:

```text
make3d-production-pipeline-sample
```

Open the guide first:

```text
production_pipeline/OPEN_THIS_FIRST.txt
```

Then open this glTF:

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

`hero/make3d_hero_character_vertex_color.gltf` is the only primary review target. It includes the character-specialized mesh, detail volumes, fine detail hints, and semantic `COLOR_0` vertex coloring.

---

## Production pipeline output in hero-only review mode

The standard production sample writes:

```text
OPEN_THIS_FIRST.txt
input_noisy_character.tga
output/hero/make3d_hero_character.obj
output/hero/make3d_hero_character_material.gltf
output/hero/make3d_hero_character_material.bin
output/hero/make3d_hero_character_vertex_color.gltf
output/hero/make3d_hero_character_vertex_color.bin
output/debug_mask_refined.ppm
output/debug_depth_refined.ppm
output/debug_depth_inferred.ppm
output/debug_depth_learned.ppm
output/production_report.md
output/production_report.json
```

The standard sample intentionally does **not** generate:

```text
output/raw/*
output/polished/*
output/voxel/*
```

Those fallback paths are still useful for engineering tests, but they are not the portfolio review target and can look broken on hard inputs.

---

## Production file guide

| File | Purpose |
|---|---|
| `OPEN_THIS_FIRST.txt` | Explicit review guide. Start here before opening any mesh. |
| `input_noisy_character.tga` | Synthetic noisy character input used to demonstrate mask refinement and cleanup. |
| `output/hero/make3d_hero_character.obj` | Character-specialized hero mesh with head, torso, limbs, hair, clothing, hands, feet, face hints, hair strand hints, clothing folds, finger hints, and shoe soles. |
| `output/hero/make3d_hero_character_material.gltf` | Hero mesh with material metadata for viewers with limited vertex color support. |
| `output/hero/make3d_hero_character_vertex_color.gltf` | Preferred review output. Semantic `COLOR_0` glTF with skin, hair, face, clothing, lower clothing, and shoe color regions. Open this first. |
| `output/debug_mask_refined.ppm` | Refined foreground mask. Confirms that small noisy regions were removed. |
| `output/debug_depth_refined.ppm` | Base depth preview generated from the refined mask. |
| `output/debug_depth_inferred.ppm` | Shape-inference adjusted depth preview. |
| `output/debug_depth_learned.ppm` | Learned-shape depth preview used by the hero path. |
| `output/production_report.md` | Human-readable report combining shape inference, learned shape model, hero reconstruction, mask refinement, and review target path. |
| `output/production_report.json` | Machine-readable production report. |

---

## Hero output pipeline

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
  ↓
hero detail enhancer
  ↓
hero fine detail pass
  ↓
semantic palette extraction
  ↓
hero semantic vertex-color glTF
```

The hero path is intentionally specialized. It is designed to avoid broken generic image extrusion output and make the sample look more like a character/figure asset.

---

## Learned shape training output

The training tool writes:

```text
learned_shape.weights
learned_shape_training_report.md
learned_shape_training_report.json
```

In CI, these are uploaded as:

```text
make3d-learned-shape-training
```

The production sample can consume the generated weights:

```bash
Make3DGenerateProductionPipelineSample output/production_pipeline output/learned_shape_training/learned_shape.weights
```

---

## Advanced build output

A normal advanced CLI build can still write generic reconstruction files:

```text
make3d_advanced.obj
make3d_advanced.mtl
make3d_advanced.gltf
make3d_advanced.bin
make3d_advanced_material.gltf
make3d_advanced_material.bin
debug_mask.ppm
debug_depth.ppm
make3d_report.md
make3d_report.json
```

The advanced CLI path remains useful for testing raw reconstruction behavior. For portfolio review, prefer the hero-only production output.

---

## Mesh quality output

The mesh quality generator writes:

```text
mesh_quality_report.md
mesh_quality_report.json
quality_input.ppm
raw/make3d_advanced.obj
raw/make3d_advanced.gltf
cleaned/make3d_cleaned.obj
cleaned/make3d_cleaned.gltf
cleaned/make3d_cleaned_material.gltf
polished/make3d_polished.obj
polished/make3d_polished_material.gltf
```

This is useful for engineering review because it shows Make3D does not only generate a mesh; it also inspects, cleans, and reports mesh quality.

---

## Suggested portfolio explanation

> The current Make3D production artifact uses hero-only review mode. It skips broken-looking generic fallback meshes and exposes a single primary review target: a hero character semantic vertex-color glTF generated through foreground refinement, shape/depth inference, local learned-shape support, character-specific geometry priors, hair/clothing/hands/feet details, fine facial and clothing hints, debug depth images, and Markdown/JSON reports.
