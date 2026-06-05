# Make3D Output Artifacts

This document explains the files produced by the advanced Make3D pipeline.

## Advanced build output

A normal advanced build writes the following files:

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

## File guide

| File | Purpose |
|---|---|
| `make3d_advanced.obj` | Simple geometry export for Blender, Maya, Unity, and general inspection. |
| `make3d_advanced.mtl` | OBJ material file. |
| `make3d_advanced.gltf` | Geometry-focused glTF export. |
| `make3d_advanced.bin` | Binary buffer used by the geometry glTF file. |
| `make3d_advanced_material.gltf` | glTF export with material metadata. This is the preferred modern asset output. |
| `make3d_advanced_material.bin` | Binary buffer used by the material glTF file. |
| `debug_mask.ppm` | Foreground mask preview. Useful for checking segmentation quality. |
| `debug_depth.ppm` | Estimated or prepared depth preview. Useful for checking shape reconstruction input. |
| `make3d_report.md` | Human-readable reconstruction report. |
| `make3d_report.json` | Machine-readable reconstruction report. |

## Which file should reviewers open first?

For quick review:

1. Open `make3d_advanced_material.gltf` in a glTF viewer, Blender, or Unity.
2. Inspect `debug_mask.ppm` to confirm what the tool treated as foreground.
3. Inspect `debug_depth.ppm` to understand the depth basis for the mesh.
4. Read `make3d_report.md` for vertex/triangle counts and warnings.

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
```

## What the quality report proves

The quality report compares raw and cleaned mesh states.

It reports:

- vertex count before and after cleanup
- triangle count before and after cleanup
- invalid triangle count
- degenerate triangle count
- boundary edge count
- non-manifold edge count
- surface area
- whether the mesh is valid for export
- whether the mesh is a watertight candidate

This is useful for portfolio review because it shows that Make3D does not only generate a mesh; it also inspects and cleans the result.

## Suggested portfolio explanation

> The output includes not only OBJ/glTF geometry, but also material glTF, debug mask/depth images, and Markdown/JSON quality reports. This makes the pipeline inspectable and suitable for technical review.
