#include "Make3DAdvancedCore.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

static make3d::ImageRGBA CreateCharacterImage() {
    constexpr int W = 128;
    constexpr int H = 160;
    make3d::ImageRGBA image;
    image.width = W;
    image.height = H;
    image.pixels.assign(static_cast<size_t>(W) * H * 4, 0);

    auto set = [&](int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        size_t p = (static_cast<size_t>(y) * W + x) * 4;
        image.pixels[p + 0] = r;
        image.pixels[p + 1] = g;
        image.pixels[p + 2] = b;
        image.pixels[p + 3] = a;
    };
    auto ellipse = [&](float cx, float cy, float rx, float ry, unsigned char r, unsigned char g, unsigned char b) {
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
            float dx = (x - cx) / rx;
            float dy = (y - cy) / ry;
            if (dx * dx + dy * dy <= 1.0f) set(x, y, r, g, b, 255);
        }
    };
    auto capsule = [&](float x0, float y0, float x1, float y1, float radius, unsigned char r, unsigned char g, unsigned char b) {
        float vx = x1 - x0;
        float vy = y1 - y0;
        float len2 = vx * vx + vy * vy;
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
            float t = len2 > 0 ? ((x - x0) * vx + (y - y0) * vy) / len2 : 0;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            float px = x0 + vx * t;
            float py = y0 + vy * t;
            float dx = x - px;
            float dy = y - py;
            if (dx * dx + dy * dy <= radius * radius) set(x, y, r, g, b, 255);
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

static make3d::ImageRGBA CreateBoxImage() {
    constexpr int W = 128;
    constexpr int H = 128;
    make3d::ImageRGBA image;
    image.width = W;
    image.height = H;
    image.pixels.assign(static_cast<size_t>(W) * H * 4, 0);
    for (int y = 28; y < 100; ++y) {
        for (int x = 24; x < 104; ++x) {
            size_t p = (static_cast<size_t>(y) * W + x) * 4;
            image.pixels[p + 0] = static_cast<unsigned char>(150 + (x - 24));
            image.pixels[p + 1] = static_cast<unsigned char>(110 + (y - 28));
            image.pixels[p + 2] = 80;
            image.pixels[p + 3] = 255;
        }
    }
    return image;
}

static bool SavePPM(const fs::path& path, const make3d::ImageRGBA& image) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    for (int i = 0; i < image.width * image.height; ++i) {
        size_t p = static_cast<size_t>(i) * 4;
        char rgb[3] = {
            static_cast<char>(image.pixels[p + 0]),
            static_cast<char>(image.pixels[p + 1]),
            static_cast<char>(image.pixels[p + 2])
        };
        file.write(rgb, 3);
    }
    return true;
}

static int GenerateOne(const fs::path& root, const std::string& name, const make3d::ImageRGBA& image, make3d::ReconstructionMode mode) {
    fs::path sampleDir = root / name;
    fs::create_directories(sampleDir);
    fs::path inputPath = sampleDir / "input.ppm";
    if (!SavePPM(inputPath, image)) {
        std::cerr << "failed to write " << inputPath << "\n";
        return 1;
    }

    make3d::AdvancedOptions options;
    options.mode = mode;
    options.quality = make3d::QualityPreset::Detailed;
    options.maxGridResolution = 160;
    options.volumeRadialSegments = 24;
    options.exportObj = true;
    options.exportGltf = true;
    options.writeDebugImages = true;

    auto output = make3d::BuildModelFromImage(inputPath, std::nullopt, sampleDir / "output", options);
    if (!output.ok) {
        std::cerr << "sample generation failed for " << name << ": " << output.message << "\n";
        return 1;
    }
    std::cout << name << ": " << output.report.vertices << " vertices, " << output.report.triangles << " triangles\n";
    return 0;
}

int main(int argc, char** argv) {
    fs::path root = argc >= 2 ? fs::path(argv[1]) : fs::path("samples/generated");
    int failures = 0;
    failures += GenerateOne(root, "character_hybrid", CreateCharacterImage(), make3d::ReconstructionMode::HybridVolume);
    failures += GenerateOne(root, "box_volume", CreateBoxImage(), make3d::ReconstructionMode::SilhouetteVolume);
    if (failures == 0) {
        std::cout << "Generated advanced samples under " << root.u8string() << "\n";
    }
    return failures == 0 ? 0 : 1;
}
