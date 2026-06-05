#include "Make3DProductionPipeline.h"
#include "Make3DHeroDetailEnhancer.h"
#include "Make3DHeroFineDetailPass.h"
#include "Make3DHeroSemanticGltfExporter.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <vector>

namespace make3d {

namespace fs = std::filesystem;

namespace {

static void WriteDebugMask(const fs::path& path, int w, int h, const std::vector<std::uint8_t>& mask) {
    std::vector<std::uint8_t> rgb(static_cast<size_t>(w) * h * 3, 0);
    for (int i = 0; i < w * h; ++i) {
        std::uint8_t v = mask[static_cast<size_t>(i)] ? 255 : 0;
        rgb[static_cast<size_t>(i) * 3 + 0] = v;
        rgb[static_cast<size_t>(i) * 3 + 1] = v;
        rgb[static_cast<size_t>(i) * 3 + 2] = v;
    }
    SaveDebugPPM(path, w, h, rgb, nullptr);
}

static void WriteDebugDepth(const fs::path& path, int w, int h, const DepthImage& depth) {
    std::vector<std::uint8_t> rgb(static_cast<size_t>(w) * h * 3, 0);
    for (int i = 0; i < w * h; ++i) {
        float d = depth.values[static_cast<size_t>(i)];
        if (d < 0.0f) d = 0.0f;
        if (d > 1.0f) d = 1.0f;
        std::uint8_t v = static_cast<std::uint8_t>(d * 255.0f);
        rgb[static_cast<size_t>(i) * 3 + 0] = v;
        rgb[static_cast<size_t>(i) * 3 + 1] = v;
        rgb[static_cast<size_t>(i) * 3 + 2] = v;
    }
    SaveDebugPPM(path, w, h, rgb, nullptr);
}

static std::string MakeProductionMarkdown(const ProductionPipelineResult& result) {
    std::ostringstream o;
    o << "# Make3D Production Pipeline Report\n\n";
    o << "## Shape inference\n\n" << result.shapeInferenceReport.ToMarkdown() << "\n";
    o << "## Learned shape model\n\n" << result.learnedShapeReport.ToMarkdown() << "\n";
    o << "## Hero character reconstruction\n\n" << result.heroReport.ToMarkdown() << "\n";
    o << "## Reconstruction\n\n" << result.reconstructionReport.ToMarkdown() << "\n";
    o << "## Mask refinement\n\n" << result.maskReport.ToMarkdown() << "\n";
    o << "## Mesh polish\n\n" << result.polishReport.ToMarkdown() << "\n";
    o << "## Voxel-style volume\n\n" << result.voxelReport.ToMarkdown() << "\n";
    o << "## Output files\n\n";
    o << "- hero/make3d_hero_character.obj\n";
    o << "- hero/make3d_hero_character_material.gltf\n";
    o << "- hero/make3d_hero_character_vertex_color.gltf\n";
    o << "- raw/make3d_raw.obj\n";
    o << "- raw/make3d_raw_material.gltf\n";
    o << "- polished/make3d_polished.obj\n";
    o << "- polished/make3d_polished_material.gltf\n";
    o << "- polished/make3d_polished_vertex_color.gltf\n";
    o << "- voxel/make3d_voxel_volume.obj\n";
    o << "- voxel/make3d_voxel_volume_material.gltf\n";
    o << "- voxel/make3d_voxel_volume_vertex_color.gltf\n";
    o << "- debug_mask_refined.ppm\n";
    o << "- debug_depth_refined.ppm\n";
    o << "- debug_depth_inferred.ppm\n";
    o << "- debug_depth_learned.ppm\n";
    return o.str();
}

static std::string MakeProductionJson(const ProductionPipelineResult& result) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"shapeInference\": " << result.shapeInferenceReport.ToJson() << ",\n";
    o << "  \"learnedShape\": " << result.learnedShapeReport.ToJson() << ",\n";
    o << "  \"heroCharacter\": " << result.heroReport.ToJson() << ",\n";
    o << "  \"reconstruction\": " << result.reconstructionReport.ToJson() << ",\n";
    o << "  \"maskRefine\": " << result.maskReport.ToJson() << ",\n";
    o << "  \"polish\": " << result.polishReport.ToJson() << ",\n";
    o << "  \"voxelVolume\": " << result.voxelReport.ToJson() << ",\n";
    o << "  \"heroMaterialGltf\": \"hero/make3d_hero_character_material.gltf\",\n";
    o << "  \"heroVertexColorGltf\": \"hero/make3d_hero_character_vertex_color.gltf\",\n";
    o << "  \"rawMaterialGltf\": \"raw/make3d_raw_material.gltf\",\n";
    o << "  \"polishedMaterialGltf\": \"polished/make3d_polished_material.gltf\",\n";
    o << "  \"polishedVertexColorGltf\": \"polished/make3d_polished_vertex_color.gltf\",\n";
    o << "  \"voxelMaterialGltf\": \"voxel/make3d_voxel_volume_material.gltf\",\n";
    o << "  \"voxelVertexColorGltf\": \"voxel/make3d_voxel_volume_vertex_color.gltf\"\n";
    o << "}\n";
    return o.str();
}

} // namespace

ProductionPipelineResult BuildProductionModelFromImage(
    const fs::path& colorPath,
    const std::optional<fs::path>& depthPath,
    const fs::path& outputDir,
    const ProductionPipelineOptions& options) {

    ProductionPipelineResult result;
    std::string error;
    auto color = LoadImageRGBA(colorPath, &error);
    if (!color) {
        result.message = error;
        return result;
    }

    std::optional<DepthImage> providedDepth;
    if (depthPath) providedDepth = LoadDepthImage(*depthPath, &error);

    fs::create_directories(outputDir);
    fs::create_directories(outputDir / "raw");
    fs::create_directories(outputDir / "polished");
    fs::create_directories(outputDir / "voxel");
    fs::create_directories(outputDir / "hero");

    result.reconstructionReport.imageWidth = color->width;
    result.reconstructionReport.imageHeight = color->height;

    std::vector<std::uint8_t> mask = BuildForegroundMask(*color, &result.reconstructionReport);
    if (result.reconstructionReport.foregroundPixels == 0) {
        result.message = "Foreground extraction failed.";
        return result;
    }

    result.maskReport = RefineForegroundMask(mask, color->width, color->height, options.maskRefine);
    result.reconstructionReport.foregroundPixels = result.maskReport.foregroundAfter;
    result.reconstructionReport.foregroundCoverage = static_cast<float>(result.maskReport.foregroundAfter) / static_cast<float>(color->width * color->height);

    DepthImage depth = PrepareDepth(*color, providedDepth, mask, options.reconstruction, &result.reconstructionReport);
    DepthImage reconstructionDepth = depth;
    if (options.enableShapeInference) {
        result.shapeInferenceReport = RunShapeInference(*color, mask, depth, options.shapeInference);
        if (!result.shapeInferenceReport.adjustedDepth.values.empty()) {
            reconstructionDepth = result.shapeInferenceReport.adjustedDepth;
        }
    }
    DepthImage inferredDepth = reconstructionDepth;
    if (options.enableLearnedShapeModel) {
        result.learnedShapeReport = RunLearnedShapeModel(*color, mask, reconstructionDepth, result.shapeInferenceReport, options.learnedShape);
        if (result.learnedShapeReport.ok && !result.learnedShapeReport.learnedDepth.values.empty()) {
            reconstructionDepth = result.learnedShapeReport.learnedDepth;
        }
    }

    if (options.exportHeroCharacter) {
        result.heroMesh = BuildHeroCharacterMesh(*color, reconstructionDepth, mask, options.heroCharacter, &result.heroReport);
        AddHeroDetailVolumes(result.heroMesh, options.heroCharacter, &result.heroReport);
        AddHeroFineDetails(result.heroMesh, options.heroCharacter);
        result.heroReport.vertices = static_cast<int>(result.heroMesh.positions.size() / 3);
        result.heroReport.triangles = static_cast<int>(result.heroMesh.indices.size() / 3);
        if (!result.heroMesh.positions.empty() && !result.heroMesh.indices.empty()) {
            result.heroObjPath = outputDir / "hero" / "make3d_hero_character.obj";
            result.heroMaterialGltfPath = outputDir / "hero" / "make3d_hero_character_material.gltf";
            result.heroVertexColorGltfPath = outputDir / "hero" / "make3d_hero_character_vertex_color.gltf";
            GltfMaterialOptions heroMaterial;
            heroMaterial.materialName = "Make3DHeroCharacterMaterial";
            heroMaterial.baseColorFactor = {0.76f, 0.70f, 0.64f, 1.0f};
            if (!ExportOBJ(result.heroMesh, result.heroObjPath, "", &error)) { result.message = error; return result; }
            if (!ExportGLTFWithMaterial(result.heroMesh, result.heroMaterialGltfPath, heroMaterial, &error)) { result.message = error; return result; }
            if (options.exportVertexColorGltf) {
                HeroSemanticPalette palette = ExtractHeroSemanticPalette(*color, mask);
                HeroSemanticGltfOptions semanticOptions;
                semanticOptions.materialName = "Make3DHeroSemanticVertexColorMaterial";
                if (!ExportHeroSemanticGLTF(result.heroMesh, palette, result.heroVertexColorGltfPath, semanticOptions, &error)) { result.message = error; return result; }
            }
        }
    }

    result.rawMesh = ReconstructMesh(*color, reconstructionDepth, mask, options.reconstruction, &result.reconstructionReport);
    if (result.rawMesh.positions.empty() || result.rawMesh.indices.empty()) {
        result.message = "Mesh reconstruction failed.";
        return result;
    }

    result.polishedMesh = result.rawMesh;
    result.polishReport = PolishMesh(result.polishedMesh, options.polish);

    if (options.exportVoxelVolume) {
        result.voxelMesh = BuildVoxelVolumeMesh(*color, reconstructionDepth, mask, options.voxel, &result.voxelReport);
        if (!result.voxelMesh.positions.empty() && !result.voxelMesh.indices.empty()) {
            MeshPolishOptions voxelPolish = options.polish;
            voxelPolish.keepLargestComponentOnly = true;
            voxelPolish.smoothingIterations = std::max(2, options.polish.smoothingIterations / 2);
            PolishMesh(result.voxelMesh, voxelPolish);
            result.voxelObjPath = outputDir / "voxel" / "make3d_voxel_volume.obj";
            result.voxelMaterialGltfPath = outputDir / "voxel" / "make3d_voxel_volume_material.gltf";
            result.voxelVertexColorGltfPath = outputDir / "voxel" / "make3d_voxel_volume_vertex_color.gltf";
            GltfMaterialOptions material;
            material.materialName = "Make3DVoxelVolumeMaterial";
            material.baseColorFactor = {0.66f, 0.76f, 0.90f, 1.0f};
            if (!ExportOBJ(result.voxelMesh, result.voxelObjPath, "", &error)) { result.message = error; return result; }
            if (!ExportGLTFWithMaterial(result.voxelMesh, result.voxelMaterialGltfPath, material, &error)) { result.message = error; return result; }
            if (options.exportVertexColorGltf) {
                VertexColorGltfOptions colorOptions;
                colorOptions.materialName = "Make3DVoxelVertexColorMaterial";
                if (!ExportGLTFWithVertexColors(result.voxelMesh, *color, result.voxelVertexColorGltfPath, colorOptions, &error)) { result.message = error; return result; }
            }
        }
    }

    if (options.exportRaw) {
        result.rawObjPath = outputDir / "raw" / "make3d_raw.obj";
        result.rawMaterialGltfPath = outputDir / "raw" / "make3d_raw_material.gltf";
        if (!ExportOBJ(result.rawMesh, result.rawObjPath, "", &error)) { result.message = error; return result; }
        GltfMaterialOptions material;
        material.materialName = "Make3DRawMaterial";
        material.baseColorFactor = {0.62f, 0.62f, 0.62f, 1.0f};
        if (!ExportGLTFWithMaterial(result.rawMesh, result.rawMaterialGltfPath, material, &error)) { result.message = error; return result; }
    }

    if (options.exportPolished) {
        result.polishedObjPath = outputDir / "polished" / "make3d_polished.obj";
        result.polishedMaterialGltfPath = outputDir / "polished" / "make3d_polished_material.gltf";
        result.polishedVertexColorGltfPath = outputDir / "polished" / "make3d_polished_vertex_color.gltf";
        if (!ExportOBJ(result.polishedMesh, result.polishedObjPath, "", &error)) { result.message = error; return result; }
        GltfMaterialOptions material;
        material.materialName = "Make3DPolishedMaterial";
        material.baseColorFactor = {0.72f, 0.78f, 0.86f, 1.0f};
        if (!ExportGLTFWithMaterial(result.polishedMesh, result.polishedMaterialGltfPath, material, &error)) { result.message = error; return result; }
        if (options.exportVertexColorGltf) {
            VertexColorGltfOptions colorOptions;
            colorOptions.materialName = "Make3DPolishedVertexColorMaterial";
            if (!ExportGLTFWithVertexColors(result.polishedMesh, *color, result.polishedVertexColorGltfPath, colorOptions, &error)) { result.message = error; return result; }
        }
    }

    if (options.writeDebugImages) {
        WriteDebugMask(outputDir / "debug_mask_refined.ppm", color->width, color->height, mask);
        WriteDebugDepth(outputDir / "debug_depth_refined.ppm", color->width, color->height, depth);
        WriteDebugDepth(outputDir / "debug_depth_inferred.ppm", color->width, color->height, inferredDepth);
        WriteDebugDepth(outputDir / "debug_depth_learned.ppm", color->width, color->height, reconstructionDepth);
    }

    if (options.writeReports) {
        result.productionReportPath = outputDir / "production_report.md";
        std::ofstream md(result.productionReportPath, std::ios::binary);
        md << MakeProductionMarkdown(result);
        std::ofstream js(outputDir / "production_report.json", std::ios::binary);
        js << MakeProductionJson(result);
    }

    result.ok = true;
    result.message = "Production pipeline finished.";
    return result;
}

} // namespace make3d
