#pragma once

#include "Make3DDeliveryPack.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace make3d {

struct AssetAuditPackResult {
    bool ok = false;
    std::filesystem::path qualityGatePath;
    std::filesystem::path engineProfilesPath;
    std::filesystem::path colliderManifestPath;
    std::filesystem::path lodManifestPath;
    std::filesystem::path assetCatalogPath;
};

inline bool WriteAssetAuditText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return true;
}

inline bool WriteQualityGate(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    const auto& v = asset.complete.asset.validation;
    std::ostringstream o;
    o << "# Make3D Quality Gate\n\n";
    o << "- Game-ready candidate: " << (v.gameReadyCandidate ? "yes" : "no") << "\n";
    o << "- Score: " << v.gameReadyScore << "\n";
    o << "- Vertices: " << v.vertices << "\n";
    o << "- Triangles: " << v.triangles << "\n";
    o << "- Collision: " << (v.hasCollisionMesh ? "yes" : "no") << "\n";
    o << "- LOD1: " << (v.hasLodMesh ? "yes" : "no") << "\n";
    o << "- LOD2: " << (!asset.lod2Path.empty() ? "yes" : "no") << "\n";
    o << "- UVs: " << (v.hasUvs ? "yes" : "no") << "\n";
    o << "- Runtime checklist: " << (!asset.runtimeChecklistPath.empty() ? "yes" : "no") << "\n";
    return WriteAssetAuditText(path, o.str());
}

inline bool WriteEngineProfiles(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"assetType\": \"" << ToString(asset.complete.asset.assetType) << "\",\n";
    o << "  \"profiles\": [\n";
    o << "    {\"engine\": \"Unity\", \"upAxis\": \"Y\", \"scale\": 1.0, \"mesh\": \"" << asset.complete.asset.gltfPath.generic_string() << "\"},\n";
    o << "    {\"engine\": \"Unreal\", \"upAxis\": \"Z\", \"scale\": 100.0, \"mesh\": \"" << asset.complete.asset.gltfPath.generic_string() << "\"},\n";
    o << "    {\"engine\": \"Godot\", \"upAxis\": \"Y\", \"scale\": 1.0, \"mesh\": \"" << asset.complete.asset.gltfPath.generic_string() << "\"}\n";
    o << "  ]\n";
    o << "}\n";
    return WriteAssetAuditText(path, o.str());
}

inline bool WriteColliderManifest(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"collider_manifest\",\n";
    o << "  \"primary\": \"" << asset.complete.asset.collisionObjPath.generic_string() << "\",\n";
    o << "  \"strategy\": \"generated bounds collider for fast game-engine validation\"\n";
    o << "}\n";
    return WriteAssetAuditText(path, o.str());
}

inline bool WriteLodManifest(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"lod_manifest\",\n";
    o << "  \"lod0\": \"" << asset.complete.asset.gltfPath.generic_string() << "\",\n";
    o << "  \"lod1\": \"" << asset.complete.asset.lodObjPath.generic_string() << "\",\n";
    o << "  \"lod2\": \"" << asset.lod2Path.generic_string() << "\"\n";
    o << "}\n";
    return WriteAssetAuditText(path, o.str());
}

inline bool WriteAssetCatalog(const std::filesystem::path& path, const FinalGameAssetResult& asset, const DeliveryPackResult& delivery, const AssetAuditPackResult& audit) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"catalogType\": \"Make3DAssetCatalog\",\n";
    o << "  \"assetType\": \"" << ToString(asset.complete.asset.assetType) << "\",\n";
    o << "  \"deliveryManifest\": \"" << delivery.deliveryManifestPath.generic_string() << "\",\n";
    o << "  \"turntablePlan\": \"" << delivery.turntablePlanPath.generic_string() << "\",\n";
    o << "  \"qualityGate\": \"" << audit.qualityGatePath.generic_string() << "\",\n";
    o << "  \"engineProfiles\": \"" << audit.engineProfilesPath.generic_string() << "\",\n";
    o << "  \"colliderManifest\": \"" << audit.colliderManifestPath.generic_string() << "\",\n";
    o << "  \"lodManifest\": \"" << audit.lodManifestPath.generic_string() << "\"\n";
    o << "}\n";
    return WriteAssetAuditText(path, o.str());
}

inline AssetAuditPackResult BuildAssetAuditPack(const FinalGameAssetResult& asset, const DeliveryPackResult& delivery, const std::filesystem::path& outputDir) {
    AssetAuditPackResult result;
    if (!asset.ok || !delivery.ok) return result;
    result.qualityGatePath = outputDir / "MAKE3D_QUALITY_GATE.md";
    result.engineProfilesPath = outputDir / "make3d_engine_profiles.json";
    result.colliderManifestPath = outputDir / "make3d_collider_manifest.json";
    result.lodManifestPath = outputDir / "make3d_lod_manifest.json";
    result.assetCatalogPath = outputDir / "make3d_asset_catalog.json";
    bool a = WriteQualityGate(result.qualityGatePath, asset);
    bool b = WriteEngineProfiles(result.engineProfilesPath, asset);
    bool c = WriteColliderManifest(result.colliderManifestPath, asset);
    bool d = WriteLodManifest(result.lodManifestPath, asset);
    bool e = WriteAssetCatalog(result.assetCatalogPath, asset, delivery, result);
    result.ok = a && b && c && d && e;
    return result;
}

} // namespace make3d
