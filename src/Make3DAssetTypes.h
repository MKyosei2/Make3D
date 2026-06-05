#pragma once

#include "Make3DAdvancedCore.h"

#include <cstdint>
#include <string>
#include <vector>

namespace make3d {

enum class GameAssetType {
    Unknown,
    Character,
    Building,
    Vehicle,
    Furniture,
    ToolOrWeapon,
    Machine,
    Creature,
    TerrainPiece,
    ArchitecturalPart,
    FlatRelief,
    ComplexObject,
    GenericProp
};

struct AssetTypeClassifierOptions {
    bool preferBuildings = true;
    bool preferCharacterForTallSymmetricSilhouettes = true;
    float buildingRectangularityThreshold = 0.50f;
    float complexEdgeDensityThreshold = 0.32f;
    int edgeSampleStride = 1;
};

struct AssetClassificationResult {
    GameAssetType assetType = GameAssetType::Unknown;
    int foregroundPixels = 0;
    float coverage = 0.0f;
    float aspectRatio = 1.0f;       // bbox height / bbox width
    float rectangularity = 0.0f;    // foreground area / bbox area
    float symmetryX = 0.0f;         // left/right silhouette symmetry
    float symmetryY = 0.0f;         // top/bottom silhouette symmetry
    float rowStability = 0.0f;      // stable horizontal extents imply boxy assets
    float columnStability = 0.0f;   // stable vertical extents imply boxy assets
    float straightEdgeScore = 0.0f;
    float edgeDensity = 0.0f;
    float massCenterY = 0.5f;
    std::vector<std::string> reasons;
    std::vector<std::string> warnings;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

const char* ToString(GameAssetType type);

AssetClassificationResult InferGameAssetType(
    const ImageRGBA& image,
    const std::vector<std::uint8_t>& mask,
    const DepthImage& depth,
    const AssetTypeClassifierOptions& options = AssetTypeClassifierOptions{});

} // namespace make3d
