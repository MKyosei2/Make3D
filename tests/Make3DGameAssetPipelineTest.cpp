#include "Make3DGameAssetFinalization.h"

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

static fs::path MakeBoxInput(const fs::path& dir, const char* name) {
    constexpr int W = 128;
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
    fs::path out = fs::current_path() / "game_asset_pipeline_test";
    fs::remove_all(out);
    fs::create_directories(out);

    make3d::FinalGameAssetOptions options;
    options.complete.generation.maskRefine.keepLargestComponentOnly = true;
    options.complete.generation.triangleBudget = 20000;
    options.complete.enginePreset = make3d::GameEnginePreset::Unity;
    options.complete.textureSize = 32;
    options.textureSize = 32;

    std::vector<fs::path> frames;
    frames.push_back(MakeBoxInput(out, "frame0.tga"));
    frames.push_back(MakeBoxInput(out, "frame1.tga"));
    make3d::FinalGameAssetResult finalAsset = make3d::BuildFinalGameAssetFromFrames(frames, out / "output", options);
    if (!finalAsset.ok) return Fail(finalAsset.message);

    const make3d::CompletedGameAssetResult& complete = finalAsset.complete;
    const make3d::GameAssetResult& result = complete.asset;
    if (result.assetType != make3d::GameAssetType::Building) return Fail("expected building asset type");
    if (result.parts.size() < 4) return Fail("editable building parts missing");
    if (!result.validation.hasCollisionMesh) return Fail("collision mesh missing");
    if (!result.validation.hasLodMesh) return Fail("lod mesh missing");
    if (!result.validation.hasUvs) return Fail("uv projection missing");
    if (!result.validation.gameReadyCandidate) return Fail("not game-ready candidate");
    if (!fs::exists(result.objPath)) return Fail("OBJ missing");
    if (!fs::exists(result.gltfPath)) return Fail("glTF missing");
    if (!fs::exists(result.collisionObjPath)) return Fail("collision OBJ missing");
    if (!fs::exists(result.lodObjPath)) return Fail("LOD OBJ missing");
    if (!fs::exists(result.reportPath)) return Fail("report missing");
    if (!fs::exists(complete.manifestPath)) return Fail("manifest missing");
    if (!fs::exists(complete.albedoTexturePath)) return Fail("albedo placeholder missing");
    if (!fs::exists(complete.normalTexturePath)) return Fail("normal placeholder missing");
    if (!fs::exists(complete.roughnessTexturePath)) return Fail("roughness placeholder missing");
    if (!fs::exists(complete.rigMetadataPath)) return Fail("rig metadata missing");
    if (!fs::exists(complete.enginePresetPath)) return Fail("engine preset missing");
    if (!fs::exists(finalAsset.projectedTexturePath)) return Fail("projected texture missing");
    if (!fs::exists(finalAsset.retopoProxyPath)) return Fail("retopo proxy missing");
    if (!fs::exists(finalAsset.bindingMetadataPath)) return Fail("binding metadata missing");
    if (!fs::exists(finalAsset.animationPreviewPath)) return Fail("animation preview missing");
    if (!fs::exists(finalAsset.meshCheckPath)) return Fail("mesh check missing");
    if (!fs::exists(finalAsset.frameReportPath)) return Fail("frame report missing");
    if (!finalAsset.meshCheck.usable) return Fail("mesh check not usable");
    if (finalAsset.retopoProxy.positions.empty()) return Fail("retopo proxy mesh empty");
    if (complete.bounds.sizeX <= 0.0f || complete.bounds.sizeY <= 0.0f || complete.bounds.sizeZ <= 0.0f) return Fail("invalid bounds");
    if (complete.joints.empty()) return Fail("pivot metadata missing");
    if (!Contains(result.reportPath, "Game-ready candidate")) return Fail("report metric missing");
    if (!Contains(complete.enginePresetPath, "Unity")) return Fail("engine preset content missing");
    if (!Contains(complete.rigMetadataPath, "pivot_center")) return Fail("pivot center missing");
    if (!Contains(finalAsset.bindingMetadataPath, "procedural binding")) return Fail("binding metadata content missing");
    if (!Contains(finalAsset.animationPreviewPath, "turntable_preview")) return Fail("animation preview content missing");
    if (!Contains(finalAsset.frameReportPath, "frameCount")) return Fail("frame report content missing");

    std::cout << "[PASS] Make3D final game asset pipeline regression test\n";
    std::cout << "Review target: " << result.gltfPath.u8string() << "\n";
    std::cout << "Manifest: " << complete.manifestPath.u8string() << "\n";
    std::cout << "Retopo proxy: " << finalAsset.retopoProxyPath.u8string() << "\n";
    return 0;
}
