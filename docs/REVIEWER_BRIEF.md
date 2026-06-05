# Make3D Reviewer Brief

## Claim

Make3D is an inspectable C++17 / Win32 hero-style character asset generation pipeline. It is not presented as a finished ZBrush replacement. The defensible claim is that the project implements a reproducible, local, staged pipeline for generating and reviewing a character-style glTF asset from a 2D input.

## Open this first

From the CI artifact:

```text
make3d-production-pipeline-sample
```

Open:

```text
production_pipeline/OPEN_THIS_FIRST.txt
production_pipeline/output/hero/make3d_hero_character_vertex_color.gltf
```

The standard production sample runs in **hero-only review mode**. It intentionally does not emit raw / polished / voxel fallback meshes as review artifacts, because those generic reconstruction paths can look broken and are not the portfolio target.

## What the pipeline does

```text
2D character image
  -> foreground extraction
  -> mask refinement
  -> pseudo-depth preparation
  -> shape inference
  -> local learned-shape inference
  -> hero character base mesh
  -> hair / clothing / hand / foot detail volumes
  -> face / hair strand / clothing fold / finger / shoe detail hints
  -> semantic palette extraction
  -> COLOR_0 semantic vertex-color glTF
  -> debug images and Markdown / JSON reports
```

## Implemented proof points

| Area | Evidence |
|---|---|
| Local C++ backend | `src/Make3DAdvancedCore.*` |
| Production wrapper | `src/Make3DProductionPipeline.*` |
| Hero character geometry | `src/Make3DHeroCharacterModel.*` |
| Hero detail volumes | `src/Make3DHeroDetailEnhancer.*` |
| Fine detail hints | `src/Make3DHeroFineDetailPass.*` |
| Semantic glTF coloring | `src/Make3DHeroSemanticGltfExporter.*` |
| Shape inference | `src/Make3DShapeInference.*` |
| Local learned model | `src/Make3DLearnedShapeModel.*` |
| Synthetic trainer | `src/Make3DLearnedShapeTrainer.*`, `tools/TrainLearnedShapeModel.cpp` |
| Hero-only regression guard | `tests/Make3DHeroOnlyProductionTest.cpp` |
| CI artifact generation | `.github/workflows/make3d-advanced.yml` |

## Review target behavior

The regression test `Make3DHeroOnlyProductionTest` verifies that hero-only mode:

- generates hero OBJ;
- generates hero material glTF;
- generates hero semantic vertex-color glTF;
- includes `COLOR_0` in the hero glTF;
- writes the production report;
- does not create raw / polished / voxel fallback output directories.

This prevents the previous failure mode where a reviewer could open a broken generic reconstruction and judge that as the main output.

## What is intentionally not claimed

Make3D does not claim:

- perfect arbitrary single-image reconstruction;
- production-ready retopology;
- true UV unwrapping;
- real texture projection;
- ZBrush-quality sculpting;
- robust real-world pose / body-part estimation.

## Strong resume wording

Japanese:

> C++17/Win32で、2Dキャラクター画像からhero-style 3Dアセットを生成するMake3Dを開発。前景抽出、mask refinement、疑似depth、shape inference、local learned-shape inference、キャラクター専用の頭・胴体・腕・脚・髪・服・手足・顔パーツ生成、semantic vertex-color glTF出力、debug image、Markdown/JSON report、CI artifact生成、hero-only regression testまで実装し、生成過程と出力品質を検証可能にした。

English:

> Developed Make3D, a C++17/Win32 hero-style character asset generation pipeline from 2D inputs. Implemented foreground refinement, pseudo-depth estimation, shape inference, local learned-shape inference, character-specific geometry priors, hair/clothing/face/detail passes, semantic vertex-color glTF export, debug images, Markdown/JSON reports, CI artifact generation, and a hero-only regression test to keep review outputs focused and reproducible.

## Current next step

The next critical step is not adding more surface features. It is confirming CI/build success and inspecting the generated hero-only artifact in Blender or a glTF viewer.
