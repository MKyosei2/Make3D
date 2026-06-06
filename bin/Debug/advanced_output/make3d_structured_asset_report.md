# Make3D Structured Asset Build Report

Success: `true`

Message: Image-fitted structured character mesh generated.

## Analysis

# Make3D Asset Analysis Report

## Classification

# Game Asset Classification Report

| Metric | Value |
|---|---:|
| Asset type | Character |
| Foreground pixels | 5540 |
| Coverage | 0.338135 |
| Aspect ratio | 1.19802 |
| Rectangularity | 0.453318 |
| Symmetry X | 0.968378 |
| Symmetry Y | 0.641725 |
| Row stability | 0.173554 |
| Column stability | 0.445545 |
| Straight edge score | 0.624738 |
| Edge density | 0.185199 |
| Mass center Y | 0.497096 |

## Reasons

- Compact symmetric humanoid-like silhouette classified as character.
- Auto structured resolver selected Character from shape scores: character=1.39, building=0.32, furniture=0.31, vehicle=0.23, generic=0.32.


## Shape descriptors

| Metric | Value |
|---|---:|
| Foreground pixels | 5540 |
| Bounds | 14, 5 - 114, 125 |
| Coverage | 0.338135 |
| Aspect ratio | 1.19802 |
| Rectangularity | 0.453318 |
| Perimeter ratio | 2.14865 |
| Symmetry X | 0.968378 |
| Symmetry Y | 0.641725 |
| Row stability | 0.173554 |
| Column stability | 0.445545 |
| Straight edge score | 0.624738 |
| Edge density | 0.185199 |
| Mass center | 0.502723, 0.497096 |
| Contour complexity | 1 |

## Contour

| Metric | Value |
|---|---:|
| Sampled contour points | 256 |

## Palette

| Index | RGB | Coverage |
|---:|---|---:|
| 0 | 48, 34, 26 | 0.403791 |
| 1 | 226, 176, 123 | 0.162635 |
| 2 | 232, 93, 74 | 0.159206 |
| 3 | 235, 188, 140 | 0.101444 |
| 4 | 176, 59, 67 | 0.0911552 |
| 5 | 38, 64, 116 | 0.0397112 |

## Part hints

| Name | Role | Confidence | Bounds |
|---|---|---:|---|
| main_volume | main | 0.95 | 14,5 - 114,125 |
| head | head | 0.774702 | 49,5 - 79,31 |
| torso | torso | 0.78 | 42,31 - 86,75 |
| left_arm | arm | 0.58 | 16,35 - 46,84 |
| right_arm | arm | 0.58 | 82,35 - 112,84 |
| left_leg | leg | 0.66 | 44,75 - 64,126 |
| right_leg | leg | 0.66 | 64,75 - 84,126 |

## Plan

# Make3D Asset Plan

| Field | Value |
|---|---:|
| OK | yes |
| Asset type | Character |
| Asset name | Make3D_Character_Planned |
| Target size | 1.45, 2, 0.7 |
| Pivot | 0, 0, 0 |
| Wants collider | yes |
| Wants LOD | yes |

## Parts

| Name | Kind | Confidence | Center | Size | Material |
|---|---|---:|---|---|---:|
| main_volume | MainVolume | 0.95 | -0.00717822, 1.00826, 0 | 1.43564, 1.98347, 0.49 | 0 |
| head | CharacterHead | 0.774702 | -0.00717824, 1.78512, 0 | 0.430693, 0.429752, 0.49 | 1 |
| torso | CharacterTorso | 0.78 | -0.00717819, 1.20661, 0 | 0.631683, 0.727273, 0.49 | 2 |
| left_arm | CharacterArm | 0.58 | -0.480941, 1.09917, 0 | 0.430693, 0.809917, 0.49 | 3 |
| right_arm | CharacterArm | 0.58 | 0.466584, 1.09917, 0 | 0.430693, 0.809917, 0.49 | 4 |
| left_leg | CharacterLeg | 0.66 | -0.150743, 0.421488, 0 | 0.287129, 0.842975, 0.49 | 5 |
| right_leg | CharacterLeg | 0.66 | 0.136386, 0.421488, 0 | 0.287129, 0.842975, 0.49 | 0 |

## Mesh validation

| Metric | Value |
|---|---:|
| OK | yes |
| Vertices | 8913 |
| Triangles | 14676 |
| Invalid indices | 0 |
| Degenerate triangles | 2168 |
| Non-finite positions | 0 |
| Missing normals | 0 |
| Missing UVs | 0 |
| Bounds min | -0.588801, 0, -0.182238 |
| Bounds max | 0.588801, 2, 0.182238 |

### Warnings

- Mesh contains degenerate triangles.

## Warnings

- Mesh contains degenerate triangles.
- Character limbs fitted from silhouette edge anchors and skeleton-like side anchors.
- This is still a procedural proxy mesh, not a learned neural reconstruction.
