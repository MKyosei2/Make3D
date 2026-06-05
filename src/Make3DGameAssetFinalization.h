#pragma once

#include "Make3DGameAssetCompletion.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace make3d {

struct FinalMeshCheck {
    int vertices = 0;
    int triangles = 0;
    int boundaryEdges = 0;
    int invalidIndices = 0;
    bool usable = false;
};

struct FinalGameAssetOptions {
    CompletedGameAssetOptions complete;
    bool writeProjectedTexture = true;
    bool writeRetopoProxy = true;
    bool writeBindingMetadata = true;
    bool writeAnimationPreview = true;
    bool writeMeshCheck = true;
    bool writeFrameReport = true;
    int textureSize = 128;
};

struct FinalGameAssetResult {
    bool ok = false;
    std::string message;
    CompletedGameAssetResult complete;
    MeshData retopoProxy;
    FinalMeshCheck meshCheck;
    std::filesystem::path projectedTexturePath;
    std::filesystem::path retopoProxyPath;
    std::filesystem::path bindingMetadataPath;
    std::filesystem::path animationPreviewPath;
    std::filesystem::path meshCheckPath;
    std::filesystem::path frameReportPath;
};

namespace final_detail {

inline int Verts(const MeshData& m) { return static_cast<int>(m.positions.size() / 3); }
inline int Tris(const MeshData& m) { return static_cast<int>(m.indices.size() / 3); }

inline void V(MeshData& m, float x, float y, float z) {
    m.positions.push_back(x); m.positions.push_back(y); m.positions.push_back(z);
    m.normals.push_back(0.0f); m.normals.push_back(1.0f); m.normals.push_back(0.0f);
    m.uvs.push_back(x); m.uvs.push_back(z);
}

inline void T(MeshData& m, int a, int b, int c) {
    m.indices.push_back(static_cast<std::uint32_t>(a));
    m.indices.push_back(static_cast<std::uint32_t>(b));
    m.indices.push_back(static_cast<std::uint32_t>(c));
}

inline void Box(MeshData& m, const GameAssetBounds& b, float scaleX, float scaleY, float scaleZ, float offsetY) {
    int base = Verts(m);
    float sx = std::max(0.05f, b.sizeX * scaleX), sy = std::max(0.05f, b.sizeY * scaleY), sz = std::max(0.05f, b.sizeZ * scaleZ);
    float cx = b.centerX, cy = b.centerY + offsetY, cz = b.centerZ;
    float x0 = cx - sx * 0.5f, x1 = cx + sx * 0.5f;
    float y0 = cy - sy * 0.5f, y1 = cy + sy * 0.5f;
    float z0 = cz - sz * 0.5f, z1 = cz + sz * 0.5f;
    V(m,x0,y0,z0); V(m,x1,y0,z0); V(m,x1,y1,z0); V(m,x0,y1,z0);
    V(m,x0,y0,z1); V(m,x1,y0,z1); V(m,x1,y1,z1); V(m,x0,y1,z1);
    const int f[12][3] = {{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{3,7,6},{3,6,2},{1,2,6},{1,6,5},{0,4,7},{0,7,3}};
    for (auto& q : f) T(m, base + q[0], base + q[1], base + q[2]);
}

inline bool Text(const std::filesystem::path& p, const std::string& s) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    if (!out) return false;
    out << s;
    return true;
}

inline bool ProjectTexture(const std::filesystem::path& input, const std::filesystem::path& outPath, int size) {
    std::string err;
    auto img = LoadImageRGBA(input, &err);
    if (!img) return false;
    size = std::max(16, size);
    std::filesystem::create_directories(outPath.parent_path());
    std::ofstream out(outPath, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << size << " " << size << "\n255\n";
    for (int y = 0; y < size; ++y) {
        int sy = std::min(img->height - 1, y * img->height / size);
        for (int x = 0; x < size; ++x) {
            int sx = std::min(img->width - 1, x * img->width / size);
            size_t i = (static_cast<size_t>(sy) * img->width + sx) * 4;
            unsigned char rgb[3] = {img->pixels[i], img->pixels[i + 1], img->pixels[i + 2]};
            out.write(reinterpret_cast<const char*>(rgb), 3);
        }
    }
    return true;
}

inline std::pair<std::uint32_t, std::uint32_t> Edge(std::uint32_t a, std::uint32_t b) {
    return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
}

} // namespace final_detail

inline MeshData BuildRetopoProxyMesh(GameAssetType type, const GameAssetBounds& b) {
    MeshData m;
    final_detail::Box(m, b, 1.0f, 1.0f, 1.0f, 0.0f);
    if (type == GameAssetType::Character) {
        final_detail::Box(m, b, 0.35f, 0.22f, 0.45f, b.sizeY * 0.40f);
    } else if (type == GameAssetType::Building) {
        final_detail::Box(m, b, 1.08f, 0.10f, 1.04f, b.sizeY * 0.55f);
    }
    RecomputeNormals(m);
    ApplyPlanarUvProjection(m);
    return m;
}

inline FinalMeshCheck RunFinalMeshCheck(const MeshData& m) {
    FinalMeshCheck c;
    c.vertices = final_detail::Verts(m);
    c.triangles = final_detail::Tris(m);
    std::vector<std::pair<std::uint32_t, std::uint32_t>> edges;
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        std::uint32_t a = m.indices[i], b = m.indices[i + 1], d = m.indices[i + 2];
        if (a >= static_cast<std::uint32_t>(c.vertices) || b >= static_cast<std::uint32_t>(c.vertices) || d >= static_cast<std::uint32_t>(c.vertices)) { ++c.invalidIndices; continue; }
        edges.push_back(final_detail::Edge(a,b)); edges.push_back(final_detail::Edge(b,d)); edges.push_back(final_detail::Edge(d,a));
    }
    std::sort(edges.begin(), edges.end());
    for (size_t i = 0; i < edges.size();) {
        size_t j = i + 1;
        while (j < edges.size() && edges[j] == edges[i]) ++j;
        if (j - i == 1) ++c.boundaryEdges;
        i = j;
    }
    c.usable = c.vertices > 0 && c.triangles > 0 && c.invalidIndices == 0;
    return c;
}

inline std::string MeshCheckJson(const FinalMeshCheck& c) {
    std::ostringstream o;
    o << "{\n  \"vertices\": " << c.vertices << ",\n  \"triangles\": " << c.triangles << ",\n  \"boundaryEdges\": " << c.boundaryEdges << ",\n  \"invalidIndices\": " << c.invalidIndices << ",\n  \"usable\": " << (c.usable ? "true" : "false") << "\n}\n";
    return o.str();
}

inline bool WriteBindingMetadata(const std::filesystem::path& path, const CompletedGameAssetResult& a) {
    std::ostringstream o;
    o << "{\n  \"assetType\": \"" << ToString(a.asset.assetType) << "\",\n  \"note\": \"procedural binding zones for later engine rig conversion\",\n  \"joints\": [\n";
    for (size_t i = 0; i < a.joints.size(); ++i) {
        const auto& j = a.joints[i];
        o << "    {\"name\": \"" << j.name << "\", \"parent\": " << j.parent << ", \"center\": [" << j.x << ", " << j.y << ", " << j.z << "]}";
        if (i + 1 < a.joints.size()) o << ",";
        o << "\n";
    }
    o << "  ]\n}\n";
    return final_detail::Text(path, o.str());
}

inline bool WriteAnimPreview(const std::filesystem::path& path, GameAssetType type) {
    std::ostringstream o;
    o << "{\n  \"assetType\": \"" << ToString(type) << "\",\n  \"clips\": [\n    {\"name\": \"turntable_preview\", \"frames\": 120, \"rootYawDegrees\": 360},\n    {\"name\": \"idle_validation_pose\", \"frames\": 60, \"rootYawDegrees\": 0}\n  ]\n}\n";
    return final_detail::Text(path, o.str());
}

inline bool WriteFrameReport(const std::filesystem::path& path, const std::vector<std::filesystem::path>& frames) {
    std::ostringstream o;
    o << "{\n  \"frameCount\": " << frames.size() << ",\n  \"method\": \"multi-frame ingestion hook; temporal fusion can be implemented behind this report\",\n  \"frames\": [";
    for (size_t i = 0; i < frames.size(); ++i) {
        if (i) o << ", ";
        o << "\"" << frames[i].generic_string() << "\"";
    }
    o << "]\n}\n";
    return final_detail::Text(path, o.str());
}

inline FinalGameAssetResult BuildFinalGameAssetFromFrames(const std::vector<std::filesystem::path>& frames, const std::filesystem::path& out, const FinalGameAssetOptions& options = FinalGameAssetOptions{}) {
    FinalGameAssetResult r;
    if (frames.empty()) { r.message = "no frames supplied"; return r; }
    r.complete = BuildCompleteGameAssetFromImage(frames.front(), std::nullopt, out, options.complete);
    if (!r.complete.ok) { r.message = r.complete.message; return r; }
    if (options.writeProjectedTexture) { r.projectedTexturePath = out / "textures" / "make3d_projected_texture.ppm"; final_detail::ProjectTexture(frames.front(), r.projectedTexturePath, options.textureSize); }
    if (options.writeRetopoProxy) { r.retopoProxy = BuildRetopoProxyMesh(r.complete.asset.assetType, r.complete.bounds); r.retopoProxyPath = out / "make3d_retopology_proxy.obj"; std::string e; ExportOBJ(r.retopoProxy, r.retopoProxyPath, "", &e); }
    if (options.writeBindingMetadata) { r.bindingMetadataPath = out / "make3d_binding_metadata.json"; WriteBindingMetadata(r.bindingMetadataPath, r.complete); }
    if (options.writeAnimationPreview) { r.animationPreviewPath = out / "make3d_animation_preview.json"; WriteAnimPreview(r.animationPreviewPath, r.complete.asset.assetType); }
    if (options.writeMeshCheck) { r.meshCheck = RunFinalMeshCheck(r.complete.asset.mesh); r.meshCheckPath = out / "make3d_final_mesh_check.json"; final_detail::Text(r.meshCheckPath, MeshCheckJson(r.meshCheck)); }
    if (options.writeFrameReport) { r.frameReportPath = out / "make3d_frame_report.json"; WriteFrameReport(r.frameReportPath, frames); }
    r.ok = true;
    r.message = "Final game asset export finished.";
    return r;
}

inline FinalGameAssetResult BuildFinalGameAssetFromImage(const std::filesystem::path& image, const std::filesystem::path& out, const FinalGameAssetOptions& options = FinalGameAssetOptions{}) {
    return BuildFinalGameAssetFromFrames({image}, out, options);
}

} // namespace make3d
