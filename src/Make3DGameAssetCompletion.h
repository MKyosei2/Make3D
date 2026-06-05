#pragma once

#include "Make3DGameAssetPipeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace make3d {

enum class GameEnginePreset {
    Generic,
    Unity,
    Unreal,
    Godot
};

struct GameAssetBounds {
    float minX = 0.0f;
    float minY = 0.0f;
    float minZ = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float maxZ = 0.0f;
    float centerX = 0.0f;
    float centerY = 0.0f;
    float centerZ = 0.0f;
    float sizeX = 0.0f;
    float sizeY = 0.0f;
    float sizeZ = 0.0f;
};

struct GameAssetJoint {
    std::string name;
    int parent = -1;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct CompletedGameAssetOptions {
    GameAssetGenerationOptions generation;
    GameEnginePreset enginePreset = GameEnginePreset::Generic;
    bool writeManifest = true;
    bool writeTexturePlaceholders = true;
    bool writeRigMetadata = true;
    bool writeEngineImportPreset = true;
    bool forcePlanarUvProjection = true;
    int textureSize = 64;
    float unitScale = 1.0f;
};

struct CompletedGameAssetResult {
    bool ok = false;
    std::string message;
    GameAssetResult asset;
    GameAssetBounds bounds;
    std::vector<GameAssetJoint> joints;
    std::filesystem::path manifestPath;
    std::filesystem::path albedoTexturePath;
    std::filesystem::path normalTexturePath;
    std::filesystem::path roughnessTexturePath;
    std::filesystem::path rigMetadataPath;
    std::filesystem::path enginePresetPath;
};

inline const char* ToString(GameEnginePreset preset) {
    switch (preset) {
        case GameEnginePreset::Generic: return "Generic";
        case GameEnginePreset::Unity: return "Unity";
        case GameEnginePreset::Unreal: return "Unreal";
        case GameEnginePreset::Godot: return "Godot";
    }
    return "Generic";
}

inline GameAssetBounds ComputeGameAssetBounds(const MeshData& mesh) {
    GameAssetBounds b;
    if (mesh.positions.empty()) return b;
    b.minX = b.minY = b.minZ = std::numeric_limits<float>::max();
    b.maxX = b.maxY = b.maxZ = -std::numeric_limits<float>::max();
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        b.minX = std::min(b.minX, mesh.positions[i + 0]);
        b.minY = std::min(b.minY, mesh.positions[i + 1]);
        b.minZ = std::min(b.minZ, mesh.positions[i + 2]);
        b.maxX = std::max(b.maxX, mesh.positions[i + 0]);
        b.maxY = std::max(b.maxY, mesh.positions[i + 1]);
        b.maxZ = std::max(b.maxZ, mesh.positions[i + 2]);
    }
    b.centerX = (b.minX + b.maxX) * 0.5f;
    b.centerY = (b.minY + b.maxY) * 0.5f;
    b.centerZ = (b.minZ + b.maxZ) * 0.5f;
    b.sizeX = b.maxX - b.minX;
    b.sizeY = b.maxY - b.minY;
    b.sizeZ = b.maxZ - b.minZ;
    return b;
}

inline void ApplyPlanarUvProjection(MeshData& mesh) {
    GameAssetBounds b = ComputeGameAssetBounds(mesh);
    if (mesh.positions.empty()) return;
    float sx = std::max(0.0001f, b.sizeX);
    float sy = std::max(0.0001f, b.sizeY);
    float sz = std::max(0.0001f, b.sizeZ);
    mesh.uvs.clear();
    mesh.uvs.reserve(mesh.positions.size() / 3 * 2);
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        float x = mesh.positions[i + 0];
        float y = mesh.positions[i + 1];
        float z = mesh.positions[i + 2];
        float ax = std::abs(x - b.centerX) / sx;
        float ay = std::abs(y - b.centerY) / sy;
        float az = std::abs(z - b.centerZ) / sz;
        float u = 0.0f;
        float v = 0.0f;
        if (ay >= ax && ay >= az) {
            u = (x - b.minX) / sx;
            v = (z - b.minZ) / sz;
        } else if (az >= ax) {
            u = (x - b.minX) / sx;
            v = (y - b.minY) / sy;
        } else {
            u = (z - b.minZ) / sz;
            v = (y - b.minY) / sy;
        }
        mesh.uvs.push_back(std::clamp(u, 0.0f, 1.0f));
        mesh.uvs.push_back(std::clamp(v, 0.0f, 1.0f));
    }
}

inline bool WritePpmTexture(const std::filesystem::path& path, int size, std::array<std::uint8_t, 3> a, std::array<std::uint8_t, 3> b, std::string* error = nullptr) {
    size = std::max(8, size);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (error) *error = "failed to open texture for writing";
        return false;
    }
    out << "P6\n" << size << " " << size << "\n255\n";
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            bool checker = ((x / 8) + (y / 8)) % 2 == 0;
            std::array<std::uint8_t, 3> c = checker ? a : b;
            out.write(reinterpret_cast<const char*>(c.data()), 3);
        }
    }
    return true;
}

inline std::vector<GameAssetJoint> BuildRigMetadata(GameAssetType type, const GameAssetBounds& bounds) {
    std::vector<GameAssetJoint> joints;
    if (type == GameAssetType::Character) {
        float x = bounds.centerX;
        float z = bounds.centerZ;
        float y0 = bounds.minY;
        float h = std::max(0.01f, bounds.sizeY);
        joints.push_back({"root", -1, x, y0, z});
        joints.push_back({"hips", 0, x, y0 + h * 0.38f, z});
        joints.push_back({"spine", 1, x, y0 + h * 0.58f, z});
        joints.push_back({"chest", 2, x, y0 + h * 0.72f, z});
        joints.push_back({"neck", 3, x, y0 + h * 0.82f, z});
        joints.push_back({"head", 4, x, y0 + h * 0.92f, z});
        joints.push_back({"left_arm", 3, bounds.minX, y0 + h * 0.68f, z});
        joints.push_back({"right_arm", 3, bounds.maxX, y0 + h * 0.68f, z});
        joints.push_back({"left_leg", 1, x - bounds.sizeX * 0.16f, y0 + h * 0.08f, z});
        joints.push_back({"right_leg", 1, x + bounds.sizeX * 0.16f, y0 + h * 0.08f, z});
    } else {
        joints.push_back({"root", -1, bounds.centerX, bounds.minY, bounds.centerZ});
        joints.push_back({"pivot_center", 0, bounds.centerX, bounds.centerY, bounds.centerZ});
        joints.push_back({"top_handle", 1, bounds.centerX, bounds.maxY, bounds.centerZ});
    }
    return joints;
}

inline bool WriteRigMetadataJson(const std::filesystem::path& path, GameAssetType type, const std::vector<GameAssetJoint>& joints, std::string* error = nullptr) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (error) *error = "failed to open rig metadata for writing";
        return false;
    }
    out << "{\n";
    out << "  \"assetType\": \"" << ToString(type) << "\",\n";
    out << "  \"note\": \"metadata skeleton or pivot handles; skinning is not generated yet\",\n";
    out << "  \"joints\": [\n";
    for (size_t i = 0; i < joints.size(); ++i) {
        const auto& j = joints[i];
        out << "    {\"name\": \"" << j.name << "\", \"parent\": " << j.parent << ", \"translation\": [" << j.x << ", " << j.y << ", " << j.z << "]}";
        if (i + 1 < joints.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return true;
}

inline bool WriteEnginePresetJson(const std::filesystem::path& path, GameEnginePreset preset, GameAssetType type, const GameAssetBounds& b, float unitScale, std::string* error = nullptr) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (error) *error = "failed to open engine preset for writing";
        return false;
    }
    const char* upAxis = preset == GameEnginePreset::Unreal ? "Z" : "Y";
    float recommendedScale = unitScale;
    if (preset == GameEnginePreset::Unreal) recommendedScale *= 100.0f;
    out << "{\n";
    out << "  \"engine\": \"" << ToString(preset) << "\",\n";
    out << "  \"assetType\": \"" << ToString(type) << "\",\n";
    out << "  \"unitScale\": " << unitScale << ",\n";
    out << "  \"recommendedImportScale\": " << recommendedScale << ",\n";
    out << "  \"upAxis\": \"" << upAxis << "\",\n";
    out << "  \"pivot\": {\"x\": " << b.centerX << ", \"y\": " << b.minY << ", \"z\": " << b.centerZ << "},\n";
    out << "  \"collision\": \"use generated *_collision.obj\",\n";
    out << "  \"lod\": \"use generated *_lod1.obj as LOD1\",\n";
    out << "  \"materials\": \"semantic part names are listed in game_asset_report.md\"\n";
    out << "}\n";
    return true;
}

inline bool WriteManifestJson(const std::filesystem::path& path, const CompletedGameAssetResult& r, std::string* error = nullptr) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (error) *error = "failed to open manifest for writing";
        return false;
    }
    const auto& a = r.asset;
    out << "{\n";
    out << "  \"ok\": " << (r.ok ? "true" : "false") << ",\n";
    out << "  \"assetType\": \"" << ToString(a.assetType) << "\",\n";
    out << "  \"mesh\": \"" << a.gltfPath.generic_string() << "\",\n";
    out << "  \"obj\": \"" << a.objPath.generic_string() << "\",\n";
    out << "  \"collision\": \"" << a.collisionObjPath.generic_string() << "\",\n";
    out << "  \"lod1\": \"" << a.lodObjPath.generic_string() << "\",\n";
    out << "  \"albedoTexture\": \"" << r.albedoTexturePath.generic_string() << "\",\n";
    out << "  \"normalTexture\": \"" << r.normalTexturePath.generic_string() << "\",\n";
    out << "  \"roughnessTexture\": \"" << r.roughnessTexturePath.generic_string() << "\",\n";
    out << "  \"rigMetadata\": \"" << r.rigMetadataPath.generic_string() << "\",\n";
    out << "  \"enginePreset\": \"" << r.enginePresetPath.generic_string() << "\",\n";
    out << "  \"bounds\": {\"min\": [" << r.bounds.minX << ", " << r.bounds.minY << ", " << r.bounds.minZ << "], \"max\": [" << r.bounds.maxX << ", " << r.bounds.maxY << ", " << r.bounds.maxZ << "], \"size\": [" << r.bounds.sizeX << ", " << r.bounds.sizeY << ", " << r.bounds.sizeZ << "]},\n";
    out << "  \"validation\": " << a.validation.ToJson() << ",\n";
    out << "  \"parts\": [\n";
    for (size_t i = 0; i < a.parts.size(); ++i) {
        const auto& p = a.parts[i];
        out << "    {\"name\": \"" << p.name << "\", \"material\": \"" << p.semanticMaterial << "\", \"vertices\": " << p.vertexCount << ", \"triangles\": " << p.triangleCount << "}";
        if (i + 1 < a.parts.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return true;
}

inline CompletedGameAssetResult BuildCompleteGameAssetFromImage(const std::filesystem::path& colorPath, const std::optional<std::filesystem::path>& depthPath, const std::filesystem::path& outputDir, const CompletedGameAssetOptions& options = CompletedGameAssetOptions{}) {
    CompletedGameAssetResult r;
    GameAssetGenerationOptions generation = options.generation;
    r.asset = BuildGameAssetFromImage(colorPath, depthPath, outputDir, generation);
    if (!r.asset.ok) {
        r.message = r.asset.message;
        return r;
    }
    if (options.forcePlanarUvProjection) {
        ApplyPlanarUvProjection(r.asset.mesh);
        r.asset.validation = ValidateGameAsset(r.asset.mesh, r.asset.parts, r.asset.collisionMesh, r.asset.lodMesh, generation.triangleBudget);
    }
    r.bounds = ComputeGameAssetBounds(r.asset.mesh);
    r.joints = BuildRigMetadata(r.asset.assetType, r.bounds);
    std::string error;
    if (options.writeTexturePlaceholders) {
        r.albedoTexturePath = outputDir / "textures" / "make3d_albedo_placeholder.ppm";
        r.normalTexturePath = outputDir / "textures" / "make3d_normal_placeholder.ppm";
        r.roughnessTexturePath = outputDir / "textures" / "make3d_roughness_placeholder.ppm";
        WritePpmTexture(r.albedoTexturePath, options.textureSize, {180, 188, 196}, {132, 142, 152}, &error);
        WritePpmTexture(r.normalTexturePath, options.textureSize, {128, 128, 255}, {126, 126, 245}, &error);
        WritePpmTexture(r.roughnessTexturePath, options.textureSize, {210, 210, 210}, {180, 180, 180}, &error);
    }
    if (options.writeRigMetadata) {
        r.rigMetadataPath = outputDir / "make3d_rig_or_pivots.json";
        WriteRigMetadataJson(r.rigMetadataPath, r.asset.assetType, r.joints, &error);
    }
    if (options.writeEngineImportPreset) {
        r.enginePresetPath = outputDir / "make3d_engine_import_preset.json";
        WriteEnginePresetJson(r.enginePresetPath, options.enginePreset, r.asset.assetType, r.bounds, options.unitScale, &error);
    }
    if (options.writeManifest) {
        r.manifestPath = outputDir / "make3d_asset_manifest.json";
        WriteManifestJson(r.manifestPath, r, &error);
    }
    r.ok = true;
    r.message = "Complete game asset export finished.";
    return r;
}

} // namespace make3d
