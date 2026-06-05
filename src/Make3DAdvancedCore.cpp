#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

#include "Make3DAdvancedCore.h"
#include "Make3DGameAssetGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <unordered_map>

namespace make3d {

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct BBox {
    int minX = 0;
    int minY = 0;
    int maxX = -1;
    int maxY = -1;
    bool valid = false;
};

struct ShapeStats {
    BBox box;
    int area = 0;
    float coverage = 0.0f;
    float aspect = 1.0f;
    float rectangularity = 0.0f;
    float edgeDensity = 0.0f;
};

Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(const Vec3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }

float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float Length(const Vec3& v) { return std::sqrt(Dot(v, v)); }
Vec3 Normalize(const Vec3& v) {
    float len = Length(v);
    return len > 1.0e-6f ? Vec3{v.x / len, v.y / len, v.z / len} : Vec3{0, 1, 0};
}

float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

int VertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
int TriangleCount(const MeshData& mesh) { return static_cast<int>(mesh.indices.size() / 3); }

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

int AddVertex(MeshData& mesh, float x, float y, float z, float u, float v) {
    int index = VertexCount(mesh);
    mesh.positions.push_back(x);
    mesh.positions.push_back(y);
    mesh.positions.push_back(z);
    mesh.normals.push_back(0.0f);
    mesh.normals.push_back(0.0f);
    mesh.normals.push_back(0.0f);
    mesh.uvs.push_back(u);
    mesh.uvs.push_back(v);
    return index;
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

BBox ComputeBBox(const std::vector<std::uint8_t>& mask, int w, int h) {
    BBox b;
    b.minX = w;
    b.minY = h;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!mask[static_cast<size_t>(y) * w + x]) continue;
            b.valid = true;
            b.minX = std::min(b.minX, x);
            b.minY = std::min(b.minY, y);
            b.maxX = std::max(b.maxX, x);
            b.maxY = std::max(b.maxY, y);
        }
    }
    if (!b.valid) {
        b.minX = b.minY = 0;
        b.maxX = b.maxY = -1;
    }
    return b;
}

ShapeStats AnalyzeMask(const ImageRGBA& image, const std::vector<std::uint8_t>& mask) {
    ShapeStats s;
    const int w = image.width;
    const int h = image.height;
    s.box = ComputeBBox(mask, w, h);
    if (!s.box.valid) return s;

    for (auto v : mask) if (v) ++s.area;
    s.coverage = (w > 0 && h > 0) ? static_cast<float>(s.area) / static_cast<float>(w * h) : 0.0f;
    int bw = s.box.maxX - s.box.minX + 1;
    int bh = s.box.maxY - s.box.minY + 1;
    s.aspect = bw > 0 ? static_cast<float>(bh) / static_cast<float>(bw) : 1.0f;
    s.rectangularity = (bw > 0 && bh > 0) ? static_cast<float>(s.area) / static_cast<float>(bw * bh) : 0.0f;

    int edgeCount = 0;
    int fgCount = 0;
    for (int y = std::max(1, s.box.minY); y <= std::min(h - 2, s.box.maxY); ++y) {
        for (int x = std::max(1, s.box.minX); x <= std::min(w - 2, s.box.maxX); ++x) {
            size_t idx = static_cast<size_t>(y) * w + x;
            if (!mask[idx]) continue;
            ++fgCount;
            auto lumAt = [&](int xx, int yy) {
                size_t q = (static_cast<size_t>(yy) * w + xx) * 4;
                return 0.299f * image.pixels[q + 0] + 0.587f * image.pixels[q + 1] + 0.114f * image.pixels[q + 2];
            };
            float gx = lumAt(x + 1, y) - lumAt(x - 1, y);
            float gy = lumAt(x, y + 1) - lumAt(x, y - 1);
            if (std::sqrt(gx * gx + gy * gy) > 35.0f) ++edgeCount;
        }
    }
    s.edgeDensity = fgCount > 0 ? static_cast<float>(edgeCount) / static_cast<float>(fgCount) : 0.0f;
    return s;
}

std::vector<float> DistanceToBoundary(const std::vector<std::uint8_t>& mask, int w, int h) {
    constexpr float INF = 1.0e20f;
    std::vector<float> dist(static_cast<size_t>(w) * h, 0.0f);
    if (w <= 0 || h <= 0) return dist;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t idx = static_cast<size_t>(y) * w + x;
            if (!mask[idx]) {
                dist[idx] = 0.0f;
                continue;
            }
            bool boundary = x == 0 || y == 0 || x == w - 1 || y == h - 1;
            if (!boundary) boundary = !mask[idx - 1] || !mask[idx + 1] || !mask[idx - w] || !mask[idx + w];
            dist[idx] = boundary ? 0.0f : INF;
        }
    }

    auto relax = [&](int x, int y, int nx, int ny, float cost) {
        if (nx < 0 || nx >= w || ny < 0 || ny >= h) return;
        size_t idx = static_cast<size_t>(y) * w + x;
        size_t nidx = static_cast<size_t>(ny) * w + nx;
        if (!mask[idx] || !mask[nidx]) return;
        dist[idx] = std::min(dist[idx], dist[nidx] + cost);
    };

    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        relax(x, y, x - 1, y, 1.0f); relax(x, y, x, y - 1, 1.0f);
        relax(x, y, x - 1, y - 1, 1.41421356f); relax(x, y, x + 1, y - 1, 1.41421356f);
    }
    for (int y = h - 1; y >= 0; --y) for (int x = w - 1; x >= 0; --x) {
        relax(x, y, x + 1, y, 1.0f); relax(x, y, x, y + 1, 1.0f);
        relax(x, y, x + 1, y + 1, 1.41421356f); relax(x, y, x - 1, y + 1, 1.41421356f);
    }

    float maxDist = 1.0f;
    for (size_t i = 0; i < dist.size(); ++i) if (mask[i] && dist[i] < INF) maxDist = std::max(maxDist, dist[i]);
    for (size_t i = 0; i < dist.size(); ++i) dist[i] = mask[i] ? Clamp01(dist[i] / maxDist) : 0.0f;
    return dist;
}

void MorphologicalClose(std::vector<std::uint8_t>& mask, int w, int h, int passes) {
    auto dilate = [&]() {
        auto src = mask;
        for (int y = 1; y < h - 1; ++y) for (int x = 1; x < w - 1; ++x) {
            size_t idx = static_cast<size_t>(y) * w + x;
            if (src[idx]) continue;
            bool on = false;
            for (int oy = -1; oy <= 1 && !on; ++oy) for (int ox = -1; ox <= 1; ++ox) if (src[static_cast<size_t>(y + oy) * w + (x + ox)]) { on = true; break; }
            if (on) mask[idx] = 255;
        }
    };
    auto erode = [&]() {
        auto src = mask;
        for (int y = 1; y < h - 1; ++y) for (int x = 1; x < w - 1; ++x) {
            size_t idx = static_cast<size_t>(y) * w + x;
            if (!src[idx]) continue;
            bool keep = true;
            for (int oy = -1; oy <= 1 && keep; ++oy) for (int ox = -1; ox <= 1; ++ox) if (!src[static_cast<size_t>(y + oy) * w + (x + ox)]) { keep = false; break; }
            if (!keep) mask[idx] = 0;
        }
    };
    for (int i = 0; i < passes; ++i) dilate();
    for (int i = 0; i < passes; ++i) erode();
}

void SmoothDepth(DepthImage& depth, const std::vector<std::uint8_t>& mask, int passes) {
    int w = depth.width;
    int h = depth.height;
    for (int pass = 0; pass < passes; ++pass) {
        auto src = depth.values;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                size_t idx = static_cast<size_t>(y) * w + x;
                if (!mask[idx]) continue;
                float sum = 0.0f;
                float weight = 0.0f;
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        size_t n = static_cast<size_t>(y + oy) * w + (x + ox);
                        if (!mask[n]) continue;
                        float wgt = (ox == 0 && oy == 0) ? 4.0f : 1.0f;
                        sum += src[n] * wgt;
                        weight += wgt;
                    }
                }
                if (weight > 0.0f) depth.values[idx] = sum / weight;
            }
        }
    }
}

void ComputeDepthStats(const DepthImage& depth, const std::vector<std::uint8_t>& mask, ReconstructionReport* report) {
    if (!report || depth.values.empty()) return;
    float mn = std::numeric_limits<float>::max();
    float mx = -std::numeric_limits<float>::max();
    double sum = 0.0;
    int count = 0;
    for (size_t i = 0; i < depth.values.size(); ++i) {
        if (!mask.empty() && !mask[i]) continue;
        float v = depth.values[i];
        mn = std::min(mn, v);
        mx = std::max(mx, v);
        sum += v;
        ++count;
    }
    if (count == 0) return;
    report->depthMin = mn;
    report->depthMax = mx;
    report->depthMean = static_cast<float>(sum / count);
}

// Remaining helper functions are unchanged legacy reconstruction support.

} // namespace

