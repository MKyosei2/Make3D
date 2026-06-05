#pragma once

#include "Make3DAdvancedCore.h"
#include "Make3DHeroCharacterModel.h"
#include "Make3DMaskRefiner.h"
#include "Make3DShapeInference.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace make3d {

enum class GameAssetType {
    Character,
    Building,
    Prop,
    EnvironmentPiece,
    Unknown
};

struct GameAssetPart {
    std::string name;
    std::string semanticMaterial;
    int firstVertex = 0;
    int vertexCount = 0;
    int firstTriangle = 0;
    int triangleCount = 0;
};

struct GameAssetValidationReport {
    int vertices = 0;
    int triangles = 0;
    int degenerateTriangles = 0;
    int duplicateVertexBuckets = 0;
    int materialSlots = 0;
    int parts = 0;
    int estimatedDrawCalls = 0;
    bool hasNormals = false;
    bool hasUvs = false;
    bool hasCollisionMesh = false;
    bool hasLodMesh = false;
    bool withinTriangleBudget = false;
    bool editableParts = false;
    bool gameReadyCandidate = false;
    float gameReadyScore = 0.0f;
    std::vector<std::string> warnings;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

struct GameAssetGenerationOptions {
    AdvancedOptions reconstruction;
    MaskRefineOptions maskRefine;
    HeroCharacterOptions heroCharacter;
    bool enableAutoTypeClassification = true;
    GameAssetType forcedType = GameAssetType::Unknown;
    bool exportObj = true;
    bool exportGltf = true;
    bool exportCollisionObj = true;
    bool exportLodObj = true;
    bool writeReport = true;
    int triangleBudget = 20000;
};

struct GameAssetResult {
    bool ok = false;
    std::string message;
    GameAssetType assetType = GameAssetType::Unknown;
    MeshData mesh;
    MeshData collisionMesh;
    MeshData lodMesh;
    std::vector<GameAssetPart> parts;
    ShapeInferenceResult shapeInference;
    MaskRefineReport maskReport;
    ReconstructionReport reconstructionReport;
    GameAssetValidationReport validation;
    std::filesystem::path objPath;
    std::filesystem::path gltfPath;
    std::filesystem::path collisionObjPath;
    std::filesystem::path lodObjPath;
    std::filesystem::path reportPath;
};

const char* ToString(GameAssetType type);

GameAssetType ClassifyGameAssetType(
    const ImageRGBA& image,
    const std::vector<std::uint8_t>& refinedMask,
    const ShapeInferenceResult& shapeInference);

GameAssetValidationReport ValidateGameAsset(
    const MeshData& mesh,
    const std::vector<GameAssetPart>& parts,
    const MeshData& collisionMesh,
    const MeshData& lodMesh,
    int triangleBudget);

GameAssetResult BuildGameAssetFromImage(
    const std::filesystem::path& colorPath,
    const std::optional<std::filesystem::path>& depthPath,
    const std::filesystem::path& outputDir,
    const GameAssetGenerationOptions& options = GameAssetGenerationOptions{});

} // namespace make3d
