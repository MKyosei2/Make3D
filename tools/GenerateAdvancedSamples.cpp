#include "Make3DAdvancedCore.h"
#include "Make3DCompleteGameAssetPipeline.h"
#include "Make3DMaskRefiner.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

namespace fs = std::filesystem;

static make3d::ImageRGBA Blank(int w, int h) {
    make3d::ImageRGBA image;
    image.width = w;
    image.height = h;
    image.pixels.assign(static_cast<size_t>(w) * h * 4, 0);
    return image;
}

static void Set(make3d::ImageRGBA& image, int x, int y, unsigned char r, unsigned char g, unsigned char b) {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) return;
    size_t p = (static_cast<size_t>(y) * image.width + x) * 4;
    image.pixels[p + 0] = r;
    image.pixels[p + 1] = g;
    image.pixels[p + 2] = b;
    image.pixels[p + 3] = 255;
}

static make3d::ImageRGBA CreateCharacterImage() {
    auto image = Blank(128, 160);
    auto ellipse = [&](float cx, float cy, float rx, float ry, unsigned char r, unsigned char g, unsigned char b) {
        for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
            float dx = (x - cx) / rx;
            float dy = (y - cy) / ry;
            if (dx * dx + dy * dy <= 1.0f) Set(image, x, y, r, g, b);
        }
    };
    auto capsule = [&](float x0, float y0, float x1, float y1, float radius, unsigned char r, unsigned char g, unsigned char b) {
        float vx = x1 - x0, vy = y1 - y0, len2 = vx * vx + vy * vy;
        for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
            float t = len2 > 0 ? ((x - x0) * vx + (y - y0) * vy) / len2 : 0.0f;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            float px = x0 + vx * t, py = y0 + vy * t;
            float dx = x - px, dy = y - py;
            if (dx * dx + dy * dy <= radius * radius) Set(image, x, y, r, g, b);
        }
    };
    ellipse(64, 26, 16, 18, 235, 210, 180);
    capsule(64, 48, 64, 92, 20, 70, 130, 220);
    capsule(43, 55, 22, 92, 7, 80, 140, 230);
    capsule(85, 55, 106, 92, 7, 80, 140, 230);
    capsule(55, 94, 47, 148, 8, 55, 80, 190);
    capsule(73, 94, 81, 148, 8, 55, 80, 190);
    return image;
}

static make3d::ImageRGBA CreateBuildingImage() {
    auto image = Blank(128, 128);
    for (int y = 24; y < 116; ++y) for (int x = 34; x < 94; ++x) Set(image, x, y, 150, 155, 165);
    for (int y = 16; y < 28; ++y) for (int x = 28; x < 100; ++x) Set(image, x, y, 105, 90, 80);
    for (int row = 0; row < 3; ++row) for (int col = 0; col < 3; ++col) {
        int x0 = 44 + col * 17;
        int y0 = 38 + row * 20;
        for (int y = y0; y < y0 + 9; ++y) for (int x = x0; x < x0 + 9; ++x) Set(image, x, y, 80, 120, 170);
    }
    for (int y = 96; y < 116; ++y) for (int x = 59; x < 69; ++x) Set(image, x, y, 80, 65, 55);
    return image;
}

static make3d::ImageRGBA CreatePropImage() {
    auto image = Blank(128, 128);
    for (int y = 30; y < 100; ++y) for (int x = 48; x < 80; ++x) Set(image, x, y, 110, 160, 110);
    for (int y = 24; y < 40; ++y) for (int x = 42; x < 86; ++x) Set(image, x, y, 130, 180, 130);
    for (int y = 88; y < 108; ++y) for (int x = 38; x < 90; ++x) Set(image, x, y, 90, 120, 90);
    return image;
}

static bool SaveTGA(const fs::path& path, const make3d::ImageRGBA& image) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    unsigned char header[18] = {};
    header[2] = 2;
    header[12] = static_cast<unsigned char>(image.width & 255);
    header[13] = static_cast<unsigned char>((image.width >> 8) & 255);
    header[14] = static_cast<unsigned char>(image.height & 255);
    header[15] = static_cast<unsigned char>((image.height >> 8) & 255);
    header[16] = 32;
    header[17] = 8 | 0x20;
    file.write(reinterpret_cast<const char*>(header), sizeof(header));
    for (int i = 0; i < image.width * image.height; ++i) {
        size_t p = static_cast<size_t>(i) * 4;
        unsigned char bgra[4] = {image.pixels[p + 2], image.pixels[p + 1], image.pixels[p + 0], image.pixels[p + 3]};
        file.write(reinterpret_cast<const char*>(bgra), 4);
    }
    return true;
}

static int GenerateOne(const fs::path& root, const std::string& name, const make3d::ImageRGBA& image) {
    fs::path sampleDir = root / name;
    fs::create_directories(sampleDir);
    if (!SaveTGA(sampleDir / "input.tga", image)) {
        std::cerr << "failed to write input for " << name << "\n";
        return 1;
    }

    make3d::ReconstructionReport report;
    auto mask = make3d::BuildForegroundMask(image, &report);
    make3d::MaskRefineOptions refine;
    refine.keepLargestComponentOnly = true;
    refine.smoothingIterations = 2;
    make3d::RefineForegroundMask(mask, image.width, image.height, refine);

    make3d::AdvancedOptions advanced;
    advanced.mode = make3d::ReconstructionMode::HybridVolume;
    advanced.quality = make3d::QualityPreset::Detailed;
    advanced.maxGridResolution = 96;
    advanced.volumeRadialSegments = 24;
    auto depth = make3d::PrepareDepth(image, std::nullopt, mask, advanced, &report);

    make3d::CompleteGameAssetOptions options;
    options.textureSize = 256;
    options.generator.enforceSafeMeshQuality = true;
    auto result = make3d::BuildCompleteGameAsset(image, depth, mask, sampleDir / "complete", options);
    if (!result.ok) {
        std::cerr << "safe sample generation failed for " << name << ": " << result.message << "\n";
        return 1;
    }

    std::ofstream guide(sampleDir / "OPEN_THIS_FIRST.txt", std::ios::binary);
    guide << "Phase 0 safe sample. Open this first:\n";
    guide << result.texturedObjPath.u8string() << "\n";
    guide << "Fallback glTF: " << result.base.gltfPath.u8string() << "\n";
    guide << "Quality gate is recorded in: " << result.base.reportPath.u8string() << "\n";
    std::cout << name << ": " << result.finalValidation.vertices << " vertices, " << result.finalValidation.triangles << " triangles\n";
    return 0;
}

int main(int argc, char** argv) {
    fs::path root = argc >= 2 ? fs::path(argv[1]) : fs::path("samples/generated");
    int failures = 0;
    failures += GenerateOne(root, "character_safe_game_asset", CreateCharacterImage());
    failures += GenerateOne(root, "building_safe_game_asset", CreateBuildingImage());
    failures += GenerateOne(root, "prop_safe_game_asset", CreatePropImage());
    if (failures == 0) std::cout << "Generated Phase 0 safe samples under " << root.u8string() << "\n";
    return failures == 0 ? 0 : 1;
}
