// Legacy Visual Studio project entry point.
//
// The original Make3DAdvancedGui.cpp used the raw ImageDrivenDetailedMesh preview path:
//   ReconstructMesh(color, depth, mask, options, report)
// That path produced silhouette-extrusion style meshes and is no longer the default
// portfolio-facing GUI.
//
// Keep this filename because Make3D.vcxproj still compiles src\Make3DAdvancedGui.cpp.
// Route it to the structured multi-asset GUI so Visual Studio launches the same
// behavior as the CMake Make3DAdvancedGui target.
//
// The current .vcxproj does not list Make3DStructuredAssetBuilder.cpp yet, so include
// the implementation here for Visual Studio builds. CMake builds compile the builder
// as part of Make3DAdvancedCore and do not use this legacy wrapper.

#include "Make3DStructuredAssetBuilder.cpp"
#include "Make3DAdvancedGuiStructured.cpp"
