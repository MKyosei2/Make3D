#include "Make3DAdvancedCore.h"
#include "Make3DGltfMaterialExporter.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

void PrintUsage() {
    std::cout <<
        "Make3D Advanced CLI\n\n"
        "Usage:\n"
        "  Make3DAdvancedCLI --input <color.png> [--depth <depth.png>] --output <folder> [options]\n\n"
        "Options:\n"
        "  --mode auto|relief|volume|hybrid\n"
        "  --quality draft|standard|detailed\n"
        "  --grid <number>          Max relief grid resolution. Default: 192\n"
        "  --segments <number>      Radial segments for silhouette volume. Default: 20\n"
        "  --no-gltf                Do not export glTF\n"
        "  --no-obj                 Do not export OBJ\n"
        "  --no-debug               Do not write debug_mask.ppm / debug_depth.ppm\n";
}

bool Equals(const std::string& a, const char* b) { return a == b; }

} // namespace

int main(int argc, char** argv) {
    if (argc <= 1) {
        PrintUsage();
        return 1;
    }

    std::optional<std::filesystem::path> input;
    std::optional<std::filesystem::path> depth;
    std::filesystem::path output = "output_advanced";
    make3d::AdvancedOptions options;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto needValue = [&](const char* name) -> std::optional<std::string> {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
                return std::nullopt;
            }
            return std::string(argv[++i]);
        };

        if (Equals(arg, "--input")) {
            auto v = needValue("--input");
            if (!v) return 2;
            input = *v;
        } else if (Equals(arg, "--depth")) {
            auto v = needValue("--depth");
            if (!v) return 2;
            depth = *v;
        } else if (Equals(arg, "--output")) {
            auto v = needValue("--output");
            if (!v) return 2;
            output = *v;
        } else if (Equals(arg, "--mode")) {
            auto v = needValue("--mode");
            if (!v) return 2;
            if (*v == "auto") options.mode = make3d::ReconstructionMode::Auto;
            else if (*v == "relief") options.mode = make3d::ReconstructionMode::ReliefSurface;
            else if (*v == "volume") options.mode = make3d::ReconstructionMode::SilhouetteVolume;
            else if (*v == "hybrid") options.mode = make3d::ReconstructionMode::HybridVolume;
            else { std::cerr << "Unknown mode: " << *v << "\n"; return 2; }
        } else if (Equals(arg, "--quality")) {
            auto v = needValue("--quality");
            if (!v) return 2;
            if (*v == "draft") options.quality = make3d::QualityPreset::Draft;
            else if (*v == "standard") options.quality = make3d::QualityPreset::Standard;
            else if (*v == "detailed") options.quality = make3d::QualityPreset::Detailed;
            else { std::cerr << "Unknown quality: " << *v << "\n"; return 2; }
        } else if (Equals(arg, "--grid")) {
            auto v = needValue("--grid");
            if (!v) return 2;
            options.maxGridResolution = std::max(16, std::atoi(v->c_str()));
        } else if (Equals(arg, "--segments")) {
            auto v = needValue("--segments");
            if (!v) return 2;
            options.volumeRadialSegments = std::max(6, std::atoi(v->c_str()));
        } else if (Equals(arg, "--no-gltf")) {
            options.exportGltf = false;
        } else if (Equals(arg, "--no-obj")) {
            options.exportObj = false;
        } else if (Equals(arg, "--no-debug")) {
            options.writeDebugImages = false;
        } else if (Equals(arg, "--help") || Equals(arg, "-h")) {
            PrintUsage();
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            PrintUsage();
            return 2;
        }
    }

    if (!input) {
        std::cerr << "--input is required.\n";
        return 2;
    }

    auto result = make3d::BuildModelFromImage(*input, depth, output, options);
    if (!result.ok) {
        std::cerr << "Failed: " << result.message << "\n";
        return 3;
    }

    std::filesystem::path materialGltf = output / "make3d_advanced_material.gltf";
    if (options.exportGltf) {
        make3d::GltfMaterialOptions material;
        material.materialName = "Make3DGeneratedMaterial";
        std::string materialError;
        if (!make3d::ExportGLTFWithMaterial(result.mesh, materialGltf, material, &materialError)) {
            std::cerr << "Material glTF export failed: " << materialError << "\n";
            return 4;
        }
    }

    std::cout << result.message << "\n";
    std::cout << "Mode: " << result.report.reconstructionMode << "\n";
    std::cout << "Vertices: " << result.report.vertices << "\n";
    std::cout << "Triangles: " << result.report.triangles << "\n";
    if (options.exportObj) std::cout << "OBJ: " << result.report.objPath.u8string() << "\n";
    if (options.exportGltf) {
        std::cout << "glTF: " << result.report.gltfPath.u8string() << "\n";
        std::cout << "Material glTF: " << materialGltf.u8string() << "\n";
    }
    std::cout << "Report: " << result.report.reportPath.u8string() << "\n";
    if (!result.report.warnings.empty()) {
        std::cout << "Warnings:\n";
        for (const auto& w : result.report.warnings) std::cout << "  - " << w << "\n";
    }
    return 0;
}
