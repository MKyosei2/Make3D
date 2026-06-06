#include "Make3DStructuredAssetBuilder.h"

#include <cmath>
#include <cstdint>
#include <sstream>
#include <vector>

namespace make3d {
namespace {

constexpr float FitPi = 3.14159265358979323846f;

struct FitVec3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };

struct SilhouetteProfile {
    bool ok = false;
    int minX = 0, minY = 0, maxX = -1, maxY = -1;
    int width = 1, height = 1;
    float aspect = 1.0f;
    float massCenterY = 0.5f;
    std::vector<float> rowWidth;
    std::vector<float> rowLeft;
    std::vector<float> rowRight;
    std::vector<float> rowCenter;
};

static int FitVertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
static int FitMaxInt(int a, int b) { return a > b ? a : b; }
static int FitMinInt(int a, int b) { return a < b ? a : b; }
static float FitMax(float a, float b) { return a > b ? a : b; }
static float FitMin(float a, float b) { return a < b ? a : b; }
static float FitClamp(float v, float lo, float hi) { return FitMax(lo, FitMin(hi, v)); }
static FitVec3 FitAdd(FitVec3 a, FitVec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static FitVec3 FitSub(FitVec3 a, FitVec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static FitVec3 FitMul(FitVec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float FitDot(FitVec3 a, FitVec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static FitVec3 FitCross(FitVec3 a, FitVec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static float FitLength(FitVec3 v) { return std::sqrt(FitMax(0.0f, FitDot(v, v))); }
static FitVec3 FitNormalize(FitVec3 v) { float len = FitLength(v); return len <= 1e-6f ? FitVec3{0, 1, 0} : FitMul(v, 1.0f / len); }

static void FitAddVertex(MeshData& mesh, float x, float y, float z, float u, float v) {
    mesh.positions.push_back(x); mesh.positions.push_back(y); mesh.positions.push_back(z);
    mesh.normals.push_back(0.0f); mesh.normals.push_back(1.0f); mesh.normals.push_back(0.0f);
    mesh.uvs.push_back(u); mesh.uvs.push_back(v);
}
static void FitAddVertex(MeshData& mesh, FitVec3 p, float u, float v) { FitAddVertex(mesh, p.x, p.y, p.z, u, v); }
static void FitTri(MeshData& mesh, int a, int b, int c) {
    if (a < 0 || b < 0 || c < 0 || a == b || b == c || c == a) return;
    mesh.indices.push_back(static_cast<std::uint32_t>(a));
    mesh.indices.push_back(static_cast<std::uint32_t>(b));
    mesh.indices.push_back(static_cast<std::uint32_t>(c));
}
static void FitQuad(MeshData& mesh, int a, int b, int c, int d) { FitTri(mesh, a, b, c); FitTri(mesh, a, c, d); }

static SilhouetteProfile FitBuildProfile(const ImageRGBA& image, const std::vector<std::uint8_t>& mask) {
    SilhouetteProfile p;
    if (image.width <= 0 || image.height <= 0 || mask.size() != static_cast<size_t>(image.width * image.height)) return p;
    p.width = image.width; p.height = image.height;
    p.minX = image.width; p.minY = image.height; p.maxX = -1; p.maxY = -1;
    double sumY = 0.0; int count = 0;
    for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
        if (!mask[static_cast<size_t>(y) * image.width + x]) continue;
        p.minX = FitMinInt(p.minX, x); p.maxX = FitMaxInt(p.maxX, x);
        p.minY = FitMinInt(p.minY, y); p.maxY = FitMaxInt(p.maxY, y);
        sumY += y; ++count;
    }
    if (count <= 0 || p.maxX < p.minX || p.maxY < p.minY) return p;
    const int bw = FitMaxInt(1, p.maxX - p.minX + 1);
    const int bh = FitMaxInt(1, p.maxY - p.minY + 1);
    p.aspect = static_cast<float>(bh) / static_cast<float>(bw);
    p.massCenterY = static_cast<float>(sumY / static_cast<double>(count) / static_cast<double>(FitMaxInt(1, image.height - 1)));
    p.rowWidth.assign(static_cast<size_t>(bh), 0.0f);
    p.rowLeft.assign(static_cast<size_t>(bh), -0.5f);
    p.rowRight.assign(static_cast<size_t>(bh), 0.5f);
    p.rowCenter.assign(static_cast<size_t>(bh), 0.0f);
    for (int y = p.minY; y <= p.maxY; ++y) {
        int left = image.width, right = -1;
        for (int x = p.minX; x <= p.maxX; ++x) {
            if (!mask[static_cast<size_t>(y) * image.width + x]) continue;
            left = FitMinInt(left, x); right = FitMaxInt(right, x);
        }
        if (right >= left) {
            const size_t row = static_cast<size_t>(y - p.minY);
            p.rowWidth[row] = static_cast<float>(right - left + 1) / static_cast<float>(bw);
            p.rowLeft[row] = static_cast<float>(left - p.minX) / static_cast<float>(bw) - 0.5f;
            p.rowRight[row] = static_cast<float>(right - p.minX) / static_cast<float>(bw) - 0.5f;
            p.rowCenter[row] = (p.rowLeft[row] + p.rowRight[row]) * 0.5f;
        }
    }
    p.ok = true;
    return p;
}

static float FitSampleRows(const std::vector<float>& rows, float normalizedYFromTop, float fallback) {
    if (rows.empty()) return fallback;
    float y = FitClamp(normalizedYFromTop, 0.0f, 1.0f) * static_cast<float>(rows.size() - 1);
    int y0 = static_cast<int>(std::floor(y));
    int y1 = FitMinInt(static_cast<int>(rows.size()) - 1, y0 + 1);
    float t = y - static_cast<float>(y0);
    return rows[static_cast<size_t>(y0)] * (1.0f - t) + rows[static_cast<size_t>(y1)] * t;
}

static float FitRowWidthAt(const SilhouetteProfile& p, float normalizedYFromTop) {
    if (!p.ok) return 0.5f;
    return FitSampleRows(p.rowWidth, normalizedYFromTop, 0.5f);
}

static float FitRowLeftAt(const SilhouetteProfile& p, float normalizedYFromTop) {
    if (!p.ok) return -0.5f;
    return FitSampleRows(p.rowLeft, normalizedYFromTop, -0.5f);
}

static float FitRowRightAt(const SilhouetteProfile& p, float normalizedYFromTop) {
    if (!p.ok) return 0.5f;
    return FitSampleRows(p.rowRight, normalizedYFromTop, 0.5f);
}

static float FitRowCenterAt(const SilhouetteProfile& p, float normalizedYFromTop) {
    if (!p.ok) return 0.0f;
    return FitSampleRows(p.rowCenter, normalizedYFromTop, 0.0f);
}

static float FitMaxRowWidth(const SilhouetteProfile& p, float a, float b) {
    if (!p.ok || p.rowWidth.empty()) return 0.5f;
    int y0 = static_cast<int>(FitClamp(a, 0.0f, 1.0f) * static_cast<float>(p.rowWidth.size() - 1));
    int y1 = static_cast<int>(FitClamp(b, 0.0f, 1.0f) * static_cast<float>(p.rowWidth.size() - 1));
    if (y1 < y0) { int tmp = y0; y0 = y1; y1 = tmp; }
    float best = 0.0f;
    for (int y = y0; y <= y1; ++y) best = FitMax(best, p.rowWidth[static_cast<size_t>(y)]);
    return best;
}

static float FitSideX(const SilhouetteProfile& p, float normalizedYFromTop, float side, float baseW, float fallbackAbs) {
    const float edge = side < 0.0f ? FitRowLeftAt(p, normalizedYFromTop) : FitRowRightAt(p, normalizedYFromTop);
    const float fallback = side * fallbackAbs;
    const float modelX = edge * baseW * 1.18f;
    if (std::fabs(modelX) < std::fabs(fallback) * 0.35f) return fallback;
    return FitClamp(modelX, -baseW * 0.68f, baseW * 0.68f);
}

static void FitEllipsoid(MeshData& mesh, float cx, float cy, float cz, float rx, float ry, float rz, int segments, int rings) {
    segments = FitMaxInt(10, segments); rings = FitMaxInt(5, rings);
    const int base = FitVertexCount(mesh);
    for (int y = 0; y <= rings; ++y) {
        float v = static_cast<float>(y) / static_cast<float>(rings);
        float theta = -FitPi * 0.5f + v * FitPi;
        float sy = std::sin(theta); float cr = std::cos(theta);
        for (int x = 0; x <= segments; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(segments);
            float a = u * FitPi * 2.0f;
            FitAddVertex(mesh, cx + std::cos(a) * cr * rx, cy + sy * ry, cz + std::sin(a) * cr * rz, u, v);
        }
    }
    int stride = segments + 1;
    for (int y = 0; y < rings; ++y) for (int x = 0; x < segments; ++x) {
        int a = base + y * stride + x;
        FitQuad(mesh, a, a + 1, a + stride + 1, a + stride);
    }
}

static void FitBox(MeshData& mesh, float cx, float cy, float cz, float sx, float sy, float sz) {
    int base = FitVertexCount(mesh);
    float x0 = cx - sx * 0.5f, x1 = cx + sx * 0.5f;
    float y0 = cy - sy * 0.5f, y1 = cy + sy * 0.5f;
    float z0 = cz - sz * 0.5f, z1 = cz + sz * 0.5f;
    FitAddVertex(mesh, x0, y0, z0, 0, 0); FitAddVertex(mesh, x1, y0, z0, 1, 0); FitAddVertex(mesh, x1, y1, z0, 1, 1); FitAddVertex(mesh, x0, y1, z0, 0, 1);
    FitAddVertex(mesh, x0, y0, z1, 0, 0); FitAddVertex(mesh, x1, y0, z1, 1, 0); FitAddVertex(mesh, x1, y1, z1, 1, 1); FitAddVertex(mesh, x0, y1, z1, 0, 1);
    FitQuad(mesh, base + 0, base + 1, base + 2, base + 3); FitQuad(mesh, base + 5, base + 4, base + 7, base + 6);
    FitQuad(mesh, base + 4, base + 0, base + 3, base + 7); FitQuad(mesh, base + 1, base + 5, base + 6, base + 2);
    FitQuad(mesh, base + 3, base + 2, base + 6, base + 7); FitQuad(mesh, base + 4, base + 5, base + 1, base + 0);
}

static void FitOrientedCylinder(MeshData& mesh, FitVec3 a, FitVec3 b, float rx, float rz, int segments) {
    segments = FitMaxInt(10, segments);
    FitVec3 axis = FitNormalize(FitSub(b, a));
    FitVec3 ref = std::fabs(axis.y) < 0.88f ? FitVec3{0, 1, 0} : FitVec3{1, 0, 0};
    FitVec3 right = FitNormalize(FitCross(ref, axis));
    FitVec3 forward = FitNormalize(FitCross(axis, right));
    int base = FitVertexCount(mesh);
    for (int ring = 0; ring < 2; ++ring) {
        FitVec3 center = ring == 0 ? a : b;
        for (int s = 0; s < segments; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(segments);
            float angle = t * 2.0f * FitPi;
            FitVec3 offset = FitAdd(FitMul(right, std::cos(angle) * rx), FitMul(forward, std::sin(angle) * rz));
            FitAddVertex(mesh, FitAdd(center, offset), t, static_cast<float>(ring));
        }
    }
    for (int s = 0; s < segments; ++s) {
        int n = (s + 1) % segments;
        FitQuad(mesh, base + s, base + n, base + segments + n, base + segments + s);
    }
}

static void FitCapsule(MeshData& mesh, FitVec3 a, FitVec3 b, float rx, float ry, float rz, int segments) {
    FitOrientedCylinder(mesh, a, b, rx, rz, segments);
    FitEllipsoid(mesh, a.x, a.y, a.z, rx, ry, rz, segments, 6);
    FitEllipsoid(mesh, b.x, b.y, b.z, rx, ry, rz, segments, 6);
}

static MeshData BuildFittedCharacterMesh(const AssetPlanResult& plan, const SilhouetteProfile& profile, const StructuredAssetOptions& options) {
    MeshData mesh;
    const float h = plan.targetHeight;
    const float baseW = FitClamp(plan.targetWidth, 0.74f, 1.42f);
    const float d = FitMax(0.52f, plan.targetDepth);
    const int seg = FitMaxInt(36, options.radialSegments + 10);
    const int limbSeg = FitMaxInt(20, seg / 2);

    const float headRow = FitMaxRowWidth(profile, 0.02f, 0.27f);
    const float shoulderRow = FitMaxRowWidth(profile, 0.28f, 0.45f);
    const float torsoRow = FitMaxRowWidth(profile, 0.42f, 0.64f);
    const float hipRow = FitMaxRowWidth(profile, 0.58f, 0.74f);
    const float legRow = FitMaxRowWidth(profile, 0.72f, 0.94f);

    const float centerHead = FitRowCenterAt(profile, 0.16f) * baseW * 0.30f;
    const float centerTorso = FitRowCenterAt(profile, 0.50f) * baseW * 0.24f;
    const float headW = baseW * FitClamp(headRow + 0.10f, 0.52f, 0.98f);
    const float shoulderW = baseW * FitClamp(shoulderRow + 0.12f, 0.52f, 0.96f);
    const float torsoW = baseW * FitClamp(torsoRow + 0.08f, 0.42f, 0.78f);
    const float hipW = baseW * FitClamp(hipRow + 0.08f, 0.40f, 0.78f);
    const float legSpread = baseW * FitClamp(legRow * 0.34f, 0.10f, 0.24f);
    const float massShift = FitClamp((0.50f - profile.massCenterY) * 0.10f, -0.035f, 0.035f);

    FitEllipsoid(mesh, centerTorso, h * (0.88f + massShift), 0.0f, torsoW * 0.47f, h * 0.215f, d * 0.285f, seg, 12);
    FitEllipsoid(mesh, centerTorso, h * 0.650f, 0.0f, hipW * 0.48f, h * 0.150f, d * 0.270f, seg, 9);
    FitEllipsoid(mesh, centerTorso, h * 1.070f, 0.0f, shoulderW * 0.52f, h * 0.060f, d * 0.245f, seg, 5);
    FitEllipsoid(mesh, centerTorso, h * 1.135f, 0.0f, baseW * 0.085f, h * 0.065f, d * 0.105f, limbSeg, 5);

    FitEllipsoid(mesh, centerHead, h * 1.360f, 0.0f, headW * 0.500f, h * 0.240f, d * 0.382f, seg, 15);
    FitEllipsoid(mesh, centerHead, h * 1.505f, -d * 0.030f, headW * 0.535f, h * 0.110f, d * 0.410f, seg, 7);
    for (float x : {-0.28f, -0.13f, 0.02f, 0.17f, 0.31f}) {
        FitEllipsoid(mesh, centerHead + x * headW, h * (1.405f - std::fabs(x) * 0.040f), d * 0.325f, headW * 0.085f, h * 0.100f, d * 0.052f, limbSeg, 5);
    }
    FitEllipsoid(mesh, centerHead - headW * 0.535f, h * 1.335f, -d * 0.030f, headW * 0.055f, h * 0.145f, d * 0.130f, limbSeg, 5);
    FitEllipsoid(mesh, centerHead + headW * 0.535f, h * 1.335f, -d * 0.030f, headW * 0.055f, h * 0.145f, d * 0.130f, limbSeg, 5);

    FitEllipsoid(mesh, centerTorso, h * 0.610f, d * 0.220f, hipW * 0.55f, h * 0.045f, d * 0.055f, limbSeg, 4);
    FitBox(mesh, centerTorso - hipW * 0.165f, h * 0.617f, d * 0.260f, hipW * 0.310f, h * 0.052f, d * 0.040f);
    FitBox(mesh, centerTorso + hipW * 0.165f, h * 0.617f, d * 0.260f, hipW * 0.310f, h * 0.052f, d * 0.040f);
    FitEllipsoid(mesh, centerTorso, h * 0.875f, d * 0.270f, torsoW * 0.260f, h * 0.024f, d * 0.035f, limbSeg, 4);

    for (float side : {-1.0f, 1.0f}) {
        const float shoulderX = centerTorso + side * shoulderW * 0.49f;
        const float elbowX = FitSideX(profile, 0.47f, side, baseW, baseW * 0.46f);
        const float handX = FitSideX(profile, 0.61f, side, baseW, baseW * 0.54f);
        FitVec3 shoulder{shoulderX, h * 1.020f, 0.0f};
        FitVec3 elbow{elbowX, h * 0.800f, d * 0.030f};
        FitVec3 hand{handX, h * 0.585f, d * 0.075f};
        FitCapsule(mesh, shoulder, elbow, baseW * 0.045f, h * 0.040f, d * 0.060f, limbSeg);
        FitCapsule(mesh, elbow, hand, baseW * 0.043f, h * 0.038f, d * 0.055f, limbSeg);
        FitEllipsoid(mesh, hand.x, hand.y, hand.z, baseW * 0.090f, h * 0.064f, d * 0.100f, limbSeg, 6);
        FitEllipsoid(mesh, shoulder.x, shoulder.y, d * 0.020f, baseW * 0.065f, h * 0.050f, d * 0.072f, limbSeg, 5);
    }

    for (float side : {-1.0f, 1.0f}) {
        const float hipX = centerTorso + side * legSpread;
        const float kneeX = FitClamp(FitSideX(profile, 0.78f, side, baseW, legSpread * 1.05f), side < 0.0f ? -baseW * 0.30f : 0.0f, side < 0.0f ? 0.0f : baseW * 0.30f);
        const float ankleX = FitClamp(FitSideX(profile, 0.91f, side, baseW, legSpread * 1.12f), side < 0.0f ? -baseW * 0.36f : 0.0f, side < 0.0f ? 0.0f : baseW * 0.36f);
        FitVec3 hip{hipX, h * 0.540f, 0.0f};
        FitVec3 knee{kneeX, h * 0.335f, 0.0f};
        FitVec3 ankle{ankleX, h * 0.150f, d * 0.015f};
        FitCapsule(mesh, hip, knee, baseW * 0.056f, h * 0.045f, d * 0.065f, limbSeg);
        FitCapsule(mesh, knee, ankle, baseW * 0.052f, h * 0.043f, d * 0.060f, limbSeg);
        FitEllipsoid(mesh, ankleX, h * 0.070f, d * 0.135f, baseW * 0.130f, h * 0.080f, d * 0.290f, limbSeg, 7);
    }

    FitEllipsoid(mesh, centerHead - headW * 0.150f, h * 1.390f, d * 0.390f, headW * 0.045f, h * 0.036f, d * 0.016f, 12, 5);
    FitEllipsoid(mesh, centerHead + headW * 0.150f, h * 1.390f, d * 0.390f, headW * 0.045f, h * 0.036f, d * 0.016f, 12, 5);
    FitEllipsoid(mesh, centerHead, h * 1.315f, d * 0.402f, headW * 0.024f, h * 0.020f, d * 0.012f, 8, 4);
    FitBox(mesh, centerHead, h * 1.260f, d * 0.410f, headW * 0.230f, h * 0.012f, d * 0.012f);

    return mesh;
}

static bool FitLooksLikeCharacter(const StructuredAssetBuildResult& base, const SilhouetteProfile& profile) {
    if (base.plan.assetType == GameAssetType::Character || base.plan.assetType == GameAssetType::Creature) return true;
    if (!profile.ok) return false;
    return profile.aspect > 0.72f && profile.aspect < 1.90f && profile.massCenterY > 0.25f && profile.massCenterY < 0.82f;
}

} // namespace

StructuredAssetBuildResult BuildImageFittedStructuredAssetMesh(
    const ImageRGBA& image,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& mask,
    const StructuredAssetOptions& options) {

    StructuredAssetBuildResult result = BuildStructuredAssetMesh(image, depth, mask, options);
    SilhouetteProfile profile = FitBuildProfile(image, mask);
    if (!result.ok || !FitLooksLikeCharacter(result, profile)) return result;

    MeshData fitted = BuildFittedCharacterMesh(result.plan, profile, options);
    if (fitted.positions.empty() || fitted.indices.empty()) return result;

    RecomputeNormals(fitted);
    if (options.normalizeOutput) NormalizeMesh(fitted, options.targetHeight);
    GameAssetValidationReport validation = ValidateGameAssetMesh(fitted);
    if (!validation.ok) return result;

    result.mesh = fitted;
    result.validation = validation;
    result.message = "Image-fitted structured character mesh generated.";
    result.warnings.push_back("Character proportions fitted from foreground mask horizontal profile and silhouette edge anchors.");
    result.warnings.push_back("This is still a procedural proxy mesh, not a learned neural reconstruction.");
    return result;
}

} // namespace make3d
