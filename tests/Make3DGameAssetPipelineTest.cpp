#include "Make3DGameAssetPipeline.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int Fail(const std::string& message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

static bool SaveTga(const fs::path& path, int w, int h, const std::vector<unsigned char>& rgba) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::binary);
    if (!file || rgba.size() != static_cast<size_t>(w) * h * 4) return false;
    unsigned char header[18] = {};
    header[2] = 2;
    header[12] = static_cast<unsigned char>(w & 0xFF);
    header[13] = static_cast<unsigned char>((w >> 8) & 0xFF);
    header[14] = static_cast<unsigned char>(h & 0xFF);
    header[15] = static_cast<unsigned char>((h >> 8) & 0xFF);
    header[16] = 32;
    header[17] = 8 | 0x20;
    file.write(reinterpret_cast<const char*>(header), sizeof(header));
    for (int i = 0; i < w * h; ++i) {
        size_t p = static_cast<size_t>(i) * 4;
        unsigned char bgra[4] = {rgba[p + 2], rgba[p + 1], rgba[p + 0], rgba[p + 3]};
        file.write(reinterpret_cast<const char*>(bgra), 4);
    }
    return true;
}

static fs::path MakeBoxInput(const fs::path& dir) {
    constexpr int W = 128;
    constexpr int H = 128;
    std::vector<unsigned char> pixels(static_cast<size_t>(W) * H * 4, 0);
    auto set = [&](int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        size_t p = (static_cast<size_t>(y) * W + x) * 4;
        pixels[p + 0] = r; pixels[p + 1] = g; pixels[p + 2] = b; pixels[p + 3] = a;
    };
    for (int y = 22; y < 116; ++y) for (int x = 30; x < 98; ++x) set(x, y, 170, 170, 180, 255);
    for (int y = 14; y < 28; ++y) for (int x = 24; x < 104; ++x) set(x, y, 120, 80, 70, 255);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 3; ++col) {
            int x0 = 40 + col * 18;
            int y0 = 36 + row * 16;
            for (int y = y0; y < y0 + 7; ++y) for (int x = x0; x < x0 + 8; ++x) set(x, y, 80, 130, 190, 255);
        }
    }
    for (int y = 96; y < 116; ++y) for (int x = 58; x < 70; ++x) set(x, y, 90, 60, 40, 255);
    fs::path path = dir / "box_input.tga";
    SaveTga(path, W, H, pixels);
    return path;
}

int main() {
    fs::path out = fs::current_path() / "game_asset_pipeline_test";
    fs::remove_all(out);
    fs::create_directories(out);

    make3d::GameAssetGenerationOptions options;
    options.maskRefine.keepLargestComponentOnly = true;
    options.triangleBudget = 20000;

    fs::path input = MakeBoxInput(out);
    make3d::GameAssetResult result = make3d::BuildGameAssetFromImage(input, std::nullopt, out / "output", options);
    if (!result.ok) return Fail(result.message);
    if (result.assetType != make3d::GameAssetType::Building) return Fail("expected building asset type");
    if (result.mesh.positions.empty()) return Fail("mesh missing");
    if (result.parts.size() < 4) return Fail("editable building parts missing");
    if (!result.validation.hasCollisionMesh) return Fail("collision mesh missing");
    if (!result.validation.hasLodMesh) return Fail("lod mesh missing");
    if (!result.validation.gameReadyCandidate) return Fail("not marked as game-ready candidate");
    if (!fs::exists(result.objPath)) return Fail("OBJ missing");
    if (!fs::exists(result.gltfPath)) return Fail("glTF missing");
    if (!fs::exists(result.collisionObjPath)) return Fail("collision OBJ missing");
    if (!fs::exists(result.lodObjPath)) return Fail("LOD OBJ missing");
    if (!fs::exists(result.reportPath)) return Fail("report missing");

    std::ifstream report(result.reportPath, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(report)), std::istreambuf_iterator<char>());
    if (text.find("Game-ready candidate") == std::string::npos) return Fail("validation report missing game-ready metric");
    if (text.find("window_") == std::string::npos) return Fail("building report missing generated window parts");

    std::cout << "[PASS] Make3D game asset pipeline regression test\n";
    std::cout << "Review target: " << result.gltfPath.u8string() << "\n";
    return 0;
}
