#include "Make3DVertexColorGltfExporter.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

namespace make3d {

namespace {

static std::string EscapeJson(const std::string& value) {
    std::ostringstream oss;
    for (char c : value) {
        switch (c) {
            case '\\': oss << "\\\\"; break;
            case '"': oss << "\\\""; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default: oss << c; break;
        }
    }
    return oss.str();
}

static int VertexCount(const MeshData& mesh) {
    return static_cast<int>(mesh.positions.size() / 3);
}

static void ComputeBounds(const MeshData& mesh, float minV[3], float maxV[3]) {
    minV[0] = minV[1] = minV[2] = std::numeric_limits<float>::max();
    maxV[0] = maxV[1] = maxV[2] = -std::numeric_limits<float>::max();
    if (mesh.positions.empty()) {
        minV[0] = minV[1] = minV[2] = 0.0f;
        maxV[0] = maxV[1] = maxV[2] = 0.0f;
        return;
    }
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        minV[0] = std::min(minV[0], mesh.positions[i + 0]);
        minV[1] = std::min(minV[1], mesh.positions[i + 1]);
        minV[2] = std::min(minV[2], mesh.positions[i + 2]);
        maxV[0] = std::max(maxV[0], mesh.positions[i + 0]);
        maxV[1] = std::max(maxV[1], mesh.positions[i + 1]);
        maxV[2] = std::max(maxV[2], mesh.positions[i + 2]);
    }
}

static std::vector<float> BuildSafeNormals(const MeshData& mesh) {
    int count = VertexCount(mesh);
    if (mesh.normals.size() >= static_cast<size_t>(count) * 3) {
        return std::vector<float>(mesh.normals.begin(), mesh.normals.begin() + static_cast<size_t>(count) * 3);
    }
    std::vector<float> normals(static_cast<size_t>(count) * 3, 0.0f);
    for (int i = 0; i < count; ++i) normals[static_cast<size_t>(i) * 3 + 2] = 1.0f;
    return normals;
}

static std::vector<float> BuildSafeUVs(const MeshData& mesh) {
    int count = VertexCount(mesh);
    if (mesh.uvs.size() >= static_cast<size_t>(count) * 2) {
        return std::vector<float>(mesh.uvs.begin(), mesh.uvs.begin() + static_cast<size_t>(count) * 2);
    }

    float minV[3], maxV[3];
    ComputeBounds(mesh, minV, maxV);
    float rangeX = std::max(0.0001f, maxV[0] - minV[0]);
    float rangeY = std::max(0.0001f, maxV[1] - minV[1]);
    std::vector<float> uvs(static_cast<size_t>(count) * 2, 0.0f);
    for (int i = 0; i < count; ++i) {
        size_t p = static_cast<size_t>(i) * 3;
        uvs[static_cast<size_t>(i) * 2 + 0] = std::clamp((mesh.positions[p + 0] - minV[0]) / rangeX, 0.0f, 1.0f);
        uvs[static_cast<size_t>(i) * 2 + 1] = std::clamp((mesh.positions[p + 1] - minV[1]) / rangeY, 0.0f, 1.0f);
    }
    return uvs;
}

static std::vector<float> BuildVertexColors(const MeshData& mesh, const std::vector<float>& safeUVs, const ImageRGBA& source, const VertexColorGltfOptions& options) {
    int count = VertexCount(mesh);
    std::vector<float> colors(static_cast<size_t>(count) * 4, 1.0f);
    if (count <= 0 || source.width <= 0 || source.height <= 0 || source.pixels.size() < static_cast<size_t>(source.width * source.height * 4)) {
        for (int i = 0; i < count; ++i) {
            colors[static_cast<size_t>(i) * 4 + 0] = options.fallbackColor[0];
            colors[static_cast<size_t>(i) * 4 + 1] = options.fallbackColor[1];
            colors[static_cast<size_t>(i) * 4 + 2] = options.fallbackColor[2];
            colors[static_cast<size_t>(i) * 4 + 3] = options.fallbackColor[3];
        }
        return colors;
    }

    for (int i = 0; i < count; ++i) {
        size_t uv = static_cast<size_t>(i) * 2;
        float u = uv + 1 < safeUVs.size() ? safeUVs[uv + 0] : 0.5f;
        float v = uv + 1 < safeUVs.size() ? safeUVs[uv + 1] : 0.5f;
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);
        int x = std::clamp(static_cast<int>(u * static_cast<float>(source.width - 1)), 0, source.width - 1);
        int y = std::clamp(static_cast<int>((1.0f - v) * static_cast<float>(source.height - 1)), 0, source.height - 1);
        size_t sp = (static_cast<size_t>(y) * source.width + x) * 4;
        colors[static_cast<size_t>(i) * 4 + 0] = static_cast<float>(source.pixels[sp + 0]) / 255.0f;
        colors[static_cast<size_t>(i) * 4 + 1] = static_cast<float>(source.pixels[sp + 1]) / 255.0f;
        colors[static_cast<size_t>(i) * 4 + 2] = static_cast<float>(source.pixels[sp + 2]) / 255.0f;
        colors[static_cast<size_t>(i) * 4 + 3] = std::max(0.15f, static_cast<float>(source.pixels[sp + 3]) / 255.0f);
    }
    return colors;
}

} // namespace

bool ExportGLTFWithVertexColors(
    const MeshData& mesh,
    const ImageRGBA& sourceImage,
    const std::filesystem::path& gltfPath,
    const VertexColorGltfOptions& options,
    std::string* error) {

    int vertexCount = VertexCount(mesh);
    if (vertexCount <= 0 || mesh.indices.empty()) {
        if (error) *error = "Mesh is empty.";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(gltfPath.parent_path(), ec);
    std::filesystem::path binPath = gltfPath;
    binPath.replace_extension(".bin");

    std::vector<float> safeNormals = BuildSafeNormals(mesh);
    std::vector<float> safeUVs = BuildSafeUVs(mesh);
    std::vector<float> colors = BuildVertexColors(mesh, safeUVs, sourceImage, options);

    std::ofstream bin(binPath, std::ios::binary);
    if (!bin) {
        if (error) *error = "Failed to open glTF binary buffer.";
        return false;
    }

    auto writeVector = [&](const auto& values) {
        if (!values.empty()) bin.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(values[0])));
    };

    const size_t posOffset = 0;
    writeVector(mesh.positions);
    const size_t normOffset = mesh.positions.size() * sizeof(float);
    writeVector(safeNormals);
    const size_t uvOffset = normOffset + safeNormals.size() * sizeof(float);
    writeVector(safeUVs);
    const size_t colorOffset = uvOffset + safeUVs.size() * sizeof(float);
    writeVector(colors);
    const size_t idxOffset = colorOffset + colors.size() * sizeof(float);
    writeVector(mesh.indices);
    const size_t totalSize = idxOffset + mesh.indices.size() * sizeof(std::uint32_t);

    float minV[3], maxV[3];
    ComputeBounds(mesh, minV, maxV);

    std::ofstream gltf(gltfPath, std::ios::binary);
    if (!gltf) {
        if (error) *error = "Failed to open glTF file.";
        return false;
    }

    gltf << std::fixed << std::setprecision(6);
    gltf << "{\n";
    gltf << "  \"asset\": { \"version\": \"2.0\", \"generator\": \"Make3DAdvancedVertexColor\" },\n";
    gltf << "  \"scene\": 0,\n";
    gltf << "  \"scenes\": [{ \"nodes\": [0] }],\n";
    gltf << "  \"nodes\": [{ \"mesh\": 0, \"name\": \"Make3DVertexColoredModel\" }],\n";
    gltf << "  \"meshes\": [{ \"primitives\": [{ \"attributes\": { \"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": 2, \"COLOR_0\": 3 }, \"indices\": 4, \"material\": 0, \"mode\": 4 }] }],\n";
    gltf << "  \"materials\": [{\n";
    gltf << "    \"name\": \"" << EscapeJson(options.materialName) << "\",\n";
    gltf << "    \"doubleSided\": " << (options.doubleSided ? "true" : "false") << ",\n";
    gltf << "    \"pbrMetallicRoughness\": { \"metallicFactor\": " << options.metallicFactor << ", \"roughnessFactor\": " << options.roughnessFactor << " }\n";
    gltf << "  }],\n";
    gltf << "  \"buffers\": [{ \"uri\": \"" << EscapeJson(binPath.filename().u8string()) << "\", \"byteLength\": " << totalSize << " }],\n";
    gltf << "  \"bufferViews\": [\n";
    gltf << "    { \"buffer\": 0, \"byteOffset\": " << posOffset << ", \"byteLength\": " << mesh.positions.size() * sizeof(float) << ", \"target\": 34962 },\n";
    gltf << "    { \"buffer\": 0, \"byteOffset\": " << normOffset << ", \"byteLength\": " << safeNormals.size() * sizeof(float) << ", \"target\": 34962 },\n";
    gltf << "    { \"buffer\": 0, \"byteOffset\": " << uvOffset << ", \"byteLength\": " << safeUVs.size() * sizeof(float) << ", \"target\": 34962 },\n";
    gltf << "    { \"buffer\": 0, \"byteOffset\": " << colorOffset << ", \"byteLength\": " << colors.size() * sizeof(float) << ", \"target\": 34962 },\n";
    gltf << "    { \"buffer\": 0, \"byteOffset\": " << idxOffset << ", \"byteLength\": " << mesh.indices.size() * sizeof(std::uint32_t) << ", \"target\": 34963 }\n";
    gltf << "  ],\n";
    gltf << "  \"accessors\": [\n";
    gltf << "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": " << vertexCount << ", \"type\": \"VEC3\", \"min\": [" << minV[0] << ", " << minV[1] << ", " << minV[2] << "], \"max\": [" << maxV[0] << ", " << maxV[1] << ", " << maxV[2] << "] },\n";
    gltf << "    { \"bufferView\": 1, \"componentType\": 5126, \"count\": " << vertexCount << ", \"type\": \"VEC3\" },\n";
    gltf << "    { \"bufferView\": 2, \"componentType\": 5126, \"count\": " << vertexCount << ", \"type\": \"VEC2\" },\n";
    gltf << "    { \"bufferView\": 3, \"componentType\": 5126, \"count\": " << vertexCount << ", \"type\": \"VEC4\" },\n";
    gltf << "    { \"bufferView\": 4, \"componentType\": 5125, \"count\": " << mesh.indices.size() << ", \"type\": \"SCALAR\" }\n";
    gltf << "  ]\n";
    gltf << "}\n";
    return true;
}

} // namespace make3d
