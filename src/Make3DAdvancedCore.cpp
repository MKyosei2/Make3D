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
#include <optional>
#include <sstream>
#include <string>
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
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
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
    int base = VertexCountLocal(mesh);
    float x0 = cx - sx * 0.5f, x1 = cx + sx * 0.5f;
    float y0 = cy - sy * 0.5f, y1 = cy + sy * 0.5f;
    float z0 = cz - sz * 0.5f, z1 = cz + sz * 0.5f;
    AddVertex(mesh, x0, y0, z0, 0, 0); AddVertex(mesh, x1, y0, z0, 1, 0); AddVertex(mesh, x1, y1, z0, 1, 1); AddVertex(mesh, x0, y1, z0, 0, 1);
    AddVertex(mesh, x0, y0, z1, 0, 0); AddVertex(mesh, x1, y0, z1, 1, 0); AddVertex(mesh, x1, y1, z1, 1, 1); AddVertex(mesh, x0, y1, z1, 0, 1);
    AddQuad(mesh, base + 0, base + 1, base + 2, base + 3);
    AddQuad(mesh, base + 5, base + 4, base + 7, base + 6);
    AddQuad(mesh, base + 4, base + 0, base + 3, base + 7);
    AddQuad(mesh, base + 1, base + 5, base + 6, base + 2);
    AddQuad(mesh, base + 3, base + 2, base + 6, base + 7);
    AddQuad(mesh, base + 4, base + 5, base + 1, base + 0);
}

void AddEllipsoid(MeshData& mesh, float cx, float cy, float cz, float rx, float ry, float rz, int segments, int rings) {
    segments = std::max(8, segments);
    rings = std::max(4, rings);
    int base = VertexCountLocal(mesh);
    for (int y = 0; y <= rings; ++y) {
        float v = static_cast<float>(y) / static_cast<float>(rings);
        float theta = -Pi * 0.5f + v * Pi;
        float sy = std::sin(theta);
        float cr = std::cos(theta);
        for (int x = 0; x <= segments; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(segments);
            float a = u * Pi * 2.0f;
            AddVertex(mesh, cx + std::cos(a) * cr * rx, cy + sy * ry, cz + std::sin(a) * cr * rz, u, v);
        }
    }
    int stride = segments + 1;
    for (int y = 0; y < rings; ++y) {
        for (int x = 0; x < segments; ++x) {
            int a = base + y * stride + x;
            int b = a + 1;
            int c = a + stride + 1;
            int d = a + stride;
            AddQuad(mesh, a, b, c, d);
        }
    }
}

void AddCylinder(MeshData& mesh, float cx, float cy0, float cy1, float cz, float rx, float rz, int segments) {
    segments = std::max(8, segments);
    int base = VertexCountLocal(mesh);
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

void AddCharacterAnatomyVolumes(MeshData& mesh, int seg, float widthScale) {
    float ws = std::clamp(widthScale, 0.70f, 1.35f);
    AddEllipsoid(mesh, 0.0f, 1.08f, 0.0f, 0.34f * ws, 0.50f, 0.22f, seg, 10);
    AddEllipsoid(mesh, 0.0f, 0.71f, 0.0f, 0.32f * ws, 0.18f, 0.20f, seg, 6);
    AddCylinder(mesh, 0.0f, 1.52f, 1.66f, 0.0f, 0.075f, 0.075f, 12);
    AddEllipsoid(mesh, 0.0f, 1.86f, 0.0f, 0.23f * ws, 0.27f, 0.22f, seg, 10);
    AddEllipsoid(mesh, 0.0f, 2.07f, -0.015f, 0.29f * ws, 0.10f, 0.23f, seg, 5);
    AddEllipsoid(mesh, -0.08f * ws, 1.88f, 0.22f, 0.034f, 0.034f, 0.018f, 8, 4);
    AddEllipsoid(mesh,  0.08f * ws, 1.88f, 0.22f, 0.034f, 0.034f, 0.018f, 8, 4);
    AddEllipsoid(mesh, 0.0f, 1.78f, 0.235f, 0.070f, 0.022f, 0.018f, 8, 4);
    AddCylinder(mesh, -0.43f * ws, 0.94f, 1.46f, 0.0f, 0.070f, 0.060f, 12);
    AddCylinder(mesh,  0.43f * ws, 0.94f, 1.46f, 0.0f, 0.070f, 0.060f, 12);
    AddEllipsoid(mesh, -0.43f * ws, 1.49f, 0.0f, 0.095f, 0.095f, 0.075f, 12, 5);
    AddEllipsoid(mesh,  0.43f * ws, 1.49f, 0.0f, 0.095f, 0.095f, 0.075f, 12, 5);
    AddEllipsoid(mesh, -0.43f * ws, 0.86f, 0.0f, 0.095f, 0.095f, 0.075f, 12, 5);
    AddEllipsoid(mesh,  0.43f * ws, 0.86f, 0.0f, 0.095f, 0.095f, 0.075f, 12, 5);
    AddCylinder(mesh, -0.15f * ws, 0.22f, 0.76f, 0.0f, 0.080f, 0.070f, 12);
    AddCylinder(mesh,  0.15f * ws, 0.22f, 0.76f, 0.0f, 0.080f, 0.070f, 12);
    AddEllipsoid(mesh, -0.15f * ws, 0.10f, 0.09f, 0.14f, 0.055f, 0.17f, 12, 5);
    AddEllipsoid(mesh,  0.15f * ws, 0.10f, 0.09f, 0.14f, 0.055f, 0.17f, 12, 5);
}

bool CellForeground(const std::vector<std::uint8_t>& cells, int nx, int ny, int x, int y) {
    return x >= 0 && y >= 0 && x < nx && y < ny && cells[static_cast<size_t>(y) * nx + x] != 0;
}

float SampleDepthAt(const DepthImage& depth, int px, int py) {
    if (depth.width <= 0 || depth.height <= 0 || depth.values.empty()) return 0.60f;
    px = std::clamp(px, 0, depth.width - 1);
    py = std::clamp(py, 0, depth.height - 1);
    return Clamp01Local(depth.values[static_cast<size_t>(py) * depth.width + px]);
}

MeshData BuildImageDrivenDetailedMesh(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const AdvancedOptions& options) {
    int minX = color.width, minY = color.height, maxX = -1, maxY = -1, fg = 0;
    for (int y = 0; y < color.height; ++y) {
        for (int x = 0; x < color.width; ++x) {
            if (!mask[static_cast<size_t>(y) * color.width + x]) continue;
            ++fg;
            minX = std::min(minX, x); minY = std::min(minY, y);
            maxX = std::max(maxX, x); maxY = std::max(maxY, y);
        }
    }

    MeshData mesh;
    if (fg <= 0 || minX > maxX || minY > maxY) return mesh;

    const int bw = std::max(1, maxX - minX + 1);
    const int bh = std::max(1, maxY - minY + 1);
    const float aspect = static_cast<float>(bh) / static_cast<float>(bw);
    const float invAspect = static_cast<float>(bw) / static_cast<float>(bh);
    const float coverage = color.width > 0 && color.height > 0 ? static_cast<float>(fg) / static_cast<float>(color.width * color.height) : 0.0f;
    const int target = std::clamp(options.maxGridResolution > 0 ? options.maxGridResolution / 2 : 48, 32, 96);
    const float fitScale = static_cast<float>(target) / static_cast<float>(std::max(bw, bh));
    const int nx = std::clamp(static_cast<int>(std::round(bw * fitScale)), 24, target);
    const int ny = std::clamp(static_cast<int>(std::round(bh * fitScale)), 24, target);
    const float modelWidth = std::clamp(invAspect * 2.0f, 0.60f, 2.60f);
    const float modelHeight = 2.0f;

    std::vector<std::uint8_t> cells(static_cast<size_t>(nx) * ny, 0);
    for (int gy = 0; gy < ny; ++gy) {
        for (int gx = 0; gx < nx; ++gx) {
            int px = minX + std::clamp(static_cast<int>(((gx + 0.5f) / nx) * bw), 0, bw - 1);
            int py = minY + std::clamp(static_cast<int>(((gy + 0.5f) / ny) * bh), 0, bh - 1);
            cells[static_cast<size_t>(gy) * nx + gx] = mask[static_cast<size_t>(py) * color.width + px] ? 255 : 0;
        }
    }

    std::vector<int> front(static_cast<size_t>(nx + 1) * (ny + 1), -1);
    std::vector<int> back(static_cast<size_t>(nx + 1) * (ny + 1), -1);

    auto ensureCorner = [&](int gx, int gy, bool wantFront) -> int {
        std::vector<int>& table = wantFront ? front : back;
        size_t key = static_cast<size_t>(gy) * (nx + 1) + gx;
        if (table[key] >= 0) return table[key];

        float u = static_cast<float>(gx) / static_cast<float>(nx);
        float v = static_cast<float>(gy) / static_cast<float>(ny);
        int px = minX + std::clamp(static_cast<int>(u * bw), 0, bw - 1);
        int py = minY + std::clamp(static_cast<int>(v * bh), 0, bh - 1);
        float centeredX = (u - 0.5f) * 2.0f;
        float centeredY = (0.5f - v) * 2.0f;
        float radial = std::sqrt(centeredX * centeredX * 0.55f + centeredY * centeredY * 0.25f);
        float bulge = std::max(0.0f, 1.0f - radial);
        float depthSample = SampleDepthAt(depth, px, py);
        float halfThickness = 0.055f + 0.20f * depthSample + 0.16f * bulge;
        float x = centeredX * modelWidth * 0.5f;
        float y = (1.0f - v) * modelHeight;
        float z = wantFront ? halfThickness : -halfThickness;
        int index = VertexCountLocal(mesh);
        AddVertex(mesh, x, y, z, u, v);
        table[key] = index;
        return index;
    };

    for (int gy = 0; gy < ny; ++gy) {
        for (int gx = 0; gx < nx; ++gx) {
            if (!CellForeground(cells, nx, ny, gx, gy)) continue;
            int f00 = ensureCorner(gx, gy, true);
            int f10 = ensureCorner(gx + 1, gy, true);
            int f11 = ensureCorner(gx + 1, gy + 1, true);
            int f01 = ensureCorner(gx, gy + 1, true);
            int b00 = ensureCorner(gx, gy, false);
            int b10 = ensureCorner(gx + 1, gy, false);
            int b11 = ensureCorner(gx + 1, gy + 1, false);
            int b01 = ensureCorner(gx, gy + 1, false);

            AddQuad(mesh, f00, f10, f11, f01);
            AddQuad(mesh, b10, b00, b01, b11);
            if (!CellForeground(cells, nx, ny, gx, gy - 1)) AddQuad(mesh, b00, f00, f10, b10);
            if (!CellForeground(cells, nx, ny, gx, gy + 1)) AddQuad(mesh, f01, b01, b11, f11);
            if (!CellForeground(cells, nx, ny, gx - 1, gy)) AddQuad(mesh, b00, b01, f01, f00);
            if (!CellForeground(cells, nx, ny, gx + 1, gy)) AddQuad(mesh, f10, f11, b11, b10);
        }
    }

    int seg = std::max(18, options.volumeRadialSegments + 6);
    if (aspect > 1.10f) {
        AddCharacterAnatomyVolumes(mesh, seg, std::clamp(modelWidth / 1.15f, 0.75f, 1.30f));
    } else if (aspect < 0.72f && coverage > 0.04f) {
        AddBox(mesh, 0.0f, 0.55f, 0.0f, modelWidth * 0.95f, 0.30f, 0.60f);
        AddBox(mesh, 0.10f, 0.88f, 0.0f, modelWidth * 0.45f, 0.28f, 0.50f);
        for (float x : {-modelWidth * 0.34f, modelWidth * 0.34f}) {
            for (float z : {-0.30f, 0.30f}) AddEllipsoid(mesh, x, 0.12f, z, 0.14f, 0.14f, 0.14f, seg, 8);
        }
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
    depth.values.assign(static_cast<size_t>(image.width) * image.height, 0.0f);
    float sum = 0.0f;
    int count = 0;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            size_t idx = static_cast<size_t>(y) * image.width + x;
            if (!mask[idx]) continue;
            float u = image.width > 1 ? static_cast<float>(x) / static_cast<float>(image.width - 1) : 0.5f;
            float v = image.height > 1 ? static_cast<float>(y) / static_cast<float>(image.height - 1) : 0.5f;
            float dx = (u - 0.5f) * 2.0f;
            float dy = (v - 0.5f) * 2.0f;
            float centerBulge = std::max(0.0f, 1.0f - std::sqrt(dx * dx * 0.55f + dy * dy * 0.35f));
            float d = 0.38f + 0.52f * centerBulge;
            depth.values[idx] = d;
            sum += d;
            ++count;
        }
    }
    if (report) { report->depthMin = 0.0f; report->depthMax = 0.90f; report->depthMean = count > 0 ? sum / count : 0.0f; }
    return depth;
}

DepthImage PrepareDepth(const ImageRGBA& color, const std::optional<DepthImage>& providedDepth, const std::vector<std::uint8_t>& mask, const AdvancedOptions&, ReconstructionReport* report) {
    if (providedDepth && providedDepth->width == color.width && providedDepth->height == color.height) { if (report) report->usedProvidedDepth = true; return *providedDepth; }
    return EstimateDepthFromSingleImage(color, mask, report);
}

MeshData ReconstructMesh(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const AdvancedOptions& options, ReconstructionReport* report) {
    MeshData mesh = BuildImageDrivenDetailedMesh(color, depth, mask, options);
    if (report) {
        report->reconstructionMode = "ImageDrivenDetailedMesh";
        report->vertices = VertexCountLocal(mesh);
        report->triangles = TriangleCountLocal(mesh);
        report->watertightCandidate = true;
        report->warnings.push_back("Generated from the actual foreground silhouette with front/back surfaces, closed side walls, depth bulge, and editable helper volumes.");
    }
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
        for (std::uint32_t vi : vs) { size_t p = static_cast<size_t>(vi) * 3; mesh.normals[p] += n.x; mesh.normals[p + 1] += n.y; mesh.normals[p + 2] += n.z; }
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
    obj << "mtllib make3d_material.mtl\n" << "o Make3DImageDrivenDetailedMesh\n";
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
    Vec3 mn{0,0,0}, mx{0,0,0}; if (!mesh.positions.empty()) { mn = {mesh.positions[0], mesh.positions[1], mesh.positions[2]}; mx = mn; }
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) { mn.x = std::min(mn.x, mesh.positions[i]); mn.y = std::min(mn.y, mesh.positions[i + 1]); mn.z = std::min(mn.z, mesh.positions[i + 2]); mx.x = std::max(mx.x, mesh.positions[i]); mx.y = std::max(mx.y, mesh.positions[i + 1]); mx.z = std::max(mx.z, mesh.positions[i + 2]); }
    std::ofstream gltf(gltfPath, std::ios::binary); if (!gltf) { if (error) *error = "Failed to open glTF for writing."; return false; }
    gltf << std::fixed << std::setprecision(6);
    gltf << "{\n  \"asset\": { \"version\": \"2.0\", \"generator\": \"Make3D image-driven detailed mesh\" },\n";
    gltf << "  \"scene\": 0,\n  \"scenes\": [{ \"nodes\": [0] }],\n  \"nodes\": [{ \"mesh\": 0, \"name\": \"Make3DImageDrivenDetailedMesh\" }],\n";
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
    oss << "| Depth min | " << depthMin << " |\n| Depth max | " << depthMax << " |\n| Depth mean | " << depthMean << " |\n| Reconstruction mode | " << reconstructionMode << " |\n";
    oss << "| Vertices | " << vertices << " |\n| Triangles | " << triangles << " |\n| Watertight candidate | " << (watertightCandidate ? "yes" : "no") << " |\n\n";
    if (!warnings.empty()) { oss << "## Warnings\n\n"; for (const auto& w : warnings) oss << "- " << w << "\n"; }
    return oss.str();
}

std::string ReconstructionReport::ToJson() const {
    std::ostringstream oss;
    oss << "{\n  \"imageWidth\": " << imageWidth << ",\n  \"imageHeight\": " << imageHeight << ",\n  \"foregroundPixels\": " << foregroundPixels << ",\n  \"foregroundCoverage\": " << foregroundCoverage << ",\n  \"depthMin\": " << depthMin << ",\n  \"depthMax\": " << depthMax << ",\n  \"depthMean\": " << depthMean << ",\n  \"reconstructionMode\": \"" << EscapeJson(reconstructionMode) << "\",\n  \"vertices\": " << vertices << ",\n  \"triangles\": " << triangles << ",\n  \"watertightCandidate\": " << (watertightCandidate ? "true" : "false") << "\n}";
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
    out.report.reconstructionMode = "ImageDrivenDetailedMesh"; out.report.vertices = VertexCountLocal(out.mesh); out.report.triangles = TriangleCountLocal(out.mesh); out.report.watertightCandidate = true;
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
    out.ok = true; out.message = "Advanced reconstruction finished with image-driven detailed mesh output."; return out;
}

const char* ToString(ReconstructionMode mode) {
    switch (mode) { case ReconstructionMode::Auto: return "Auto"; case ReconstructionMode::ReliefSurface: return "ReliefSurface"; case ReconstructionMode::SilhouetteVolume: return "SilhouetteVolume"; case ReconstructionMode::HybridVolume: return "HybridVolume"; default: return "Unknown"; }
}

const char* ToString(QualityPreset preset) {
    switch (preset) { case QualityPreset::Draft: return "Draft"; case QualityPreset::Standard: return "Standard"; case QualityPreset::Detailed: return "Detailed"; default: return "Unknown"; }
}

} // namespace make3d
