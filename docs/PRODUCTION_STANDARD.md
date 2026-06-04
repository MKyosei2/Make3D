# Make3D Production Standard

This document defines the minimum bar required for Make3D to become a main portfolio project for top-tier game company applications.

Make3D must not be presented as a toy, prototype, or vague image-to-3D experiment. It must become a reproducible 3D asset generation tool with visible output quality, measurable results, and a clear technical pipeline.

---

## 1. Final target

Make3D should become a native Windows tool that can generate usable prototype 3D assets from image-based input.

The target is not photogrammetry-level perfect reconstruction from arbitrary images. The target is:

- A reliable proxy asset generation tool.
- A tool that accepts image / depth / video input.
- A tool that generates valid textured mesh output.
- A tool that can be tested, benchmarked, and demonstrated.
- A tool that clearly separates image processing, depth estimation, mesh generation, preview, and export.

The tool should be presented as:

> A C++ / Win32 image-to-proxy-3D asset generation tool that converts image, depth, or video-frame input into textured mesh data and exports OBJ / MTL assets with preview, validation, and report output.

It should not be presented as:

> A perfect single-image 3D reconstruction system.

---

## 2. Non-negotiable minimum requirements

The project is not portfolio-main-project ready until all of the following are true.

### 2.1 Reproducible demo

The repository must include:

```text
samples/
  image_alpha/
    input.png
    output/model.obj
    output/model.mtl
    output/input.png
    report.json

  image_depth/
    input.png
    depth.png
    output/model.obj
    output/model.mtl
    report.json

  video_frame/
    input.mp4 or documented external sample
    output/model.obj
    output/model.mtl
    output/video_frame.ppm
    report.json
```

If large binary samples cannot be committed, the repository must include a download instruction and a generated output sample.

### 2.2 Visual proof

The README must show:

- Input image.
- Foreground mask preview.
- Depth / pseudo-depth preview.
- Generated mesh preview.
- OBJ opened in Blender or Unity.

At least one GIF must show:

```text
input selection → build → preview → output folder → OBJ opened externally
```

### 2.3 Valid output

Every demo output must satisfy:

- OBJ loads in Blender.
- OBJ loads in Unity.
- MTL texture reference works.
- Vertex count and triangle count are written to a report.
- No invalid face indices.
- No NaN / infinity vertex positions.
- No empty mesh output.

### 2.4 Report output

Every build must write a report file:

```json
{
  "workflow": "image_depth",
  "reconstructionPreset": "relief",
  "qualityPreset": "recommended",
  "inputWidth": 512,
  "inputHeight": 512,
  "maskPixels": 120034,
  "maskCoverage": 0.45,
  "depthMin": 0.0,
  "depthMax": 1.0,
  "depthMean": 0.43,
  "vertices": 53210,
  "triangles": 105900,
  "invalidTrianglesRemoved": 0,
  "processingTimeMs": 1234,
  "outputObj": "output/model.obj"
}
```

### 2.5 Usable build path

The repository must include:

- Clear Visual Studio build instructions.
- Required SDK / Windows version notes.
- Dependency notes for `stb_image.h`.
- A release build artifact or release packaging instructions.

---

## 3. Required architecture upgrade

Current implementation is concentrated in `MainApp.cpp`. That is acceptable for a prototype, but not for a main portfolio project.

The minimum production structure should be:

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
      ReportWriter.cpp
      PortableConfig.cpp

    ImageProcessing/
      ForegroundMask.cpp
      PseudoDepth.cpp
      DepthFusion.cpp
      DepthSmoothing.cpp

    Reconstruction/
      ShapeAnalysis.cpp
      DepthMeshBuilder.cpp
      PrimitiveBoxBuilder.cpp
      HumanoidProxyBuilder.cpp
      MeshCleanup.cpp
      MeshValidation.cpp

    Preview/
      SoftwarePreviewRenderer.cpp
      BitmapUtil.cpp

    Core/
      ImageTypes.h
      MeshData.h
      MathTypes.h
      BuildSettings.h
      BuildResult.h
```

The UI must call services. It must not contain the reconstruction logic directly.

---

## 4. Feature upgrade roadmap

### Phase 1: Make current output trustworthy

- Add mesh validator.
- Add report writer.
- Add sample inputs and sample outputs.
- Add README screenshots.
- Add Blender / Unity import verification.
- Add command-line batch mode for reproducible output.

Acceptance criteria:

- `make3d.exe --input samples/image_alpha/input.png --out output --preset relief --quality recommended` works.
- It produces OBJ / MTL / report.
- Output opens in Blender and Unity.

### Phase 2: Improve 3D quality

- Add optional normal map / height map input.
- Add depth preview export.
- Add mask preview export.
- Add mesh decimation.
- Add UV cleanup.
- Add back-face / side-wall quality controls.
- Add smoothing controls.
- Add glTF export or documented Unity import path.

Acceptance criteria:

- Generated model is visually acceptable as a proxy game asset.
- The output can be imported into Unity and used in a scene without manual repair.

### Phase 3: AI-assisted depth path

If the goal is truly higher-quality image-to-3D, Make3D needs an optional AI-assisted pipeline.

Possible approach:

```text
Input image
  ↓
Segmentation model
  ↓
Depth estimation model
  ↓
Normal estimation / refinement
  ↓
Mesh reconstruction
  ↓
Texture projection
  ↓
OBJ / glTF export
```

This should be optional. The non-AI deterministic path should remain available for demonstration and testing.

Acceptance criteria:

- AI path produces better depth than pseudo-depth on at least 5 sample images.
- The repository documents model source, license, runtime dependency, and reproducible setup.

### Phase 4: Main portfolio release

- Add demo video.
- Add release zip.
- Add architecture diagram.
- Add benchmark table.
- Add known limitations.
- Add comparison gallery.

---

## 5. Test requirements

Minimum tests:

```text
tests/
  MaskTests.cpp
  DepthFusionTests.cpp
  MeshValidationTests.cpp
  ObjExportTests.cpp
  ReportWriterTests.cpp
```

Required test cases:

- Empty image fails with friendly error.
- Alpha PNG produces non-empty mask.
- Opaque image with simple background produces foreground mask.
- Depth image dimension mismatch fails.
- OBJ exporter writes valid vertex / uv / normal / face data.
- Mesh validator rejects invalid indices.
- Report writer produces valid JSON.

---

## 6. Portfolio wording rule

Allowed wording:

> C++ / Win32で画像・深度画像・動画フレームからプロキシ3Dメッシュを生成し、OBJ / MTLとして出力するネイティブツールを開発。前景抽出、疑似深度生成、深度融合、メッシュ生成、法線再計算、OBJ exporter、software preview、レポート出力を実装。

Forbidden wording until the AI / multi-view path is implemented:

> 写真から高品質な3Dモデルを完全自動生成できます。

---

## 7. Main-project readiness checklist

Make3D can become a main portfolio project only when this checklist is complete.

- [ ] README has GIF.
- [ ] README has before / after images.
- [ ] `samples/` exists.
- [ ] Sample OBJ files are included or downloadable.
- [ ] Report output exists.
- [ ] CLI mode exists.
- [ ] Mesh validation exists.
- [ ] OBJ output verified in Blender.
- [ ] OBJ output verified in Unity.
- [ ] At least 5 sample cases are documented.
- [ ] Code is split out of single-file architecture.
- [ ] Tests exist for mask, depth, mesh, export, and report.
- [ ] A release build is available.
