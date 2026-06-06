# Make3D

**Make3D** は、2D のキャラクター画像やプロップ画像から、ゲーム開発用の確認可能な proxy / hero asset を生成するための **C++17 / Win32 asset generation pipeline** です。

このリポジトリは、**Technical Artist / Tools Programmer 向けのポートフォリオ作品** として設計しています。目的は、単一画像から完璧な 3D モデルを自動生成できると主張することではありません。前景抽出、mask refinement、疑似 depth、shape inference、local learned-shape inference、geometry generation、semantic coloring、report、debug image、regression test、CI artifact を含む、検証可能な local pipeline を示すことです。

> スコープ注記: Make3D は Blender、ZBrush、photogrammetry、neural reconstruction の代替ではありません。主張できる範囲は、deterministic output と reviewable intermediate data を持つ、inspectable な C++ game-asset generation pipeline です。

---

## ポートフォリオ要約

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

現在もっとも強く見せられる path は、**hero character / game asset review pipeline** です。出力結果だけでなく、debug mask、depth image、Markdown / JSON report、CI artifact を確認できるようにしています。

---

## 課題 / 利用者 / 出力

| 項目 | 内容 |
|---|---|
| 課題 | 初期の game asset exploration では、2D concept input から素早く確認できる 3D proxy output が必要になることがあります。 |
| 主な利用者 | Technical Artist、Tools Programmer、Prototype Developer、Asset Pipeline Reviewer。 |
| 入力 | 2D character / prop image、optional depth input、optional learned-shape weights。 |
| 出力 | Hero OBJ、material glTF、semantic vertex-color glTF、debug masks / depth images、Markdown / JSON reports、CI artifacts。 |
| 安全性の方針 | 完璧な自動生成を主張せず、intermediate data と limitations を明示します。 |

---

## レビュー手順

最短で確認する場合は、まず以下を開きます。

```text
docs/REVIEWER_BRIEF.md
```

GitHub Actions artifacts を使う場合は、以下を確認します。

```text
make3d-production-pipeline-sample
```

推奨確認順:

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

standard production sample は、意図的に hero path に集中しています。fallback output は secondary であり、最初の reviewer experience として見せる対象ではありません。

---

## 現在の hero pipeline

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

## 主な出力

standard production sample output:

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

---

## 実装済み機能

| 領域 | 状態 | 内容 |
|---|---:|---|
| C++17 backend | 実装済み | `Make3DAdvancedCore`。 |
| Win32 GUI | 実装済み | `Make3DAdvancedGui`。 |
| CLI path | 実装済み | `Make3DAdvancedCLI`。 |
| Production pipeline | 実装済み | hero-only review sample / artifact path。 |
| Foreground mask extraction | 実装済み | alpha / background estimation。 |
| Mask refinement | 実装済み | component removal、hole fill、smoothing。 |
| Pseudo-depth generation | 実装済み | silhouette / luminance / depth bias。 |
| Shape inference | 実装済み | character / prop / flat classification と depth adjustment。 |
| Local learned shape model | 実装済み | built-in weights と external weights の load / save。 |
| Synthetic trainer | 実装済み | `learned_shape.weights` と training reports を生成。 |
| Hero character base mesh | 実装済み | head、torso、arms、legs。 |
| Hero detail enhancer | 実装済み | hair volume、clothing shell、neck / shoulder、hands、feet。 |
| Hero fine detail pass | 実装済み | face、hair、clothing、fingers、shoes の detail hints。 |
| Semantic glTF exporter | 実装済み | `COLOR_0` semantic vertex colors。 |
| Regression guard | 実装済み | fallback mesh が main review artifact にならないようにします。 |
| OBJ / glTF export | 実装済み | geometry、material、vertex-color glTF。 |
| Markdown / JSON reports | 実装済み | production、reconstruction、mesh quality、training reports。 |
| CI workflow | 実装済み | build / test / package / sample / training artifact upload。 |
| Preview / final mesh separation | 作業中 | interactive preview mesh と final export mesh を分離します。 |
| Stage benchmark output | 作業中 | `make3d_benchmark.json` / `make3d_benchmark.md` を予定。 |
| Final quality | 主張しない | true UV、texture projection、retopology などが今後の課題です。 |

---

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

重要な target:

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

## Portfolio sample の生成

```bash
Make3DGenerateProductionPipelineSample output/production_pipeline
```

trained weights を使う場合:

```bash
Make3DTrainLearnedShapeModel output/learned_shape_training
Make3DGenerateProductionPipelineSample output/production_pipeline output/learned_shape_training/learned_shape.weights
```

---

## GitHub Actions artifacts

workflow は build、test、training weights generation、sample generation、portable package staging、artifact upload を行います。

```text
make3d-advanced-portable-package
make3d-smoke-test-output
make3d-generated-advanced-samples
make3d-mesh-quality-report
make3d-learned-shape-training
make3d-production-pipeline-sample
```

review で最も重要な artifact:

```text
make3d-production-pipeline-sample
```

---

## Technical highlights

- C++17 standalone backend。
- external service call に依存しない deterministic / inspectable pipeline。
- weight save / load を持つ local learned-shape inference stage。
- local `learned_shape.weights` generation 用の synthetic trainer。
- review output を強くする character-specialized hero mesh path。
- semantic hero glTF exporter と `COLOR_0` vertex coloring。
- main review artifact を固定する regression test。
- debug mask / depth images と Markdown / JSON reports。
- 再現性のある評価用 CI-generated artifacts。

---

## Active performance work

現在の engineering target は、preview mesh generation と final export mesh generation を分ける profiled build path です。

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

---

## 現在の制限

Make3D は、単一画像から完全な 3D モデルを自動生成できるとは主張しません。

残っている high-end work:

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

## 次の改善

### Short term

- stage profiler / preview-final separation branch を merge する。
- benchmark smoke test を CMake に登録して実行する。
- GUI preview を preview mesh generation に通す。
- save / export を final mesh generation に通す。
- sample benchmark reports を `docs/reports/` に commit する。
- turntable screenshots または animated GIF を追加する。

### Mid term

- texture projection path を改善する。
- true UV unwrapping または export-friendly UV generation を追加する。
- prop、vehicle、machine、furniture など typed samples を増やす。
- mesh quality thresholds を CI に追加する。
- reviewer-friendly comparison pages を生成する。

### Long term

- local learned model quality を改善する。
- character part detection を追加する。
- retopology / remeshing pass を追加する。
- AssetUtility と Unity validation workflow へ integration する。

---

## ポートフォリオ用説明文

> C++17 / Win32 で、2D キャラクター画像から hero-style OBJ / glTF asset を生成する inspectable な asset generation pipeline を開発しました。foreground extraction、mask refinement、pseudo-depth、local learned-shape inference、character-specific geometric priors、semantic vertex coloring、debug images、reports、CI artifacts、regression tests を実装し、完璧な自動生成ではなく、editable game-development proxy asset を生成する再現可能な tool pipeline として設計しています。

日本語履歴書向け:

> C++17 / Win32 で、2D キャラクター画像から hero-style 3D アセットを生成する Make3D を開発。前景抽出、mask refinement、疑似 depth、shape inference、local learned-shape inference、キャラクター専用 geometry、semantic vertex-color glTF 出力、debug image、Markdown / JSON report、CI artifact 生成、hero-only regression test まで実装し、生成過程と出力品質を検証可能にした。

避けるべき主張:

```text
perfect single-image 3D reconstruction
Blender/ZBrush replacement
production-ready automatic character sculpting
accurate automatic reconstruction for every image
```

使うべき主張:

```text
Inspectable C++ asset generation pipeline
Game asset proxy generator
Hero character glTF generation path
Semantic vertex-color exporter
Debuggable reconstruction reports
Stage-profiler-ready asset pipeline
```
