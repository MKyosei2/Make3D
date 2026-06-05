#pragma once

#include "Make3DGeometryRefinementPack.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace make3d {

struct ProductionQualityPackResult {
    bool ok = false;
    std::filesystem::path tangentFramePath;
    std::filesystem::path pbrMaterialSetPath;
    std::filesystem::path skeletonBindingReportPath;
    std::filesystem::path engineImportValidationPath;
    std::filesystem::path readinessScorePath;
    std::filesystem::path textureBakeManifestPath;
};

inline bool WriteProductionText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return true;
}

inline bool WriteTangentFrame(const std::filesystem::path& path, const MeshData& mesh) {
    std::ostringstream o;
    int vc = static_cast<int>(mesh.positions.size() / 3);
    o << "{\n  \"type\": \"tangent_frame_set\",\n  \"vertexCount\": " << vc << ",\n  \"frames\": [\n";
    for (int i = 0; i < vc; ++i) {
        float nx = (static_cast<size_t>(i) * 3 + 2 < mesh.normals.size()) ? mesh.normals[static_cast<size_t>(i) * 3 + 0] : 0.0f;
        float ny = (static_cast<size_t>(i) * 3 + 2 < mesh.normals.size()) ? mesh.normals[static_cast<size_t>(i) * 3 + 1] : 1.0f;
        float nz = (static_cast<size_t>(i) * 3 + 2 < mesh.normals.size()) ? mesh.normals[static_cast<size_t>(i) * 3 + 2] : 0.0f;
        o << "    {\"v\": " << i << ", \"normal\": [" << nx << ", " << ny << ", " << nz << "], \"tangent\": [1, 0, 0], \"bitangent\": [0, 0, 1]}";
        if (i + 1 < vc) o << ",";
        o << "\n";
    }
    o << "  ]\n}\n";
    return WriteProductionText(path, o.str());
}

inline bool WritePbrMaterialSet(const std::filesystem::path& path, const FinalGameAssetResult& asset, const GeometryRefinementPackResult& geometry, const QualityPackResult& quality) {
    std::ostringstream o;
    o << "{\n  \"type\": \"pbr_material_set\",\n";
    o << "  \"albedo\": \"" << quality.projectedAlbedoPath.generic_string() << "\",\n";
    o << "  \"normal\": \"" << geometry.normalMapPath.generic_string() << "\",\n";
    o << "  \"occlusion\": \"" << geometry.occlusionMapPath.generic_string() << "\",\n";
    o << "  \"roughness\": \"" << asset.complete.roughnessTexturePath.generic_string() << "\",\n";
    o << "  \"parts\": [\n";
    const auto& parts = asset.complete.asset.parts;
    for (size_t i = 0; i < parts.size(); ++i) {
        o << "    {\"name\": \"" << parts[i].name << "\", \"material\": \"" << parts[i].semanticMaterial << "\"}";
        if (i + 1 < parts.size()) o << ",";
        o << "\n";
    }
    o << "  ]\n}\n";
    return WriteProductionText(path, o.str());
}

inline bool WriteSkeletonBindingReport2(const std::filesystem::path& path, const FinalGameAssetResult& asset, const QualityPackResult& quality) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"skeleton_binding_report\",\n";
    o << "  \"jointCount\": " << asset.complete.joints.size() << ",\n";
    o << "  \"weights\": \"" << quality.skinWeightsPath.generic_string() << "\",\n";
    o << "  \"bindingMode\": \"nearest-joint weighted fallback, ready for later rig conversion\"\n";
    o << "}\n";
    return WriteProductionText(path, o.str());
}

inline bool WriteEngineImportValidation2(const std::filesystem::path& path, const FinalGameAssetResult& asset, const GeometryRefinementPackResult& geometry) {
    const auto& v = asset.complete.asset.validation;
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"engine_import_validation\",\n";
    o << "  \"hasGltf\": " << (!asset.complete.asset.gltfPath.empty() ? "true" : "false") << ",\n";
    o << "  \"hasCollision\": " << (v.hasCollisionMesh ? "true" : "false") << ",\n";
    o << "  \"hasLod1\": " << (v.hasLodMesh ? "true" : "false") << ",\n";
    o << "  \"hasLod2\": " << (!asset.lod2Path.empty() ? "true" : "false") << ",\n";
    o << "  \"hasRepairedMesh\": " << (!geometry.repairedMeshPath.empty() ? "true" : "false") << ",\n";
    o << "  \"hasPbrTextures\": " << (!geometry.normalMapPath.empty() && !geometry.occlusionMapPath.empty() ? "true" : "false") << "\n";
    o << "}\n";
    return WriteProductionText(path, o.str());
}

inline bool WriteReadinessScore(const std::filesystem::path& path, const FinalGameAssetResult& asset, const GeometryRefinementPackResult& geometry) {
    float score = asset.complete.asset.validation.gameReadyScore;
    if (!asset.lod2Path.empty()) score += 0.04f;
    if (!geometry.repairedMeshPath.empty()) score += 0.04f;
    if (!geometry.normalMapPath.empty()) score += 0.03f;
    if (!geometry.occlusionMapPath.empty()) score += 0.03f;
    score = std::min(1.0f, score);
    std::ostringstream o;
    o << "{\n  \"type\": \"production_readiness_score\",\n  \"score\": " << score << ",\n  \"candidate\": " << (score >= 0.85f ? "true" : "false") << "\n}\n";
    return WriteProductionText(path, o.str());
}

inline bool WriteTextureBakeManifest(const std::filesystem::path& path, const FinalGameAssetResult& asset, const QualityPackResult& quality, const GeometryRefinementPackResult& geometry) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"texture_bake_manifest\",\n";
    o << "  \"projectedAlbedo\": \"" << quality.projectedAlbedoPath.generic_string() << "\",\n";
    o << "  \"atlas\": \"" << asset.textureAtlasPath.generic_string() << "\",\n";
    o << "  \"normal\": \"" << geometry.normalMapPath.generic_string() << "\",\n";
    o << "  \"occlusion\": \"" << geometry.occlusionMapPath.generic_string() << "\",\n";
    o << "  \"roughness\": \"" << asset.complete.roughnessTexturePath.generic_string() << "\"\n";
    o << "}\n";
    return WriteProductionText(path, o.str());
}

inline ProductionQualityPackResult BuildProductionQualityPack(const FinalGameAssetResult& asset, const QualityPackResult& quality, const GeometryRefinementPackResult& geometry, const std::filesystem::path& outputDir) {
    ProductionQualityPackResult result;
    if (!asset.ok || !quality.ok || !geometry.ok) return result;
    result.tangentFramePath = outputDir / "make3d_tangent_frames.json";
    result.pbrMaterialSetPath = outputDir / "make3d_pbr_material_set.json";
    result.skeletonBindingReportPath = outputDir / "make3d_skeleton_binding_report.json";
    result.engineImportValidationPath = outputDir / "make3d_engine_import_validation.json";
    result.readinessScorePath = outputDir / "make3d_production_readiness_score.json";
    result.textureBakeManifestPath = outputDir / "make3d_texture_bake_manifest.json";
    bool a = WriteTangentFrame(result.tangentFramePath, geometry.repairedMesh);
    bool b = WritePbrMaterialSet(result.pbrMaterialSetPath, asset, geometry, quality);
    bool c = WriteSkeletonBindingReport2(result.skeletonBindingReportPath, asset, quality);
    bool d = WriteEngineImportValidation2(result.engineImportValidationPath, asset, geometry);
    bool e = WriteReadinessScore(result.readinessScorePath, asset, geometry);
    bool f = WriteTextureBakeManifest(result.textureBakeManifestPath, asset, quality, geometry);
    result.ok = a && b && c && d && e && f;
    return result;
}

} // namespace make3d
