#pragma once

#include "Make3DAdvancedCore.h"
#include "Make3DGameAssetGenerator.h"

#include <filesystem>
#include <string>
#include <vector>

namespace make3d {

struct CompleteGameAssetOptions {
    GameAssetGeneratorOptions generator;
    bool repairMesh = true;
    bool regeneratePlanarUv = true;
    bool writeProjectedTexture = true;
    bool writeTexturedObj = true;
    bool writeImportGuide = true;
    bool writeUnityMetadata = true;
    int textureSize = 256;
    int lodGridStep = 2;
};

struct MeshRepairReport {
    int inputVertices = 0;
    int inputTriangles = 0;
    int outputVertices = 0;
    int outputTriangles = 0;
    int removedInvalidTriangles = 0;
    int removedDegenerateTriangles = 0;
    int repairedNonFinitePositions = 0;
    int regeneratedUvVertices = 0;
    bool normalsRecomputed = false;
    std::vector<std::string> warnings;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

struct CompleteGameAssetResult {
    bool ok = false;
    std::string message;
    GameAssetGenerationResult base;
    MeshData repairedMesh;
    MeshRepairReport repairReport;
    GameAssetValidationReport finalValidation;
    std::filesystem::path texturedObjPath;
    std::filesystem::path texturePath;
    std::filesystem::path completeReportPath;
    std::filesystem::path completeManifestPath;
    std::filesystem::path importGuidePath;
    std::filesystem::path unityMetadataPath;
};

MeshRepairReport RepairGameAssetMesh(MeshData& mesh, bool regeneratePlanarUv = true);

bool WriteProjectedTexturePPM(
    const ImageRGBA& color,
    const std::vector<std::uint8_t>& mask,
    const std::filesystem::path& texturePath,
    int textureSize,
    std::string* error = nullptr);

CompleteGameAssetResult BuildCompleteGameAsset(
    const ImageRGBA& color,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& mask,
    const std::filesystem::path& outputDir,
    const CompleteGameAssetOptions& options = CompleteGameAssetOptions{});

} // namespace make3d
