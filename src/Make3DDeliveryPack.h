#pragma once

#include "Make3DGameAssetFinalization.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace make3d {

struct DeliveryPackResult {
    bool ok = false;
    std::filesystem::path turntablePlanPath;
    std::filesystem::path deliveryManifestPath;
};

inline bool WriteDeliveryText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return true;
}

inline bool WriteStandaloneTurntablePlan(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"turntable_preview_plan\",\n";
    o << "  \"mesh\": \"" << asset.complete.asset.gltfPath.generic_string() << "\",\n";
    o << "  \"durationSeconds\": 4.0,\n";
    o << "  \"frames\": [0, 45, 90, 135, 180, 225, 270, 315],\n";
    o << "  \"boundsSize\": [" << asset.complete.bounds.sizeX << ", " << asset.complete.bounds.sizeY << ", " << asset.complete.bounds.sizeZ << "]\n";
    o << "}\n";
    return WriteDeliveryText(path, o.str());
}

inline bool WriteDeliveryManifest(const std::filesystem::path& path, const FinalGameAssetResult& asset, const std::filesystem::path& turntablePlanPath) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"packageType\": \"Make3DDeliveryPack\",\n";
    o << "  \"assetType\": \"" << ToString(asset.complete.asset.assetType) << "\",\n";
    o << "  \"gltf\": \"" << asset.complete.asset.gltfPath.generic_string() << "\",\n";
    o << "  \"obj\": \"" << asset.complete.asset.objPath.generic_string() << "\",\n";
    o << "  \"collision\": \"" << asset.complete.asset.collisionObjPath.generic_string() << "\",\n";
    o << "  \"lod1\": \"" << asset.complete.asset.lodObjPath.generic_string() << "\",\n";
    o << "  \"lod2\": \"" << asset.lod2Path.generic_string() << "\",\n";
    o << "  \"atlas\": \"" << asset.textureAtlasPath.generic_string() << "\",\n";
    o << "  \"projectedTexture\": \"" << asset.projectedTexturePath.generic_string() << "\",\n";
    o << "  \"vertexInfluences\": \"" << asset.vertexInfluencesPath.generic_string() << "\",\n";
    o << "  \"frameConsistency\": \"" << asset.frameConsistencyPath.generic_string() << "\",\n";
    o << "  \"turntablePlan\": \"" << turntablePlanPath.generic_string() << "\",\n";
    o << "  \"runtimeChecklist\": \"" << asset.runtimeChecklistPath.generic_string() << "\"\n";
    o << "}\n";
    return WriteDeliveryText(path, o.str());
}

inline DeliveryPackResult BuildDeliveryPack(const FinalGameAssetResult& asset, const std::filesystem::path& outputDir) {
    DeliveryPackResult result;
    if (!asset.ok) return result;
    result.turntablePlanPath = outputDir / "make3d_turntable_plan.json";
    result.deliveryManifestPath = outputDir / "make3d_delivery_manifest.json";
    bool a = WriteStandaloneTurntablePlan(result.turntablePlanPath, asset);
    bool b = WriteDeliveryManifest(result.deliveryManifestPath, asset, result.turntablePlanPath);
    result.ok = a && b;
    return result;
}

} // namespace make3d
