#pragma once

#include "Make3DAdvancedCore.h"
#include "Make3DGameAssetGenerator.h"
#include "Make3DGltfMaterialExporter.h"
#include "Make3DHeroCharacterModel.h"
#include "Make3DLearnedShapeModel.h"
#include "Make3DMaskRefiner.h"
#include "Make3DModelPolisher.h"
#include "Make3DShapeInference.h"
#include "Make3DVertexColorGltfExporter.h"
#include "Make3DVoxelVolume.h"

#include <filesystem>
#include <optional>
#include <string>

namespace make3d {

struct ProductionPipelineOptions {
    AdvancedOptions reconstruction;
    MaskRefineOptions maskRefine;
    MeshPolishOptions polish;
    VoxelVolumeOptions voxel;
    HeroCharacterOptions heroCharacter;
    ShapeInferenceOptions shapeInference;
    LearnedShapeModelOptions learnedShape;
    GameAssetGeneratorOptions gameAsset;
    bool exportRaw = false;
    bool exportPolished = false;
    bool exportVoxelVolume = false;
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
    MeshData rawMesh;
    MeshData polishedMesh;
    MeshData voxelMesh;
    MeshData heroMesh;
    MeshData gameAssetMesh;
    ReconstructionReport reconstructionReport;
    MaskRefineReport maskReport;
    MeshPolishReport polishReport;
    VoxelVolumeReport voxelReport;
    HeroCharacterReport heroReport;
    ShapeInferenceResult shapeInferenceReport;
    LearnedShapeModelResult learnedShapeReport;
    GameAssetGenerationResult gameAssetReport;
    std::filesystem::path rawObjPath;
    std::filesystem::path rawMaterialGltfPath;
    std::filesystem::path polishedObjPath;
    std::filesystem::path polishedMaterialGltfPath;
    std::filesystem::path polishedVertexColorGltfPath;
    std::filesystem::path voxelObjPath;
    std::filesystem::path voxelMaterialGltfPath;
    std::filesystem::path voxelVertexColorGltfPath;
    std::filesystem::path heroObjPath;
    std::filesystem::path heroMaterialGltfPath;
    std::filesystem::path heroVertexColorGltfPath;
    std::filesystem::path gameAssetObjPath;
    std::filesystem::path gameAssetGltfPath;
    std::filesystem::path gameAssetReportPath;
    std::filesystem::path gameAssetManifestPath;
    std::filesystem::path productionReportPath;
};

ProductionPipelineResult BuildProductionModelFromImage(
    const std::filesystem::path& colorPath,
    const std::optional<std::filesystem::path>& depthPath,
    const std::filesystem::path& outputDir,
    const ProductionPipelineOptions& options = ProductionPipelineOptions{});

} // namespace make3d
