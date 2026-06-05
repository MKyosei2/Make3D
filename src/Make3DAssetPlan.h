#pragma once

#include "Make3DAssetAnalysis.h"
#include "Make3DAssetTypes.h"

#include <string>
#include <vector>

namespace make3d {

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

AssetPlanResult BuildAssetPlanFromAnalysis(
    const AssetAnalysisResult& analysis,
    const AssetPlanOptions& options = AssetPlanOptions{});

} // namespace make3d
