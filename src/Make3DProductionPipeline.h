#pragma once

#include "Make3DAdvancedCore.h"
#include "Make3DGameAssetGenerator.h"
#include "Make3DGltfMaterialExporter.h"
#include "Make3DHeroCharacterModel.h"
#include "Make3DHeroFineDetailPass.h"
#include "Make3DLearnedShapeModel.h"
#include "Make3DMaskRefiner.h"
#include "Make3DShapeInference.h"

#include <filesystem>
#include <optional>
#include <string>

namespace make3d {

struct ProductionPipelineOptions {
    AdvancedOptions reconstruction;
    MaskRefineOptions maskRefine;
    ShapeInferenceOptions shapeInference;
    LearnedShapeModelOptions learnedShape;
    GameAssetGeneratorOptions gameAsset;
    bool exportHeroCharacter = true;
    bool exportGameAsset = true;
    bool exportVertexColorGltf = true;
    bool enableShapeInference = true;
    bool enableLearnedShapeModel = true;
    bool writeReports = true;
    bool writeDebugImages = true;
};

struct ProductionPipelineResult {
    bool ok = false;
    std::string message;
    MeshData heroMesh;
    MeshData gameAssetMesh;
    ReconstructionReport reconstructionReport;
    MaskRefineReport maskReport;
    ShapeInferenceResult shapeInferenceReport;
    LearnedShapeModelResult learnedShapeReport;
    HeroCharacterReport heroReport;
    HeroFineDetailReport heroFineDetailReport;
    GameAssetGenerationResult gameAssetReport;
    std::filesystem::path heroObjPath;
    std::filesystem::path heroMaterialGltfPath;
    std::filesystem::path heroVertexColorGltfPath;
    std::filesystem::path gameAssetObjPath;
    std::filesystem::path gameAssetGltfPath;
    std::filesystem::path gameAssetReportPath;
    std::filesystem::path gameAssetManifestPath;
    std::filesystem::path productionReportPath;

    // Legacy fallback paths are intentionally kept empty in hero-only review mode.
    // Tests assert this so reviewers do not accidentally open old raw/polished/voxel outputs.
    std::filesystem::path rawObjPath;
    std::filesystem::path polishedObjPath;
    std::filesystem::path voxelObjPath;
};

ProductionPipelineResult BuildProductionModelFromImage(
    const std::filesystem::path& colorPath,
    const std::optional<std::filesystem::path>& depthPath,
    const std::filesystem::path& outputDir,
    const ProductionPipelineOptions& options = ProductionPipelineOptions{});

} // namespace make3d
