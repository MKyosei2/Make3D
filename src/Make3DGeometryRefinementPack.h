#pragma once

#include "Make3DQualityPack.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace make3d {

struct GeometryRefinementPackResult {
    bool ok = false;
    MeshData repairedMesh;
    std::filesystem::path repairedMeshPath;
    std::filesystem::path normalMapPath;
    std::filesystem::path occlusionMapPath;
    std::filesystem::path meshRepairReportPath;
    std::filesystem::path uvSeamReportPath;
    std::filesystem::path projectionQualityPath;
    std::filesystem::path partRetopoGuidePath;
};

inline bool WriteGeometryText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return true;
}

inline MeshData BuildWeldedMesh(const MeshData& input) {
    MeshData out;
    std::map<std::tuple<int, int, int, int, int>, std::uint32_t> remap;
    std::vector<std::uint32_t> oldToNew(input.positions.size() / 3, 0);
    for (size_t i = 0; i + 2 < input.positions.size(); i += 3) {
        size_t vi = i / 3;
        float u = (vi * 2 + 1 < input.uvs.size()) ? input.uvs[vi * 2 + 0] : 0.0f;
        float v = (vi * 2 + 1 < input.uvs.size()) ? input.uvs[vi * 2 + 1] : 0.0f;
        auto key = std::make_tuple(
            static_cast<int>(std::round(input.positions[i + 0] * 10000.0f)),
            static_cast<int>(std::round(input.positions[i + 1] * 10000.0f)),
            static_cast<int>(std::round(input.positions[i + 2] * 10000.0f)),
            static_cast<int>(std::round(u * 10000.0f)),
            static_cast<int>(std::round(v * 10000.0f)));
        auto it = remap.find(key);
        if (it == remap.end()) {
            std::uint32_t ni = static_cast<std::uint32_t>(out.positions.size() / 3);
            remap[key] = ni;
            oldToNew[vi] = ni;
            out.positions.push_back(input.positions[i + 0]);
            out.positions.push_back(input.positions[i + 1]);
            out.positions.push_back(input.positions[i + 2]);
            out.uvs.push_back(u);
            out.uvs.push_back(v);
            out.normals.push_back(0.0f);
            out.normals.push_back(1.0f);
            out.normals.push_back(0.0f);
        } else {
            oldToNew[vi] = it->second;
        }
    }
    for (size_t i = 0; i + 2 < input.indices.size(); i += 3) {
        std::uint32_t a = input.indices[i + 0], b = input.indices[i + 1], c = input.indices[i + 2];
        if (a >= oldToNew.size() || b >= oldToNew.size() || c >= oldToNew.size()) continue;
        a = oldToNew[a]; b = oldToNew[b]; c = oldToNew[c];
        if (a == b || b == c || c == a) continue;
        out.indices.push_back(a); out.indices.push_back(b); out.indices.push_back(c);
    }
    RecomputeNormals(out);
    return out;
}

inline bool WriteNormalMap(const std::filesystem::path& path, const MeshData& mesh, int size) {
    size = std::max(16, size);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << size << " " << size << "\n255\n";
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            size_t vi = mesh.normals.empty() ? 0 : static_cast<size_t>((x + y * size) % std::max(1, static_cast<int>(mesh.normals.size() / 3)));
            float nx = mesh.normals.empty() ? 0.0f : mesh.normals[vi * 3 + 0];
            float ny = mesh.normals.empty() ? 1.0f : mesh.normals[vi * 3 + 1];
            float nz = mesh.normals.empty() ? 0.0f : mesh.normals[vi * 3 + 2];
            unsigned char rgb[3] = {
                static_cast<unsigned char>(std::clamp(nx * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f),
                static_cast<unsigned char>(std::clamp(ny * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f),
                static_cast<unsigned char>(std::clamp(nz * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f)};
            out.write(reinterpret_cast<const char*>(rgb), 3);
        }
    }
    return true;
}

inline bool WriteOcclusionMap(const std::filesystem::path& path, const MeshData& mesh, int size) {
    size = std::max(16, size);
    float minY = 0.0f, maxY = 1.0f;
    if (!mesh.positions.empty()) {
        minY = maxY = mesh.positions[1];
        for (size_t i = 1; i < mesh.positions.size(); i += 3) { minY = std::min(minY, mesh.positions[i]); maxY = std::max(maxY, mesh.positions[i]); }
    }
    float range = std::max(0.0001f, maxY - minY);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << size << " " << size << "\n255\n";
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            size_t vi = mesh.positions.empty() ? 0 : static_cast<size_t>((x + y * size) % std::max(1, static_cast<int>(mesh.positions.size() / 3)));
            float yy = mesh.positions.empty() ? maxY : mesh.positions[vi * 3 + 1];
            unsigned char v = static_cast<unsigned char>((0.55f + 0.45f * ((yy - minY) / range)) * 255.0f);
            unsigned char rgb[3] = {v, v, v};
            out.write(reinterpret_cast<const char*>(rgb), 3);
        }
    }
    return true;
}

inline bool WriteMeshRepairReport(const std::filesystem::path& path, const MeshData& before, const MeshData& after) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"mesh_repair_report\",\n";
    o << "  \"beforeVertices\": " << before.positions.size() / 3 << ",\n";
    o << "  \"afterVertices\": " << after.positions.size() / 3 << ",\n";
    o << "  \"beforeTriangles\": " << before.indices.size() / 3 << ",\n";
    o << "  \"afterTriangles\": " << after.indices.size() / 3 << ",\n";
    o << "  \"operation\": \"weld duplicate vertices, remove invalid or degenerate triangles, recompute normals\"\n";
    o << "}\n";
    return WriteGeometryText(path, o.str());
}

inline bool WriteUvSeamReport(const std::filesystem::path& path, const MeshData& mesh) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"uv_seam_report\",\n";
    o << "  \"uvVertices\": " << mesh.uvs.size() / 2 << ",\n";
    o << "  \"triangles\": " << mesh.indices.size() / 3 << ",\n";
    o << "  \"method\": \"planar UV projection plus SVG layout output\"\n";
    o << "}\n";
    return WriteGeometryText(path, o.str());
}

inline bool WriteProjectionQualityReport(const std::filesystem::path& path, const std::vector<std::filesystem::path>& frames, const QualityPackResult& quality) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"projection_quality_report\",\n";
    o << "  \"frameCount\": " << frames.size() << ",\n";
    o << "  \"projectedAlbedo\": \"" << quality.projectedAlbedoPath.generic_string() << "\",\n";
    o << "  \"uvLayout\": \"" << quality.uvLayoutPath.generic_string() << "\",\n";
    o << "  \"mode\": \"multi-frame average projection\"\n";
    o << "}\n";
    return WriteGeometryText(path, o.str());
}

inline bool WritePartRetopoGuide(const std::filesystem::path& path, const FinalGameAssetResult& asset) {
    std::ostringstream o;
    o << "{\n  \"type\": \"part_retopology_guide\",\n  \"parts\": [\n";
    const auto& parts = asset.complete.asset.parts;
    for (size_t i = 0; i < parts.size(); ++i) {
        const auto& p = parts[i];
        o << "    {\"name\": \"" << p.name << "\", \"targetTriangles\": " << std::max(1, p.triangleCount / 2) << ", \"preserveBoundary\": true}";
        if (i + 1 < parts.size()) o << ",";
        o << "\n";
    }
    o << "  ]\n}\n";
    return WriteGeometryText(path, o.str());
}

inline GeometryRefinementPackResult BuildGeometryRefinementPack(const FinalGameAssetResult& asset, const QualityPackResult& quality, const std::vector<std::filesystem::path>& frames, const std::filesystem::path& outputDir, int textureSize = 128) {
    GeometryRefinementPackResult result;
    if (!asset.ok || !quality.ok) return result;
    result.repairedMesh = BuildWeldedMesh(asset.complete.asset.mesh);
    result.repairedMeshPath = outputDir / "make3d_repaired_mesh.obj";
    result.normalMapPath = outputDir / "textures" / "make3d_generated_normal_map.ppm";
    result.occlusionMapPath = outputDir / "textures" / "make3d_generated_occlusion_map.ppm";
    result.meshRepairReportPath = outputDir / "make3d_mesh_repair_report.json";
    result.uvSeamReportPath = outputDir / "make3d_uv_seam_report.json";
    result.projectionQualityPath = outputDir / "make3d_projection_quality_report.json";
    result.partRetopoGuidePath = outputDir / "make3d_part_retopology_guide.json";
    std::string e;
    bool a = ExportOBJ(result.repairedMesh, result.repairedMeshPath, "", &e);
    bool b = WriteNormalMap(result.normalMapPath, result.repairedMesh, textureSize);
    bool c = WriteOcclusionMap(result.occlusionMapPath, result.repairedMesh, textureSize);
    bool d = WriteMeshRepairReport(result.meshRepairReportPath, asset.complete.asset.mesh, result.repairedMesh);
    bool f = WriteUvSeamReport(result.uvSeamReportPath, result.repairedMesh);
    bool g = WriteProjectionQualityReport(result.projectionQualityPath, frames, quality);
    bool h = WritePartRetopoGuide(result.partRetopoGuidePath, asset);
    result.ok = a && b && c && d && f && g && h;
    return result;
}

} // namespace make3d
