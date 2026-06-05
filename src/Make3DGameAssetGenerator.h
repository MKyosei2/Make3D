#pragma once

#include "Make3DAdvancedCore.h"
#include "Make3DAssetTypes.h"
#include "Make3DMeshQualityGate.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace make3d {

struct AssetAnalysisOptions {
    int maxContourPoints = 256;
    int paletteBinsPerChannel = 4;
    int maxPaletteColors = 6;
    bool enablePartHints = true;
};

struct ContourPoint2D {
    int x = 0;
    int y = 0;
    float nx = 0.0f;
    float ny = 0.0f;
};

struct ShapeDescriptorReport {
    int foregroundPixels = 0;
    int minX = 0;
    int minY = 0;
    int maxX = -1;
    int maxY = -1;
    float coverage = 0.0f;
    float aspectRatio = 1.0f;
    float rectangularity = 0.0f;
    float perimeterRatio = 0.0f;
    float symmetryX = 0.0f;
    float symmetryY = 0.0f;
    float rowStability = 0.0f;
    float columnStability = 0.0f;
    float straightEdgeScore = 0.0f;
    float edgeDensity = 0.0f;
    float massCenterX = 0.5f;
    float massCenterY = 0.5f;
    float contourComplexity = 0.0f;
    std::string ToMarkdown() const;
    std::string ToJson() const;
};

struct PaletteColor {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    int pixels = 0;
    float coverage = 0.0f;
};

struct PartHint2D {
    std::string name;
    int minX = 0;
    int minY = 0;
    int maxX = -1;
    int maxY = -1;
    float confidence = 0.0f;
    std::string role;
};

struct AssetAnalysisResult {
    bool ok = false;
    AssetClassificationResult classification;
    ShapeDescriptorReport shape;
    std::vector<ContourPoint2D> contour;
    std::vector<PaletteColor> palette;
    std::vector<PartHint2D> partHints;
    std::vector<std::string> warnings;
    std::string ToMarkdown() const;
    std::string ToJson() const;
};

enum class PlannedPartKind {
    MainVolume,
    CharacterHead,
    CharacterTorso,
    CharacterArm,
    CharacterLeg,
    CharacterHair,
    CharacterClothing,
    BuildingFacade,
    BuildingRoof,
    BuildingDoor,
    BuildingWindowGrid,
    VehicleBody,
    VehicleCabin,
    VehicleWheel,
    ComplexCore,
    ComplexProtrusion,
    DetailPanel,
    DetailBand
};

const char* ToString(PlannedPartKind kind);

struct PlannedAssetPart {
    PlannedPartKind kind = PlannedPartKind::MainVolume;
    std::string name;
    float confidence = 0.0f;
    float centerX = 0.0f;
    float centerY = 0.0f;
    float centerZ = 0.0f;
    float sizeX = 1.0f;
    float sizeY = 1.0f;
    float sizeZ = 1.0f;
    int materialSlot = 0;
    std::string ToMarkdownRow() const;
    std::string ToJson() const;
};

struct PlannedMaterialSlot {
    std::string name;
    std::uint8_t r = 180;
    std::uint8_t g = 180;
    std::uint8_t b = 180;
    float coverage = 1.0f;
    std::string ToJson() const;
};

struct AssetPlanOptions {
    float targetHeight = 2.0f;
    float defaultDepth = 0.55f;
    bool includeCollider = true;
    bool includeLod = true;
    bool includeEditFriendlyParts = true;
};

struct AssetPlanResult {
    bool ok = false;
    GameAssetType assetType = GameAssetType::Unknown;
    std::string assetName = "Make3D_PlannedAsset";
    float targetHeight = 2.0f;
    float targetWidth = 1.0f;
    float targetDepth = 0.55f;
    float pivotX = 0.0f;
    float pivotY = 0.0f;
    float pivotZ = 0.0f;
    bool wantsCollider = true;
    bool wantsLod = true;
    std::vector<PlannedAssetPart> parts;
    std::vector<PlannedMaterialSlot> materials;
    std::vector<std::string> warnings;
    std::string ToMarkdown() const;
    std::string ToJson() const;
};

struct GameAssetGeneratorOptions {
    bool generateColliderProxy = true;
    bool generateLodProxy = true;
    bool addBuildingDetails = true;
    bool addPropDetailBands = true;
    bool enforceSafeMeshQuality = true;
    bool useAssetAnalysisPlan = true;
    int radialSegments = 24;
    int gridResolution = 96;
    float targetHeight = 2.0f;
    float extrusionDepth = 0.42f;
    float buildingDepth = 0.72f;
    MeshQualityGateOptions qualityGate;
    AssetAnalysisOptions analysis;
    AssetPlanOptions plan;
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
    AssetAnalysisResult analysis;
    AssetPlanResult plan;
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

AssetAnalysisResult AnalyzeAsset(
    const ImageRGBA& image,
    const std::vector<std::uint8_t>& mask,
    const DepthImage& depth,
    const AssetAnalysisOptions& options = AssetAnalysisOptions{});

AssetPlanResult BuildAssetPlanFromAnalysis(
    const AssetAnalysisResult& analysis,
    const AssetPlanOptions& options = AssetPlanOptions{});

GameAssetValidationReport ValidateGameAssetMesh(const MeshData& mesh);

GameAssetGenerationResult BuildGenericGameAsset(
    const ImageRGBA& color,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& mask,
    const std::filesystem::path& outputDir,
    const GameAssetGeneratorOptions& options = GameAssetGeneratorOptions{});

} // namespace make3d
