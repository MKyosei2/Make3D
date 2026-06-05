#pragma once

#include "Make3DAdvancedCore.h"
#include "Make3DAssetTypes.h"
#include "Make3DGameAssetGenerator.h"

#include <filesystem>
#include <string>

namespace make3d {

struct TypedProceduralAssetOptions {
    float targetHeight = 2.0f;
    int radialSegments = 18;
    bool addDetails = true;
};

struct TypedProceduralAssetResult {
    bool ok = false;
    std::string message;
    GameAssetType assetType = GameAssetType::Unknown;
    MeshData mesh;
    GameAssetValidationReport validation;
    std::filesystem::path objPath;
    std::filesystem::path gltfPath;
    std::filesystem::path reportPath;
    std::filesystem::path manifestPath;
};

MeshData BuildFurnitureProceduralAsset(const TypedProceduralAssetOptions& options = TypedProceduralAssetOptions{});
MeshData BuildMachineProceduralAsset(const TypedProceduralAssetOptions& options = TypedProceduralAssetOptions{});
MeshData BuildToolProceduralAsset(const TypedProceduralAssetOptions& options = TypedProceduralAssetOptions{});
MeshData BuildTerrainPieceProceduralAsset(const TypedProceduralAssetOptions& options = TypedProceduralAssetOptions{});

TypedProceduralAssetResult ExportTypedProceduralAsset(
    GameAssetType assetType,
    const std::filesystem::path& outputDir,
    const TypedProceduralAssetOptions& options = TypedProceduralAssetOptions{});

} // namespace make3d
