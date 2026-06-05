#include "Make3DAdvancedCore.h"
#include "Make3DCompleteGameAssetPipeline.h"
#include "Make3DMaskRefiner.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct CliOptions {
    fs::path input;
    std::optional<fs::path> depth;
    fs::path output = "make3d_game_asset_output";
    int textureSize = 256;
    int grid = 96;
    int segments = 24;
    float height = 2.0f;
    bool repair = true;
    bool texture = true;
    bool help = false;
};

static void PrintUsage() {
    std::cout << "Make3DGameAssetCLI\n";
    std::cout << "Usage:\n";
    std::cout << "  Make3DGameAssetCLI --input <image> --output <folder> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --depth <image>          Optional depth image\n";
    std::cout << "  --texture-size <n>       Projected texture size, default 256\n";
    std::cout << "  --grid <n>               Reconstruction grid, default 96\n";
    std::cout << "  --segments <n>           Radial segments, default 24\n";
    std::cout << "  --height <value>         Target asset height, default 2.0\n";
    std::cout << "  --no-repair              Skip mesh repair pass\n";
    std::cout << "  --no-texture             Skip projected texture and textured OBJ\n";
}

static bool Parse(int argc, char** argv, CliOptions& o) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) { std::cerr << "Missing value for " << name << "\n"; return nullptr; }
            return argv[++i];
        };
        if (a == "--help" || a == "-h") o.help = true;
        else if (a == "--input") { const char* v = need("--input"); if (!v) return false; o.input = v; }
        else if (a == "--depth") { const char* v = need("--depth"); if (!v) return false; o.depth = fs::path(v); }
        else if (a == "--output") { const char* v = need("--output"); if (!v) return false; o.output = v; }
        else if (a == "--texture-size") { const char* v = need("--texture-size"); if (!v) return false; o.textureSize = std::stoi(v); }
        else if (a == "--grid") { const char* v = need("--grid"); if (!v) return false; o.grid = std::stoi(v); }
        else if (a == "--segments") { const char* v = need("--segments"); if (!v) return false; o.segments = std::stoi(v); }
        else if (a == "--height") { const char* v = need("--height"); if (!v) return false; o.height = std::stof(v); }
        else if (a == "--no-repair") o.repair = false;
        else if (a == "--no-texture") o.texture = false;
        else { std::cerr << "Unknown option: " << a << "\n"; return false; }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    CliOptions cli;
    if (!Parse(argc, argv, cli)) { PrintUsage(); return 1; }
    if (cli.help || cli.input.empty()) { PrintUsage(); return cli.help ? 0 : 1; }

    std::string error;
    auto color = make3d::LoadImageRGBA(cli.input, &error);
    if (!color) { std::cerr << error << "\n"; return 2; }

    std::optional<make3d::DepthImage> providedDepth;
    if (cli.depth) {
        providedDepth = make3d::LoadDepthImage(*cli.depth, &error);
        if (!providedDepth) { std::cerr << error << "\n"; return 3; }
    }

    make3d::ReconstructionReport report;
    std::vector<std::uint8_t> mask = make3d::BuildForegroundMask(*color, &report);
    if (report.foregroundPixels <= 0) { std::cerr << "Foreground extraction failed.\n"; return 4; }

    make3d::MaskRefineOptions refine;
    refine.keepLargestComponentOnly = true;
    refine.smoothingIterations = 2;
    make3d::RefineForegroundMask(mask, color->width, color->height, refine);

    make3d::AdvancedOptions advanced;
    advanced.mode = make3d::ReconstructionMode::HybridVolume;
    advanced.quality = make3d::QualityPreset::Detailed;
    advanced.maxGridResolution = cli.grid;
    advanced.volumeRadialSegments = cli.segments;
    make3d::DepthImage depth = make3d::PrepareDepth(*color, providedDepth, mask, advanced, &report);

    make3d::CompleteGameAssetOptions options;
    options.generator.gridResolution = cli.grid;
    options.generator.radialSegments = cli.segments;
    options.generator.targetHeight = cli.height;
    options.textureSize = cli.textureSize;
    options.repairMesh = cli.repair;
    options.writeProjectedTexture = cli.texture;
    options.writeTexturedObj = cli.texture;

    auto result = make3d::BuildCompleteGameAsset(*color, depth, mask, cli.output, options);
    if (!result.ok) { std::cerr << result.message << "\n"; return 5; }

    std::cout << result.message << "\n";
    std::cout << "Asset type: " << make3d::ToString(result.base.classification.assetType) << "\n";
    std::cout << "Open first: " << result.texturedObjPath.u8string() << "\n";
    std::cout << "Fallback glTF: " << result.base.gltfPath.u8string() << "\n";
    std::cout << "Collider: " << result.base.colliderObjPath.u8string() << "\n";
    std::cout << "LOD proxy: " << result.base.lodObjPath.u8string() << "\n";
    std::cout << "Report: " << result.completeReportPath.u8string() << "\n";
    std::cout << "Manifest: " << result.completeManifestPath.u8string() << "\n";
    return 0;
}
