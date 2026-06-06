// Legacy Visual Studio project entry point.
//
// Keep this filename because Make3D.vcxproj still compiles src\Make3DAdvancedGui.cpp.
// Route Visual Studio to the stable structured GUI. The experimental hero/color preview
// path is intentionally not used here because it made the current preview worse.
// The stable GUI now uses the sculpted character builder internally for Character assets.

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100) // unreferenced formal parameter from included GUI helpers
#pragma warning(disable : 4505) // unreferenced internal helper functions removed by optimizer
#endif

#include "Make3DStructuredAssetBuilder.cpp"
#include "Make3DSculptedStructuredAssetBuilder.cpp"
#define BuildStructuredAssetMesh BuildSculptedStructuredAssetMesh
#include "Make3DAdvancedGuiStable.cpp"
#undef BuildStructuredAssetMesh

#ifdef _MSC_VER
#pragma warning(pop)
#endif
