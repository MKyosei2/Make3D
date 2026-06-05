#pragma once

#include "Make3DScenePack.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace make3d {

struct QualityPackResult {
    bool ok = false;
    std::filesystem::path uvLayoutPath;
    std::filesystem::path projectedAlbedoPath;
    std::filesystem::path retopoPlanPath;
    std::filesystem::path skinWeightsPath;
    std::filesystem::path temporalFusionPath;
    std::filesystem::path facadeFeaturesPath;
    std::filesystem::path gltfPackagePath;
};

inline bool WriteQualityText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return true;
}

inline int QualityVertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
inline int QualityTriangleCount(const MeshData& mesh) { return static_cast<int>(mesh.indices.size() / 3); }

inline bool WriteUvLayoutSvg(const std::filesystem::path& path, const MeshData& mesh) {
    std::ostringstream o;
    o << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"512\" height=\"512\" viewBox=\"0 0 512 512\">\n";
    o << "<rect width=\"512\" height=\"512\" fill=\"white\"/>\n";
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::uint32_t ia = mesh.indices[i + 0], ib = mesh.indices[i + 1], ic = mesh.indices[i + 2];
        if ((static_cast<size_t>(ia) + 1) * 2 > mesh.uvs.size() || (static_cast<size_t>(ib) + 1) * 2 > mesh.uvs.size() || (static_cast<size_t>(ic) + 1) * 2 > mesh.uvs.size()) continue;
        auto ux = [&](std::uint32_t idx) { return std::clamp(mesh.uvs[static_cast<size_t>(idx) * 2 + 0], 0.0f, 1.0f) * 500.0f + 6.0f; };
        auto uy = [&](std::uint32_t idx) { return 506.0f - std::clamp(mesh.uvs[static_cast<size_t>(idx) * 2 + 1], 0.0f, 1.0f) * 500.0f; };
        o << "<polygon points=\"" << ux(ia) << "," << uy(ia) << " " << ux(ib) << "," << uy(ib) << " " << ux(ic) << "," << uy(ic) << "\" fill=\"none\" stroke=\"black\" stroke-width=\"1\"/>\n";
    }
    o << "</svg>\n";
    return WriteQualityText(path, o.str());
}

inline bool WriteProjectedAlbedoFromFrames(const std::filesystem::path& path, const std::vector<std::filesystem::path>& frames, int size) {
    if (frames.empty()) return false;
    std::vector<ImageRGBA> images;
    for (const auto& f : frames) {
        std::string e;
        auto img = LoadImageRGBA(f, &e);
        if (img) images.push_back(*img);
    }
    if (images.empty()) return false;
    size = std::max(16, size);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << size << " " << size << "\n255\n";
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int r = 0, g = 0, b = 0, n = 0;
            for (const auto& img : images) {
                int sx = std::min(img.width - 1, x * img.width / size);
                int sy = std::min(img.height - 1, y * img.height / size);
                size_t p = (static_cast<size_t>(sy) * img.width + sx) * 4;
                r += img.pixels[p + 0]; g += img.pixels[p + 1]; b += img.pixels[p + 2]; ++n;
            }
            unsigned char rgb[3] = {static_cast<unsigned char>(r / n), static_cast<unsigned char>(g / n), static_cast<unsigned char>(b / n)};
            out.write(reinterpret_cast<const char*>(rgb), 3);
        }
    }
    return true;
}

inline bool WriteRetopoPlan(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"retopology_plan\",\n";
    o << "  \"sourceTriangles\": " << QualityTriangleCount(asset.complete.asset.mesh) << ",\n";
    o << "  \"targetTriangles\": " << std::max(12, QualityTriangleCount(asset.complete.asset.mesh) / 2) << ",\n";
    o << "  \"proxyTriangles\": " << QualityTriangleCount(asset.retopoProxy) << ",\n";
    o << "  \"strategy\": \"part-aware quad proxy with LOD validation\"\n";
    o << "}\n";
    return WriteQualityText(path, o.str());
}

inline bool WriteSkinWeightsV2(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    const auto& mesh = asset.complete.asset.mesh;
    const auto& joints = asset.complete.joints;
    int vc = QualityVertexCount(mesh);
    std::ostringstream o;
    o << "{\n  \"type\": \"skin_weights_v2\",\n  \"vertexCount\": " << vc << ",\n  \"weights\": [\n";
    for (int v = 0; v < vc; ++v) {
        float x = mesh.positions[static_cast<size_t>(v) * 3 + 0];
        float y = mesh.positions[static_cast<size_t>(v) * 3 + 1];
        float z = mesh.positions[static_cast<size_t>(v) * 3 + 2];
        int best = 0, second = 0;
        float bd = 1e30f, sd = 1e30f;
        for (size_t j = 0; j < joints.size(); ++j) {
            float dx = x - joints[j].x, dy = y - joints[j].y, dz = z - joints[j].z;
            float d = dx * dx + dy * dy + dz * dz;
            if (d < bd) { second = best; sd = bd; best = static_cast<int>(j); bd = d; }
            else if (d < sd) { second = static_cast<int>(j); sd = d; }
        }
        float w0 = sd > 0.0001f ? sd / (bd + sd) : 1.0f;
        w0 = std::clamp(w0, 0.55f, 1.0f);
        o << "    {\"v\": " << v << ", \"j0\": " << best << ", \"w0\": " << w0 << ", \"j1\": " << second << ", \"w1\": " << (1.0f - w0) << "}";
        if (v + 1 < vc) o << ",";
        o << "\n";
    }
    o << "  ]\n}\n";
    return WriteQualityText(path, o.str());
}

inline bool WriteTemporalFusionReport(const std::filesystem::path& path, const std::vector<std::filesystem::path>& frames) {
    std::ostringstream o;
    o << "{\n  \"type\": \"temporal_fusion_report\",\n  \"frameCount\": " << frames.size() << ",\n  \"referenceFrame\": \"" << (frames.empty() ? std::string() : frames.front().generic_string()) << "\",\n  \"fusionMode\": \"average color plus stable first-frame geometry\"\n}\n";
    return WriteQualityText(path, o.str());
}

inline bool WriteFacadeFeatures(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    std::ostringstream o;
    o << "{\n  \"type\": \"facade_feature_map\",\n  \"features\": [\n";
    const auto& parts = asset.complete.asset.parts;
    for (size_t i = 0; i < parts.size(); ++i) {
        const auto& p = parts[i];
        std::string kind = "part";
        if (p.name.find("window") != std::string::npos) kind = "window";
        else if (p.name.find("door") != std::string::npos) kind = "door";
        else if (p.name.find("roof") != std::string::npos) kind = "roof";
        o << "    {\"name\": \"" << p.name << "\", \"kind\": \"" << kind << "\", \"material\": \"" << p.semanticMaterial << "\"}";
        if (i + 1 < parts.size()) o << ",";
        o << "\n";
    }
    o << "  ]\n}\n";
    return WriteQualityText(path, o.str());
}

inline bool WriteGltfPackageManifest(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"gltf_package_manifest\",\n";
    o << "  \"gltf\": \"" << asset.complete.asset.gltfPath.generic_string() << "\",\n";
    o << "  \"materials\": " << asset.complete.asset.parts.size() << ",\n";
    o << "  \"textures\": [\"" << asset.textureAtlasPath.generic_string() << "\", \"" << asset.projectedTexturePath.generic_string() << "\"],\n";
    o << "  \"lods\": [\"" << asset.complete.asset.gltfPath.generic_string() << "\", \"" << asset.complete.asset.lodObjPath.generic_string() << "\", \"" << asset.lod2Path.generic_string() << "\"]\n";
    o << "}\n";
    return WriteQualityText(path, o.str());
}

inline QualityPackResult BuildQualityPack(const FinalGameAssetResult& asset, const std::vector<std::filesystem::path>& frames, const std::filesystem::path& outputDir, int textureSize = 128) {
    QualityPackResult result;
    if (!asset.ok) return result;
    result.uvLayoutPath = outputDir / "make3d_uv_layout.svg";
    result.projectedAlbedoPath = outputDir / "textures" / "make3d_projected_albedo_from_frames.ppm";
    result.retopoPlanPath = outputDir / "make3d_retopology_plan.json";
    result.skinWeightsPath = outputDir / "make3d_skin_weights_v2.json";
    result.temporalFusionPath = outputDir / "make3d_temporal_fusion_report.json";
    result.facadeFeaturesPath = outputDir / "make3d_facade_features.json";
    result.gltfPackagePath = outputDir / "make3d_gltf_package_manifest.json";
    bool a = WriteUvLayoutSvg(result.uvLayoutPath, asset.complete.asset.mesh);
    bool b = WriteProjectedAlbedoFromFrames(result.projectedAlbedoPath, frames, textureSize);
    bool c = WriteRetopoPlan(result.retopoPlanPath, asset);
    bool d = WriteSkinWeightsV2(result.skinWeightsPath, asset);
    bool e = WriteTemporalFusionReport(result.temporalFusionPath, frames);
    bool f = WriteFacadeFeatures(result.facadeFeaturesPath, asset);
    bool g = WriteGltfPackageManifest(result.gltfPackagePath, asset);
    result.ok = a && b && c && d && e && f && g;
    return result;
}

} // namespace make3d
