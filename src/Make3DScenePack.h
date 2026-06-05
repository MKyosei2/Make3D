#pragma once

#include "Make3DAssetAuditPack.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace make3d {

struct ScenePackResult {
    bool ok = false;
    std::filesystem::path previewPath;
    std::filesystem::path materialsPath;
    std::filesystem::path scalePath;
    std::filesystem::path placementPath;
};

inline bool WriteScenePackText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return true;
}

inline bool WriteScenePreview(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"scene_preview_config\",\n";
    o << "  \"mesh\": \"" << asset.complete.asset.gltfPath.generic_string() << "\",\n";
    o << "  \"wireframe\": true,\n";
    o << "  \"normals\": true,\n";
    o << "  \"lodCompare\": true\n";
    o << "}\n";
    return WriteScenePackText(path, o.str());
}

inline bool WriteSceneMaterials(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    std::ostringstream o;
    o << "{\n  \"type\": \"scene_materials\",\n  \"parts\": [\n";
    const auto& parts = asset.complete.asset.parts;
    for (size_t i = 0; i < parts.size(); ++i) {
        const auto& p = parts[i];
        o << "    {\"name\": \"" << p.name << "\", \"material\": \"" << p.semanticMaterial << "\"}";
        if (i + 1 < parts.size()) o << ",";
        o << "\n";
    }
    o << "  ]\n}\n";
    return WriteScenePackText(path, o.str());
}

inline bool WriteSceneScale(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    const auto& b = asset.complete.bounds;
    std::ostringstream o;
    o << "# Make3D Scale Report\n\n";
    o << "- Pivot: [" << b.centerX << ", " << b.minY << ", " << b.centerZ << "]\n";
    o << "- Size: [" << b.sizeX << ", " << b.sizeY << ", " << b.sizeZ << "]\n";
    o << "- Unit scale: 1.0\n";
    return WriteScenePackText(path, o.str());
}

inline bool WriteScenePlacement(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"scene_placement\",\n";
    o << "  \"assetType\": \"" << ToString(asset.complete.asset.assetType) << "\",\n";
    o << "  \"position\": [0, 0, 0],\n";
    o << "  \"rotationDegrees\": [0, 0, 0],\n";
    o << "  \"scale\": [1, 1, 1]\n";
    o << "}\n";
    return WriteScenePackText(path, o.str());
}

inline ScenePackResult BuildScenePack(const FinalGameAssetResult& asset, const AssetAuditPackResult& audit, const std::filesystem::path& outputDir) {
    ScenePackResult result;
    if (!asset.ok || !audit.ok) return result;
    result.previewPath = outputDir / "make3d_scene_preview.json";
    result.materialsPath = outputDir / "make3d_scene_materials.json";
    result.scalePath = outputDir / "MAKE3D_SCALE_REPORT.md";
    result.placementPath = outputDir / "make3d_scene_placement.json";
    bool a = WriteScenePreview(result.previewPath, asset);
    bool b = WriteSceneMaterials(result.materialsPath, asset);
    bool c = WriteSceneScale(result.scalePath, asset);
    bool d = WriteScenePlacement(result.placementPath, asset);
    result.ok = a && b && c && d;
    return result;
}

} // namespace make3d
