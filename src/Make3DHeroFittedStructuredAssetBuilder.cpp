#include "Make3DStructuredAssetBuilder.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace make3d {
namespace {

struct HeroProfile {
    bool ok = false;
    int minX = 0;
    int minY = 0;
    int maxX = -1;
    int maxY = -1;
    float aspect = 1.0f;
};

static int HeroVertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
static int HeroMaxInt(int a, int b) { return a > b ? a : b; }
static int HeroMinInt(int a, int b) { return a < b ? a : b; }
static float HeroMax(float a, float b) { return a > b ? a : b; }
static float HeroMin(float a, float b) { return a < b ? a : b; }
static float HeroClamp(float v, float lo, float hi) { return HeroMax(lo, HeroMin(hi, v)); }
static int HeroClampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static bool HeroMaskAt(const ImageRGBA& image, const std::vector<std::uint8_t>& mask, int x, int y) {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) return false;
    const size_t id = static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x);
    return id < mask.size() && mask[id] != 0;
}

static HeroProfile BuildHeroProfile(const ImageRGBA& image, const std::vector<std::uint8_t>& mask) {
    HeroProfile p;
    if (image.width <= 0 || image.height <= 0 || mask.size() != static_cast<size_t>(image.width * image.height)) return p;
    p.minX = image.width;
    p.minY = image.height;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            if (!HeroMaskAt(image, mask, x, y)) continue;
            p.minX = HeroMinInt(p.minX, x);
            p.maxX = HeroMaxInt(p.maxX, x);
            p.minY = HeroMinInt(p.minY, y);
            p.maxY = HeroMaxInt(p.maxY, y);
        }
    }
    if (p.maxX < p.minX || p.maxY < p.minY) return p;
    const int bw = HeroMaxInt(1, p.maxX - p.minX + 1);
    const int bh = HeroMaxInt(1, p.maxY - p.minY + 1);
    p.aspect = static_cast<float>(bh) / static_cast<float>(bw);
    p.ok = true;
    return p;
}

static void HeroAddVertex(MeshData& mesh, float x, float y, float z, float u, float v) {
    mesh.positions.push_back(x);
    mesh.positions.push_back(y);
    mesh.positions.push_back(z);
    mesh.normals.push_back(0.0f);
    mesh.normals.push_back(1.0f);
    mesh.normals.push_back(0.0f);
    mesh.uvs.push_back(u);
    mesh.uvs.push_back(v);
}

static void HeroTri(MeshData& mesh, int a, int b, int c) {
    if (a < 0 || b < 0 || c < 0 || a == b || b == c || c == a) return;
    mesh.indices.push_back(static_cast<std::uint32_t>(a));
    mesh.indices.push_back(static_cast<std::uint32_t>(b));
    mesh.indices.push_back(static_cast<std::uint32_t>(c));
}

static void HeroQuad(MeshData& mesh, int a, int b, int c, int d) {
    HeroTri(mesh, a, b, c);
    HeroTri(mesh, a, c, d);
}

static void ApplyHeroProjectionUVs(MeshData& mesh) {
    const int count = HeroVertexCount(mesh);
    if (count <= 0 || mesh.positions.size() < static_cast<size_t>(count) * 3) return;
    float minX = mesh.positions[0], maxX = mesh.positions[0];
    float minY = mesh.positions[1], maxY = mesh.positions[1];
    for (int i = 0; i < count; ++i) {
        const size_t p = static_cast<size_t>(i) * 3;
        minX = HeroMin(minX, mesh.positions[p + 0]);
        maxX = HeroMax(maxX, mesh.positions[p + 0]);
        minY = HeroMin(minY, mesh.positions[p + 1]);
        maxY = HeroMax(maxY, mesh.positions[p + 1]);
    }
    const float rx = HeroMax(0.0001f, maxX - minX);
    const float ry = HeroMax(0.0001f, maxY - minY);
    mesh.uvs.assign(static_cast<size_t>(count) * 2, 0.0f);
    for (int i = 0; i < count; ++i) {
        const size_t p = static_cast<size_t>(i) * 3;
        mesh.uvs[static_cast<size_t>(i) * 2 + 0] = HeroClamp((mesh.positions[p + 0] - minX) / rx, 0.0f, 1.0f);
        mesh.uvs[static_cast<size_t>(i) * 2 + 1] = HeroClamp((mesh.positions[p + 1] - minY) / ry, 0.0f, 1.0f);
    }
}

static void AddSolidHeroShell(MeshData& mesh, const ImageRGBA& image, const std::vector<std::uint8_t>& mask, float targetHeight, float targetDepth) {
    const HeroProfile profile = BuildHeroProfile(image, mask);
    if (!profile.ok) return;

    const int bw = HeroMaxInt(1, profile.maxX - profile.minX + 1);
    const int bh = HeroMaxInt(1, profile.maxY - profile.minY + 1);
    const int cols = HeroClampInt(bw / 3, 32, 88);
    const int rows = HeroClampInt(bh / 3, 36, 96);
    const float shellWidth = HeroClamp(targetHeight / HeroMax(0.75f, profile.aspect), targetHeight * 0.50f, targetHeight * 0.98f);
    const float shellHeight = targetHeight * 1.46f;
    const float frontZ = targetDepth * 0.62f;
    const float backZ = -targetDepth * 0.18f;

    const size_t gridSize = static_cast<size_t>(cols + 1) * static_cast<size_t>(rows + 1);
    std::vector<int> front(gridSize, -1);
    std::vector<int> back(gridSize, -1);

    for (int gy = 0; gy <= rows; ++gy) {
        const float vy = rows > 0 ? static_cast<float>(gy) / static_cast<float>(rows) : 0.0f;
        const int py = profile.minY + static_cast<int>(vy * static_cast<float>(bh - 1) + 0.5f);
        for (int gx = 0; gx <= cols; ++gx) {
            const float ux = cols > 0 ? static_cast<float>(gx) / static_cast<float>(cols) : 0.0f;
            const int px = profile.minX + static_cast<int>(ux * static_cast<float>(bw - 1) + 0.5f);
            bool inside = false;
            for (int oy = -1; oy <= 1 && !inside; ++oy) {
                for (int ox = -1; ox <= 1 && !inside; ++ox) inside = HeroMaskAt(image, mask, px + ox, py + oy);
            }
            if (!inside) continue;
            const float x = (ux - 0.5f) * shellWidth;
            const float y = targetHeight * 1.48f - vy * shellHeight;
            const size_t id = static_cast<size_t>(gy) * static_cast<size_t>(cols + 1) + static_cast<size_t>(gx);
            front[id] = HeroVertexCount(mesh);
            HeroAddVertex(mesh, x, y, frontZ, ux, 1.0f - vy);
            back[id] = HeroVertexCount(mesh);
            HeroAddVertex(mesh, x * 0.92f, y, backZ, ux, 1.0f - vy);
        }
    }

    auto node = [cols](int gx, int gy) -> size_t {
        return static_cast<size_t>(gy) * static_cast<size_t>(cols + 1) + static_cast<size_t>(gx);
    };
    auto cellOk = [&](int gx, int gy) -> bool {
        if (gx < 0 || gy < 0 || gx >= cols || gy >= rows) return false;
        return front[node(gx, gy)] >= 0 && front[node(gx + 1, gy)] >= 0 && front[node(gx + 1, gy + 1)] >= 0 && front[node(gx, gy + 1)] >= 0;
    };

    for (int gy = 0; gy < rows; ++gy) {
        for (int gx = 0; gx < cols; ++gx) {
            if (!cellOk(gx, gy)) continue;
            const int fa = front[node(gx, gy)];
            const int fb = front[node(gx + 1, gy)];
            const int fc = front[node(gx + 1, gy + 1)];
            const int fd = front[node(gx, gy + 1)];
            const int ba = back[node(gx, gy)];
            const int bb = back[node(gx + 1, gy)];
            const int bc = back[node(gx + 1, gy + 1)];
            const int bd = back[node(gx, gy + 1)];
            HeroQuad(mesh, fa, fb, fc, fd);
            HeroQuad(mesh, bb, ba, bd, bc);
        }
    }

    for (int gy = 0; gy <= rows; ++gy) {
        for (int gx = 0; gx < cols; ++gx) {
            const int f0 = front[node(gx, gy)];
            const int f1 = front[node(gx + 1, gy)];
            if (f0 < 0 || f1 < 0) continue;
            const bool upper = cellOk(gx, gy - 1);
            const bool lower = cellOk(gx, gy);
            if (upper == lower) continue;
            const int b0 = back[node(gx, gy)];
            const int b1 = back[node(gx + 1, gy)];
            HeroQuad(mesh, f0, f1, b1, b0);
        }
    }
    for (int gx = 0; gx <= cols; ++gx) {
        for (int gy = 0; gy < rows; ++gy) {
            const int f0 = front[node(gx, gy)];
            const int f1 = front[node(gx, gy + 1)];
            if (f0 < 0 || f1 < 0) continue;
            const bool left = cellOk(gx - 1, gy);
            const bool right = cellOk(gx, gy);
            if (left == right) continue;
            const int b0 = back[node(gx, gy)];
            const int b1 = back[node(gx, gy + 1)];
            HeroQuad(mesh, f1, f0, b0, b1);
        }
    }
}

} // namespace

StructuredAssetBuildResult BuildHeroFittedStructuredAssetMesh(
    const ImageRGBA& image,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& mask,
    const StructuredAssetOptions& options) {

    StructuredAssetBuildResult result = BuildImageFittedStructuredAssetMesh(image, depth, mask, options);
    if (!result.ok) return result;
    if (result.plan.assetType != GameAssetType::Character && result.plan.assetType != GameAssetType::Creature) return result;

    MeshData hero = result.mesh;
    const float targetHeight = HeroMax(0.25f, options.targetHeight);
    const float targetDepth = HeroMax(0.52f, result.plan.targetDepth);
    AddSolidHeroShell(hero, image, mask, targetHeight, targetDepth);
    if (hero.positions.empty() || hero.indices.empty()) return result;

    RecomputeNormals(hero);
    if (options.normalizeOutput) NormalizeMesh(hero, targetHeight);
    ApplyHeroProjectionUVs(hero);
    GameAssetValidationReport validation = ValidateGameAssetMesh(hero);
    if (!validation.ok) return result;

    result.mesh = hero;
    result.validation = validation;
    result.message = "Hero image-fitted structured character mesh generated.";
    result.warnings.push_back("Solid hero silhouette shell added with front, back, and side walls so the source outline drives the model body.");
    result.warnings.push_back("Procedural character parts are retained as support volumes behind the silhouette-driven body.");
    return result;
}

} // namespace make3d
