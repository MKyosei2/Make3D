# Make3D Generic Game Asset Pipeline

Make3D now includes a C++17 generic game asset path in addition to the hero character pipeline.

The goal of this path is not to claim perfect arbitrary reconstruction. The goal is to produce inspectable, editable, engine-oriented proxy assets from a 2D image, illustration, or frame using a deterministic C++ pipeline.

## Main CLI

```text
Make3DGameAssetCLI --input <image> --output <folder> [options]
```

Options:

```text
--depth <image>          Optional depth image
--texture-size <n>       Projected texture size, default 256
--grid <n>               Reconstruction grid, default 96
--segments <n>           Radial segments, default 24
--height <value>         Target asset height, default 2.0
--no-repair              Skip mesh repair pass
--no-texture             Skip projected texture and textured OBJ
```

Example:

```text
Make3DGameAssetCLI --input input.png --output out_game_asset --texture-size 512 --grid 128
```

## Pipeline

```text
image/depth input
  -> foreground mask
  -> mask refinement
  -> pseudo-depth preparation
  -> asset type classification
  -> generic asset generation
  -> collider proxy
  -> LOD proxy
  -> mesh validation
  -> mesh repair
  -> planar UV regeneration
  -> projected texture PPM
  -> textured OBJ/MTL
  -> import guide
  -> Unity-style metadata JSON
```

## Asset type classification

The current classifier emits these categories:

```text
Unknown
Character
Building
Vehicle
Furniture
ToolOrWeapon
Machine
Creature
TerrainPiece
ArchitecturalPart
FlatRelief
ComplexObject
GenericProp
```

The generator currently has explicit behavior for:

- building-like assets
- architectural parts
- vehicle-like wide assets
- generic props and complex fallback assets
- flat relief assets through the existing relief/hybrid reconstruction path

## Outputs

The complete asset path writes:

```text
base/make3d_game_asset.obj
base/make3d_game_asset.gltf
base/make3d_game_asset_collider.obj
base/make3d_game_asset_lod_proxy.obj
base/make3d_game_asset_report.md
base/make3d_game_asset_manifest.json
projected_texture.ppm
make3d_complete_textured.obj
make3d_complete_textured.mtl
complete_game_asset_report.md
complete_game_asset_manifest.json
IMPORT_GUIDE.md
unity_import_metadata.json
```

## Sample suite

Run:

```text
Make3DGenerateCompleteGameAssetSuite <output-folder>
```

It generates three deterministic samples:

- building
- vehicle-like prop
- generic prop

Each sample includes a complete asset output folder with textured OBJ, projected texture, collider proxy, LOD proxy, import guide, report, and manifest.

## Validation and repair

The mesh validator checks:

- empty mesh
- invalid face indices
- degenerate triangles
- non-finite positions
- missing or mismatched normals
- missing or mismatched UVs
- bounds

The repair pass:

- removes invalid triangles
- removes degenerate triangles
- replaces non-finite positions with zero
- regenerates planar UVs
- recomputes normals

## Current limitations

- The complete game asset path is available through `Make3DGameAssetCLI` and sample tools, but the existing hero-focused `BuildProductionModelFromImage` function is not fully rewired to always emit complete game asset outputs.
- Texture projection is a simple front-planar PPM projection. It is useful for inspection but not a full UV atlas or professional texture bake.
- AssetType-specific generators are still early. Building, vehicle-like, and generic prop paths exist, but furniture, machinery, terrain pieces, creatures, and complex props need deeper procedural models.
- The pipeline remains deterministic and dependency-light. It does not add external neural models or large third-party libraries.

## Portfolio wording

Recommended wording:

```text
Make3D is a deterministic C++17 image-to-game-asset proxy generator. It classifies 2D image input into broad asset categories, generates inspectable OBJ/glTF proxy geometry, writes collider and LOD proxy assets, performs mesh validation/repair, creates simple projected texture output, and emits import metadata for engine-side review.
```

Avoid claiming:

```text
Perfect production 3D reconstruction from any single image.
Automatic AAA-quality retopology and UV unwrapping.
Full hidden-side inference from one view.
```
