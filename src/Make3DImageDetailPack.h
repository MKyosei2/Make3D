#pragma once

#include "Make3DProductionQualityPack.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace make3d {

struct ImageDetailPackResult {
    bool ok = false;
    MeshData displacedMesh;
    std::filesystem::path edgeMapPath;
    std::filesystem::path heightMapPath;
    std::filesystem::path detailNormalPath;
    std::filesystem::path displacedMeshPath;
    std::filesystem::path featureLinesPath;
    std::filesystem::path detailQualityPath;
};

inline bool WriteImageDetailText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return true;
}

inline unsigned char LumaAt(const ImageRGBA& img, int x, int y) {
    x = std::clamp(x, 0, img.width - 1);
    y = std::clamp(y, 0, img.height - 1);
    size_t p = (static_cast<size_t>(y) * img.width + x) * 4;
    return static_cast<unsigned char>((static_cast<int>(img.pixels[p]) * 30 + static_cast<int>(img.pixels[p + 1]) * 59 + static_cast<int>(img.pixels[p + 2]) * 11) / 100);
}

inline bool WriteEdgeMap(const std::filesystem::path& path, const ImageRGBA& img, int size) {
    size = std::max(16, size);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << size << " " << size << "\n255\n";
    for (int y = 0; y < size; ++y) {
        int sy = y * img.height / size;
        for (int x = 0; x < size; ++x) {
            int sx = x * img.width / size;
            int gx = static_cast<int>(LumaAt(img, sx + 1, sy)) - static_cast<int>(LumaAt(img, sx - 1, sy));
            int gy = static_cast<int>(LumaAt(img, sx, sy + 1)) - static_cast<int>(LumaAt(img, sx, sy - 1));
            unsigned char e = static_cast<unsigned char>(std::min(255, std::abs(gx) + std::abs(gy)));
            unsigned char rgb[3] = {e, e, e};
            out.write(reinterpret_cast<const char*>(rgb), 3);
        }
    }
    return true;
}

inline bool WriteHeightMap(const std::filesystem::path& path, const ImageRGBA& img, int size) {
    size = std::max(16, size);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << size << " " << size << "\n255\n";
    for (int y = 0; y < size; ++y) {
        int sy = y * img.height / size;
        for (int x = 0; x < size; ++x) {
            int sx = x * img.width / size;
            unsigned char h = LumaAt(img, sx, sy);
            unsigned char rgb[3] = {h, h, h};
            out.write(reinterpret_cast<const char*>(rgb), 3);
        }
    }
    return true;
}

inline bool WriteDetailNormalMap(const std::filesystem::path& path, const ImageRGBA& img, int size) {
    size = std::max(16, size);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << size << " " << size << "\n255\n";
    for (int y = 0; y < size; ++y) {
        int sy = y * img.height / size;
        for (int x = 0; x < size; ++x) {
            int sx = x * img.width / size;
            float dx = (static_cast<float>(LumaAt(img, sx + 1, sy)) - static_cast<float>(LumaAt(img, sx - 1, sy))) / 255.0f;
            float dy = (static_cast<float>(LumaAt(img, sx, sy + 1)) - static_cast<float>(LumaAt(img, sx, sy - 1))) / 255.0f;
            float nx = -dx, ny = -dy, nz = 1.0f;
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 0.0001f) { nx /= len; ny /= len; nz /= len; }
            unsigned char rgb[3] = {
                static_cast<unsigned char>((nx * 0.5f + 0.5f) * 255.0f),
                static_cast<unsigned char>((ny * 0.5f + 0.5f) * 255.0f),
                static_cast<unsigned char>((nz * 0.5f + 0.5f) * 255.0f)};
            out.write(reinterpret_cast<const char*>(rgb), 3);
        }
    }
    return true;
}

inline MeshData BuildImageDisplacedMesh(const MeshData& input, const ImageRGBA& img, float amount) {
    MeshData out = input;
    if (out.normals.size() != out.positions.size()) RecomputeNormals(out);
    for (size_t vi = 0; vi * 3 + 2 < out.positions.size(); ++vi) {
        float u = (vi * 2 + 1 < out.uvs.size()) ? out.uvs[vi * 2 + 0] : 0.5f;
        float v = (vi * 2 + 1 < out.uvs.size()) ? out.uvs[vi * 2 + 1] : 0.5f;
        int x = static_cast<int>(std::clamp(u, 0.0f, 1.0f) * static_cast<float>(img.width - 1));
        int y = static_cast<int>((1.0f - std::clamp(v, 0.0f, 1.0f)) * static_cast<float>(img.height - 1));
        float h = static_cast<float>(LumaAt(img, x, y)) / 255.0f - 0.5f;
        out.positions[vi * 3 + 0] += out.normals[vi * 3 + 0] * h * amount;
        out.positions[vi * 3 + 1] += out.normals[vi * 3 + 1] * h * amount;
        out.positions[vi * 3 + 2] += out.normals[vi * 3 + 2] * h * amount;
    }
    RecomputeNormals(out);
    return out;
}

inline bool WriteFeatureLines(const std::filesystem::path& path, const ImageRGBA& img) {
    int strongEdges = 0;
    int samples = 0;
    for (int y = 1; y + 1 < img.height; y += 2) {
        for (int x = 1; x + 1 < img.width; x += 2) {
            int gx = static_cast<int>(LumaAt(img, x + 1, y)) - static_cast<int>(LumaAt(img, x - 1, y));
            int gy = static_cast<int>(LumaAt(img, x, y + 1)) - static_cast<int>(LumaAt(img, x, y - 1));
            if (std::abs(gx) + std::abs(gy) > 48) ++strongEdges;
            ++samples;
        }
    }
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"image_feature_lines\",\n";
    o << "  \"samples\": " << samples << ",\n";
    o << "  \"strongEdges\": " << strongEdges << ",\n";
    o << "  \"edgeDensity\": " << (samples ? static_cast<float>(strongEdges) / static_cast<float>(samples) : 0.0f) << "\n";
    o << "}\n";
    return WriteImageDetailText(path, o.str());
}

inline bool WriteDetailQuality(const std::filesystem::path& path, const ImageDetailPackResult& detail) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"image_detail_quality_report\",\n";
    o << "  \"edgeMap\": \"" << detail.edgeMapPath.generic_string() << "\",\n";
    o << "  \"heightMap\": \"" << detail.heightMapPath.generic_string() << "\",\n";
    o << "  \"detailNormal\": \"" << detail.detailNormalPath.generic_string() << "\",\n";
    o << "  \"displacedPreviewMesh\": \"" << detail.displacedMeshPath.generic_string() << "\"\n";
    o << "}\n";
    return WriteImageDetailText(path, o.str());
}

inline ImageDetailPackResult BuildImageDetailPack(const FinalGameAssetResult& asset, const GeometryRefinementPackResult& geometry, const std::vector<std::filesystem::path>& frames, const std::filesystem::path& outputDir, int textureSize = 128) {
    ImageDetailPackResult result;
    if (!asset.ok || !geometry.ok || frames.empty()) return result;
    std::string error;
    auto img = LoadImageRGBA(frames.front(), &error);
    if (!img) return result;
    result.edgeMapPath = outputDir / "textures" / "make3d_image_edge_map.ppm";
    result.heightMapPath = outputDir / "textures" / "make3d_image_height_map.ppm";
    result.detailNormalPath = outputDir / "textures" / "make3d_image_detail_normal.ppm";
    result.displacedMeshPath = outputDir / "make3d_image_displaced_preview.obj";
    result.featureLinesPath = outputDir / "make3d_image_feature_lines.json";
    result.detailQualityPath = outputDir / "make3d_image_detail_quality.json";
    result.displacedMesh = BuildImageDisplacedMesh(geometry.repairedMesh, *img, 0.035f);
    std::string e;
    bool a = WriteEdgeMap(result.edgeMapPath, *img, textureSize);
    bool b = WriteHeightMap(result.heightMapPath, *img, textureSize);
    bool c = WriteDetailNormalMap(result.detailNormalPath, *img, textureSize);
    bool d = ExportOBJ(result.displacedMesh, result.displacedMeshPath, "", &e);
    bool f = WriteFeatureLines(result.featureLinesPath, *img);
    bool g = WriteDetailQuality(result.detailQualityPath, result);
    result.ok = a && b && c && d && f && g;
    return result;
}

} // namespace make3d
