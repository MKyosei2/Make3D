#include "Make3DProductionPipeline.h"
#include "Make3DHeroDetailEnhancer.h"
#include "Make3DHeroFineDetailPass.h"
#include "Make3DHeroSemanticGltfExporter.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <vector>

namespace make3d {
namespace fs = std::filesystem;
namespace {

static void WriteDebugMask(const fs::path& p, int w, int h, const std::vector<std::uint8_t>& m) {
    std::vector<std::uint8_t> rgb(static_cast<size_t>(w) * h * 3, 0);
    for (int i = 0; i < w * h; ++i) {
        std::uint8_t v = m[static_cast<size_t>(i)] ? 255 : 0;
        rgb[static_cast<size_t>(i) * 3 + 0] = v;
        rgb[static_cast<size_t>(i) * 3 + 1] = v;
        rgb[static_cast<size_t>(i) * 3 + 2] = v;
    }
    SaveDebugPPM(p, w, h, rgb, nullptr);
}

static void WriteDebugDepth(const fs::path& p, int w, int h, const DepthImage& d) {
    std::vector<std::uint8_t> rgb(static_cast<size_t>(w) * h * 3, 0);
    for (int i = 0; i < w * h; ++i) {
        float v = d.values[static_cast<size_t>(i)];
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        std::uint8_t b = static_cast<std::uint8_t>(v * 255.0f);
        rgb[static_cast<size_t>(i) * 3 + 0] = b;
        rgb[static_cast<size_t>(i) * 3 + 1] = b;
        rgb[static_cast<size_t>(i) * 3 + 2] = b;
    }
    SaveDebugPPM(p, w, h, rgb, nullptr);
}

static std::string MaybePath(const fs::path& p, const char* disabled) {
    return p.empty() ? std::string(disabled) : p.generic_string();
}

static std::string DetailJson(const HeroFineDetailReport& r) {
    std::ostringstream o;
    o << "{\"faceFeatureVertices\":" << r.faceFeatureVertices
      << ",\"hairStrandVertices\":" << r.hairStrandVertices
      << ",\"clothingFoldVertices\":" << r.clothingFoldVertices
      << ",\"fingerVertices\":" << r.fingerVertices
      << ",\"shoeSoleVertices\":" << r.shoeSoleVertices
      << ",\"verticesAfter\":" << r.verticesAfter
      << ",\"trianglesAfter\":" << r.trianglesAfter << "}";
    return o.str();
}

static std::string DetailMarkdown(const HeroFineDetailReport& r) {
    std::ostringstream o;
    o << "# Hero Fine Detail Report\n\n| Metric | Value |\n|---|---:|\n";
    o << "| Face feature vertices | " << r.faceFeatureVertices << " |\n";
    o << "| Hair strand vertices | " << r.hairStrandVertices << " |\n";
    o << "| Clothing fold vertices | " << r.clothingFoldVertices << " |\n";
    o << "| Finger hint vertices | " << r.fingerVertices << " |\n";
    o << "| Shoe sole vertices | " << r.shoeSoleVertices << " |\n";
    o << "| Vertices after | " << r.verticesAfter << " |\n";
    o << "| Triangles after | " << r.trianglesAfter << " |\n";
    return o.str();
}

static std::string MakeProductionMarkdown(const ProductionPipelineResult& r) {
    std::ostringstream o;
    o << "# Make3D Production Pipeline Report\n\n";
    o << "## Review target\n\nOpen this first:\n\n```text\n" << MaybePath(r.heroVertexColorGltfPath, "hero output not generated") << "\n```\n\n";
    o << "The review target is generated through the hero character path: refined mask, depth inference, BuildHeroCharacterMesh, detail volumes, fine detail pass, semantic palette extraction, and COLOR_0 glTF export. Generic game-asset output is secondary.\n\n";
    if (!r.heroMesh.positions.empty()) {
        o << "## Hero character\n\n" << r.heroReport.ToMarkdown() << "\n";
        o << "## Hero fine detail\n\n" << DetailMarkdown(r.heroFineDetailReport) << "\n";
    }
    if (!r.gameAssetMesh.positions.empty()) {
        o << "## Secondary safe game asset\n\n" << r.gameAssetReport.classification.ToMarkdown() << "\n";
        o << "### Mesh validation\n\n" << r.gameAssetReport.validation.ToMarkdown() << "\n";
        o << "### Quality gate\n\n" << r.gameAssetReport.qualityGate.ToMarkdown() << "\n";
        o << "### Metadata\n\n" << r.gameAssetReport.metadata.ToMarkdown() << "\n";
    }
    o << "## Shape inference\n\n" << r.shapeInferenceReport.ToMarkdown() << "\n";
    o << "## Learned shape model\n\n" << r.learnedShapeReport.ToMarkdown() << "\n";
    o << "## Reconstruction\n\n" << r.reconstructionReport.ToMarkdown() << "\n";
    o << "## Mask refinement\n\n" << r.maskReport.ToMarkdown() << "\n";
    o << "## Output files\n\n";
    o << "- " << MaybePath(r.heroObjPath, "hero OBJ disabled or failed") << "\n";
    o << "- " << MaybePath(r.heroMaterialGltfPath, "hero material glTF disabled or failed") << "\n";
    o << "- " << MaybePath(r.heroVertexColorGltfPath, "hero review glTF disabled or failed") << "\n";
    o << "- " << MaybePath(r.gameAssetObjPath, "game asset OBJ disabled or failed") << "\n";
    o << "- " << MaybePath(r.gameAssetGltfPath, "game asset glTF disabled or failed") << "\n";
    o << "- " << MaybePath(r.gameAssetReportPath, "game asset report disabled or failed") << "\n";
    o << "- " << MaybePath(r.gameAssetManifestPath, "game asset manifest disabled or failed") << "\n";
    return o.str();
}

static std::string MakeProductionJson(const ProductionPipelineResult& r) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"reviewTarget\": \"" << MaybePath(r.heroVertexColorGltfPath, "") << "\",\n";
    o << "  \"pipelineMode\": \"hero-character-production\",\n";
    o << "  \"hero\": " << r.heroReport.ToJson() << ",\n";
    o << "  \"heroFineDetail\": " << DetailJson(r.heroFineDetailReport) << ",\n";
    o << "  \"shapeInference\": " << r.shapeInferenceReport.ToJson() << ",\n";
    o << "  \"learnedShape\": " << r.learnedShapeReport.ToJson() << ",\n";
    o << "  \"reconstruction\": " << r.reconstructionReport.ToJson() << ",\n";
    o << "  \"maskRefine\": " << r.maskReport.ToJson() << ",\n";
    o << "  \"gameAssetType\": \"" << ToString(r.gameAssetReport.classification.assetType) << "\",\n";
    o << "  \"gameAssetObj\": \"" << MaybePath(r.gameAssetObjPath, "") << "\",\n";
    o << "  \"gameAssetGltf\": \"" << MaybePath(r.gameAssetGltfPath, "") << "\",\n";
    o << "  \"gameAssetReport\": \"" << MaybePath(r.gameAssetReportPath, "") << "\",\n";
    o << "  \"gameAssetManifest\": \"" << MaybePath(r.gameAssetManifestPath, "") << "\",\n";
    o << "  \"gameAssetQualityGate\": " << r.gameAssetReport.qualityGate.ToJson() << ",\n";
    o << "  \"heroObj\": \"" << MaybePath(r.heroObjPath, "") << "\",\n";
    o << "  \"heroMaterialGltf\": \"" << MaybePath(r.heroMaterialGltfPath, "") << "\",\n";
    o << "  \"heroReviewGltf\": \"" << MaybePath(r.heroVertexColorGltfPath, "") << "\"\n";
    o << "}\n";
    return o.str();
}

static void WriteReportsAndDebugImages(const fs::path& out, const ImageRGBA& color, const std::vector<std::uint8_t>& mask, const DepthImage& depth, const DepthImage& inferred, const DepthImage& recon, ProductionPipelineResult& r, const ProductionPipelineOptions& opt) {
    if (opt.writeDebugImages) {
        WriteDebugMask(out / "debug_mask_refined.ppm", color.width, color.height, mask);
        WriteDebugDepth(out / "debug_depth_refined.ppm", color.width, color.height, depth);
        WriteDebugDepth(out / "debug_depth_inferred.ppm", color.width, color.height, inferred);
        WriteDebugDepth(out / "debug_depth_learned.ppm", color.width, color.height, recon);
    }
    if (opt.writeReports) {
        r.productionReportPath = out / "production_report.md";
        std::ofstream md(r.productionReportPath, std::ios::binary);
        md << MakeProductionMarkdown(r);
        std::ofstream js(out / "production_report.json", std::ios::binary);
        js << MakeProductionJson(r);
    }
}

static bool BuildProductionGameAsset(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const fs::path& out, const ProductionPipelineOptions& opt, ProductionPipelineResult& r) {
    if (!opt.exportGameAsset) return true;
    r.gameAssetReport = BuildGenericGameAsset(color, depth, mask, out / "game_asset", opt.gameAsset);
    if (!r.gameAssetReport.ok) {
        r.message = r.gameAssetReport.message;
        return false;
    }
    r.gameAssetMesh = r.gameAssetReport.mesh;
    r.gameAssetObjPath = r.gameAssetReport.objPath;
    r.gameAssetGltfPath = r.gameAssetReport.gltfPath;
    r.gameAssetReportPath = r.gameAssetReport.reportPath;
    r.gameAssetManifestPath = r.gameAssetReport.manifestPath;
    return true;
}

static bool ExportHeroCharacter(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const fs::path& outputDir, const ProductionPipelineOptions& opt, ProductionPipelineResult& r, std::string& error) {
    if (!opt.exportHeroCharacter) return true;
    fs::create_directories(outputDir / "hero");
    HeroCharacterOptions heroOptions;
    r.heroMesh = BuildHeroCharacterMesh(color, depth, mask, heroOptions, &r.heroReport);
    if (r.heroMesh.positions.empty() || r.heroMesh.indices.empty() || !r.heroReport.ok) {
        error = "Hero character mesh generation failed.";
        return false;
    }
    AddHeroDetailVolumes(r.heroMesh, heroOptions, &r.heroReport);
    AddHeroFineDetails(r.heroMesh, heroOptions, HeroFineDetailOptions{}, &r.heroFineDetailReport);
    RecomputeNormals(r.heroMesh);
    r.heroObjPath = outputDir / "hero" / "make3d_hero_character.obj";
    r.heroMaterialGltfPath = outputDir / "hero" / "make3d_hero_character_material.gltf";
    r.heroVertexColorGltfPath = outputDir / "hero" / "make3d_hero_character_vertex_color.gltf";
    if (!ExportOBJ(r.heroMesh, r.heroObjPath, "", &error)) return false;
    GltfMaterialOptions material;
    material.materialName = "Make3DHeroCharacterMaterial";
    material.baseColorFactor = {0.72f, 0.74f, 0.76f, 1.0f};
    if (!ExportGLTFWithMaterial(r.heroMesh, r.heroMaterialGltfPath, material, &error)) return false;
    HeroSemanticPalette palette = ExtractHeroSemanticPalette(color, mask);
    if (!ExportHeroSemanticGLTF(r.heroMesh, palette, r.heroVertexColorGltfPath, HeroSemanticGltfOptions{}, &error)) return false;
    return true;
}

} // namespace

ProductionPipelineResult BuildProductionModelFromImage(const fs::path& colorPath, const std::optional<fs::path>& depthPath, const fs::path& outputDir, const ProductionPipelineOptions& options) {
    ProductionPipelineResult result;
    std::string error;
    auto color = LoadImageRGBA(colorPath, &error);
    if (!color) { result.message = error; return result; }
    std::optional<DepthImage> providedDepth;
    if (depthPath) providedDepth = LoadDepthImage(*depthPath, &error);
    fs::create_directories(outputDir);
    if (options.exportGameAsset) fs::create_directories(outputDir / "game_asset");
    if (options.exportHeroCharacter) fs::create_directories(outputDir / "hero");
    result.reconstructionReport.imageWidth = color->width;
    result.reconstructionReport.imageHeight = color->height;
    std::vector<std::uint8_t> mask = BuildForegroundMask(*color, &result.reconstructionReport);
    if (result.reconstructionReport.foregroundPixels == 0) { result.message = "Foreground extraction failed."; return result; }
    result.maskReport = RefineForegroundMask(mask, color->width, color->height, options.maskRefine);
    result.reconstructionReport.foregroundPixels = result.maskReport.foregroundAfter;
    result.reconstructionReport.foregroundCoverage = static_cast<float>(result.maskReport.foregroundAfter) / static_cast<float>(color->width * color->height);
    DepthImage depth = PrepareDepth(*color, providedDepth, mask, options.reconstruction, &result.reconstructionReport);
    DepthImage reconstructionDepth = depth;
    if (options.enableShapeInference) {
        result.shapeInferenceReport = RunShapeInference(*color, mask, depth, options.shapeInference);
        if (!result.shapeInferenceReport.adjustedDepth.values.empty()) reconstructionDepth = result.shapeInferenceReport.adjustedDepth;
    }
    DepthImage inferredDepth = reconstructionDepth;
    if (options.enableLearnedShapeModel) {
        result.learnedShapeReport = RunLearnedShapeModel(*color, mask, reconstructionDepth, result.shapeInferenceReport, options.learnedShape);
        if (result.learnedShapeReport.ok && !result.learnedShapeReport.learnedDepth.values.empty()) reconstructionDepth = result.learnedShapeReport.learnedDepth;
    }
    result.reconstructionReport.reconstructionMode = "HeroCharacterProduction";
    result.reconstructionReport.watertightCandidate = true;
    result.reconstructionReport.warnings.push_back("Production review output is generated by the hero character path. Generic game asset output is secondary.");
    if (!ExportHeroCharacter(*color, reconstructionDepth, mask, outputDir, options, result, error)) { result.message = error; return result; }
    if (!BuildProductionGameAsset(*color, reconstructionDepth, mask, outputDir, options, result)) return result;
    if (!result.heroMesh.positions.empty()) {
        result.reconstructionReport.vertices = static_cast<int>(result.heroMesh.positions.size() / 3);
        result.reconstructionReport.triangles = static_cast<int>(result.heroMesh.indices.size() / 3);
    } else {
        result.reconstructionReport.vertices = static_cast<int>(result.gameAssetMesh.positions.size() / 3);
        result.reconstructionReport.triangles = static_cast<int>(result.gameAssetMesh.indices.size() / 3);
    }
    WriteReportsAndDebugImages(outputDir, *color, mask, depth, inferredDepth, reconstructionDepth, result, options);
    result.ok = true;
    result.message = "Production pipeline finished with real hero character output.";
    return result;
}

} // namespace make3d
