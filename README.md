# Make3D

**Make3D** は、画像・深度画像・動画フレームからゲーム開発向けの **3D proxy asset** を生成する C++ / Win32 製ツールです。

このプロジェクトは、単に画像を前後へ押し出すだけのデモではなく、前景抽出、疑似 depth 推定、relief / volume / hybrid mesh reconstruction、OBJ / glTF 出力、debug image、Markdown / JSON report までを持つ、検証可能な画像→3D生成パイプラインを目指しています。

> Note: Make3D は、任意の画像から完全な production-ready 3D モデルを復元する生成 AI ではありません。現在の主目的は、ゲーム制作の初期検証やプロキシアセット生成に使える deterministic / inspectable な 3D asset generation tool を C++ で実装することです。

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
- GUIだけでなくCLI/CIで再現可能にしたい

---

## Current architecture

```text
Make3D
├─ MainApp.cpp
│  └─ Existing Win32 GUI and legacy reconstruction path
│
├─ src/Make3DAdvancedCore.h
├─ src/Make3DAdvancedCore.cpp
│  └─ Advanced reconstruction backend
│
├─ src/Make3DAdvancedCLI.cpp
│  └─ Command-line validation path
│
├─ src/Make3DGuiAdapter.h
├─ src/Make3DGuiAdapter.cpp
│  └─ Adapter for wiring the advanced backend into the GUI
│
├─ tools/GenerateAdvancedSamples.cpp
│  └─ Synthetic sample generator
│
├─ tests/Make3DAdvancedCoreSmokeTest.cpp
│  └─ Smoke test for mask/depth/mesh/export pipeline
│
└─ .github/workflows/make3d-advanced.yml
   └─ CI build, test, and sample generation
```

---

## Advanced reconstruction pipeline

```text
Color image
  ↓
LoadImageRGBA
  ↓
BuildForegroundMask
  ├─ alpha mask
  └─ background color estimation
  ↓
Morphological mask cleanup
  ↓
PrepareDepth
  ├─ provided depth PNG
  └─ single-image pseudo-depth estimation
  ↓
Depth smoothing
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
Export
  ├─ OBJ / MTL
  ├─ glTF + .bin
  ├─ debug_mask.ppm
  ├─ debug_depth.ppm
  ├─ make3d_report.md
  └─ make3d_report.json
```

---

## Reconstruction modes

| Mode | Purpose | Description |
|---|---|---|
| Auto | Default | 入力画像のmask/shapeから再構成方法を選ぶ |
| ReliefSurface | Surface detail | depth fieldからfront/back surfaceとboundary wallを生成 |
| SilhouetteVolume | Volumetric proxy | silhouetteのrow widthからring-based volume meshを生成 |
| HybridVolume | Stronger proxy asset | volume meshとrelief detailを組み合わせる |

### ReliefSurface

front/back depth surface を生成し、境界にwallを作ります。従来の単純な押し出しに近い用途も扱えますが、front surface は depth field によって変形します。

### SilhouetteVolume

シルエットの横幅を高さ方向にサンプリングし、複数のringから立体的なvolumeを作ります。単なる板状メッシュよりも、複数角度から見たときにプロキシアセットとして使いやすくなります。

### HybridVolume

SilhouetteVolume と ReliefSurface を組み合わせます。単画像からでも「体積」と「前面の起伏」を両方持たせるためのモードです。

---

## Feature status

| Feature | Status | Notes |
|---|---:|---|
| Win32 GUI | Implemented | `MainApp.cpp` に既存GUIあり |
| PNG / image loading | Implemented | `stb_image` 使用 |
| Video representative frame | Implemented | Media Foundation使用、既存GUI側 |
| Foreground mask | Implemented | alpha / background estimation |
| Mask cleanup | Implemented | morphological close |
| Single-image pseudo-depth | Implemented | silhouette distance + luminance + vertical bias |
| Provided depth image | Implemented | optional depth PNG |
| Relief mesh | Implemented | front/back depth surface + boundary wall |
| Silhouette volume | Implemented | row-width based radial volume |
| Hybrid volume | Implemented | volume + relief |
| OBJ / MTL export | Implemented | geometry export |
| glTF export | Implemented | geometry + external binary buffer |
| Debug mask/depth images | Implemented | PPM output |
| Markdown / JSON report | Implemented | mesh stats and warnings |
| CLI | Implemented | `Make3DAdvancedCLI` |
| Smoke test | Implemented | synthetic image → mesh → OBJ/glTF |
| Sample generator | Implemented | synthetic character / box samples |
| CI | Implemented | Windows build/test/sample generation |
| Full GUI integration | In progress | adapter added; direct `MainApp.cpp` connection is next |
| Texture/material glTF export | Partial | geometry buffer export is implemented; material texture expansion remains |
| Production validation | In progress | real sample set and screenshots are still needed |

---

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run tests:

```bash
ctest --test-dir build -C Release --output-on-failure
```

---

## CLI usage

```bash
Make3DAdvancedCLI --input samples/input.png --output output --mode hybrid --quality detailed
```

With depth:

```bash
Make3DAdvancedCLI --input samples/input.png --depth samples/depth.png --output output --mode hybrid --quality detailed
```

Options:

```text
--mode auto|relief|volume|hybrid
--quality draft|standard|detailed
--grid <number>
--segments <number>
--no-gltf
--no-obj
--no-debug
```

---

## Generate samples

```bash
Make3DGenerateAdvancedSamples output/generated_samples
```

Generated sample folders contain:

```text
input.ppm
output/make3d_advanced.obj
output/make3d_advanced.mtl
output/make3d_advanced.gltf
output/make3d_advanced.bin
output/debug_mask.ppm
output/debug_depth.ppm
output/make3d_report.md
output/make3d_report.json
```

CI uploads these generated samples as an artifact.

---

## Report output

Each advanced build writes a Markdown and JSON report.

Tracked metrics include:

- image width / height
- foreground pixel count
- foreground coverage
- depth min / max / mean
- reconstruction mode
- vertex count
- triangle count
- whether provided depth was used
- whether the mesh is a watertight candidate
- warnings

This is important for portfolio use because the generation process becomes inspectable instead of relying only on screenshots.

---

## GitHub Actions

The workflow at `.github/workflows/make3d-advanced.yml` runs:

```text
checkout
cmake configure
cmake build
ctest
sample generation
artifact upload
```

Artifacts:

- `make3d-smoke-test-output`
- `make3d-generated-advanced-samples`

---

## GUI integration status

The old Win32 GUI remains in `MainApp.cpp`. The advanced backend is not hard-wired into every GUI path yet, but the adapter is already added:

```text
src/Make3DGuiAdapter.h
src/Make3DGuiAdapter.cpp
```

The adapter maps GUI selections into advanced backend options:

| Existing GUI selection | Advanced backend |
|---|---|
| Auto | Auto |
| Relief | ReliefSurface |
| PrimitiveBox | SilhouetteVolume |
| HumanoidProxy | HybridVolume |
| Easy | Draft |
| Recommended | Standard |
| Detailed | Detailed |

Next step is to update `MainApp.cpp` so the build button can choose the advanced backend directly.

---

## Technical highlights

- C++17 standalone reconstruction backend
- Namespaced core types to avoid collision with legacy GUI types
- Deterministic foreground extraction
- Single-image pseudo-depth estimation
- Volume reconstruction from silhouette row profiles
- Hybrid volume + relief generation
- OBJ and glTF geometry export
- Markdown/JSON validation report
- CMake build and CTest smoke test
- GitHub Actions build/test/sample generation

---

## Limitations

Make3D is not yet a full production 3D reconstruction system.

Current limitations:

- Single-image reconstruction cannot infer hidden backside details.
- glTF export currently focuses on geometry and external binary buffer; texture/material expansion remains.
- Real-world photo backgrounds may require better segmentation.
- Existing GUI is still partly using the legacy reconstruction path.
- Video workflow still uses representative-frame logic in the legacy GUI path.
- Automated tests are smoke tests, not full geometry correctness tests.

These limitations are intentionally documented to keep the portfolio claim defensible.

---

## Portfolio positioning

Strong wording:

> Make3D is a deterministic C++ image-to-3D proxy asset generator. It extracts foreground regions, estimates or consumes depth, reconstructs relief / volume / hybrid meshes, exports OBJ and glTF, and writes debug artifacts and reports so the generation process can be inspected.

Avoid wording:

> Make3D can generate perfect production 3D models from any image.

The second claim is too broad. The project is strongest when presented as an inspectable game-asset proxy generation pipeline.

---

## Next roadmap

1. Fully wire the advanced backend into `MainApp.cpp`.
2. Add real image samples and generated outputs to the repository or Releases.
3. Add README screenshots/GIFs.
4. Expand glTF material and texture export.
5. Add mesh cleanup: duplicate merge, manifold checks, decimation.
6. Add multi-frame video sampling.
7. Add more tests for depth, mesh integrity, and export correctness.
