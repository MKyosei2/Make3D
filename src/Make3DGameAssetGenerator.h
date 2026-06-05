#pragma once

#include "Make3DAdvancedCore.h"
#include "Make3DAssetTypes.h"
#include "Make3DMeshQualityGate.h"

#include <filesystem>
#include <string>
#include <vector>

namespace make3d {

struct GameAssetGeneratorOptions {
    bool generateColliderProxy = true;
    bool generateLodProxy = true;
    bool addBuildingDetails = true;
    bool addPropDetailBands = true;
    bool enforceSafeMeshQuality = true;
    int radialSegments = 24;
    int gridResolution = 96;
    float targetHeight = 2.0f;
    float extrusionDepth = 0.42f;
    float buildingDepth = 0.72f;
    MeshQualityGateOptions qualityGate;
};

struct GameAssetValidationReport {
    bool ok = false;
    int vertices = 0;
    int triangles = 0;
    int invalidIndices = 0;
    int degenerateTriangles = 0;
    int nonFinitePositions = 0;
    int missingNormals = 0;
    int missingUvs = 0;
    float boundsMinX = 0.0f;
    float boundsMinY = 0.0f;
    float boundsMinZ = 0.0f;
    float boundsMaxX = 0.0f;
    float boundsMaxY = 0.0f;
    float boundsMaxZ = 0.0f;
    std::vector<std::string> warnings;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

struct GameAssetMetadata {
    std::string assetName = "Make3DGameAsset";
    GameAssetType assetType = GameAssetType::Unknown;
    float pivotX = 0.0f;
    float pivotY = 0.0f;
    float pivotZ = 0.0f;
    float unitScaleMeters = 1.0f;
    float colliderMinX = 0.0f;
    float colliderMinY = 0.0f;
    float colliderMinZ = 0.0f;
    float colliderMaxX = 0.0f;
    float colliderMaxY = 0.0f;
    float colliderMaxZ = 0.0f;
    int lod0Vertices = 0;
    int lod0Triangles = 0;
    int lodProxyVertices = 0;
    int lodProxyTriangles = 0;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

struct GameAssetGenerationResult {
    bool ok = false;
    std::string message;
    AssetClassificationResult classification;
    MeshData mesh;
    MeshData colliderMesh;
    MeshData lodProxyMesh;
    GameAssetValidationReport validation;
    GameAssetValidationReport colliderValidation;
    GameAssetValidationReport lodValidation;
    MeshQualityGateReport qualityGate;
    GameAssetMetadata metadata;
    std::filesystem::path objPath;
    std::filesystem::path gltfPath;
    std::filesystem::path colliderObjPath;
    std::filesystem::path lodObjPath;
    std::filesystem::path reportPath;
    std::filesystem::path manifestPath;
};

GameAssetValidationReport ValidateGameAssetMesh(const MeshData& mesh);

GameAssetGenerationResult BuildGenericGameAsset(
    const ImageRGBA& color,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& mask,
    const std::filesystem::path& outputDir,
    const GameAssetGeneratorOptions& options = GameAssetGeneratorOptions{});

} // namespace make3d
