#include "Make3DHeroSemanticGltfExporter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace make3d {

namespace fs = std::filesystem;

namespace {

static std::string EscapeJson(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '\\') o << "\\\\";
        else if (c == '"') o << "\\\"";
        else if (c == '\n') o << "\\n";
        else o << c;
    }
    return o.str();
}

static int VertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
static int IndexCount(const MeshData& mesh) { return static_cast<int>(mesh.indices.size()); }

struct Bounds {
    float minX = 0, minY = 0, minZ = 0;
    float maxX = 0, maxY = 0, maxZ = 0;
};

static Bounds ComputeBounds(const MeshData& mesh) {
    Bounds b;
    if (mesh.positions.size() < 3) return b;
    b.minX = b.maxX = mesh.positions[0];
    b.minY = b.maxY = mesh.positions[1];
    b.minZ = b.maxZ = mesh.positions[2];
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        b.minX = std::min(b.minX, mesh.positions[i + 0]);
        b.minY = std::min(b.minY, mesh.positions[i + 1]);
        b.minZ = std::min(b.minZ, mesh.positions[i + 2]);
        b.maxX = std::max(b.maxX, mesh.positions[i + 0]);
        b.maxY = std::max(b.maxY, mesh.positions[i + 1]);
        b.maxZ = std::max(b.maxZ, mesh.positions[i + 2]);
    }
    return b;
}

static std::array<float, 4> AverageRegionColor(const ImageRGBA& image, const std::vector<std::uint8_t>& mask, float y0, float y1, std::array<float, 4> fallback) {
    if (image.width <= 0 || image.height <= 0 || image.pixels.size() < static_cast<size_t>(image.width * image.height * 4)) return fallback;
    double r = 0, g = 0, b = 0, a = 0;
    int count = 0;
    int startY = std::max(0, static_cast<int>(std::floor(y0 * image.height)));
    int endY = std::min(image.height, static_cast<int>(std::ceil(y1 * image.height)));
    for (int y = startY; y < endY; ++y) {
        for (int x = 0; x < image.width; ++x) {
            int id = y * image.width + x;
            if (!mask.empty() && mask.size() == static_cast<size_t>(image.width * image.height) && !mask[static_cast<size_t>(id)]) continue;
            size_t p = static_cast<size_t>(id) * 4;
            if (image.pixels[p + 3] < 16) continue;
            r += image.pixels[p + 0] / 255.0;
            g += image.pixels[p + 1] / 255.0;
            b += image.pixels[p + 2] / 255.0;
            a += image.pixels[p + 3] / 255.0;
            ++count;
        }
    }
    if (count <= 0) return fallback;
    return {static_cast<float>(r / count), static_cast<float>(g / count), static_cast<float>(b / count), static_cast<float>(std::max(0.6, a / count))};
}

static std::array<float, 4> Mix(const std::array<float, 4>& a, const std::array<float, 4>& b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return {a[0] * (1.0f - t) + b[0] * t, a[1] * (1.0f - t) + b[1] * t, a[2] * (1.0f - t) + b[2] * t, a[3] * (1.0f - t) + b[3] * t};
}

static std::array<float, 4> SemanticColor(float x, float y, float z, const Bounds& b, const HeroSemanticPalette& p) {
    float h = std::max(0.0001f, b.maxY - b.minY);
    float yf = (y - b.minY) / h;
    float xf = (x - b.minX) / std::max(0.0001f, b.maxX - b.minX);
    float zf = (z - b.minZ) / std::max(0.0001f, b.maxZ - b.minZ);

    if (yf > 0.83f && zf < 0.60f) return p.hair;
    if (yf > 0.76f && std::abs(xf - 0.5f) < 0.20f && zf >= 0.48f) return Mix(p.face, p.skin, 0.25f);
    if (yf > 0.70f) return p.skin;
    if (yf > 0.42f) return p.clothing;
    if (yf > 0.19f) return p.lowerClothing;
    if (yf < 0.12f && zf > 0.48f) return p.shoes;
    if ((xf < 0.18f || xf > 0.82f) && yf > 0.25f && yf < 0.66f) return p.skin;
    return p.lowerClothing;
}

static std::vector<float> BuildSafeNormals(const MeshData& mesh) {
    int vc = VertexCount(mesh);
    std::vector<float> normals(static_cast<size_t>(vc) * 3, 0.0f);
    if (mesh.normals.size() >= normals.size()) {
        std::copy(mesh.normals.begin(), mesh.normals.begin() + static_cast<std::ptrdiff_t>(normals.size()), normals.begin());
    } else {
        for (int i = 0; i < vc; ++i) normals[static_cast<size_t>(i) * 3 + 2] = 1.0f;
    }
    return normals;
}

static std::vector<float> BuildSafeUvs(const MeshData& mesh, const Bounds& b) {
    int vc = VertexCount(mesh);
    std::vector<float> uvs(static_cast<size_t>(vc) * 2, 0.0f);
    if (mesh.uvs.size() >= uvs.size()) {
        std::copy(mesh.uvs.begin(), mesh.uvs.begin() + static_cast<std::ptrdiff_t>(uvs.size()), uvs.begin());
    } else {
        float dx = std::max(0.0001f, b.maxX - b.minX);
        float dy = std::max(0.0001f, b.maxY - b.minY);
        for (int i = 0; i < vc; ++i) {
            float x = mesh.positions[static_cast<size_t>(i) * 3 + 0];
            float y = mesh.positions[static_cast<size_t>(i) * 3 + 1];
            uvs[static_cast<size_t>(i) * 2 + 0] = (x - b.minX) / dx;
            uvs[static_cast<size_t>(i) * 2 + 1] = 1.0f - (y - b.minY) / dy;
        }
    }
    return uvs;
}

static bool WriteBin(const fs::path& path, const std::vector<float>& positions, const std::vector<float>& normals, const std::vector<float>& uvs, const std::vector<float>& colors, const std::vector<std::uint32_t>& indices, std::string* error) {
    std::ofstream f(path, std::ios::binary);
    if (!f) { if (error) *error = "Failed to write hero semantic glTF binary."; return false; }
    auto writeFloats = [&](const std::vector<float>& v) { f.write(reinterpret_cast<const char*>(v.data()), static_cast<std::streamsize>(v.size() * sizeof(float))); };
    writeFloats(positions);
    writeFloats(normals);
    writeFloats(uvs);
    writeFloats(colors);
    if (!indices.empty()) f.write(reinterpret_cast<const char*>(indices.data()), static_cast<std::streamsize>(indices.size() * sizeof(std::uint32_t)));
    return true;
}

} // namespace

HeroSemanticPalette ExtractHeroSemanticPalette(const ImageRGBA& image, const std::vector<std::uint8_t>& mask) {
    HeroSemanticPalette p;
    p.hair = AverageRegionColor(image, mask, 0.00f, 0.22f, p.hair);
    p.skin = AverageRegionColor(image, mask, 0.08f, 0.34f, p.skin);
    p.face = Mix(p.skin, {1.0f, 0.82f, 0.70f, 1.0f}, 0.28f);
    p.clothing = AverageRegionColor(image, mask, 0.32f, 0.66f, p.clothing);
    p.lowerClothing = AverageRegionColor(image, mask, 0.62f, 0.88f, p.lowerClothing);
    p.shoes = AverageRegionColor(image, mask, 0.84f, 1.00f, p.shoes);
    return p;
}

bool ExportHeroSemanticGLTF(const MeshData& mesh, const HeroSemanticPalette& palette, const fs::path& gltfPath, const HeroSemanticGltfOptions& options, std::string* error) {
    int vc = VertexCount(mesh);
    int ic = IndexCount(mesh);
    if (vc <= 0 || ic <= 0 || mesh.positions.size() < static_cast<size_t>(vc) * 3) {
        if (error) *error = "Hero semantic glTF export requires positions and indices.";
        return false;
    }

    std::error_code ec;
    fs::create_directories(gltfPath.parent_path(), ec);
    fs::path binPath = gltfPath;
    binPath.replace_extension(".bin");

    Bounds b = ComputeBounds(mesh);
    std::vector<float> normals = BuildSafeNormals(mesh);
    std::vector<float> uvs = BuildSafeUvs(mesh, b);
    std::vector<float> colors(static_cast<size_t>(vc) * 4, 1.0f);
    for (int i = 0; i < vc; ++i) {
        float x = mesh.positions[static_cast<size_t>(i) * 3 + 0];
        float y = mesh.positions[static_cast<size_t>(i) * 3 + 1];
        float z = mesh.positions[static_cast<size_t>(i) * 3 + 2];
        auto c = SemanticColor(x, y, z, b, palette);
        colors[static_cast<size_t>(i) * 4 + 0] = c[0];
        colors[static_cast<size_t>(i) * 4 + 1] = c[1];
        colors[static_cast<size_t>(i) * 4 + 2] = c[2];
        colors[static_cast<size_t>(i) * 4 + 3] = c[3];
    }

    if (!WriteBin(binPath, mesh.positions, normals, uvs, colors, mesh.indices, error)) return false;

    const size_t posBytes = mesh.positions.size() * sizeof(float);
    const size_t normalBytes = normals.size() * sizeof(float);
    const size_t uvBytes = uvs.size() * sizeof(float);
    const size_t colorBytes = colors.size() * sizeof(float);
    const size_t indexBytes = mesh.indices.size() * sizeof(std::uint32_t);
    const size_t normalOffset = posBytes;
    const size_t uvOffset = normalOffset + normalBytes;
    const size_t colorOffset = uvOffset + uvBytes;
    const size_t indexOffset = colorOffset + colorBytes;
    const size_t totalBytes = indexOffset + indexBytes;

    std::ofstream g(gltfPath, std::ios::binary);
    if (!g) { if (error) *error = "Failed to write hero semantic glTF."; return false; }
    g << std::fixed << std::setprecision(6);
    g << "{\n";
    g << "  \"asset\": {\"version\": \"2.0\", \"generator\": \"Make3D Hero Semantic Exporter\"},\n";
    g << "  \"scene\": 0,\n";
    g << "  \"scenes\": [{\"nodes\": [0]}],\n";
    g << "  \"nodes\": [{\"mesh\": 0, \"name\": \"Make3D_Hero_Character\"}],\n";
    g << "  \"meshes\": [{\"primitives\": [{\"attributes\": {\"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": 2, \"COLOR_0\": 3}, \"indices\": 4, \"material\": 0}]}],\n";
    g << "  \"materials\": [{\"name\": \"" << EscapeJson(options.materialName) << "\", \"doubleSided\": " << (options.doubleSided ? "true" : "false") << ", \"pbrMetallicRoughness\": {\"baseColorFactor\": [1,1,1,1], \"metallicFactor\": " << options.metallicFactor << ", \"roughnessFactor\": " << options.roughnessFactor << "}}],\n";
    g << "  \"buffers\": [{\"uri\": \"" << EscapeJson(binPath.filename().u8string()) << "\", \"byteLength\": " << totalBytes << "}],\n";
    g << "  \"bufferViews\": [\n";
    g << "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": " << posBytes << ", \"target\": 34962},\n";
    g << "    {\"buffer\": 0, \"byteOffset\": " << normalOffset << ", \"byteLength\": " << normalBytes << ", \"target\": 34962},\n";
    g << "    {\"buffer\": 0, \"byteOffset\": " << uvOffset << ", \"byteLength\": " << uvBytes << ", \"target\": 34962},\n";
    g << "    {\"buffer\": 0, \"byteOffset\": " << colorOffset << ", \"byteLength\": " << colorBytes << ", \"target\": 34962},\n";
    g << "    {\"buffer\": 0, \"byteOffset\": " << indexOffset << ", \"byteLength\": " << indexBytes << ", \"target\": 34963}\n";
    g << "  ],\n";
    g << "  \"accessors\": [\n";
    g << "    {\"bufferView\": 0, \"componentType\": 5126, \"count\": " << vc << ", \"type\": \"VEC3\", \"min\": [" << b.minX << "," << b.minY << "," << b.minZ << "], \"max\": [" << b.maxX << "," << b.maxY << "," << b.maxZ << "]},\n";
    g << "    {\"bufferView\": 1, \"componentType\": 5126, \"count\": " << vc << ", \"type\": \"VEC3\"},\n";
    g << "    {\"bufferView\": 2, \"componentType\": 5126, \"count\": " << vc << ", \"type\": \"VEC2\"},\n";
    g << "    {\"bufferView\": 3, \"componentType\": 5126, \"count\": " << vc << ", \"type\": \"VEC4\"},\n";
    g << "    {\"bufferView\": 4, \"componentType\": 5125, \"count\": " << ic << ", \"type\": \"SCALAR\"}\n";
    g << "  ]\n";
    g << "}\n";
    return true;
}

} // namespace make3d
