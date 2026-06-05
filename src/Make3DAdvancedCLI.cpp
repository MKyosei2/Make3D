#include "Make3DAdvancedCore.h"
#include "Make3DBenchmark.h"
#include "Make3DProductionPipeline.h"

#include <algorithm>
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
        "Default behavior:\n"
        "  Runs only the production-safe hero/game-asset pipeline. Legacy raw reconstruction\n"
        "  and raw/polished/voxel fallback outputs are not exposed from this CLI.\n\n"
        "Options:\n"
        "  --mode auto|relief|volume|hybrid\n"
        "  --quality draft|standard|detailed\n"
        "  --grid <number>          Max analysis grid resolution. Default: 192\n"
        "  --segments <number>      Procedural radial segments. Default: 20\n"
        "  --benchmark              Also run profiled preview/final mesh generation and write benchmark reports\n"
        "  --no-debug               Do not write debug images\n";
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
    bool writeDebug = true;
    bool benchmark = false;

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
        } else if (Equals(arg, "--benchmark")) {
            benchmark = true;
        } else if (Equals(arg, "--no-debug")) {
            options.writeDebugImages = false;
            writeDebug = false;
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

    make3d::ProductionPipelineOptions production;
    production.reconstruction = options;
    production.exportHeroCharacter = true;
    production.exportGameAsset = true;
    production.exportVertexColorGltf = true;
    production.writeDebugImages = writeDebug;
    production.writeReports = true;
    production.maskRefine.keepLargestComponentOnly = true;
    production.gameAsset.targetHeight = 2.0f;
    production.gameAsset.gridResolution = options.maxGridResolution;
    production.gameAsset.radialSegments = options.volumeRadialSegments;
    production.gameAsset.useAssetAnalysisPlan = true;
    production.gameAsset.enforceSafeMeshQuality = true;

    auto result = make3d::BuildProductionModelFromImage(*input, depth, output, production);
    if (!result.ok) {
        std::cerr << "Failed: " << result.message << "\n";
        return 3;
    }

    std::cout << result.message << "\n";
    std::cout << "Pipeline: production-safe hero/game-asset\n";
    std::cout << "Hero OBJ: " << result.heroObjPath.u8string() << "\n";
    std::cout << "Hero semantic glTF: " << result.heroVertexColorGltfPath.u8string() << "\n";
    std::cout << "Game asset OBJ: " << result.gameAssetObjPath.u8string() << "\n";
    std::cout << "Game asset glTF: " << result.gameAssetGltfPath.u8string() << "\n";
    std::cout << "Game asset report: " << result.gameAssetReportPath.u8string() << "\n";
    std::cout << "Game asset manifest: " << result.gameAssetManifestPath.u8string() << "\n";
    std::cout << "Production report: " << result.productionReportPath.u8string() << "\n";

    if (benchmark) {
        make3d::AdvancedOptions benchmarkOptions = options;
        benchmarkOptions.writeDebugImages = writeDebug;
        std::filesystem::path benchmarkOutput = output / "benchmark";
        auto profiled = make3d::BuildProfiledModelFromImage(*input, depth, benchmarkOutput, benchmarkOptions);
        if (!profiled.ok) {
            std::cerr << "Benchmark generation failed: " << profiled.message << "\n";
            return 4;
        }
        std::cout << "Benchmark report JSON: " << profiled.benchmarkJsonPath.u8string() << "\n";
        std::cout << "Benchmark report Markdown: " << profiled.benchmarkMarkdownPath.u8string() << "\n";
        std::cout << "Preview mesh triangles: " << (profiled.previewMesh.indices.size() / 3) << "\n";
        std::cout << "Final mesh triangles: " << (profiled.finalMesh.indices.size() / 3) << "\n";
    }

    return 0;
}
