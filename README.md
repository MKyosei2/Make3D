# Make3D

**Make3D** は、PNG 画像、深度画像、または動画フレームから簡易的な 3D メッシュを生成し、OBJ / MTL 形式で書き出す Windows 向けスタンドアロンアプリケーションです。

このプロジェクトは、画像から直接フォトリアルな 3D モデルを復元する AI ツールではありません。  
主眼は、**2D 入力から前景抽出・深度推定・形状分類・メッシュ生成・OBJ 書き出し・プレビュー表示までを C++ / Win32 で実装すること**です。

ゲーム会社向けのポートフォリオとしては、以下のような技術要素を示すことを目的としています。

- C++ によるネイティブ Windows アプリ開発
- Win32 API を使った GUI 実装
- 画像処理
- 動画フレーム抽出
- 深度画像処理
- 2D mask からの 3D mesh reconstruction
- OBJ / MTL exporter
- 簡易 software rendering preview
- ユーザー向けツールとしての workflow 設計

---

## 1. このツールが解決しようとしている課題

3D アセット制作では、簡単なプロトタイプやアイデア検証のために、画像から素早く 3D 形状を作りたい場面があります。

たとえば:

- 1 枚の画像から簡易的な relief mesh を作りたい
- 画像と深度画像のペアから OBJ を生成したい
- 動画の代表フレームから簡易 3D 化したい
- 生成結果を Unity / Blender / Maya / DCC ツールで確認したい
- GUI 付きの小さな Windows ツールとして扱いたい
- AI ではなく、処理過程が追える deterministic な簡易生成ツールを作りたい

Make3D は、こうした用途に対して、画像入力から OBJ 出力までを 1 つのアプリ内で完結させることを目指したツールです。

---

## 2. Concept

### From 2D input to inspectable 3D mesh

Make3D は、入力画像をそのまま black box 的に 3D 化するのではなく、以下の処理段階を明確に分けています。

```text
Input image / depth image / video
  ↓
Image loading
  ↓
Foreground mask generation
  ↓
Depth generation or depth fusion
  ↓
Depth cleanup / smoothing
  ↓
Shape analysis
  ↓
Reconstruction preset selection
  ↓
Mesh generation
  ↓
Normal recomputation / smoothing
  ↓
OBJ + MTL export
  ↓
Preview rendering
```

各段階が比較的分かりやすい関数に分かれており、将来的にアルゴリズムを差し替えやすい構成を目指しています。

### Lightweight native tool

Make3D は Unity Editor 拡張ではなく、Windows ネイティブアプリとして作っています。

- Win32 API による GUI
- stb_image による画像読み込み
- Media Foundation による動画フレーム取得
- C++ standard library による file / vector / filesystem 処理
- OBJ / MTL の直接書き出し

そのため、Unity や Python 環境がなくても単体で実行できるツールとして設計できます。

### Deterministic, explainable reconstruction

このツールは生成 AI ではありません。  
入力画像から foreground mask や depth を作り、手続き的に mesh を生成します。

そのため、以下のような利点があります。

- どの処理で形状が作られたか説明しやすい
- 失敗理由を UI に出しやすい
- 出力形式が単純で DCC に渡しやすい
- 将来的な検証・テストがしやすい

---

## 3. Current feature overview

| Feature | Status | Notes |
|---|---:|---|
| PNG color image input | Implemented | `stb_image` で RGBA として読み込み |
| PNG depth image input | Implemented | grayscale / first channel を depth として扱う |
| Single image pseudo-depth | Implemented | foreground mask から疑似 depth を生成 |
| Multi-image depth fusion | Implemented | 複数 depth を median-weighted に融合 |
| Video input | Implemented | Media Foundation で代表フレームを抽出 |
| Foreground mask | Implemented | alpha mask または背景推定から作成 |
| Depth cleanup | Implemented | hole filling と smoothing |
| Reconstruction presets | Implemented | Auto / Relief / Primitive Box / Humanoid Proxy |
| OBJ export | Implemented | positions / UV / normals / faces を出力 |
| MTL export | Implemented | texture reference 付き material を出力 |
| Texture copy / frame save | Implemented | PNG copy または PPM frame 保存 |
| Model preview | Implemented | Software rasterizer 風の簡易プレビュー |
| Drag & drop | Implemented | dropped file を color / depth / video に分類 |
| Async build flag | Implemented | build 中の UI 操作抑制 |
| Config file | Partial | `portable_config.ini` で output folder を指定可能 |
| Automated tests | Not yet | 今後追加予定 |
| Production validation | Not yet | sample / benchmark の追加が必要 |

---

## 4. Supported input workflows

Make3D には大きく 2 つの workflow があります。

```text
1. Image + optional Depth
2. Video
```

### 4.1 Image + optional Depth workflow

通常画像を入力し、必要に応じて対応する深度画像を追加します。

```text
Color PNG
  ↓
Foreground mask
  ↓
Pseudo depth or provided depth
  ↓
Mesh reconstruction
  ↓
OBJ / MTL export
```

深度画像がある場合:

```text
Color PNG + Depth PNG
  ↓
Validate dimensions
  ↓
Depth fusion
  ↓
Hole filling
  ↓
Smoothing
  ↓
Mesh generation
```

深度画像がない場合:

```text
Color PNG only
  ↓
Foreground mask
  ↓
Pseudo depth from mask distance
  ↓
Relief / primitive / humanoid proxy generation
```

### 4.2 Video workflow

動画 workflow では、Media Foundation を使って動画から代表フレームを取り出し、そのフレームから mask と depth を作ります。

```text
MP4 / MOV / AVI / WMV
  ↓
Representative frame extraction
  ↓
Foreground mask from frame
  ↓
Depth from luminance
  ↓
Mesh generation
  ↓
OBJ / MTL export
```

動画入力では、まず代表フレームを 1 枚取り出す方式です。  
複数フレームからの multi-view reconstruction ではありません。

---

## 5. Reconstruction presets

Make3D には複数の reconstruction mode があります。

| Preset | Description | Use case |
|---|---|---|
| Auto | shape analysis の結果から生成方法を選ぶ | 通常の利用 |
| Relief | depth / pseudo-depth から厚み付き relief mesh を作る | 画像を立体レリーフ化したい場合 |
| Primitive Box | mask の bounding box から box primitive を作る | 箱・正方形に近い対象 |
| Humanoid Proxy | 人型らしい silhouette から簡易 humanoid mesh を作る | キャラクターの proxy 生成 |

### Auto mode

Auto mode では、foreground mask と depth から `ShapeAnalysis` を行い、対象が box らしいか、人型らしいか、通常 relief として扱うべきかを推定します。

ShapeAnalysis で見ている主な特徴:

- bounding box
- foreground area
- rectangularity
- aspect ratio
- head width
- shoulder width
- waist width
- hip width
- lower gap ratio
- mean depth
- likely human flag
- likely box flag

この情報を使って、生成 preset を自動選択します。

---

## 6. Quality presets

出力品質は以下の preset で切り替えます。

| Preset | Purpose | Behavior |
|---|---|---|
| Easy | 軽量・高速 | smoothing 少なめ、depth scale 控えめ |
| Recommended | 標準 | smoothing と depth scale のバランス |
| Detailed | 詳細 | smoothing 多め、depth scale 強め |

内部的には、quality preset によって主に以下が変化します。

- depth smoothing pass count
- depth scale
- mesh smoothing iteration
- preview / output の見た目の安定性

---

## 7. Core data structures

### ImageRGBA

画像入力を RGBA として保持する構造体です。

```cpp
struct ImageRGBA {
    int width;
    int height;
    std::vector<unsigned char> pixels;
};
```

### DepthImage

深度画像を float 配列として保持します。

```cpp
struct DepthImage {
    int width;
    int height;
    std::vector<float> values;
};
```

### MeshData

生成された mesh を OBJ 出力可能な形で保持します。

```cpp
struct MeshData {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> uvs;
    std::vector<int> indices;
};
```

### ShapeAnalysis

foreground mask から推定した形状情報を保持します。

```cpp
struct ShapeAnalysis {
    BBox2i bbox;
    int area;
    float rectangularity;
    float aspect;
    float headWidth;
    float shoulderWidth;
    float waistWidth;
    float hipWidth;
    float lowerGapRatio;
    float meanDepth;
    bool likelyHuman;
    bool likelyBox;
};
```

---

## 8. Image loading

Make3D は `stb_image` を使って PNG を読み込みます。

### Color image

通常画像は RGBA として読み込まれます。

```text
PNG
  ↓
stbi_load(..., 4)
  ↓
ImageRGBA
```

Alpha channel がある場合、foreground mask に利用できます。

### Depth image

Depth PNG は最初の channel を 0.0 - 1.0 の depth として読みます。

```text
Depth PNG
  ↓
stbi_load(...)
  ↓
first channel / 255.0
  ↓
DepthImage.values
```

---

## 9. Foreground mask generation

Make3D は 3D 化対象を示す mask を作ります。

### Alpha-based mask

画像が alpha channel を持つ場合、alpha 値が一定以上の pixel を foreground として扱います。

### Background-estimation mask

alpha がない、または画像全体が opaque の場合、画像の border pixel から背景色を推定し、背景との差分が大きい pixel を foreground とします。

```text
Border pixels
  ↓
Estimate background color and variance
  ↓
Color distance threshold
  ↓
Foreground mask
  ↓
Small smoothing / cleanup
```

### Video frame mask

動画フレームでは、四隅の pixel を背景サンプルとして扱い、背景との差分から mask を作ります。

---

## 10. Pseudo-depth generation

深度画像がない場合でも、foreground mask から疑似 depth を作ります。

考え方:

```text
Foreground mask
  ↓
Detect boundary pixels
  ↓
Distance transform-like propagation
  ↓
Normalize distance to 0.0 - 1.0
  ↓
Apply curve
  ↓
Pseudo depth
```

これにより、輪郭付近は浅く、中心に近い領域ほど膨らむような relief depth を作れます。

---

## 11. Depth fusion and cleanup

複数の depth image が入力された場合、Make3D は pixel ごとに depth を融合します。

### Median-weighted fusion

各 pixel の depth samples を集め、median を基準に外れ値の影響を抑えながら weighted average を取ります。

```text
Depth samples per pixel
  ↓
Sort samples
  ↓
Median
  ↓
Weight by distance from median
  ↓
Fused depth
```

### Hole filling

mask 内で depth が欠けている pixel に対し、周囲の有効 depth から平均値を入れます。

### Edge-aware smoothing

近傍 pixel の depth 差を見ながら smoothing します。  
大きく depth が違う場合は weight を下げることで、急な段差が潰れすぎないようにしています。

---

## 12. Mesh generation

Make3D は用途に応じて mesh 生成方式を切り替えます。

### 12.1 Closed depth mesh

Relief 系の基本形です。

```text
Depth map + mask
  ↓
Front vertices
  ↓
Back vertices
  ↓
Front faces
  ↓
Back faces
  ↓
Side faces along mask boundary
```

特徴:

- 前面と背面を持つ
- mask 境界に側面を作る
- OBJ として閉じた形状に近い mesh を出せる
- depth map の形状を直接反映しやすい

### 12.2 Primitive box

対象が箱状・矩形状と判定される場合、mask の bounding box から box primitive を生成します。

特徴:

- 非常に軽量
- 安定して閉じた mesh が作れる
- 正方形・箱状の画像に向いている

### 12.3 Humanoid proxy

対象が人型 silhouette に近い場合、head / torso / limbs を primitive の組み合わせとして生成します。

使用する情報:

- head width
- shoulder width
- waist width
- hip width
- body height
- lower gap ratio
- mean depth

生成要素:

- ellipsoid head
- torso proxy
- arm cylinders
- leg cylinders

これは正確な人体復元ではなく、キャラクターや人型 silhouette の proxy model を作るための簡易生成です。

---

## 13. Mesh cleanup

生成 mesh は以下の処理で整えられます。

### Invalid triangle removal

存在しない vertex index や、同じ vertex を複数参照する degenerate triangle を除外します。

### Normal recomputation

triangle normal を各 vertex に加算し、normalize して vertex normals を作ります。

### Laplacian smoothing

Quality preset に応じて、mesh positions に Laplacian smoothing をかけます。

```text
Vertex neighbors
  ↓
Average neighbor position
  ↓
Interpolate by alpha
  ↓
Repeat iterations
```

これにより、depth 由来の jagged な形状を少し滑らかにします。

---

## 14. OBJ / MTL export

Make3D は生成 mesh を OBJ / MTL として書き出します。

出力内容:

- `v` positions
- `vt` UVs
- `vn` normals
- `f` triangle faces
- `mtllib`
- `usemtl`
- `.mtl` material
- `map_Kd` texture reference

画像 workflow では入力 color PNG を output folder にコピーし、MTL から参照します。  
動画 workflow では代表フレームを PPM として保存し、MTL から参照します。

出力例:

```text
output/
  model_auto.obj
  model_auto.mtl
  input.png
```

または:

```text
output/
  model_from_video.obj
  model_from_video.mtl
  video_frame.ppm
```

---

## 15. Preview renderer

Make3D は生成した mesh をアプリ内で確認するため、簡易的な preview renderer を持っています。

内部では:

- mesh positions を yaw / pitch で回転
- bounding box に合わせて screen scale
- triangle rasterization
- z-buffer
- simple directional lighting
- grayscale shading

を行い、Win32 の `HBITMAP` として preview 表示します。

これは DCC renderer ではありませんが、OBJ を開く前に「形が生成されたか」を確認するための lightweight preview です。

---

## 16. GUI / UX design

Make3D は Win32 API ベースの GUI アプリです。

主な UI 要素:

- workflow mode selection
- image / depth / video input buttons
- drag & drop support
- file list
- reconstruction preset combo box
- quality preset combo box
- output folder field
- build button
- open output button
- input preview
- model preview
- status text
- progress bar

### Friendly validation

入力が不足している場合、ユーザーに分かる日本語メッセージを表示します。

例:

- 通常画像を追加してください
- 深度画像を使う場合は枚数をそろえてください
- 動画ファイルを 1 本追加してください
- 前景を抽出できませんでした
- OBJ ファイルの書き出しに失敗しました

ツールとしての使いやすさを意識し、失敗時に silent fail しない設計を目指しています。

---

## 17. Build / runtime requirements

### Platform

- Windows
- Visual Studio / MSVC 想定
- C++17 以降推奨

### Native dependencies

Make3D は Windows API と Media Foundation を使用します。

使用している主な Windows components:

- Win32 API
- Common Controls
- Common Dialogs
- Shell API
- Media Foundation
- COM

リンク対象の例:

- `comctl32.lib`
- `shell32.lib`
- `mfplat.lib`
- `mfreadwrite.lib`
- `mfuuid.lib`
- `mf.lib`
- `ole32.lib`

### Third-party single-header dependency

- `stb_image.h`

`stb_image` は PNG などの画像読み込みに使っています。

---

## 18. Basic usage

1. アプリを起動する
2. workflow を選ぶ
   - 画像 + 深度画像
   - 動画
3. 入力を追加する
   - 通常画像 PNG
   - 深度画像 PNG
   - または動画ファイル
4. reconstruction preset を選ぶ
   - Auto
   - Relief
   - Primitive Box
   - Humanoid Proxy
5. quality preset を選ぶ
   - Easy
   - Recommended
   - Detailed
6. output folder を指定する
7. Build を押す
8. output folder に生成された OBJ / MTL / texture を確認する
9. 必要に応じて Blender / Unity / Maya などで開く

---

## 19. File classification

Drag & drop 時には、拡張子とファイル名から入力を自動分類します。

| File type | Handling |
|---|---|
| `.png` | color image or depth image |
| filename contains `depth` | depth image として扱う |
| filename contains `_d` | depth image として扱う |
| filename contains `z` | depth image として扱う |
| `.mp4` | video input |
| `.mov` | video input |
| `.avi` | video input |
| `.wmv` | video input |

---

## 20. Configuration

`portable_config.ini` を使って output folder の初期値を変えられます。

探索候補:

```text
config/portable_config.ini
portable_config.ini
```

設定例:

```ini
output_folder=output
```

---

## 21. Current limitations

現在の Make3D は研究・ポートフォリオ向けの簡易 3D 生成ツールです。

主な制限:

- AI ベースの単眼深度推定ではない
- 写真から正確な 3D 形状を復元するものではない
- 動画 workflow は代表フレーム 1 枚からの生成であり、multi-view reconstruction ではない
- foreground mask は背景が単純な画像ほど安定する
- alpha 付き PNG の方が前景抽出が安定する
- depth PNG がない場合は疑似 depth なので、形状精度は限定的
- humanoid proxy は silhouette 由来の簡易 proxy であり、rigged character ではない
- texture workflow は基本的な diffuse texture reference のみ
- output は OBJ / MTL 中心で、FBX / glTF には未対応
- 生成 mesh の polygon reduction / optimization は今後の拡張対象
- 現在は大きな single-file 実装であり、production では module 分割が必要

---

## 22. Recommended refactor plan

現在の実装は `MainApp.cpp` に機能が集約されています。  
production-level に近づける場合、以下のような module 分割が望ましいです。

```text
Make3D/
  src/
    App/
      Win32App.cpp
      MainWindow.cpp
      AppState.cpp

    IO/
      ImageLoader.cpp
      VideoFrameExtractor.cpp
      ObjExporter.cpp
      PortableConfig.cpp

    ImageProcessing/
      ForegroundMask.cpp
      DepthImage.cpp
      DepthFusion.cpp
      DepthSmoothing.cpp

    Reconstruction/
      ShapeAnalysis.cpp
      DepthMeshBuilder.cpp
      PrimitiveBuilder.cpp
      HumanoidProxyBuilder.cpp
      MeshCleanup.cpp

    Preview/
      SoftwarePreviewRenderer.cpp
      BitmapUtil.cpp

    Core/
      MeshData.h
      ImageTypes.h
      MathTypes.h
      BuildResult.h

  samples/
    input.png
    depth.png

  docs/
    Architecture.md
    ReconstructionPipeline.md
    Format.md

  tests/
    MaskTests.cpp
    DepthFusionTests.cpp
    ObjExportTests.cpp
```

### Refactor goals

- UI と処理を分離する
- image / video / mesh / export を独立させる
- unit test を追加しやすくする
- reconstruction algorithm を差し替え可能にする
- CLI mode を追加しやすくする
- build artifact を repository から分離する

---

## 23. Verification plan

ポートフォリオとして説得力を高めるため、以下の検証を追加予定です。

### Sample cases

| Sample | Purpose |
|---|---|
| alpha_png_simple | alpha mask から relief mesh 生成 |
| color_depth_pair | color + depth PNG から mesh 生成 |
| box_shape | Primitive Box auto detection |
| humanoid_silhouette | Humanoid Proxy auto detection |
| video_sample | representative frame から mesh 生成 |

### Output report

Build 後に以下を出力すると、技術的な説明力が上がります。

- input file list
- workflow mode
- reconstruction preset
- quality preset
- image resolution
- foreground mask pixel count
- depth min / max / mean
- generated vertex count
- generated triangle count
- output OBJ path
- processing time

### Visual comparison

README に以下を載せる予定です。

```text
Input image
Depth / pseudo-depth preview
Foreground mask preview
Generated mesh preview
OBJ opened in Blender / Unity
```

---

## 24. Roadmap

### Short term

- README に screenshot / GIF を追加
- sample input / output を追加
- output report を追加
- build 手順を明確化
- build artifacts を整理
- source file を module 分割する

### Mid term

- CLI mode を追加
- foreground mask preview を追加
- depth preview を追加
- OBJ export report を追加
- glTF export を検討
- mesh decimation / simplification を追加
- texture atlas / texture copy workflow を改善

### Long term

- multi-frame video reconstruction
- AI depth estimation との連携
- Unity import pipeline との連携
- Make3D → AssetUtility → Unity 最適化 pipeline
- batch processing mode
- automated regression tests
- sample gallery

---

## 25. Relationship with other projects

Make3D は、他の Unity / DCC tool projects と組み合わせることで、次のような pipeline portfolio として見せられます。

```text
Image / Video input
  ↓
Make3D
  ↓
OBJ / MTL output
  ↓
Unity import
  ↓
AssetUtility scan / optimization
  ↓
Scene / prefab validation
```

また、MAYAtoUnity と合わせると、以下のような技術領域を広く示せます。

| Project | Focus |
|---|---|
| Make3D | 画像 / 動画から mesh を生成する native tool |
| MAYAtoUnity | Maya scene file を Unity に再構築する DCC importer |
| AssetUtility | Unity 内 asset を可視化・最適化する Editor tool |

この 3 つを並べることで、**asset generation → DCC import → Unity optimization** という制作パイプライン全体への理解を示せます。

---

## 26. Portfolio / technical appeal points

このプロジェクトで示せる技術要素:

- C++ application development
- Win32 GUI programming
- Native file dialog / drag & drop handling
- Media Foundation video frame extraction
- stb_image based image loading
- Image mask generation
- Depth map processing
- Procedural mesh generation
- OBJ / MTL format writing
- Software preview rendering
- User-facing tool workflow design
- Error handling and validation
- Game asset pipeline awareness

---

## 27. How to present this project

書類・ポートフォリオでは、以下のように説明できます。

> 画像・深度画像・動画フレームから簡易 3D メッシュを生成し、OBJ / MTL として書き出す Windows ネイティブツールを C++ / Win32 で開発しました。  
> `stb_image` による PNG 読み込み、Media Foundation による動画フレーム抽出、foreground mask 生成、疑似 depth 作成、depth fusion、mesh reconstruction、normal recomputation、OBJ / MTL export、software preview renderer までを実装しています。  
> AI に依存しない deterministic な pipeline として、入力から出力までの処理過程を説明できる形にしており、ゲーム制作向けのプロトタイプ asset generation tool として設計しました。

---

## 28. Disclaimer

This project is an independent prototype / portfolio project for technical research.  
It is not a photogrammetry system, neural reconstruction system, or production-grade commercial 3D scanner.  
Generated results should be treated as prototype meshes or proxy assets and should be validated in DCC tools or game engines before production use.
