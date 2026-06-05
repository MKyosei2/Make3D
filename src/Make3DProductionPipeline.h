#pragma once

#include "Make3DAdvancedCore.h"
#include "Make3DGltfMaterialExporter.h"
#include "Make3DMaskRefiner.h"
#include "Make3DModelPolisher.h"

#include <filesystem>
#include <optional>
#include <string>

namespace make3d {

struct ProductionPipelineOptions {
    AdvancedOptions reconstruction;
    MaskRefineOptions maskRefine;
    MeshPolishOptions polish;
    bool exportRaw = true;
    bool exportPolished = true;
    bool writeReports = true;
    bool writeDebugImages = true;
};

struct ProductionPipelineResult {
    bool ok = false;
    std::string message;
    MeshData rawMesh;
    MeshData polishedMesh;
    ReconstructionReport reconstructionReport;
    MaskRefineReport maskReport;
    MeshPolishReport polishReport;
    std::filesystem::path rawObjPath;
    std::filesystem::path rawMaterialGltfPath;
    std::filesystem::path polishedObjPath;
    std::filesystem::path polishedMaterialGltfPath;
    std::filesystem::path productionReportPath;
};

ProductionPipelineResult BuildProductionModelFromImage(
    const std::filesystem::path& colorPath,
    const std::optional<std::filesystem::path>& depthPath,
    const std::filesystem::path& outputDir,
    const ProductionPipelineOptions& options = ProductionPipelineOptions{});

} // namespace make3d
