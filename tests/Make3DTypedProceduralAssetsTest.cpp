#include "Make3DTypedProceduralAssets.h"

#include <filesystem>
#include <iostream>

static int Fail(const char* message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

static bool CheckType(make3d::GameAssetType type, const std::filesystem::path& out) {
    make3d::TypedProceduralAssetOptions options;
    options.targetHeight = 2.0f;
    options.radialSegments = 16;
    auto result = make3d::ExportTypedProceduralAsset(type, out, options);
    if (!result.ok) return false;
    if (result.mesh.positions.empty()) return false;
    if (result.mesh.indices.empty()) return false;
    if (!result.validation.ok) return false;
    if (!std::filesystem::exists(result.objPath)) return false;
    if (!std::filesystem::exists(result.gltfPath)) return false;
    if (!std::filesystem::exists(result.reportPath)) return false;
    if (!std::filesystem::exists(result.manifestPath)) return false;
    return true;
}

int main() {
    const std::filesystem::path root = std::filesystem::current_path() / "typed_procedural_assets_test";
    if (!CheckType(make3d::GameAssetType::Furniture, root / "furniture")) return Fail("Furniture generation failed");
    if (!CheckType(make3d::GameAssetType::Machine, root / "machine")) return Fail("Machine generation failed");
    if (!CheckType(make3d::GameAssetType::ToolOrWeapon, root / "tool")) return Fail("Tool generation failed");
    if (!CheckType(make3d::GameAssetType::TerrainPiece, root / "terrain")) return Fail("Terrain generation failed");
    std::cout << "[PASS] Make3D typed procedural assets test\n";
    return 0;
}
