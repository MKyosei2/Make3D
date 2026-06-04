# GUI Integration Plan

The advanced reconstruction backend is separated from the Win32 GUI so it can be built and tested first.

## Current state

- `MainApp.cpp` keeps the existing GUI and existing reconstruction path.
- `src/Make3DAdvancedCore.h` and `src/Make3DAdvancedCore.cpp` contain the new backend.
- `src/Make3DAdvancedCLI.cpp` provides a command-line validation path.

## Integration strategy

1. Keep the current GUI behavior available.
2. Add an advanced backend option instead of replacing the old path immediately.
3. Convert GUI quality and reconstruction selections into `make3d::AdvancedOptions`.
4. Call `make3d::BuildModelFromImage` from the existing build workflow.
5. Show the generated report path in the GUI result message.
6. Keep the old reconstruction path as a fallback until the advanced backend is validated with samples.

## Suggested mode mapping

| Existing GUI mode | Advanced backend mode |
|---|---|
| Auto | Auto or HybridVolume |
| Relief | ReliefSurface |
| PrimitiveBox | SilhouetteVolume |
| HumanoidProxy | HybridVolume |

## Suggested quality mapping

| Existing GUI quality | Advanced backend quality |
|---|---|
| Easy | Draft |
| Recommended | Standard |
| Detailed | Detailed |

## Naming note

`MainApp.cpp` currently defines global `ImageRGBA`, `DepthImage`, and `MeshData` types.

The advanced backend defines namespaced types:

- `make3d::ImageRGBA`
- `make3d::DepthImage`
- `make3d::MeshData`

This avoids direct symbol collision, but the preview renderer in the GUI still expects the old mesh type. The next cleanup should either rename the GUI preview type or switch the preview renderer to consume `make3d::MeshData`.

## Recommended next change

The next safe change is to add a small adapter function inside `MainApp.cpp` that:

- reads the current GUI quality preset
- reads the current reconstruction preset
- creates `make3d::AdvancedOptions`
- calls `make3d::BuildModelFromImage`
- copies the output path and message into the existing `BuildResult`

After that, add a GUI toggle named `Advanced Hybrid` or `Advanced Engine`.

## Validation command

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
