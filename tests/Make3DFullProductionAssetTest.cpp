#include "Make3DFullProductionAsset.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int Fail(const std::string& m) {
    std::cerr << "[FAIL] " << m << "\n";
    return 1;
}

static bool SaveTga(const fs::path& path, int w, int h, const std::vector<unsigned char>& rgba) {
    fs::create_directories(path.parent_path());
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

static fs::path MakeInput(const fs::path& dir, const char* name, int shift) {
    constexpr int W = 96;
    constexpr int H = 96;
    std::vector<unsigned char> pixels(static_cast<size_t>(W) * H * 4, 0);
    auto set = [&](int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
        x += shift;
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        size_t p = (static_cast<size_t>(y) * W + x) * 4;
        pixels[p + 0] = r; pixels[p + 1] = g; pixels[p + 2] = b; pixels[p + 3] = a;
    };
    for (int y = 18; y < 84; ++y) for (int x = 24; x < 72; ++x) set(x, y, 172, 174, 182, 255);
    for (int y = 10; y < 22; ++y) for (int x = 18; x < 78; ++x) set(x, y, 122, 82, 70, 255);
    for (int y = 30; y < 36; ++y) for (int x = 32; x < 40; ++x) set(x, y, 80, 130, 190, 255);
    for (int y = 30; y < 36; ++y) for (int x = 56; x < 64; ++x) set(x, y, 80, 130, 190, 255);
    for (int y = 64; y < 84; ++y) for (int x = 44; x < 52; ++x) set(x, y, 90, 60, 40, 255);
    fs::path path = dir / name;
    SaveTga(path, W, H, pixels);
    return path;
}

static bool Contains(const fs::path& path, const std::string& needle) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text.find(needle) != std::string::npos;
}

int main() {
    fs::path root = fs::current_path() / "full_production_asset_test";
    fs::remove_all(root);
    fs::create_directories(root);

    make3d::FullProductionAssetOptions options;
    options.finalOptions.complete.generation.maskRefine.keepLargestComponentOnly = true;
    options.finalOptions.complete.generation.triangleBudget = 20000;
    options.finalOptions.complete.enginePreset = make3d::GameEnginePreset::Unity;
    options.finalOptions.complete.textureSize = 32;
    options.finalOptions.textureSize = 32;
    options.textureSize = 32;

    std::vector<fs::path> frames = {MakeInput(root, "frame0.tga", 0), MakeInput(root, "frame1.tga", 1)};
    make3d::FullProductionAssetResult full = make3d::BuildFullProductionAssetFromFrames(frames, root / "output", options);
    if (!full.ok) return Fail(full.message);
    if (!fs::exists(full.finalAsset.complete.asset.gltfPath)) return Fail("gltf missing");
    if (!fs::exists(full.finalAsset.lod2Path)) return Fail("lod2 missing");
    if (!fs::exists(full.delivery.deliveryManifestPath)) return Fail("delivery missing");
    if (!fs::exists(full.audit.assetCatalogPath)) return Fail("catalog missing");
    if (!fs::exists(full.scene.previewPath)) return Fail("scene preview missing");
    if (!fs::exists(full.quality.gltfPackagePath)) return Fail("quality package missing");
    if (!fs::exists(full.geometry.repairedMeshPath)) return Fail("repaired mesh missing");
    if (!fs::exists(full.production.readinessScorePath)) return Fail("readiness missing");
    if (!fs::exists(full.detail.detailQualityPath)) return Fail("detail quality missing");
    if (!fs::exists(full.acceptanceReportPath)) return Fail("acceptance missing");
    if (!fs::exists(full.outputIndexPath)) return Fail("index missing");
    if (!Contains(full.acceptanceReportPath, "full_production_asset_ready")) return Fail("acceptance content missing");
    if (!Contains(full.outputIndexPath, "full_production_asset_output_index")) return Fail("index content missing");
    std::cout << "[PASS] Make3D full production asset regression test\n";
    return 0;
}
