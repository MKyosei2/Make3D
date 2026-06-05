# Make3D

**Make3D** は、画像・深度画像・動画フレームからゲーム開発向けの **3D proxy asset** を生成する C++ / Win32 製ツールです。

このプロジェクトは、単に画像を前後へ押し出すだけのデモではなく、前景抽出、mask refinement、疑似 depth 推定、relief / volume / hybrid mesh reconstruction、mesh cleanup / polish、OBJ / glTF / material glTF 出力、debug image、Markdown / JSON report までを持つ、検証可能な画像→3D生成パイプラインを目指しています。

> Note: Make3D は、任意の画像から完全な production-ready 3D モデルを復元する生成 AI ではありません。現在の主目的は、ゲーム制作の初期検証やプロキシアセット生成に使える deterministic / inspectable な 3D asset generation tool を C++ で実装することです。

---

## Reviewer: open this first

ポートフォリオ確認では、raw output ではなく **production pipeline artifact** を最初に見てください。

GitHub Actions artifact:

```text
make3d-production-pipeline-sample
```

最初に開くファイル:

```text
production_pipeline/output/polished/make3d_polished_material.gltf
```

次に確認するファイル:

```text
production_pipeline/output/debug_mask_refined.ppm
production_pipeline/output/debug_depth_refined.ppm
production_pipeline/output/production_report.md
```

この経路は、単純なraw reconstructionではなく、以下を通した出力です。

```text
input image
  ↓
foreground mask
  ↓
mask refinement
  ↓
depth preparation
  ↓
hybrid mesh reconstruction
  ↓
mesh cleanup
  ↓
component filtering
  ↓
Laplacian smoothing
  ↓
polished material glTF
  ↓
production report
```

詳しい出力物の読み方は `docs/OUTPUT_ARTIFACTS.md` を参照してください。

---

## Why this project exists

ゲーム開発では、企画初期やレベルブロックアウト、背景/小物/キャラクター案の検証時に、2D素材から素早く3D形状を起こしたい場面があります。

Make3D は以下の課題を扱います。

- 1枚の画像から簡易3D形状を作りたい
- alpha付きPNGや単純背景画像から前景だけを抽出したい
- depth画像がある場合は形状に反映したい
- depth画像がない場合でも疑似depthでプロキシ形状を作りたい
- 出力結果を Unity / Blender / Maya / DCC で確認したい
- 生成過程を debug mask / debug depth / report で説明できるようにしたい
- 生成meshをvalidation / cleanup / polishしてから確認したい
- GUIだけでなくCLI/CIで再現可能にしたい

---

## Current architecture

```text
Make3D
├─ MainApp.cpp
│  └─ Existing Win32 GUI and legacy reconstruction path
│
├─ src/Make3DAdvancedGui.cpp
│  └─ Lightweight Win32 GUI directly using the advanced backend
│
├─ src/Make3DAdvancedCore.h / .cpp
│  └─ Advanced reconstruction backend
│
├─ src/Make3DProductionPipeline.h / .cpp
│  └─ Refined production-style pipeline wrapper
│
├─ src/Make3DMaskRefiner.h / .cpp
│  └─ Foreground mask cleanup, hole fill, component filtering, smoothing
│
├─ src/Make3DModelPolisher.h / .cpp
│  └─ Mesh component filtering and Laplacian smoothing
│
├─ src/Make3DMeshTools.h / .cpp
│  └─ Mesh validation and cleanup tools
│
├─ src/Make3DGltfMaterialExporter.h / .cpp
│  └─ glTF exporter with material / PBR metadata support
│
├─ tools/GenerateAdvancedSamples.cpp
├─ tools/GenerateMeshQualityReport.cpp
├─ tools/GenerateProductionPipelineSample.cpp
│
├─ tests/
│  ├─ Make3DAdvancedCoreSmokeTest.cpp
│  ├─ Make3DMeshToolsTest.cpp
│  ├─ Make3DGltfMaterialExporterTest.cpp
│  ├─ Make3DModelPolisherTest.cpp
│  └─ Make3DMaskRefinerTest.cpp
│
└─ .github/workflows/make3d-advanced.yml
   └─ CI build, test, package, sample generation, quality report, production pipeline artifact
```

---

## Production pipeline

The production pipeline is the recommended output path for review.

```text
Color image
  ↓
BuildForegroundMask
  ↓
RefineForegroundMask
  ├─ small component removal
  ├─ largest component selection
  ├─ interior hole filling
  └─ jagged edge smoothing
  ↓
PrepareDepth
  ├─ provided depth PNG
  └─ single-image pseudo-depth estimation
  ↓
ReconstructMesh
  ├─ ReliefSurface
  ├─ SilhouetteVolume
  └─ HybridVolume
  ↓
PolishMesh
  ├─ cleanup
  ├─ small island / component filtering
  ├─ boundary-aware Laplacian smoothing
  └─ normal recomputation
  ↓
Export
  ├─ raw OBJ / material glTF
  ├─ polished OBJ / material glTF
  ├─ refined mask/depth debug images
  └─ production Markdown / JSON report
```

Run the production sample generator:

```bash
Make3DGenerateProductionPipelineSample output/production_pipeline
```

Important output:

```text
output/polished/make3d_polished_material.gltf
output/debug_mask_refined.ppm
output/debug_depth_refined.ppm
output/production_report.md
output/production_report.json
```

---

## Advanced reconstruction modes

| Mode | Purpose | Description |
|---|---|---|
| Auto | Default | 入力画像のmask/shapeから再構成方法を選ぶ |
| ReliefSurface | Surface detail | depth fieldからfront/back surfaceとboundary wallを生成 |
| SilhouetteVolume | Volumetric proxy | silhouetteのrow widthからring-based volume meshを生成 |
| HybridVolume | Stronger proxy asset | volume meshとrelief detailを組み合わせる |

`HybridVolume` は主力デモ用のモードです。volume mesh と relief detail を組み合わせ、単なる板状の押し出しよりもプロキシアセットとして見せやすい出力を狙います。

---

## Feature status

| Feature | Status | Notes |
|---|---:|---|
| Legacy Win32 GUI | Implemented | `MainApp.cpp` に既存GUIあり |
| Advanced Win32 GUI | Implemented | `Make3DAdvancedGui` target |
| Production pipeline wrapper | Implemented | mask refine → reconstruction → polish → material glTF → report |
| PNG / image loading | Implemented | `stb_image` 使用 |
| Video representative frame | Implemented | Media Foundation使用、既存GUI側 |
| Foreground mask | Implemented | alpha / background estimation |
| Mask refinement | Implemented | component removal, hole fill, jagged edge smoothing |
| Single-image pseudo-depth | Implemented | silhouette distance + luminance + vertical bias |
| Provided depth image | Implemented | optional depth PNG |
| Relief mesh | Implemented | front/back depth surface + boundary wall |
| Silhouette volume | Implemented | row-width based radial volume |
| Hybrid volume | Implemented | volume + relief |
| Mesh validation | Implemented | invalid triangle, degenerate triangle, boundary edge, non-manifold edge checks |
| Mesh cleanup | Implemented | invalid/degenerate triangle removal and unused vertex compaction |
| Mesh polish | Implemented | component filtering and Laplacian smoothing |
| OBJ / MTL export | Implemented | geometry export |
| geometry glTF export | Implemented | geometry + external binary buffer |
| material glTF export | Implemented | material, baseColorFactor, roughness, metallic, doubleSided, optional texture URI |
| Debug mask/depth images | Implemented | PPM output |
| Markdown / JSON reports | Implemented | reconstruction, quality, and production reports |
| CLI | Implemented | `Make3DAdvancedCLI` |
| CI | Implemented | Windows build/test/package/sample/quality/production pipeline generation |
| Real-world validation | In progress | real sample set and screenshots are still needed |

---

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

Build targets:

```text
Make3DAdvancedGui
Make3DAdvancedCLI
Make3DGenerateAdvancedSamples
Make3DGenerateMeshQualityReport
Make3DGenerateProductionPipelineSample
Make3DAdvancedCoreSmokeTest
Make3DMeshToolsTest
Make3DGltfMaterialExporterTest
Make3DModelPolisherTest
Make3DMaskRefinerTest
```

Run tests:

```bash
ctest --test-dir build -C Release --output-on-failure
```

---

## Advanced GUI usage

After building on Windows, run:

```text
build/Release/Make3DAdvancedGui.exe
```

Workflow:

1. Choose a color image.
2. Optionally choose a depth image.
3. Choose output folder.
4. Select reconstruction mode: Auto / Relief Surface / Silhouette Volume / Hybrid Volume.
5. Select quality: Draft / Standard / Detailed.
6. Click `Build Advanced 3D`.
7. Open the output folder and inspect OBJ, geometry glTF, material glTF, debug images, and reports.

---

## CLI usage

```bash
Make3DAdvancedCLI --input samples/input.png --output output --mode hybrid --quality detailed
```

With depth:

```bash
Make3DAdvancedCLI --input samples/input.png --depth samples/depth.png --output output --mode hybrid --quality detailed
```

Typical output files:

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

---

## Mesh quality report

```bash
Make3DGenerateMeshQualityReport output/mesh_quality
```

This tool generates a synthetic input, reconstructs a raw mesh, validates it, cleans it, polishes it, and writes geometry/material glTF outputs.

Output structure:

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

---

## glTF material export

`Make3DGltfMaterialExporter` writes glTF 2.0 files with:

- `materials`
- `pbrMetallicRoughness`
- `baseColorFactor`
- `metallicFactor`
- `roughnessFactor`
- `doubleSided`
- optional `baseColorTexture`
- optional `images` and `textures`

This makes generated assets easier to inspect in Blender, Unity, and glTF viewers than geometry-only output.

---

## GitHub Actions

The workflow at `.github/workflows/make3d-advanced.yml` runs:

```text
checkout
cmake configure
cmake build
ctest
sample generation
mesh quality report generation
production pipeline sample generation
portable package staging
artifact upload
```

Artifacts:

- `make3d-advanced-portable-package`
- `make3d-smoke-test-output`
- `make3d-generated-advanced-samples`
- `make3d-mesh-quality-report`
- `make3d-production-pipeline-sample`

---

## Technical highlights

- C++17 standalone reconstruction backend
- Lightweight Win32 GUI for the advanced backend
- Production pipeline wrapper for refined review output
- Deterministic foreground extraction
- Mask refinement before reconstruction
- Single-image pseudo-depth estimation
- Volume reconstruction from silhouette row profiles
- Hybrid volume + relief generation
- Mesh validation, cleanup, component filtering, and smoothing
- OBJ and geometry glTF export
- material glTF export with PBR metadata
- Markdown/JSON reconstruction, quality, and production reports
- CMake build and CTest smoke/integration tests
- GitHub Actions build/test/sample/package pipeline

---

## Limitations

Make3D is not yet a full production 3D reconstruction system.

Current limitations:

- Single-image reconstruction cannot infer hidden backside details.
- Texture image generation/copying is still limited; material glTF currently supports material factors and optional texture URI.
- Real-world photo backgrounds may require better segmentation.
- The original `MainApp.cpp` GUI still exists as a legacy path; the new `Make3DAdvancedGui` and production pipeline are the current review paths.
- Video workflow still uses representative-frame logic in the legacy GUI path.
- Automated tests are smoke/integration tests, not full geometry correctness tests.
- SDF / voxel / marching-cubes style reconstruction is still a future improvement.

These limitations are intentionally documented to keep the portfolio claim defensible.

---

## Portfolio positioning

Strong wording:

> Make3D is a deterministic C++ image-to-3D proxy asset generator. It refines foreground masks, estimates or consumes depth, reconstructs relief / volume / hybrid meshes, polishes mesh outputs, exports OBJ and material glTF, and generates debug artifacts plus production reports so the generation process can be inspected.

Avoid wording:

> Make3D can generate perfect production 3D models from any image.

The second claim is too broad. The project is strongest when presented as an inspectable game-asset proxy generation pipeline.

---

## Next roadmap

1. Add real image samples and generated outputs to the repository or Releases.
2. Add README screenshots/GIFs.
3. Add texture image copying into the material glTF output folder.
4. Add stronger mesh cleanup: duplicate merge, manifold repair, decimation.
5. Add SDF / voxel / marching-cubes style reconstruction.
6. Add multi-frame video sampling.
7. Add more tests for depth, mesh integrity, and export correctness.
