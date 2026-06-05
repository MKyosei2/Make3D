# Make3D Output Artifacts

This document explains the files produced by the advanced Make3D pipeline.

## Recommended review path

For portfolio review, inspect the production pipeline output first:

```text
production_pipeline/output/voxel/make3d_voxel_volume_vertex_color.gltf
production_pipeline/output/voxel/make3d_voxel_volume_material.gltf
production_pipeline/output/polished/make3d_polished_vertex_color.gltf
production_pipeline/output/polished/make3d_polished_material.gltf
production_pipeline/output/debug_mask_refined.ppm
production_pipeline/output/debug_depth_refined.ppm
production_pipeline/output/production_report.md
```

Open `voxel/make3d_voxel_volume_vertex_color.gltf` first when judging whether the output feels like a closed 3D volume with source-image color. Open `voxel/make3d_voxel_volume_material.gltf` next if the viewer has limited vertex color support. Open `polished/make3d_polished_vertex_color.gltf` to inspect the refined hybrid reconstruction path.

This path shows the refined production pipeline rather than the raw reconstruction only.

---

## Production pipeline output

The production pipeline writes:

```text
input_noisy_character.tga
output/raw/make3d_raw.obj
output/raw/make3d_raw_material.gltf
output/polished/make3d_polished.obj
output/polished/make3d_polished_material.gltf
output/polished/make3d_polished_vertex_color.gltf
output/polished/make3d_polished_vertex_color.bin
output/voxel/make3d_voxel_volume.obj
output/voxel/make3d_voxel_volume_material.gltf
output/voxel/make3d_voxel_volume_vertex_color.gltf
output/voxel/make3d_voxel_volume_vertex_color.bin
output/debug_mask_refined.ppm
output/debug_depth_refined.ppm
output/production_report.md
output/production_report.json
```

## Production file guide

| File | Purpose |
|---|---|
| `input_noisy_character.tga` | Synthetic noisy input used to demonstrate mask refinement and cleanup. TGA is used because it is reliably decoded by the image loader. |
| `output/raw/make3d_raw.obj` | Raw mesh before production polishing. |
| `output/raw/make3d_raw_material.gltf` | Raw material glTF before production polishing. |
| `output/polished/make3d_polished.obj` | Polished OBJ after mask refinement, cleanup, component filtering, and smoothing. |
| `output/polished/make3d_polished_material.gltf` | Refined hybrid reconstruction output with material metadata. |
| `output/polished/make3d_polished_vertex_color.gltf` | Refined hybrid reconstruction output with `COLOR_0` vertex colors sampled from the source image. |
| `output/voxel/make3d_voxel_volume.obj` | Closed voxel-style volume OBJ generated from refined mask rows and depth-based thickness. |
| `output/voxel/make3d_voxel_volume_material.gltf` | Closed-volume glTF with material metadata. Use this if the viewer has limited vertex color support. |
| `output/voxel/make3d_voxel_volume_vertex_color.gltf` | Preferred closed-volume review output. Open this first in a glTF viewer, Blender, or Unity. |
| `output/debug_mask_refined.ppm` | Refined foreground mask. Confirms that small noisy regions were removed. |
| `output/debug_depth_refined.ppm` | Depth preview generated from the refined mask. |
| `output/production_report.md` | Human-readable report combining reconstruction, mask refinement, mesh polishing, and voxel volume statistics. |
| `output/production_report.json` | Machine-readable version of the production report. |

---

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

## Advanced file guide

| File | Purpose |
|---|---|
| `make3d_advanced.obj` | Simple geometry export for Blender, Maya, Unity, and general inspection. |
| `make3d_advanced.mtl` | OBJ material file. |
| `make3d_advanced.gltf` | Geometry-focused glTF export. |
| `make3d_advanced.bin` | Binary buffer used by the geometry glTF file. |
| `make3d_advanced_material.gltf` | glTF export with material metadata. |
| `make3d_advanced_material.bin` | Binary buffer used by the material glTF file. |
| `debug_mask.ppm` | Foreground mask preview. Useful for checking segmentation quality. |
| `debug_depth.ppm` | Estimated or prepared depth preview. Useful for checking shape reconstruction input. |
| `make3d_report.md` | Human-readable reconstruction report. |
| `make3d_report.json` | Machine-readable reconstruction report. |

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

## What the quality report proves

The quality report compares raw, cleaned, and polished mesh states.

It reports:

- vertex count before and after cleanup/polish
- triangle count before and after cleanup/polish
- invalid triangle count
- degenerate triangle count
- boundary edge count
- non-manifold edge count
- component count before and after polish
- removed component count
- smoothing iterations
- surface area
- whether the mesh is valid for export
- whether the mesh is a watertight candidate

This is useful for portfolio review because it shows that Make3D does not only generate a mesh; it also inspects, cleans, and polishes the result.

## Suggested portfolio explanation

> The output includes not only OBJ/glTF geometry, but also closed-volume vertex-color glTF, refined debug mask/depth images, and Markdown/JSON production reports. This makes the pipeline inspectable and suitable for technical review.
