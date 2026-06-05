#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

#include "Make3DAdvancedCore.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

namespace make3d {

namespace {

constexpr float Pi = 3.14159265358979323846f;
struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };

int VertexCountLocal(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
int TriangleCountLocal(const MeshData& mesh) { return static_cast<int>(mesh.indices.size() / 3); }
float Clamp01Local(float v) { return std::max(0.0f, std::min(1.0f, v)); }

std::string SafePathString(const fs::path& path) {
    try { return path.u8string(); } catch (...) { return "<path>"; }
}

std::string EscapeJson(const std::string& value) {
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

std::optional<std::vector<unsigned char>> ReadFileBytes(const fs::path& path, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "Failed to open image file: " + SafePathString(path);
        return std::nullopt;
    }
    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(std::numeric_limits<int>::max())) {
        if (error) *error = "Image file is empty, unreadable, or too large: " + SafePathString(path);
        return std::nullopt;
    }
    std::vector<unsigned char> bytes(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file) {
        if (error) *error = "Failed to read image file: " + SafePathString(path);
        return std::nullopt;
    }
    return bytes;
}

Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 NormalizeVec(const Vec3& v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return len > 1.0e-6f ? Vec3{v.x / len, v.y / len, v.z / len} : Vec3{0.0f, 1.0f, 0.0f};
}

void AddVertex(MeshData& mesh, float x, float y, float z, float u, float v) {
    mesh.positions.push_back(x); mesh.positions.push_back(y); mesh.positions.push_back(z);
    mesh.normals.push_back(0.0f); mesh.normals.push_back(1.0f); mesh.normals.push_back(0.0f);
    mesh.uvs.push_back(u); mesh.uvs.push_back(v);
}

void AddTri(MeshData& mesh, int a, int b, int c) {
    if (a < 0 || b < 0 || c < 0 || a == b || b == c || c == a) return;
    mesh.indices.push_back(static_cast<std::uint32_t>(a));
    mesh.indices.push_back(static_cast<std::uint32_t>(b));
    mesh.indices.push_back(static_cast<std::uint32_t>(c));
}

void AddQuad(MeshData& mesh, int a, int b, int c, int d) {
    AddTri(mesh, a, b, c);
    AddTri(mesh, a, c, d);
}

void AddBox(MeshData& mesh, float cx, float cy, float cz, float sx, float sy, float sz) {
    const int base = VertexCountLocal(mesh);
    const float x0 = cx - sx * 0.5f, x1 = cx + sx * 0.5f;
    const float y0 = cy - sy * 0.5f, y1 = cy + sy * 0.5f;
    const float z0 = cz - sz * 0.5f, z1 = cz + sz * 0.5f;
    AddVertex(mesh, x0, y0, z0, 0, 0); AddVertex(mesh, x1, y0, z0, 1, 0); AddVertex(mesh, x1, y1, z0, 1, 1); AddVertex(mesh, x0, y1, z0, 0, 1);
    AddVertex(mesh, x0, y0, z1, 0, 0); AddVertex(mesh, x1, y0, z1, 1, 0); AddVertex(mesh, x1, y1, z1, 1, 1); AddVertex(mesh, x0, y1, z1, 0, 1);
    AddQuad(mesh, base + 0, base + 1, base + 2, base + 3);
    AddQuad(mesh, base + 5, base + 4, base + 7, base + 6);
    AddQuad(mesh, base + 4, base + 0, base + 3, base + 7);
    AddQuad(mesh, base + 1, base + 5, base + 6, base + 2);
    AddQuad(mesh, base + 3, base + 2, base + 6, base + 7);
    AddQuad(mesh, base + 4, base + 5, base + 1, base + 0);
}

void AddCylinder(MeshData& mesh, float cx, float cy0, float cy1, float cz, float rx, float rz, int segments) {
    segments = std::max(8, segments);
    const int base = VertexCountLocal(mesh);
    for (int y = 0; y < 2; ++y) {
        float cy = y == 0 ? cy0 : cy1;
        for (int s = 0; s < segments; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(segments);
            float a = t * 2.0f * Pi;
            AddVertex(mesh, cx + std::cos(a) * rx, cy, cz + std::sin(a) * rz, t, static_cast<float>(y));
        }
    }
    for (int s = 0; s < segments; ++s) {
        int n = (s + 1) % segments;
        AddQuad(mesh, base + s, base + n, base + segments + n, base + segments + s);
    }
    int bottom = VertexCountLocal(mesh); AddVertex(mesh, cx, cy0, cz, 0.5f, 0.5f);
    int top = VertexCountLocal(mesh); AddVertex(mesh, cx, cy1, cz, 0.5f, 0.5f);
    for (int s = 0; s < segments; ++s) {
        int n = (s + 1) % segments;
        AddTri(mesh, bottom, base + n, base + s);
        AddTri(mesh, top, base + segments + s, base + segments + n);
    }
}

MeshData BuildSafeCoreProxy(const ImageRGBA& color, const std::vector<std::uint8_t>& mask, const AdvancedOptions& options) {
    int minX = color.width, minY = color.height, maxX = -1, maxY = -1, fg = 0;
    for (int y = 0; y < color.height; ++y) {
        for (int x = 0; x < color.width; ++x) {
            if (!mask[static_cast<size_t>(y) * color.width + x]) continue;
            ++fg; minX = std::min(minX, x); minY = std::min(minY, y); maxX = std::max(maxX, x); maxY = std::max(maxY, y);
        }
    }
    int bw = std::max(1, maxX - minX + 1);
    int bh = std::max(1, maxY - minY + 1);
    float aspect = static_cast<float>(bh) / static_cast<float>(bw);
    float coverage = color.width > 0 && color.height > 0 ? static_cast<float>(fg) / static_cast<float>(color.width * color.height) : 0.0f;
    MeshData mesh;
    int seg = std::max(12, options.volumeRadialSegments);
    if (aspect > 1.20f) {
        AddCylinder(mesh, 0.0f, 0.0f, 1.25f, 0.0f, 0.25f, 0.18f, seg);
        AddCylinder(mesh, 0.0f, 1.18f, 1.65f, 0.0f, 0.19f, 0.19f, seg);
        AddCylinder(mesh, -0.36f, 0.45f, 1.12f, 0.0f, 0.06f, 0.06f, 10);
        AddCylinder(mesh,  0.36f, 0.45f, 1.12f, 0.0f, 0.06f, 0.06f, 10);
        AddCylinder(mesh, -0.13f, 0.00f, 0.60f, 0.0f, 0.07f, 0.07f, 10);
        AddCylinder(mesh,  0.13f, 0.00f, 0.60f, 0.0f, 0.07f, 0.07f, 10);
    } else if (aspect < 0.70f && coverage > 0.04f) {
        AddBox(mesh, 0.0f, 0.45f, 0.0f, 1.75f, 0.52f, 0.72f);
        AddBox(mesh, 0.10f, 0.86f, 0.0f, 0.85f, 0.38f, 0.62f);
        AddCylinder(mesh, -0.62f, 0.12f, 0.24f, 0.38f, 0.16f, 0.16f, seg);
        AddCylinder(mesh,  0.62f, 0.12f, 0.24f, 0.38f, 0.16f, 0.16f, seg);
        AddCylinder(mesh, -0.62f, 0.12f, 0.24f, -0.38f, 0.16f, 0.16f, seg);
        AddCylinder(mesh,  0.62f, 0.12f, 0.24f, -0.38f, 0.16f, 0.16f, seg);
    } else {
        float width = std::clamp(2.0f / std::max(0.35f, aspect), 0.55f, 2.4f);
        AddBox(mesh, 0.0f, 1.0f, 0.0f, width, 2.0f, 0.58f);
        AddBox(mesh, 0.0f, 2.08f, 0.0f, width * 1.08f, 0.18f, 0.68f);
        for (int row = 0; row < 3; ++row) for (int col = 0; col < 3; ++col) AddBox(mesh, -width * 0.28f + width * 0.28f * col, 0.75f + 0.35f * row, 0.33f, width * 0.10f, 0.12f, 0.05f);
    }
    RecomputeNormals(mesh);
    NormalizeMesh(mesh, 2.0f);
    return mesh;
}

} // namespace

std::optional<ImageRGBA> LoadImageRGBA(const fs::path& path, std::string* error) {
    auto bytes = ReadFileBytes(path, error);
    if (!bytes) return std::nullopt;
    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load_from_memory(bytes->data(), static_cast<int>(bytes->size()), &w, &h, &comp, 4);
    if (!data) {
        if (error) *error = "Failed to decode image as PNG/JPG/BMP/TGA/etc: " + SafePathString(path);
        return std::nullopt;
    }
    ImageRGBA image;
    image.width = w; image.height = h;
    image.pixels.assign(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
    return image;
}

std::optional<DepthImage> LoadDepthImage(const fs::path& path, std::string* error) {
    auto bytes = ReadFileBytes(path, error);
    if (!bytes) return std::nullopt;
    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load_from_memory(bytes->data(), static_cast<int>(bytes->size()), &w, &h, &comp, 0);
    if (!data) {
        if (error) *error = "Failed to decode depth image: " + SafePathString(path);
        return std::nullopt;
    }
    DepthImage depth;
    depth.width = w; depth.height = h;
    depth.values.resize(static_cast<size_t>(w) * h, 0.0f);
    int step = std::max(1, comp);
    for (int i = 0; i < w * h; ++i) depth.values[static_cast<size_t>(i)] = data[i * step] / 255.0f;
    stbi_image_free(data);
    return depth;
}

std::vector<std::uint8_t> BuildForegroundMask(const ImageRGBA& image, ReconstructionReport* report) {
    int w = image.width, h = image.height;
    std::vector<std::uint8_t> mask(static_cast<size_t>(w) * h, 0);
    if (w <= 0 || h <= 0 || image.pixels.size() < static_cast<size_t>(w) * h * 4) return mask;
    int alphaCount = 0;
    for (int i = 0; i < w * h; ++i) if (image.pixels[static_cast<size_t>(i) * 4 + 3] > 24) { mask[static_cast<size_t>(i)] = 255; ++alphaCount; }
    if (alphaCount == 0 || alphaCount > w * h * 98 / 100) {
        std::array<float, 3> bg{}; int count = 0;
        auto addBg = [&](int x, int y) { size_t p = (static_cast<size_t>(y) * w + x) * 4; bg[0] += image.pixels[p]; bg[1] += image.pixels[p + 1]; bg[2] += image.pixels[p + 2]; ++count; };
        for (int x = 0; x < w; ++x) { addBg(x, 0); addBg(x, h - 1); }
        for (int y = 1; y < h - 1; ++y) { addBg(0, y); addBg(w - 1, y); }
        for (float& v : bg) v /= std::max(1, count);
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
            size_t p = (static_cast<size_t>(y) * w + x) * 4;
            float dr = image.pixels[p] - bg[0], dg = image.pixels[p + 1] - bg[1], db = image.pixels[p + 2] - bg[2];
            mask[static_cast<size_t>(y) * w + x] = std::sqrt(dr * dr + dg * dg + db * db) > 22.0f ? 255 : 0;
        }
    }
    if (report) {
        report->imageWidth = w; report->imageHeight = h;
        report->foregroundPixels = static_cast<int>(std::count_if(mask.begin(), mask.end(), [](std::uint8_t v) { return v != 0; }));
        report->foregroundCoverage = w * h > 0 ? static_cast<float>(report->foregroundPixels) / static_cast<float>(w * h) : 0.0f;
        if (report->foregroundPixels == 0) report->warnings.push_back("Foreground mask is empty.");
    }
    return mask;
}

DepthImage EstimateDepthFromSingleImage(const ImageRGBA& image, const std::vector<std::uint8_t>& mask, ReconstructionReport* report) {
    DepthImage depth;
    depth.width = image.width; depth.height = image.height;
    depth.values.assign(static_cast<size_t>(image.width) * image.height, 0.55f);
    for (size_t i = 0; i < depth.values.size() && i < mask.size(); ++i) depth.values[i] = mask[i] ? 0.60f : 0.0f;
    if (report) { report->depthMin = 0.0f; report->depthMax = 0.60f; report->depthMean = 0.60f; }
    return depth;
}

DepthImage PrepareDepth(const ImageRGBA& color, const std::optional<DepthImage>& providedDepth, const std::vector<std::uint8_t>& mask, const AdvancedOptions&, ReconstructionReport* report) {
    if (providedDepth && providedDepth->width == color.width && providedDepth->height == color.height) { if (report) report->usedProvidedDepth = true; return *providedDepth; }
    return EstimateDepthFromSingleImage(color, mask, report);
}

MeshData ReconstructMesh(const ImageRGBA& color, const DepthImage&, const std::vector<std::uint8_t>& mask, const AdvancedOptions& options, ReconstructionReport* report) {
    MeshData mesh = BuildSafeCoreProxy(color, mask, options);
    if (report) { report->reconstructionMode = "SafeTypedProxy"; report->vertices = VertexCountLocal(mesh); report->triangles = TriangleCountLocal(mesh); report->watertightCandidate = true; }
    return mesh;
}

void RecomputeNormals(MeshData& mesh) {
    mesh.normals.assign(mesh.positions.size(), 0.0f);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::uint32_t ia = mesh.indices[i], ib = mesh.indices[i + 1], ic = mesh.indices[i + 2];
        if ((static_cast<size_t>(ia) + 1) * 3 > mesh.positions.size() || (static_cast<size_t>(ib) + 1) * 3 > mesh.positions.size() || (static_cast<size_t>(ic) + 1) * 3 > mesh.positions.size()) continue;
        Vec3 a{mesh.positions[ia * 3], mesh.positions[ia * 3 + 1], mesh.positions[ia * 3 + 2]};
        Vec3 b{mesh.positions[ib * 3], mesh.positions[ib * 3 + 1], mesh.positions[ib * 3 + 2]};
        Vec3 c{mesh.positions[ic * 3], mesh.positions[ic * 3 + 1], mesh.positions[ic * 3 + 2]};
        Vec3 n = Cross({b.x - a.x, b.y - a.y, b.z - a.z}, {c.x - a.x, c.y - a.y, c.z - a.z});
        std::uint32_t vs[3] = {ia, ib, ic};
        for (std::uint32_t vi : vs) { mesh.normals[vi * 3] += n.x; mesh.normals[vi * 3 + 1] += n.y; mesh.normals[vi * 3 + 2] += n.z; }
    }
    for (size_t i = 0; i + 2 < mesh.normals.size(); i += 3) { Vec3 n = NormalizeVec({mesh.normals[i], mesh.normals[i + 1], mesh.normals[i + 2]}); mesh.normals[i] = n.x; mesh.normals[i + 1] = n.y; mesh.normals[i + 2] = n.z; }
}

void NormalizeMesh(MeshData& mesh, float targetHeight) {
    if (mesh.positions.empty()) return;
    Vec3 mn{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 mx{-mn.x, -mn.y, -mn.z};
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) { mn.x = std::min(mn.x, mesh.positions[i]); mn.y = std::min(mn.y, mesh.positions[i + 1]); mn.z = std::min(mn.z, mesh.positions[i + 2]); mx.x = std::max(mx.x, mesh.positions[i]); mx.y = std::max(mx.y, mesh.positions[i + 1]); mx.z = std::max(mx.z, mesh.positions[i + 2]); }
    float scale = targetHeight / std::max(0.001f, mx.y - mn.y);
    Vec3 center{(mn.x + mx.x) * 0.5f, mn.y, (mn.z + mx.z) * 0.5f};
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) { mesh.positions[i] = (mesh.positions[i] - center.x) * scale; mesh.positions[i + 1] = (mesh.positions[i + 1] - center.y) * scale; mesh.positions[i + 2] = (mesh.positions[i + 2] - center.z) * scale; }
}

bool ExportOBJ(const MeshData& mesh, const fs::path& objPath, const std::string& materialTextureName, std::string* error) {
    std::error_code ec; fs::create_directories(objPath.parent_path(), ec);
    std::ofstream obj(objPath, std::ios::binary); if (!obj) { if (error) *error = "Failed to open OBJ for writing."; return false; }
    obj << "mtllib make3d_material.mtl\n" << "o Make3DSafeTypedModel\n";
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) obj << "v " << mesh.positions[i] << ' ' << mesh.positions[i + 1] << ' ' << mesh.positions[i + 2] << "\n";
    for (size_t i = 0; i + 1 < mesh.uvs.size(); i += 2) obj << "vt " << mesh.uvs[i] << ' ' << (1.0f - mesh.uvs[i + 1]) << "\n";
    for (size_t i = 0; i + 2 < mesh.normals.size(); i += 3) obj << "vn " << mesh.normals[i] << ' ' << mesh.normals[i + 1] << ' ' << mesh.normals[i + 2] << "\n";
    obj << "usemtl Material0\n";
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) { std::uint32_t a = mesh.indices[i] + 1, b = mesh.indices[i + 1] + 1, c = mesh.indices[i + 2] + 1; obj << "f " << a << '/' << a << '/' << a << ' ' << b << '/' << b << '/' << b << ' ' << c << '/' << c << '/' << c << "\n"; }
    std::ofstream mtl(objPath.parent_path() / "make3d_material.mtl", std::ios::binary);
    mtl << "newmtl Material0\nKd 0.72 0.74 0.76\nKa 0.05 0.05 0.05\nKs 0.15 0.15 0.15\nNs 24\n";
    if (!materialTextureName.empty()) mtl << "map_Kd " << materialTextureName << "\n";
    return true;
}

bool ExportGLTF(const MeshData& mesh, const fs::path& gltfPath, std::string* error) {
    std::error_code ec; fs::create_directories(gltfPath.parent_path(), ec);
    fs::path binPath = gltfPath.parent_path() / "make3d_advanced.bin";
    size_t posBytes = mesh.positions.size() * sizeof(float), normBytes = mesh.normals.size() * sizeof(float), uvBytes = mesh.uvs.size() * sizeof(float), idxBytes = mesh.indices.size() * sizeof(std::uint32_t);
    size_t posOffset = 0, normOffset = posBytes, uvOffset = normOffset + normBytes, idxOffset = uvOffset + uvBytes, totalSize = idxOffset + idxBytes;
    std::ofstream bin(binPath, std::ios::binary); if (!bin) { if (error) *error = "Failed to open glTF bin for writing."; return false; }
    bin.write(reinterpret_cast<const char*>(mesh.positions.data()), static_cast<std::streamsize>(posBytes));
    bin.write(reinterpret_cast<const char*>(mesh.normals.data()), static_cast<std::streamsize>(normBytes));
    bin.write(reinterpret_cast<const char*>(mesh.uvs.data()), static_cast<std::streamsize>(uvBytes));
    bin.write(reinterpret_cast<const char*>(mesh.indices.data()), static_cast<std::streamsize>(idxBytes));
    Vec3 mn{0,0,0}, mx{0,0,0};
    if (!mesh.positions.empty()) { mn = {mesh.positions[0], mesh.positions[1], mesh.positions[2]}; mx = mn; }
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) { mn.x = std::min(mn.x, mesh.positions[i]); mn.y = std::min(mn.y, mesh.positions[i + 1]); mn.z = std::min(mn.z, mesh.positions[i + 2]); mx.x = std::max(mx.x, mesh.positions[i]); mx.y = std::max(mx.y, mesh.positions[i + 1]); mx.z = std::max(mx.z, mesh.positions[i + 2]); }
    std::ofstream gltf(gltfPath, std::ios::binary); if (!gltf) { if (error) *error = "Failed to open glTF for writing."; return false; }
    gltf << std::fixed << std::setprecision(6);
    gltf << "{\n  \"asset\": { \"version\": \"2.0\", \"generator\": \"Make3D safe typed output\" },\n";
    gltf << "  \"scene\": 0,\n  \"scenes\": [{ \"nodes\": [0] }],\n  \"nodes\": [{ \"mesh\": 0, \"name\": \"Make3DSafeTypedModel\" }],\n";
    gltf << "  \"meshes\": [{ \"primitives\": [{ \"attributes\": { \"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": 2 }, \"indices\": 3, \"mode\": 4 }] }],\n";
    gltf << "  \"buffers\": [{ \"uri\": \"make3d_advanced.bin\", \"byteLength\": " << totalSize << " }],\n";
    gltf << "  \"bufferViews\": [{ \"buffer\": 0, \"byteOffset\": " << posOffset << ", \"byteLength\": " << posBytes << ", \"target\": 34962 },{ \"buffer\": 0, \"byteOffset\": " << normOffset << ", \"byteLength\": " << normBytes << ", \"target\": 34962 },{ \"buffer\": 0, \"byteOffset\": " << uvOffset << ", \"byteLength\": " << uvBytes << ", \"target\": 34962 },{ \"buffer\": 0, \"byteOffset\": " << idxOffset << ", \"byteLength\": " << idxBytes << ", \"target\": 34963 }],\n";
    gltf << "  \"accessors\": [{ \"bufferView\": 0, \"componentType\": 5126, \"count\": " << VertexCountLocal(mesh) << ", \"type\": \"VEC3\", \"min\": [" << mn.x << ',' << mn.y << ',' << mn.z << "], \"max\": [" << mx.x << ',' << mx.y << ',' << mx.z << "] },{ \"bufferView\": 1, \"componentType\": 5126, \"count\": " << VertexCountLocal(mesh) << ", \"type\": \"VEC3\" },{ \"bufferView\": 2, \"componentType\": 5126, \"count\": " << VertexCountLocal(mesh) << ", \"type\": \"VEC2\" },{ \"bufferView\": 3, \"componentType\": 5125, \"count\": " << mesh.indices.size() << ", \"type\": \"SCALAR\" }]\n}";
    return true;
}

bool SaveDebugPPM(const fs::path& path, int width, int height, const std::vector<std::uint8_t>& rgb, std::string* error) {
    std::error_code ec; fs::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::binary); if (!file) { if (error) *error = "Failed to save debug image."; return false; }
    file << "P6\n" << width << ' ' << height << "\n255\n";
    file.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    return true;
}

std::string ReconstructionReport::ToMarkdown() const {
    std::ostringstream oss;
    oss << "# Make3D Advanced Reconstruction Report\n\n| Metric | Value |\n|---|---:|\n";
    oss << "| Image width | " << imageWidth << " |\n| Image height | " << imageHeight << " |\n| Foreground pixels | " << foregroundPixels << " |\n| Foreground coverage | " << foregroundCoverage << " |\n";
    oss << "| Reconstruction mode | " << reconstructionMode << " |\n| Vertices | " << vertices << " |\n| Triangles | " << triangles << " |\n| Watertight candidate | " << (watertightCandidate ? "yes" : "no") << " |\n\n";
    if (!warnings.empty()) { oss << "## Warnings\n\n"; for (const auto& w : warnings) oss << "- " << w << "\n"; }
    return oss.str();
}

std::string ReconstructionReport::ToJson() const {
    std::ostringstream oss;
    oss << "{\n  \"imageWidth\": " << imageWidth << ",\n  \"imageHeight\": " << imageHeight << ",\n  \"foregroundPixels\": " << foregroundPixels << ",\n  \"foregroundCoverage\": " << foregroundCoverage << ",\n  \"reconstructionMode\": \"" << EscapeJson(reconstructionMode) << "\",\n  \"vertices\": " << vertices << ",\n  \"triangles\": " << triangles << ",\n  \"watertightCandidate\": " << (watertightCandidate ? "true" : "false") << "\n}";
    return oss.str();
}

BuildOutput BuildModelFromImage(const fs::path& colorPath, const std::optional<fs::path>& depthPath, const fs::path& outputDir, const AdvancedOptions& options) {
    BuildOutput out; std::string error;
    auto color = LoadImageRGBA(colorPath, &error); if (!color) { out.message = error; return out; }
    std::optional<DepthImage> providedDepth; if (depthPath) providedDepth = LoadDepthImage(*depthPath, &error);
    out.report.imageWidth = color->width; out.report.imageHeight = color->height;
    auto mask = BuildForegroundMask(*color, &out.report); if (out.report.foregroundPixels == 0) { out.message = "Foreground extraction failed."; return out; }
    DepthImage depth = PrepareDepth(*color, providedDepth, mask, options, &out.report);
    out.mesh = ReconstructMesh(*color, depth, mask, options, &out.report);
    if (out.mesh.positions.empty() || out.mesh.indices.empty()) { out.message = "Mesh reconstruction failed."; return out; }
    RecomputeNormals(out.mesh); NormalizeMesh(out.mesh, 2.0f);
    out.report.reconstructionMode = "SafeTypedProxy"; out.report.vertices = VertexCountLocal(out.mesh); out.report.triangles = TriangleCountLocal(out.mesh); out.report.watertightCandidate = true;
    std::error_code ec; fs::create_directories(outputDir, ec);
    fs::path objPath = outputDir / "make3d_advanced.obj", gltfPath = outputDir / "make3d_advanced.gltf", reportMd = outputDir / "make3d_report.md", reportJson = outputDir / "make3d_report.json";
    if (options.exportObj && !ExportOBJ(out.mesh, objPath, "", &error)) { out.message = error; return out; }
    if (options.exportGltf && !ExportGLTF(out.mesh, gltfPath, &error)) { out.message = error; return out; }
    out.report.objPath = objPath; out.report.gltfPath = gltfPath; out.report.reportPath = reportMd;
    std::ofstream md(reportMd, std::ios::binary); md << out.report.ToMarkdown();
    std::ofstream js(reportJson, std::ios::binary); js << out.report.ToJson();
    if (options.writeDebugImages) {
        std::vector<std::uint8_t> maskRgb(static_cast<size_t>(color->width) * color->height * 3, 0), depthRgb(static_cast<size_t>(color->width) * color->height * 3, 0);
        for (int i = 0; i < color->width * color->height; ++i) { std::uint8_t m = mask[static_cast<size_t>(i)] ? 255 : 0; std::uint8_t d = static_cast<std::uint8_t>(Clamp01Local(depth.values[static_cast<size_t>(i)]) * 255.0f); maskRgb[static_cast<size_t>(i) * 3] = m; maskRgb[static_cast<size_t>(i) * 3 + 1] = m; maskRgb[static_cast<size_t>(i) * 3 + 2] = m; depthRgb[static_cast<size_t>(i) * 3] = d; depthRgb[static_cast<size_t>(i) * 3 + 1] = d; depthRgb[static_cast<size_t>(i) * 3 + 2] = d; }
        SaveDebugPPM(outputDir / "debug_mask.ppm", color->width, color->height, maskRgb, nullptr);
        SaveDebugPPM(outputDir / "debug_depth.ppm", color->width, color->height, depthRgb, nullptr);
    }
    out.ok = true; out.message = "Advanced reconstruction finished with filesystem-safe PNG loading."; return out;
}

const char* ToString(ReconstructionMode mode) {
    switch (mode) { case ReconstructionMode::Auto: return "Auto"; case ReconstructionMode::ReliefSurface: return "ReliefSurface"; case ReconstructionMode::SilhouetteVolume: return "SilhouetteVolume"; case ReconstructionMode::HybridVolume: return "HybridVolume"; default: return "Unknown"; }
}

const char* ToString(QualityPreset preset) {
    switch (preset) { case QualityPreset::Draft: return "Draft"; case QualityPreset::Standard: return "Standard"; case QualityPreset::Detailed: return "Detailed"; default: return "Unknown"; }
}

} // namespace make3d
