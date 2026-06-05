#include "Make3DProductionPipeline.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

static make3d::ImageRGBA CreateNoisyCharacterImage() {
    constexpr int W = 160;
    constexpr int H = 192;
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
            float t = len2 > 0.0f ? ((x - x0) * vx + (y - y0) * vy) / len2 : 0.0f;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            float px = x0 + vx * t;
            float py = y0 + vy * t;
            float dx = x - px;
            float dy = y - py;
            if (dx * dx + dy * dy <= radius * radius) set(x, y, r, g, b, 255);
        }
    };

    ellipse(80, 34, 18, 22, 235, 210, 180);
    capsule(80, 62, 80, 112, 25, 70, 130, 220);
    capsule(55, 70, 24, 116, 8, 80, 140, 230);
    capsule(105, 70, 136, 116, 8, 80, 140, 230);
    capsule(68, 112, 56, 176, 9, 55, 80, 190);
    capsule(92, 112, 104, 176, 9, 55, 80, 190);

    // Add synthetic small islands and jagged noise to prove the production pipeline removes them.
    for (int y = 20; y < 28; ++y) for (int x = 130; x < 138; ++x) set(x, y, 255, 255, 255, 255);
    for (int y = 150; y < 156; ++y) for (int x = 18; x < 26; ++x) set(x, y, 255, 255, 255, 255);
    for (int i = 0; i < 80; ++i) {
        int x = (i * 37 + 13) % W;
        int y = (i * 53 + 17) % H;
        if ((i % 3) == 0) set(x, y, 255, 255, 255, 255);
    }

    return image;
}

static bool SaveTGA(const fs::path& path, const make3d::ImageRGBA& image) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    unsigned char header[18] = {};
    header[2] = 2;
    header[12] = static_cast<unsigned char>(image.width & 0xFF);
    header[13] = static_cast<unsigned char>((image.width >> 8) & 0xFF);
    header[14] = static_cast<unsigned char>(image.height & 0xFF);
    header[15] = static_cast<unsigned char>((image.height >> 8) & 0xFF);
    header[16] = 32;
    header[17] = 8 | 0x20;
    file.write(reinterpret_cast<const char*>(header), sizeof(header));

    for (int i = 0; i < image.width * image.height; ++i) {
        size_t p = static_cast<size_t>(i) * 4;
        unsigned char bgra[4] = {
            image.pixels[p + 2],
            image.pixels[p + 1],
            image.pixels[p + 0],
            image.pixels[p + 3]
        };
        file.write(reinterpret_cast<const char*>(bgra), 4);
    }
    return true;
}

int main(int argc, char** argv) {
    fs::path outDir = argc >= 2 ? fs::path(argv[1]) : fs::path("production_pipeline_sample");
    fs::create_directories(outDir);

    fs::path input = outDir / "input_noisy_character.tga";
    if (!SaveTGA(input, CreateNoisyCharacterImage())) {
        std::cerr << "Failed to write input image.\n";
        return 1;
    }

    make3d::ProductionPipelineOptions options;
    options.reconstruction.mode = make3d::ReconstructionMode::HybridVolume;
    options.reconstruction.quality = make3d::QualityPreset::Detailed;
    options.reconstruction.maxGridResolution = 176;
    options.reconstruction.volumeRadialSegments = 28;
    options.maskRefine.smoothingIterations = 3;
    options.maskRefine.keepLargestComponentOnly = true;
    options.polish.smoothingIterations = 12;
    options.polish.smoothingStrength = 0.30f;
    options.polish.keepLargestComponentOnly = true;
    options.voxel.verticalSamples = 96;
    options.voxel.radialSegments = 32;
    options.exportVertexColorGltf = true;

    make3d::ProductionPipelineResult result = make3d::BuildProductionModelFromImage(input, std::nullopt, outDir / "output", options);
    if (!result.ok) {
        std::cerr << result.message << "\n";
        return 2;
    }

    std::cout << result.message << "\n";
    std::cout << "Voxel vertex-color glTF: " << result.voxelVertexColorGltfPath.u8string() << "\n";
    std::cout << "Voxel material glTF: " << result.voxelMaterialGltfPath.u8string() << "\n";
    std::cout << "Polished vertex-color glTF: " << result.polishedVertexColorGltfPath.u8string() << "\n";
    std::cout << "Polished material glTF: " << result.polishedMaterialGltfPath.u8string() << "\n";
    std::cout << "Report: " << result.productionReportPath.u8string() << "\n";
    return 0;
}
