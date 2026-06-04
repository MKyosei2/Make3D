#include "Make3DGltfMaterialExporter.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

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

} // namespace

bool ExportGLTFWithMaterial(
    const MeshData& mesh,
    const std::filesystem::path& gltfPath,
    const GltfMaterialOptions& material,
    std::string* error) {

    if (mesh.positions.empty() || mesh.indices.empty()) {
        if (error) *error = "Mesh is empty.";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(gltfPath.parent_path(), ec);
    std::filesystem::path binPath = gltfPath;
    binPath.replace_extension(".bin");

    std::ofstream bin(binPath, std::ios::binary);
    if (!bin) {
        if (error) *error = "Failed to open glTF binary buffer.";
        return false;
    }

    auto writeVector = [&](const auto& values) {
        if (!values.empty()) {
            bin.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(values[0])));
        }
    };

    const size_t posOffset = 0;
    writeVector(mesh.positions);
    const size_t normOffset = mesh.positions.size() * sizeof(float);
    writeVector(mesh.normals);
    const size_t uvOffset = normOffset + mesh.normals.size() * sizeof(float);
    writeVector(mesh.uvs);
    const size_t idxOffset = uvOffset + mesh.uvs.size() * sizeof(float);
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
    gltf << "  \"asset\": { \"version\": \"2.0\", \"generator\": \"Make3DAdvanced\" },\n";
    gltf << "  \"scene\": 0,\n";
    gltf << "  \"scenes\": [{ \"nodes\": [0] }],\n";
    gltf << "  \"nodes\": [{ \"mesh\": 0, \"name\": \"Make3DAdvancedModel\" }],\n";
    gltf << "  \"meshes\": [{ \"primitives\": [{ \"attributes\": { \"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": 2 }, \"indices\": 3, \"material\": 0, \"mode\": 4 }] }],\n";

    gltf << "  \"materials\": [{\n";
    gltf << "    \"name\": \"" << EscapeJson(material.materialName) << "\",\n";
    gltf << "    \"doubleSided\": " << (material.doubleSided ? "true" : "false") << ",\n";
    gltf << "    \"pbrMetallicRoughness\": {\n";
    gltf << "      \"baseColorFactor\": [" << material.baseColorFactor[0] << ", " << material.baseColorFactor[1] << ", " << material.baseColorFactor[2] << ", " << material.baseColorFactor[3] << "],\n";
    gltf << "      \"metallicFactor\": " << material.metallicFactor << ",\n";
    gltf << "      \"roughnessFactor\": " << material.roughnessFactor;
    if (material.textureUri) gltf << ",\n      \"baseColorTexture\": { \"index\": 0 }\n";
    else gltf << "\n";
    gltf << "    }\n";
    gltf << "  }],\n";

    if (material.textureUri) {
        gltf << "  \"textures\": [{ \"source\": 0 }],\n";
        gltf << "  \"images\": [{ \"uri\": \"" << EscapeJson(material.textureUri->filename().u8string()) << "\" }],\n";
    }

    gltf << "  \"buffers\": [{ \"uri\": \"" << EscapeJson(binPath.filename().u8string()) << "\", \"byteLength\": " << totalSize << " }],\n";
    gltf << "  \"bufferViews\": [\n";
    gltf << "    { \"buffer\": 0, \"byteOffset\": " << posOffset << ", \"byteLength\": " << mesh.positions.size() * sizeof(float) << ", \"target\": 34962 },\n";
    gltf << "    { \"buffer\": 0, \"byteOffset\": " << normOffset << ", \"byteLength\": " << mesh.normals.size() * sizeof(float) << ", \"target\": 34962 },\n";
    gltf << "    { \"buffer\": 0, \"byteOffset\": " << uvOffset << ", \"byteLength\": " << mesh.uvs.size() * sizeof(float) << ", \"target\": 34962 },\n";
    gltf << "    { \"buffer\": 0, \"byteOffset\": " << idxOffset << ", \"byteLength\": " << mesh.indices.size() * sizeof(std::uint32_t) << ", \"target\": 34963 }\n";
    gltf << "  ],\n";
    gltf << "  \"accessors\": [\n";
    gltf << "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": " << VertexCount(mesh) << ", \"type\": \"VEC3\", \"min\": [" << minV[0] << ", " << minV[1] << ", " << minV[2] << "], \"max\": [" << maxV[0] << ", " << maxV[1] << ", " << maxV[2] << "] },\n";
    gltf << "    { \"bufferView\": 1, \"componentType\": 5126, \"count\": " << VertexCount(mesh) << ", \"type\": \"VEC3\" },\n";
    gltf << "    { \"bufferView\": 2, \"componentType\": 5126, \"count\": " << VertexCount(mesh) << ", \"type\": \"VEC2\" },\n";
    gltf << "    { \"bufferView\": 3, \"componentType\": 5125, \"count\": " << mesh.indices.size() << ", \"type\": \"SCALAR\" }\n";
    gltf << "  ]\n";
    gltf << "}\n";

    return true;
}

} // namespace make3d
