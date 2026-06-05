#include "Make3DTypedProceduralAssets.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

static void Usage() {
    std::cout << "Make3DTypedAssetCLI\n";
    std::cout << "Usage:\n";
    std::cout << "  Make3DTypedAssetCLI --type furniture|machine|tool|terrain --output <folder> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --height <value>       Target height, default 2.0\n";
    std::cout << "  --segments <n>         Radial segments, default 18\n";
    std::cout << "  --no-details           Disable secondary detail parts\n";
}

static make3d::GameAssetType ParseType(const std::string& s) {
    if (s == "furniture") return make3d::GameAssetType::Furniture;
    if (s == "machine") return make3d::GameAssetType::Machine;
    if (s == "tool" || s == "weapon") return make3d::GameAssetType::ToolOrWeapon;
    if (s == "terrain" || s == "terrain-piece") return make3d::GameAssetType::TerrainPiece;
    return make3d::GameAssetType::Unknown;
}

} // namespace

int main(int argc, char** argv) {
    make3d::GameAssetType type = make3d::GameAssetType::Unknown;
    fs::path output = "typed_asset_output";
    make3d::TypedProceduralAssetOptions options;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) { std::cerr << "Missing value for " << name << "\n"; return nullptr; }
            return argv[++i];
        };
        if (a == "--help" || a == "-h") { Usage(); return 0; }
        else if (a == "--type") { const char* v = need("--type"); if (!v) return 1; type = ParseType(v); }
        else if (a == "--output") { const char* v = need("--output"); if (!v) return 1; output = v; }
        else if (a == "--height") { const char* v = need("--height"); if (!v) return 1; options.targetHeight = std::stof(v); }
        else if (a == "--segments") { const char* v = need("--segments"); if (!v) return 1; options.radialSegments = std::stoi(v); }
        else if (a == "--no-details") options.addDetails = false;
        else { std::cerr << "Unknown option: " << a << "\n"; Usage(); return 1; }
    }

    if (type == make3d::GameAssetType::Unknown) { Usage(); return 1; }
    auto result = make3d::ExportTypedProceduralAsset(type, output, options);
    if (!result.ok) { std::cerr << result.message << "\n"; return 2; }
    std::cout << result.message << "\n";
    std::cout << "Type: " << make3d::ToString(result.assetType) << "\n";
    std::cout << "OBJ: " << result.objPath.u8string() << "\n";
    std::cout << "glTF: " << result.gltfPath.u8string() << "\n";
    std::cout << "Report: " << result.reportPath.u8string() << "\n";
    std::cout << "Manifest: " << result.manifestPath.u8string() << "\n";
    return 0;
}
