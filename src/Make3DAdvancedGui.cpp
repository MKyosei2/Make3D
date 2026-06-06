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

#include "Make3DAdvancedGuiStructured.cpp"
