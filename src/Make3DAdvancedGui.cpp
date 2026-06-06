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
// The current .vcxproj does not list all new builder .cpp files yet, so include the
// implementations here for Visual Studio builds. CMake builds compile the structured
// builder as part of Make3DAdvancedCore.

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100) // unreferenced formal parameter from included GUI helpers
#pragma warning(disable : 4505) // unreferenced internal helper functions removed by optimizer
#endif

#include "Make3DStructuredAssetBuilder.cpp"
#include "Make3DImageFittedStructuredAssetBuilder.cpp"
#include "Make3DAdvancedGuiStructured.cpp"

#ifdef _MSC_VER
#pragma warning(pop)
#endif
