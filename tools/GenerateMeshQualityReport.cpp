#include "Make3DAdvancedCore.h"
#include "Make3DGltfMaterialExporter.h"
#include "Make3DMeshTools.h"
#include "Make3DModelPolisher.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

namespace fs = std::filesystem;

static make3d::ImageRGBA CreateSyntheticAssetImage() {
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
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                float dx = (x - cx) / rx;
                float dy = (y - cy) / ry;
                if (dx * dx + dy * dy <= 1.0f) set(x, y, r, g, b, 255);
            }
        }
    };

    auto capsule = [&](float x0, float y0, float x1, float y1, float radius, unsigned char r, unsigned char g, unsigned char b) {
        float vx = x1 - x0;
        float vy = y1 - y0;
        float len2 = vx * vx + vy * vy;
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                float t = len2 > 0.0f ? ((x - x0) * vx + (y - y0) * vy) / len2 : 0.0f;
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
                float px = x0 + vx * t;
                float py = y0 + vy * t;
                float dx = x - px;
                float dy = y - py;
                if (dx * dx + dy * dy <= radius * radius) set(x, y, r, g, b, 255);
            }
        }
    };

    ellipse(64, 26, 16, 18, 235, 210, 180);
    capsule(64, 50, 64, 94, 20, 70, 130, 220);
    capsule(43, 56, 21, 94, 7, 80, 140, 230);
    capsule(85, 56, 107, 94, 7, 80, 140, 230);
    capsule(55, 94, 48, 148, 8, 55, 80, 190);
    capsule(73, 94, 80, 148, 8, 55, 80, 190);
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

int main(int argc, char** argv) {
    fs::path outDir = argc >= 2 ? fs::path(argv[1]) : fs::path("mesh_quality_output");
    std::error_code ec;
    fs::create_directories(outDir, ec);

    fs::path inputPath = outDir / "quality_input.ppm";
    if (!SavePPM(inputPath, CreateSyntheticAssetImage())) {
        std::cerr << "Failed to write synthetic input image.\n";
        return 1;
    }

    make3d::AdvancedOptions options;
    options.mode = make3d::ReconstructionMode::HybridVolume;
    options.quality = make3d::QualityPreset::Detailed;
    options.maxGridResolution = 160;
    options.volumeRadialSegments = 24;
    options.exportObj = true;
    options.exportGltf = true;
    options.writeDebugImages = true;

    make3d::BuildOutput built = make3d::BuildModelFromImage(inputPath, std::nullopt, outDir / "raw", options);
    if (!built.ok) {
        std::cerr << built.message << "\n";
        return 2;
    }

    make3d::MeshValidationReport before = make3d::ValidateMesh(built.mesh);
    make3d::MeshData cleaned = built.mesh;
    make3d::MeshValidationReport after = make3d::CleanupMesh(cleaned);

    make3d::MeshData polished = cleaned;
    make3d::MeshPolishOptions polishOptions;
    polishOptions.smoothingIterations = 10;
    polishOptions.smoothingStrength = 0.32f;
    polishOptions.keepLargestComponentOnly = true;
    make3d::MeshPolishReport polish = make3d::PolishMesh(polished, polishOptions);

    std::string error;
    if (!make3d::ExportOBJ(cleaned, outDir / "cleaned" / "make3d_cleaned.obj", "", &error)) {
        std::cerr << error << "\n";
        return 3;
    }
    if (!make3d::ExportGLTF(cleaned, outDir / "cleaned" / "make3d_cleaned.gltf", &error)) {
        std::cerr << error << "\n";
        return 4;
    }

    make3d::GltfMaterialOptions material;
    material.materialName = "Make3DCleanedMaterial";
    if (!make3d::ExportGLTFWithMaterial(cleaned, outDir / "cleaned" / "make3d_cleaned_material.gltf", material, &error)) {
        std::cerr << error << "\n";
        return 5;
    }

    make3d::GltfMaterialOptions polishedMaterial;
    polishedMaterial.materialName = "Make3DPolishedMaterial";
    polishedMaterial.baseColorFactor = {0.72f, 0.78f, 0.86f, 1.0f};
    if (!make3d::ExportOBJ(polished, outDir / "polished" / "make3d_polished.obj", "", &error)) {
        std::cerr << error << "\n";
        return 6;
    }
    if (!make3d::ExportGLTFWithMaterial(polished, outDir / "polished" / "make3d_polished_material.gltf", polishedMaterial, &error)) {
        std::cerr << error << "\n";
        return 7;
    }

    std::ofstream md(outDir / "mesh_quality_report.md", std::ios::binary);
    md << "# Make3D Mesh Quality Report\n\n";
    md << "## Before cleanup\n\n" << before.ToMarkdown() << "\n";
    md << "## After cleanup\n\n" << after.ToMarkdown() << "\n";
    md << "## Polished mesh\n\n" << polish.ToMarkdown() << "\n";
    md << "## Exported files\n\n";
    md << "- raw/make3d_advanced.obj\n";
    md << "- raw/make3d_advanced.gltf\n";
    md << "- cleaned/make3d_cleaned.obj\n";
    md << "- cleaned/make3d_cleaned.gltf\n";
    md << "- cleaned/make3d_cleaned_material.gltf\n";
    md << "- polished/make3d_polished.obj\n";
    md << "- polished/make3d_polished_material.gltf\n";

    std::ofstream js(outDir / "mesh_quality_report.json", std::ios::binary);
    js << "{\n";
    js << "  \"before\": " << before.ToJson() << ",\n";
    js << "  \"after\": " << after.ToJson() << ",\n";
    js << "  \"polished\": " << polish.ToJson() << ",\n";
    js << "  \"cleanedMaterialGltf\": \"cleaned/make3d_cleaned_material.gltf\",\n";
    js << "  \"polishedMaterialGltf\": \"polished/make3d_polished_material.gltf\"\n";
    js << "}\n";

    std::cout << "Generated mesh quality report: " << (outDir / "mesh_quality_report.md").u8string() << "\n";
    std::cout << "Cleaned material glTF: " << (outDir / "cleaned" / "make3d_cleaned_material.gltf").u8string() << "\n";
    std::cout << "Polished material glTF: " << (outDir / "polished" / "make3d_polished_material.gltf").u8string() << "\n";
    return 0;
}
