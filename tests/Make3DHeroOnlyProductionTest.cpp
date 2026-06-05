#include "Make3DProductionPipeline.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int Fail(const std::string& message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

static bool SaveTinyCharacterTga(const fs::path& path) {
    constexpr int W = 96;
    constexpr int H = 128;
    std::vector<unsigned char> pixels(static_cast<size_t>(W) * H * 4, 0);

    auto set = [&](int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        size_t p = (static_cast<size_t>(y) * W + x) * 4;
        pixels[p + 0] = r;
        pixels[p + 1] = g;
        pixels[p + 2] = b;
        pixels[p + 3] = a;
    };

    auto ellipse = [&](float cx, float cy, float rx, float ry, unsigned char r, unsigned char g, unsigned char b) {
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                float dx = (static_cast<float>(x) - cx) / rx;
                float dy = (static_cast<float>(y) - cy) / ry;
                if (dx * dx + dy * dy <= 1.0f) set(x, y, r, g, b, 255);
            }
        }
    };

    ellipse(48.0f, 18.0f, 11.0f, 14.0f, 230, 205, 180);
    ellipse(48.0f, 53.0f, 18.0f, 29.0f, 70, 130, 220);
    ellipse(30.0f, 57.0f, 5.5f, 24.0f, 70, 130, 220);
    ellipse(66.0f, 57.0f, 5.5f, 24.0f, 70, 130, 220);
    ellipse(39.0f, 101.0f, 6.0f, 24.0f, 50, 70, 180);
    ellipse(57.0f, 101.0f, 6.0f, 24.0f, 50, 70, 180);

    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    unsigned char header[18] = {};
    header[2] = 2;
    header[12] = static_cast<unsigned char>(W & 0xFF);
    header[13] = static_cast<unsigned char>((W >> 8) & 0xFF);
    header[14] = static_cast<unsigned char>(H & 0xFF);
    header[15] = static_cast<unsigned char>((H >> 8) & 0xFF);
    header[16] = 32;
    header[17] = 8 | 0x20;
    file.write(reinterpret_cast<const char*>(header), sizeof(header));

    for (int i = 0; i < W * H; ++i) {
        size_t p = static_cast<size_t>(i) * 4;
        unsigned char bgra[4] = {pixels[p + 2], pixels[p + 1], pixels[p + 0], pixels[p + 3]};
        file.write(reinterpret_cast<const char*>(bgra), 4);
    }
    return true;
}

static bool HeroMeshHasFiniteVertices(const make3d::MeshData& mesh) {
    for (float v : mesh.positions) {
        if (!std::isfinite(v)) return false;
    }
    return true;
}

static bool HeroMeshIndicesAreValid(const make3d::MeshData& mesh) {
    size_t vertexCount = mesh.positions.size() / 3;
    if (mesh.indices.size() % 3 != 0) return false;
    for (std::uint32_t i : mesh.indices) {
        if (i >= vertexCount) return false;
    }
    return true;
}

static float HeroMeshHeight(const make3d::MeshData& mesh) {
    if (mesh.positions.empty()) return 0.0f;
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (size_t i = 1; i < mesh.positions.size(); i += 3) {
        minY = std::min(minY, mesh.positions[i]);
        maxY = std::max(maxY, mesh.positions[i]);
    }
    return maxY - minY;
}

int main() {
    fs::path out = fs::current_path() / "hero_only_production_test";
    fs::remove_all(out);
    fs::create_directories(out);

    fs::path input = out / "input_character.tga";
    if (!SaveTinyCharacterTga(input)) return Fail("failed to write test input TGA");

    make3d::ProductionPipelineOptions options;
    options.exportHeroCharacter = true;
    options.exportGameAsset = true;
    options.exportRaw = false;
    options.exportPolished = false;
    options.exportVoxelVolume = false;
    options.exportVertexColorGltf = true;
    options.writeReports = true;
    options.writeDebugImages = true;
    options.maskRefine.keepLargestComponentOnly = true;

    make3d::ProductionPipelineResult result = make3d::BuildProductionModelFromImage(input, std::nullopt, out / "output", options);
    if (!result.ok) return Fail(result.message);

    if (result.heroMesh.positions.empty()) return Fail("hero mesh positions missing");
    if (result.heroMesh.indices.empty()) return Fail("hero mesh indices missing");
    if (!result.heroReport.ok) return Fail("hero report should be ok");
    if (result.heroReport.vertices < 300) return Fail("hero vertex count is too low for review output");
    if (result.heroReport.triangles < 300) return Fail("hero triangle count is too low for review output");
    if (!HeroMeshHasFiniteVertices(result.heroMesh)) return Fail("hero mesh has non-finite vertices");
    if (!HeroMeshIndicesAreValid(result.heroMesh)) return Fail("hero mesh indices are invalid");
    if (HeroMeshHeight(result.heroMesh) < 1.0f) return Fail("hero mesh bounding height is too small");
    if (result.heroFineDetailReport.verticesAfter <= result.heroReport.vertices) return Fail("fine detail pass did not increase hero vertex count");

    if (!fs::exists(result.heroObjPath)) return Fail("hero OBJ missing");
    if (!fs::exists(result.heroMaterialGltfPath)) return Fail("hero material glTF missing");
    if (!fs::exists(result.heroVertexColorGltfPath)) return Fail("hero semantic vertex-color glTF missing");
    if (!fs::exists(result.productionReportPath)) return Fail("production report missing");

    if (result.gameAssetMesh.positions.empty()) return Fail("game asset mesh positions missing");
    if (result.gameAssetMesh.indices.empty()) return Fail("game asset mesh indices missing");
    if (!result.gameAssetReport.ok) return Fail("game asset report should be ok");
    if (!fs::exists(result.gameAssetObjPath)) return Fail("game asset OBJ missing");
    if (!fs::exists(result.gameAssetGltfPath)) return Fail("game asset glTF missing");
    if (!fs::exists(result.gameAssetReportPath)) return Fail("game asset report missing");
    if (!fs::exists(result.gameAssetManifestPath)) return Fail("game asset manifest missing");

    if (!result.rawObjPath.empty()) return Fail("raw path should stay empty in hero-only mode");
    if (!result.polishedObjPath.empty()) return Fail("polished path should stay empty in hero-only mode");
    if (!result.voxelObjPath.empty()) return Fail("voxel path should stay empty in hero-only mode");
    if (fs::exists(out / "output" / "raw")) return Fail("raw directory should not exist in hero-only mode");
    if (fs::exists(out / "output" / "polished")) return Fail("polished directory should not exist in hero-only mode");
    if (fs::exists(out / "output" / "voxel")) return Fail("voxel directory should not exist in hero-only mode");

    std::ifstream gltf(result.heroVertexColorGltfPath, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(gltf)), std::istreambuf_iterator<char>());
    if (text.find("COLOR_0") == std::string::npos) return Fail("hero semantic glTF missing COLOR_0");

    std::ifstream report(result.productionReportPath, std::ios::binary);
    std::string reportText((std::istreambuf_iterator<char>(report)), std::istreambuf_iterator<char>());
    if (reportText.find("Hero character") == std::string::npos) return Fail("production report missing hero section");
    if (reportText.find("Hero fine detail") == std::string::npos) return Fail("production report missing fine detail section");
    if (reportText.find("Secondary safe game asset") == std::string::npos) return Fail("production report missing secondary game asset section");

    std::cout << "[PASS] Make3D hero-only production quality gate test\n";
    std::cout << "Review target: " << result.heroVertexColorGltfPath.u8string() << "\n";
    std::cout << "Hero vertices: " << result.heroReport.vertices << " -> " << result.heroFineDetailReport.verticesAfter << "\n";
    std::cout << "Game asset: " << result.gameAssetObjPath.u8string() << "\n";
    return 0;
}
