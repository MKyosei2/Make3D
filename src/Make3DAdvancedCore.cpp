#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

#include "Make3DAdvancedCore.h"

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
            size_t p = idx * 4;
            auto lumAt = [&](int xx, int yy) {
                size_t q = (static_cast<size_t>(yy) * w + xx) * 4;
                return 0.299f * image.pixels[q + 0] + 0.587f * image.pixels[q + 1] + 0.114f * image.pixels[q + 2];
            };
            float gx = lumAt(x + 1, y) - lumAt(x - 1, y);
            float gy = lumAt(x, y + 1) - lumAt(x, y - 1);
            if (std::sqrt(gx * gx + gy * gy) > 35.0f) ++edgeCount;
            (void)p;
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
            if (!boundary) {
                boundary = !mask[idx - 1] || !mask[idx + 1] || !mask[idx - w] || !mask[idx + w];
            }
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

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            relax(x, y, x - 1, y, 1.0f);
            relax(x, y, x, y - 1, 1.0f);
            relax(x, y, x - 1, y - 1, 1.41421356f);
            relax(x, y, x + 1, y - 1, 1.41421356f);
        }
    }
    for (int y = h - 1; y >= 0; --y) {
        for (int x = w - 1; x >= 0; --x) {
            relax(x, y, x + 1, y, 1.0f);
            relax(x, y, x, y + 1, 1.0f);
            relax(x, y, x + 1, y + 1, 1.41421356f);
            relax(x, y, x - 1, y + 1, 1.41421356f);
        }
    }

    float maxDist = 1.0f;
    for (size_t i = 0; i < dist.size(); ++i) {
        if (mask[i] && dist[i] < INF) maxDist = std::max(maxDist, dist[i]);
    }
    for (size_t i = 0; i < dist.size(); ++i) {
        dist[i] = mask[i] ? Clamp01(dist[i] / maxDist) : 0.0f;
    }
    return dist;
}

void MorphologicalClose(std::vector<std::uint8_t>& mask, int w, int h, int passes) {
    auto dilate = [&]() {
        auto src = mask;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                size_t idx = static_cast<size_t>(y) * w + x;
                if (src[idx]) continue;
                bool on = false;
                for (int oy = -1; oy <= 1 && !on; ++oy)
                    for (int ox = -1; ox <= 1; ++ox)
                        if (src[static_cast<size_t>(y + oy) * w + (x + ox)]) { on = true; break; }
                if (on) mask[idx] = 255;
            }
        }
    };
    auto erode = [&]() {
        auto src = mask;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                size_t idx = static_cast<size_t>(y) * w + x;
                if (!src[idx]) continue;
                bool keep = true;
                for (int oy = -1; oy <= 1 && keep; ++oy)
                    for (int ox = -1; ox <= 1; ++ox)
                        if (!src[static_cast<size_t>(y + oy) * w + (x + ox)]) { keep = false; break; }
                if (!keep) mask[idx] = 0;
            }
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
                float center = src[idx];
                float sum = center * 3.0f;
                float weight = 3.0f;
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (ox == 0 && oy == 0) continue;
                        size_t n = static_cast<size_t>(y + oy) * w + (x + ox);
                        if (!mask[n]) continue;
                        float diff = std::abs(src[n] - center);
                        float k = diff < 0.10f ? 1.0f : 0.25f;
                        sum += src[n] * k;
                        weight += k;
                    }
                }
                depth.values[idx] = sum / weight;
            }
        }
    }
}

void ComputeDepthStats(const DepthImage& depth, const std::vector<std::uint8_t>& mask, ReconstructionReport* report) {
    if (!report) return;
    float mn = std::numeric_limits<float>::max();
    float mx = 0.0f;
    double sum = 0.0;
    int count = 0;
    for (size_t i = 0; i < depth.values.size() && i < mask.size(); ++i) {
        if (!mask[i]) continue;
        float v = depth.values[i];
        mn = std::min(mn, v);
        mx = std::max(mx, v);
        sum += v;
        ++count;
    }
    report->depthMin = count ? mn : 0.0f;
    report->depthMax = count ? mx : 0.0f;
    report->depthMean = count ? static_cast<float>(sum / count) : 0.0f;
}

float PixelXToWorld(int x, int w, const AdvancedOptions& options) {
    return ((w > 1 ? static_cast<float>(x) / static_cast<float>(w - 1) : 0.0f) - 0.5f) * options.xyScale;
}

float PixelYToWorld(int y, int h, const AdvancedOptions& options) {
    return (0.5f - (h > 1 ? static_cast<float>(y) / static_cast<float>(h - 1) : 0.0f)) * options.xyScale;
}

MeshData BuildReliefMesh(const DepthImage& depth, const std::vector<std::uint8_t>& mask, const AdvancedOptions& options) {
    int w = depth.width;
    int h = depth.height;
    MeshData mesh;
    std::vector<int> front(static_cast<size_t>(w) * h, -1);
    std::vector<int> back(static_cast<size_t>(w) * h, -1);

    int step = 1;
    int maxDim = std::max(w, h);
    if (options.maxGridResolution > 0 && maxDim > options.maxGridResolution) {
        step = static_cast<int>(std::ceil(static_cast<float>(maxDim) / static_cast<float>(options.maxGridResolution)));
    }

    for (int y = 0; y < h; y += step) {
        for (int x = 0; x < w; x += step) {
            int idx = y * w + x;
            if (!mask[static_cast<size_t>(idx)]) continue;
            float z = depth.values[static_cast<size_t>(idx)] * options.depthScale;
            float xw = PixelXToWorld(x, w, options);
            float yw = PixelYToWorld(y, h, options);
            float u = w > 1 ? static_cast<float>(x) / static_cast<float>(w - 1) : 0.0f;
            float v = h > 1 ? static_cast<float>(y) / static_cast<float>(h - 1) : 0.0f;
            front[static_cast<size_t>(idx)] = AddVertex(mesh, xw, yw, z, u, v);
            back[static_cast<size_t>(idx)] = AddVertex(mesh, xw, yw, z - options.baseThickness, u, v);
        }
    }

    auto idxAt = [&](int x, int y) { return y * w + x; };
    for (int y = 0; y + step < h; y += step) {
        for (int x = 0; x + step < w; x += step) {
            int i00 = idxAt(x, y);
            int i10 = idxAt(x + step, y);
            int i01 = idxAt(x, y + step);
            int i11 = idxAt(x + step, y + step);
            if (front[static_cast<size_t>(i00)] >= 0 && front[static_cast<size_t>(i10)] >= 0 && front[static_cast<size_t>(i11)] >= 0)
                AddTri(mesh, front[static_cast<size_t>(i00)], front[static_cast<size_t>(i10)], front[static_cast<size_t>(i11)]);
            if (front[static_cast<size_t>(i00)] >= 0 && front[static_cast<size_t>(i11)] >= 0 && front[static_cast<size_t>(i01)] >= 0)
                AddTri(mesh, front[static_cast<size_t>(i00)], front[static_cast<size_t>(i11)], front[static_cast<size_t>(i01)]);
            if (back[static_cast<size_t>(i00)] >= 0 && back[static_cast<size_t>(i11)] >= 0 && back[static_cast<size_t>(i10)] >= 0)
                AddTri(mesh, back[static_cast<size_t>(i00)], back[static_cast<size_t>(i11)], back[static_cast<size_t>(i10)]);
            if (back[static_cast<size_t>(i00)] >= 0 && back[static_cast<size_t>(i01)] >= 0 && back[static_cast<size_t>(i11)] >= 0)
                AddTri(mesh, back[static_cast<size_t>(i00)], back[static_cast<size_t>(i01)], back[static_cast<size_t>(i11)]);
        }
    }

    // Boundary walls. This prevents the output from being a paper-thin extrusion.
    for (int y = 0; y < h; y += step) {
        for (int x = 0; x < w; x += step) {
            int i = y * w + x;
            if (!mask[static_cast<size_t>(i)] || front[static_cast<size_t>(i)] < 0) continue;
            const int dx[4] = {-step, step, 0, 0};
            const int dy[4] = {0, 0, -step, step};
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx[d];
                int ny = y + dy[d];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h || !mask[static_cast<size_t>(ny) * w + nx]) {
                    int sx = std::clamp(x + (d < 2 ? 0 : step), 0, w - 1);
                    int sy = std::clamp(y + (d < 2 ? step : 0), 0, h - 1);
                    int j = sy * w + sx;
                    if (j != i && j >= 0 && j < w * h && front[static_cast<size_t>(j)] >= 0 && back[static_cast<size_t>(j)] >= 0) {
                        AddQuad(mesh, front[static_cast<size_t>(i)], front[static_cast<size_t>(j)], back[static_cast<size_t>(j)], back[static_cast<size_t>(i)]);
                    }
                }
            }
        }
    }
    return mesh;
}

MeshData BuildSilhouetteVolume(const DepthImage& depth, const std::vector<std::uint8_t>& mask, const AdvancedOptions& options) {
    int w = depth.width;
    int h = depth.height;
    MeshData mesh;
    BBox box = ComputeBBox(mask, w, h);
    if (!box.valid) return mesh;
    int segments = std::max(8, options.volumeRadialSegments);

    std::vector<int> rows;
    int samples = options.quality == QualityPreset::Detailed ? 64 : (options.quality == QualityPreset::Standard ? 44 : 28);
    for (int i = 0; i <= samples; ++i) {
        int y = box.minY + (box.maxY - box.minY) * i / std::max(1, samples);
        rows.push_back(y);
    }

    std::vector<std::vector<int>> rings;
    for (int y : rows) {
        int left = w, right = -1;
        float depthSum = 0.0f;
        int depthCount = 0;
        for (int x = box.minX; x <= box.maxX; ++x) {
            size_t idx = static_cast<size_t>(y) * w + x;
            if (!mask[idx]) continue;
            left = std::min(left, x);
            right = std::max(right, x);
            depthSum += depth.values[idx];
            ++depthCount;
        }
        if (right < left) {
            rings.emplace_back();
            continue;
        }
        float centerX = (left + right) * 0.5f;
        float radiusX = std::max(0.005f, (right - left + 1) * 0.5f / std::max(1, w - 1) * options.xyScale);
        float yWorld = PixelYToWorld(y, h, options);
        float d = depthCount > 0 ? depthSum / depthCount : 0.5f;
        float radiusZ = std::max(options.baseThickness * 0.6f, radiusX * (0.38f + 0.48f * d));
        float zCenter = d * options.depthScale * 0.35f;
        float xWorld = PixelXToWorld(static_cast<int>(centerX), w, options);

        std::vector<int> ring;
        ring.reserve(static_cast<size_t>(segments));
        for (int s = 0; s < segments; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(segments);
            float a = t * 2.0f * kPi;
            float x = xWorld + std::cos(a) * radiusX;
            float z = zCenter + std::sin(a) * radiusZ;
            ring.push_back(AddVertex(mesh, x, yWorld, z, t, static_cast<float>(y - box.minY) / std::max(1, box.maxY - box.minY)));
        }
        rings.push_back(std::move(ring));
    }

    for (size_t r = 0; r + 1 < rings.size(); ++r) {
        if (rings[r].empty() || rings[r + 1].empty()) continue;
        for (int s = 0; s < segments; ++s) {
            int n = (s + 1) % segments;
            AddQuad(mesh, rings[r][s], rings[r][n], rings[r + 1][n], rings[r + 1][s]);
        }
    }
    if (!rings.empty()) {
        for (size_t r : {size_t(0), rings.size() - 1}) {
            if (rings[r].empty()) continue;
            Vec3 center{};
            for (int vi : rings[r]) {
                center.x += mesh.positions[static_cast<size_t>(vi) * 3 + 0];
                center.y += mesh.positions[static_cast<size_t>(vi) * 3 + 1];
                center.z += mesh.positions[static_cast<size_t>(vi) * 3 + 2];
            }
            float inv = 1.0f / static_cast<float>(rings[r].size());
            int c = AddVertex(mesh, center.x * inv, center.y * inv, center.z * inv, 0.5f, 0.5f);
            for (int s = 0; s < segments; ++s) {
                int n = (s + 1) % segments;
                if (r == 0) AddTri(mesh, c, rings[r][n], rings[r][s]);
                else AddTri(mesh, c, rings[r][s], rings[r][n]);
            }
        }
    }
    return mesh;
}

void AppendMesh(MeshData& dst, const MeshData& src) {
    std::uint32_t base = static_cast<std::uint32_t>(VertexCount(dst));
    dst.positions.insert(dst.positions.end(), src.positions.begin(), src.positions.end());
    dst.normals.insert(dst.normals.end(), src.normals.begin(), src.normals.end());
    dst.uvs.insert(dst.uvs.end(), src.uvs.begin(), src.uvs.end());
    for (auto i : src.indices) dst.indices.push_back(base + i);
}

} // namespace

const char* ToString(ReconstructionMode mode) {
    switch (mode) {
        case ReconstructionMode::Auto: return "Auto";
        case ReconstructionMode::ReliefSurface: return "ReliefSurface";
        case ReconstructionMode::SilhouetteVolume: return "SilhouetteVolume";
        case ReconstructionMode::HybridVolume: return "HybridVolume";
    }
    return "Unknown";
}

const char* ToString(QualityPreset preset) {
    switch (preset) {
        case QualityPreset::Draft: return "Draft";
        case QualityPreset::Standard: return "Standard";
        case QualityPreset::Detailed: return "Detailed";
    }
    return "Unknown";
}

std::optional<ImageRGBA> LoadImageRGBA(const fs::path& path, std::string* error) {
    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load(path.u8string().c_str(), &w, &h, &comp, 4);
    if (!data) {
        if (error) *error = "Failed to load image: " + path.u8string();
        return std::nullopt;
    }
    ImageRGBA image;
    image.width = w;
    image.height = h;
    image.pixels.assign(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
    return image;
}

std::optional<DepthImage> LoadDepthImage(const fs::path& path, std::string* error) {
    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load(path.u8string().c_str(), &w, &h, &comp, 0);
    if (!data) {
        if (error) *error = "Failed to load depth image: " + path.u8string();
        return std::nullopt;
    }
    DepthImage depth;
    depth.width = w;
    depth.height = h;
    depth.values.resize(static_cast<size_t>(w) * h, 0.0f);
    int step = std::max(1, comp);
    for (int i = 0; i < w * h; ++i) depth.values[static_cast<size_t>(i)] = data[i * step] / 255.0f;
    stbi_image_free(data);
    return depth;
}

std::vector<std::uint8_t> BuildForegroundMask(const ImageRGBA& image, ReconstructionReport* report) {
    const int w = image.width;
    const int h = image.height;
    std::vector<std::uint8_t> mask(static_cast<size_t>(w) * h, 0);
    if (w <= 0 || h <= 0 || image.pixels.size() < static_cast<size_t>(w) * h * 4) return mask;

    int alphaCount = 0;
    for (int i = 0; i < w * h; ++i) {
        std::uint8_t a = image.pixels[static_cast<size_t>(i) * 4 + 3];
        if (a > 24) {
            mask[static_cast<size_t>(i)] = 255;
            ++alphaCount;
        }
    }

    const int total = w * h;
    if (alphaCount == 0 || alphaCount > total * 98 / 100) {
        std::array<float, 3> bg{};
        std::array<float, 3> bgSq{};
        int count = 0;
        auto addBg = [&](int x, int y) {
            size_t p = (static_cast<size_t>(y) * w + x) * 4;
            for (int c = 0; c < 3; ++c) {
                float v = image.pixels[p + c];
                bg[c] += v;
                bgSq[c] += v * v;
            }
            ++count;
        };
        for (int x = 0; x < w; ++x) { addBg(x, 0); addBg(x, h - 1); }
        for (int y = 1; y < h - 1; ++y) { addBg(0, y); addBg(w - 1, y); }
        float variance = 0.0f;
        for (int c = 0; c < 3; ++c) {
            bg[c] /= std::max(1, count);
            float meanSq = bgSq[c] / std::max(1, count);
            variance += std::max(0.0f, meanSq - bg[c] * bg[c]);
        }
        float stddev = std::sqrt(variance / 3.0f);
        float threshold = std::max(18.0f, stddev * 2.2f + 12.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                size_t p = (static_cast<size_t>(y) * w + x) * 4;
                float dr = image.pixels[p + 0] - bg[0];
                float dg = image.pixels[p + 1] - bg[1];
                float db = image.pixels[p + 2] - bg[2];
                if (std::sqrt(dr * dr + dg * dg + db * db) > threshold) mask[static_cast<size_t>(y) * w + x] = 255;
                else mask[static_cast<size_t>(y) * w + x] = 0;
            }
        }
    }

    MorphologicalClose(mask, w, h, 2);

    if (report) {
        report->imageWidth = w;
        report->imageHeight = h;
        report->foregroundPixels = static_cast<int>(std::count_if(mask.begin(), mask.end(), [](std::uint8_t v) { return v != 0; }));
        report->foregroundCoverage = total > 0 ? static_cast<float>(report->foregroundPixels) / static_cast<float>(total) : 0.0f;
        if (report->foregroundPixels == 0) report->warnings.push_back("Foreground mask is empty.");
        if (report->foregroundCoverage > 0.92f) report->warnings.push_back("Foreground mask covers almost the entire image; use alpha PNG or simpler background for better reconstruction.");
    }
    return mask;
}

DepthImage EstimateDepthFromSingleImage(const ImageRGBA& image, const std::vector<std::uint8_t>& mask, ReconstructionReport* report) {
    const int w = image.width;
    const int h = image.height;
    auto dist = DistanceToBoundary(mask, w, h);
    DepthImage depth;
    depth.width = w;
    depth.height = h;
    depth.values.resize(static_cast<size_t>(w) * h, 0.0f);

    ShapeStats stats = AnalyzeMask(image, mask);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t idx = static_cast<size_t>(y) * w + x;
            if (!mask[idx]) continue;
            size_t p = idx * 4;
            float lum = (0.299f * image.pixels[p + 0] + 0.587f * image.pixels[p + 1] + 0.114f * image.pixels[p + 2]) / 255.0f;
            float verticalBias = stats.box.valid ? 1.0f - std::abs((static_cast<float>(y - stats.box.minY) / std::max(1, stats.box.maxY - stats.box.minY)) - 0.45f) : 0.5f;
            float d = 0.58f * std::pow(dist[idx], 0.74f) + 0.26f * lum + 0.16f * Clamp01(verticalBias);
            depth.values[idx] = Clamp01(d);
        }
    }

    SmoothDepth(depth, mask, 2);
    ComputeDepthStats(depth, mask, report);
    return depth;
}

DepthImage PrepareDepth(const ImageRGBA& color, const std::optional<DepthImage>& providedDepth, const std::vector<std::uint8_t>& mask, const AdvancedOptions& options, ReconstructionReport* report) {
    DepthImage depth;
    if (providedDepth && providedDepth->width == color.width && providedDepth->height == color.height) {
        depth = *providedDepth;
        if (report) report->usedProvidedDepth = true;
    } else {
        depth = EstimateDepthFromSingleImage(color, mask, report);
        if (providedDepth && report) report->warnings.push_back("Provided depth size did not match the color image. Falling back to single-image depth estimation.");
    }
    int passes = options.quality == QualityPreset::Detailed ? 3 : (options.quality == QualityPreset::Standard ? 2 : 1);
    SmoothDepth(depth, mask, passes);
    ComputeDepthStats(depth, mask, report);
    return depth;
}

MeshData ReconstructMesh(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const AdvancedOptions& options, ReconstructionReport* report) {
    ShapeStats stats = AnalyzeMask(color, mask);
    ReconstructionMode mode = options.mode;
    if (mode == ReconstructionMode::Auto) {
        if (stats.coverage > 0.18f && stats.aspect > 0.55f && stats.aspect < 3.8f) mode = ReconstructionMode::HybridVolume;
        else mode = ReconstructionMode::ReliefSurface;
    }

    MeshData mesh;
    if (mode == ReconstructionMode::ReliefSurface) {
        mesh = BuildReliefMesh(depth, mask, options);
    } else if (mode == ReconstructionMode::SilhouetteVolume) {
        mesh = BuildSilhouetteVolume(depth, mask, options);
    } else {
        MeshData relief = BuildReliefMesh(depth, mask, options);
        MeshData volume = BuildSilhouetteVolume(depth, mask, options);
        mesh = volume;
        AppendMesh(mesh, relief);
    }

    RecomputeNormals(mesh);
    NormalizeMesh(mesh, 2.0f);

    if (report) {
        report->reconstructionMode = ToString(mode);
        report->vertices = VertexCount(mesh);
        report->triangles = TriangleCount(mesh);
        report->watertightCandidate = mode == ReconstructionMode::SilhouetteVolume || mode == ReconstructionMode::HybridVolume;
        if (report->vertices == 0 || report->triangles == 0) report->warnings.push_back("Generated mesh is empty.");
        if (stats.edgeDensity > 0.35f) report->warnings.push_back("Input has high internal edge density; generated depth may contain noisy relief details.");
    }
    return mesh;
}

void RecomputeNormals(MeshData& mesh) {
    mesh.normals.assign(mesh.positions.size(), 0.0f);
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::uint32_t ia = mesh.indices[i + 0];
        std::uint32_t ib = mesh.indices[i + 1];
        std::uint32_t ic = mesh.indices[i + 2];
        if ((static_cast<size_t>(ia) + 1) * 3 > mesh.positions.size() ||
            (static_cast<size_t>(ib) + 1) * 3 > mesh.positions.size() ||
            (static_cast<size_t>(ic) + 1) * 3 > mesh.positions.size()) continue;
        Vec3 a{mesh.positions[ia * 3 + 0], mesh.positions[ia * 3 + 1], mesh.positions[ia * 3 + 2]};
        Vec3 b{mesh.positions[ib * 3 + 0], mesh.positions[ib * 3 + 1], mesh.positions[ib * 3 + 2]};
        Vec3 c{mesh.positions[ic * 3 + 0], mesh.positions[ic * 3 + 1], mesh.positions[ic * 3 + 2]};
        Vec3 n = Cross(b - a, c - a);
        for (std::uint32_t vi : {ia, ib, ic}) {
            mesh.normals[static_cast<size_t>(vi) * 3 + 0] += n.x;
            mesh.normals[static_cast<size_t>(vi) * 3 + 1] += n.y;
            mesh.normals[static_cast<size_t>(vi) * 3 + 2] += n.z;
        }
    }
    for (size_t i = 0; i + 2 < mesh.normals.size(); i += 3) {
        Vec3 n{mesh.normals[i], mesh.normals[i + 1], mesh.normals[i + 2]};
        n = Normalize(n);
        mesh.normals[i + 0] = n.x;
        mesh.normals[i + 1] = n.y;
        mesh.normals[i + 2] = n.z;
    }
}

void NormalizeMesh(MeshData& mesh, float targetHeight) {
    if (mesh.positions.empty()) return;
    Vec3 mn{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 mx{-mn.x, -mn.y, -mn.z};
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        mn.x = std::min(mn.x, mesh.positions[i + 0]); mn.y = std::min(mn.y, mesh.positions[i + 1]); mn.z = std::min(mn.z, mesh.positions[i + 2]);
        mx.x = std::max(mx.x, mesh.positions[i + 0]); mx.y = std::max(mx.y, mesh.positions[i + 1]); mx.z = std::max(mx.z, mesh.positions[i + 2]);
    }
    Vec3 center{(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f};
    float height = std::max(0.001f, mx.y - mn.y);
    float scale = targetHeight / height;
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        mesh.positions[i + 0] = (mesh.positions[i + 0] - center.x) * scale;
        mesh.positions[i + 1] = (mesh.positions[i + 1] - mn.y) * scale;
        mesh.positions[i + 2] = (mesh.positions[i + 2] - center.z) * scale;
    }
}

bool ExportOBJ(const MeshData& mesh, const fs::path& objPath, const std::string& materialTextureName, std::string* error) {
    std::error_code ec;
    fs::create_directories(objPath.parent_path(), ec);
    std::ofstream obj(objPath, std::ios::binary);
    if (!obj) { if (error) *error = "Failed to open OBJ for writing."; return false; }
    std::string mtlName = objPath.stem().u8string() + ".mtl";
    obj << "mtllib " << mtlName << "\n";
    obj << "o Make3DAdvancedModel\n";
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) obj << "v " << mesh.positions[i] << ' ' << mesh.positions[i + 1] << ' ' << mesh.positions[i + 2] << "\n";
    for (size_t i = 0; i + 1 < mesh.uvs.size(); i += 2) obj << "vt " << mesh.uvs[i] << ' ' << (1.0f - mesh.uvs[i + 1]) << "\n";
    for (size_t i = 0; i + 2 < mesh.normals.size(); i += 3) obj << "vn " << mesh.normals[i] << ' ' << mesh.normals[i + 1] << ' ' << mesh.normals[i + 2] << "\n";
    obj << "usemtl Material0\n";
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::uint32_t a = mesh.indices[i + 0] + 1;
        std::uint32_t b = mesh.indices[i + 1] + 1;
        std::uint32_t c = mesh.indices[i + 2] + 1;
        obj << "f " << a << '/' << a << '/' << a << ' ' << b << '/' << b << '/' << b << ' ' << c << '/' << c << '/' << c << "\n";
    }
    std::ofstream mtl(objPath.parent_path() / mtlName, std::ios::binary);
    if (!mtl) { if (error) *error = "Failed to open MTL for writing."; return false; }
    mtl << "newmtl Material0\nKa 1 1 1\nKd 1 1 1\nKs 0 0 0\n";
    if (!materialTextureName.empty()) mtl << "map_Kd " << materialTextureName << "\n";
    return true;
}

bool ExportGLTF(const MeshData& mesh, const fs::path& gltfPath, std::string* error) {
    std::error_code ec;
    fs::create_directories(gltfPath.parent_path(), ec);
    fs::path binPath = gltfPath;
    binPath.replace_extension(".bin");

    std::ofstream bin(binPath, std::ios::binary);
    if (!bin) { if (error) *error = "Failed to open glTF binary buffer."; return false; }
    auto writeVec = [&](const auto& v) {
        if (!v.empty()) bin.write(reinterpret_cast<const char*>(v.data()), static_cast<std::streamsize>(v.size() * sizeof(v[0])));
    };
    size_t posOffset = 0;
    writeVec(mesh.positions);
    size_t normOffset = mesh.positions.size() * sizeof(float);
    writeVec(mesh.normals);
    size_t uvOffset = normOffset + mesh.normals.size() * sizeof(float);
    writeVec(mesh.uvs);
    size_t idxOffset = uvOffset + mesh.uvs.size() * sizeof(float);
    writeVec(mesh.indices);
    size_t totalSize = idxOffset + mesh.indices.size() * sizeof(std::uint32_t);

    Vec3 mn{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 mx{-mn.x, -mn.y, -mn.z};
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        mn.x = std::min(mn.x, mesh.positions[i + 0]); mn.y = std::min(mn.y, mesh.positions[i + 1]); mn.z = std::min(mn.z, mesh.positions[i + 2]);
        mx.x = std::max(mx.x, mesh.positions[i + 0]); mx.y = std::max(mx.y, mesh.positions[i + 1]); mx.z = std::max(mx.z, mesh.positions[i + 2]);
    }

    std::ofstream gltf(gltfPath, std::ios::binary);
    if (!gltf) { if (error) *error = "Failed to open glTF for writing."; return false; }
    gltf << std::fixed << std::setprecision(6);
    gltf << "{\n";
    gltf << "  \"asset\": { \"version\": \"2.0\", \"generator\": \"Make3DAdvanced\" },\n";
    gltf << "  \"scene\": 0,\n  \"scenes\": [{ \"nodes\": [0] }],\n  \"nodes\": [{ \"mesh\": 0, \"name\": \"Make3DAdvancedModel\" }],\n";
    gltf << "  \"meshes\": [{ \"primitives\": [{ \"attributes\": { \"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": 2 }, \"indices\": 3, \"mode\": 4 }] }],\n";
    gltf << "  \"buffers\": [{ \"uri\": \"" << EscapeJson(binPath.filename().u8string()) << "\", \"byteLength\": " << totalSize << " }],\n";
    gltf << "  \"bufferViews\": [\n";
    gltf << "    { \"buffer\": 0, \"byteOffset\": " << posOffset << ", \"byteLength\": " << mesh.positions.size() * sizeof(float) << ", \"target\": 34962 },\n";
    gltf << "    { \"buffer\": 0, \"byteOffset\": " << normOffset << ", \"byteLength\": " << mesh.normals.size() * sizeof(float) << ", \"target\": 34962 },\n";
    gltf << "    { \"buffer\": 0, \"byteOffset\": " << uvOffset << ", \"byteLength\": " << mesh.uvs.size() * sizeof(float) << ", \"target\": 34962 },\n";
    gltf << "    { \"buffer\": 0, \"byteOffset\": " << idxOffset << ", \"byteLength\": " << mesh.indices.size() * sizeof(std::uint32_t) << ", \"target\": 34963 }\n";
    gltf << "  ],\n";
    gltf << "  \"accessors\": [\n";
    gltf << "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": " << VertexCount(mesh) << ", \"type\": \"VEC3\", \"min\": [" << mn.x << ',' << mn.y << ',' << mn.z << "], \"max\": [" << mx.x << ',' << mx.y << ',' << mx.z << "] },\n";
    gltf << "    { \"bufferView\": 1, \"componentType\": 5126, \"count\": " << VertexCount(mesh) << ", \"type\": \"VEC3\" },\n";
    gltf << "    { \"bufferView\": 2, \"componentType\": 5126, \"count\": " << VertexCount(mesh) << ", \"type\": \"VEC2\" },\n";
    gltf << "    { \"bufferView\": 3, \"componentType\": 5125, \"count\": " << mesh.indices.size() << ", \"type\": \"SCALAR\" }\n";
    gltf << "  ]\n}";
    return true;
}

bool SaveDebugPPM(const fs::path& path, int width, int height, const std::vector<std::uint8_t>& rgb, std::string* error) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::binary);
    if (!file) { if (error) *error = "Failed to save debug image."; return false; }
    file << "P6\n" << width << ' ' << height << "\n255\n";
    file.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    return true;
}

std::string ReconstructionReport::ToMarkdown() const {
    std::ostringstream oss;
    oss << "# Make3D Advanced Reconstruction Report\n\n";
    oss << "| Metric | Value |\n|---|---:|\n";
    oss << "| Image width | " << imageWidth << " |\n";
    oss << "| Image height | " << imageHeight << " |\n";
    oss << "| Foreground pixels | " << foregroundPixels << " |\n";
    oss << "| Foreground coverage | " << foregroundCoverage << " |\n";
    oss << "| Depth min | " << depthMin << " |\n";
    oss << "| Depth max | " << depthMax << " |\n";
    oss << "| Depth mean | " << depthMean << " |\n";
    oss << "| Reconstruction mode | " << reconstructionMode << " |\n";
    oss << "| Vertices | " << vertices << " |\n";
    oss << "| Triangles | " << triangles << " |\n";
    oss << "| Provided depth | " << (usedProvidedDepth ? "yes" : "no") << " |\n";
    oss << "| Watertight candidate | " << (watertightCandidate ? "yes" : "no") << " |\n\n";
    if (!warnings.empty()) {
        oss << "## Warnings\n\n";
        for (const auto& w : warnings) oss << "- " << w << "\n";
    }
    return oss.str();
}

std::string ReconstructionReport::ToJson() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"imageWidth\": " << imageWidth << ",\n";
    oss << "  \"imageHeight\": " << imageHeight << ",\n";
    oss << "  \"foregroundPixels\": " << foregroundPixels << ",\n";
    oss << "  \"foregroundCoverage\": " << foregroundCoverage << ",\n";
    oss << "  \"depthMin\": " << depthMin << ",\n";
    oss << "  \"depthMax\": " << depthMax << ",\n";
    oss << "  \"depthMean\": " << depthMean << ",\n";
    oss << "  \"reconstructionMode\": \"" << EscapeJson(reconstructionMode) << "\",\n";
    oss << "  \"vertices\": " << vertices << ",\n";
    oss << "  \"triangles\": " << triangles << ",\n";
    oss << "  \"usedProvidedDepth\": " << (usedProvidedDepth ? "true" : "false") << ",\n";
    oss << "  \"watertightCandidate\": " << (watertightCandidate ? "true" : "false") << ",\n";
    oss << "  \"warnings\": [";
    for (size_t i = 0; i < warnings.size(); ++i) {
        if (i) oss << ", ";
        oss << "\"" << EscapeJson(warnings[i]) << "\"";
    }
    oss << "]\n}";
    return oss.str();
}

BuildOutput BuildModelFromImage(const fs::path& colorPath, const std::optional<fs::path>& depthPath, const fs::path& outputDir, const AdvancedOptions& options) {
    BuildOutput out;
    std::string error;
    auto color = LoadImageRGBA(colorPath, &error);
    if (!color) { out.message = error; return out; }

    std::optional<DepthImage> providedDepth;
    if (depthPath) providedDepth = LoadDepthImage(*depthPath, &error);

    out.report.imageWidth = color->width;
    out.report.imageHeight = color->height;
    auto mask = BuildForegroundMask(*color, &out.report);
    if (out.report.foregroundPixels == 0) {
        out.message = "Foreground extraction failed.";
        return out;
    }

    DepthImage depth = PrepareDepth(*color, providedDepth, mask, options, &out.report);
    out.mesh = ReconstructMesh(*color, depth, mask, options, &out.report);
    if (out.mesh.positions.empty() || out.mesh.indices.empty()) {
        out.message = "Mesh reconstruction failed.";
        return out;
    }

    std::error_code ec;
    fs::create_directories(outputDir, ec);
    fs::path objPath = outputDir / "make3d_advanced.obj";
    fs::path gltfPath = outputDir / "make3d_advanced.gltf";
    fs::path reportMd = outputDir / "make3d_report.md";
    fs::path reportJson = outputDir / "make3d_report.json";

    if (options.exportObj && !ExportOBJ(out.mesh, objPath, "", &error)) { out.message = error; return out; }
    if (options.exportGltf && !ExportGLTF(out.mesh, gltfPath, &error)) { out.message = error; return out; }

    std::ofstream md(reportMd, std::ios::binary);
    md << out.report.ToMarkdown();
    std::ofstream js(reportJson, std::ios::binary);
    js << out.report.ToJson();
    out.report.objPath = objPath;
    out.report.gltfPath = gltfPath;
    out.report.reportPath = reportMd;

    if (options.writeDebugImages) {
        std::vector<std::uint8_t> maskRgb(static_cast<size_t>(color->width) * color->height * 3, 0);
        std::vector<std::uint8_t> depthRgb(static_cast<size_t>(color->width) * color->height * 3, 0);
        for (int i = 0; i < color->width * color->height; ++i) {
            std::uint8_t m = mask[static_cast<size_t>(i)] ? 255 : 0;
            std::uint8_t d = static_cast<std::uint8_t>(Clamp01(depth.values[static_cast<size_t>(i)]) * 255.0f);
            maskRgb[static_cast<size_t>(i) * 3 + 0] = m;
            maskRgb[static_cast<size_t>(i) * 3 + 1] = m;
            maskRgb[static_cast<size_t>(i) * 3 + 2] = m;
            depthRgb[static_cast<size_t>(i) * 3 + 0] = d;
            depthRgb[static_cast<size_t>(i) * 3 + 1] = d;
            depthRgb[static_cast<size_t>(i) * 3 + 2] = d;
        }
        SaveDebugPPM(outputDir / "debug_mask.ppm", color->width, color->height, maskRgb, nullptr);
        SaveDebugPPM(outputDir / "debug_depth.ppm", color->width, color->height, depthRgb, nullptr);
    }

    out.ok = true;
    out.message = "Advanced reconstruction finished.";
    return out;
}

} // namespace make3d
