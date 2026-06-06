#include "Make3DStructuredAssetBuilder.h"

#include <cmath>
#include <cstdint>

namespace make3d {
namespace {

constexpr float SculptPi = 3.14159265358979323846f;

struct SculptVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

static int SculptVertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
static int SculptMaxInt(int a, int b) { return a > b ? a : b; }
static float SculptMax(float a, float b) { return a > b ? a : b; }
static float SculptMin(float a, float b) { return a < b ? a : b; }
static float SculptClamp(float v, float lo, float hi) { return SculptMax(lo, SculptMin(hi, v)); }
static SculptVec3 SculptAdd(SculptVec3 a, SculptVec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static SculptVec3 SculptSub(SculptVec3 a, SculptVec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static SculptVec3 SculptMul(SculptVec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float SculptDot(SculptVec3 a, SculptVec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static SculptVec3 SculptCross(SculptVec3 a, SculptVec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static float SculptLength(SculptVec3 v) { return std::sqrt(SculptMax(0.0f, SculptDot(v, v))); }
static SculptVec3 SculptNormalize(SculptVec3 v) {
    const float len = SculptLength(v);
    if (len <= 1.0e-6f) return {0.0f, 1.0f, 0.0f};
    return SculptMul(v, 1.0f / len);
}

static void SculptAddVertex(MeshData& mesh, float x, float y, float z, float u, float v) {
    mesh.positions.push_back(x);
    mesh.positions.push_back(y);
    mesh.positions.push_back(z);
    mesh.normals.push_back(0.0f);
    mesh.normals.push_back(1.0f);
    mesh.normals.push_back(0.0f);
    mesh.uvs.push_back(u);
    mesh.uvs.push_back(v);
}

static void SculptAddVertex(MeshData& mesh, SculptVec3 p, float u, float v) {
    SculptAddVertex(mesh, p.x, p.y, p.z, u, v);
}

static void SculptTri(MeshData& mesh, int a, int b, int c) {
    if (a < 0 || b < 0 || c < 0 || a == b || b == c || c == a) return;
    mesh.indices.push_back(static_cast<std::uint32_t>(a));
    mesh.indices.push_back(static_cast<std::uint32_t>(b));
    mesh.indices.push_back(static_cast<std::uint32_t>(c));
}

static void SculptQuad(MeshData& mesh, int a, int b, int c, int d) {
    SculptTri(mesh, a, b, c);
    SculptTri(mesh, a, c, d);
}

static void SculptBox(MeshData& mesh, float cx, float cy, float cz, float sx, float sy, float sz) {
    const int base = SculptVertexCount(mesh);
    const float x0 = cx - sx * 0.5f, x1 = cx + sx * 0.5f;
    const float y0 = cy - sy * 0.5f, y1 = cy + sy * 0.5f;
    const float z0 = cz - sz * 0.5f, z1 = cz + sz * 0.5f;
    SculptAddVertex(mesh, x0, y0, z0, 0, 0); SculptAddVertex(mesh, x1, y0, z0, 1, 0); SculptAddVertex(mesh, x1, y1, z0, 1, 1); SculptAddVertex(mesh, x0, y1, z0, 0, 1);
    SculptAddVertex(mesh, x0, y0, z1, 0, 0); SculptAddVertex(mesh, x1, y0, z1, 1, 0); SculptAddVertex(mesh, x1, y1, z1, 1, 1); SculptAddVertex(mesh, x0, y1, z1, 0, 1);
    SculptQuad(mesh, base + 0, base + 1, base + 2, base + 3);
    SculptQuad(mesh, base + 5, base + 4, base + 7, base + 6);
    SculptQuad(mesh, base + 4, base + 0, base + 3, base + 7);
    SculptQuad(mesh, base + 1, base + 5, base + 6, base + 2);
    SculptQuad(mesh, base + 3, base + 2, base + 6, base + 7);
    SculptQuad(mesh, base + 4, base + 5, base + 1, base + 0);
}

static void SculptEllipsoid(MeshData& mesh, float cx, float cy, float cz, float rx, float ry, float rz, int segments, int rings) {
    segments = SculptMaxInt(12, segments);
    rings = SculptMaxInt(6, rings);
    const int base = SculptVertexCount(mesh);
    for (int y = 0; y <= rings; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(rings);
        const float theta = -SculptPi * 0.5f + v * SculptPi;
        const float sy = std::sin(theta);
        const float cr = std::cos(theta);
        for (int x = 0; x <= segments; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(segments);
            const float a = u * SculptPi * 2.0f;
            SculptAddVertex(mesh, cx + std::cos(a) * cr * rx, cy + sy * ry, cz + std::sin(a) * cr * rz, u, v);
        }
    }
    const int stride = segments + 1;
    for (int y = 0; y < rings; ++y) {
        for (int x = 0; x < segments; ++x) {
            const int a = base + y * stride + x;
            SculptQuad(mesh, a, a + 1, a + stride + 1, a + stride);
        }
    }
}

static void SculptCylinderBetween(MeshData& mesh, SculptVec3 a, SculptVec3 b, float rx, float rz, int segments) {
    segments = SculptMaxInt(12, segments);
    const SculptVec3 axis = SculptNormalize(SculptSub(b, a));
    SculptVec3 ref = std::fabs(axis.y) < 0.88f ? SculptVec3{0.0f, 1.0f, 0.0f} : SculptVec3{1.0f, 0.0f, 0.0f};
    const SculptVec3 right = SculptNormalize(SculptCross(ref, axis));
    const SculptVec3 forward = SculptNormalize(SculptCross(axis, right));
    const int base = SculptVertexCount(mesh);
    for (int ring = 0; ring < 2; ++ring) {
        const SculptVec3 center = ring == 0 ? a : b;
        for (int s = 0; s < segments; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(segments);
            const float ang = t * SculptPi * 2.0f;
            SculptVec3 offset = SculptAdd(SculptMul(right, std::cos(ang) * rx), SculptMul(forward, std::sin(ang) * rz));
            SculptAddVertex(mesh, SculptAdd(center, offset), t, static_cast<float>(ring));
        }
    }
    for (int s = 0; s < segments; ++s) {
        const int n = (s + 1) % segments;
        SculptQuad(mesh, base + s, base + n, base + segments + n, base + segments + s);
    }
}

static void SculptCapsule(MeshData& mesh, SculptVec3 a, SculptVec3 b, float rx, float ry, float rz, int segments) {
    SculptCylinderBetween(mesh, a, b, rx, rz, segments);
    SculptEllipsoid(mesh, a.x, a.y, a.z, rx, ry, rz, segments, 6);
    SculptEllipsoid(mesh, b.x, b.y, b.z, rx, ry, rz, segments, 6);
}

static void SculptApplyProjectionUVs(MeshData& mesh) {
    const int count = SculptVertexCount(mesh);
    if (count <= 0) return;
    float minX = mesh.positions[0], maxX = mesh.positions[0];
    float minY = mesh.positions[1], maxY = mesh.positions[1];
    for (int i = 0; i < count; ++i) {
        const size_t p = static_cast<size_t>(i) * 3;
        minX = SculptMin(minX, mesh.positions[p + 0]);
        maxX = SculptMax(maxX, mesh.positions[p + 0]);
        minY = SculptMin(minY, mesh.positions[p + 1]);
        maxY = SculptMax(maxY, mesh.positions[p + 1]);
    }
    const float rx = SculptMax(0.0001f, maxX - minX);
    const float ry = SculptMax(0.0001f, maxY - minY);
    mesh.uvs.assign(static_cast<size_t>(count) * 2, 0.0f);
    for (int i = 0; i < count; ++i) {
        const size_t p = static_cast<size_t>(i) * 3;
        mesh.uvs[static_cast<size_t>(i) * 2 + 0] = SculptClamp((mesh.positions[p + 0] - minX) / rx, 0.0f, 1.0f);
        mesh.uvs[static_cast<size_t>(i) * 2 + 1] = SculptClamp((mesh.positions[p + 1] - minY) / ry, 0.0f, 1.0f);
    }
}

static MeshData BuildSculptedCharacterMesh(const AssetPlanResult& plan, const StructuredAssetOptions& options) {
    MeshData mesh;
    const float h = SculptMax(0.25f, options.targetHeight);
    const float w = SculptClamp(plan.targetWidth, 0.78f, 1.28f);
    const float d = SculptMax(0.64f, plan.targetDepth);
    const int seg = SculptMaxInt(40, options.radialSegments + 12);
    const int detailSeg = SculptMaxInt(22, seg / 2);

    // A stylized production-proxy body should read as a single sculpted doll, not separate rods.
    // Layer large overlapping volumes first, then add raised clothing, hair, face and limb details.
    SculptEllipsoid(mesh, 0.0f, h * 0.78f, 0.0f, w * 0.295f, h * 0.300f, d * 0.315f, seg, 14);   // rounded tunic body
    SculptEllipsoid(mesh, 0.0f, h * 0.93f, d * 0.030f, w * 0.285f, h * 0.235f, d * 0.330f, seg, 12); // chest volume
    SculptEllipsoid(mesh, 0.0f, h * 0.56f, -d * 0.010f, w * 0.300f, h * 0.155f, d * 0.300f, seg, 9); // pelvis
    SculptEllipsoid(mesh, 0.0f, h * 1.085f, 0.0f, w * 0.335f, h * 0.075f, d * 0.250f, seg, 6); // soft shoulder bridge
    SculptEllipsoid(mesh, 0.0f, h * 1.155f, 0.0f, w * 0.115f, h * 0.075f, d * 0.115f, detailSeg, 5); // neck

    // Clothing as raised sculpted forms.
    SculptEllipsoid(mesh, 0.0f, h * 0.630f, d * 0.270f, w * 0.330f, h * 0.055f, d * 0.060f, detailSeg, 5); // belt/rim
    SculptBox(mesh, -w * 0.145f, h * 0.620f, d * 0.320f, w * 0.245f, h * 0.065f, d * 0.045f);
    SculptBox(mesh,  w * 0.145f, h * 0.620f, d * 0.320f, w * 0.245f, h * 0.065f, d * 0.045f);
    SculptEllipsoid(mesh, 0.0f, h * 0.900f, d * 0.315f, w * 0.165f, h * 0.030f, d * 0.035f, detailSeg, 4); // collar
    SculptEllipsoid(mesh, -w * 0.125f, h * 0.850f, d * 0.315f, w * 0.080f, h * 0.120f, d * 0.038f, detailSeg, 6);
    SculptEllipsoid(mesh,  w * 0.125f, h * 0.850f, d * 0.315f, w * 0.080f, h * 0.120f, d * 0.038f, detailSeg, 6);

    // Head and face: larger and more sculpted than the old sphere-like head.
    SculptEllipsoid(mesh, 0.0f, h * 1.365f, d * 0.010f, w * 0.365f, h * 0.245f, d * 0.390f, seg, 18);
    SculptEllipsoid(mesh, 0.0f, h * 1.325f, d * 0.170f, w * 0.305f, h * 0.185f, d * 0.250f, seg, 12); // face cheek plane
    SculptEllipsoid(mesh, -w * 0.150f, h * 1.315f, d * 0.215f, w * 0.090f, h * 0.080f, d * 0.050f, detailSeg, 5);
    SculptEllipsoid(mesh,  w * 0.150f, h * 1.315f, d * 0.215f, w * 0.090f, h * 0.080f, d * 0.050f, detailSeg, 5);

    // Hair cap, back hair and front locks. These are intentionally big so the silhouette reads as character hair.
    SculptEllipsoid(mesh, 0.0f, h * 1.500f, -d * 0.050f, w * 0.390f, h * 0.120f, d * 0.430f, seg, 8);
    SculptEllipsoid(mesh, 0.0f, h * 1.405f, -d * 0.270f, w * 0.350f, h * 0.190f, d * 0.160f, seg, 8);
    for (float t : {-0.35f, -0.20f, -0.05f, 0.11f, 0.27f}) {
        const float absT = std::fabs(t);
        SculptEllipsoid(mesh, t * w, h * (1.420f - absT * 0.030f), d * 0.360f, w * (0.070f + absT * 0.020f), h * 0.125f, d * 0.055f, detailSeg, 6);
    }
    SculptEllipsoid(mesh, -w * 0.380f, h * 1.340f, -d * 0.020f, w * 0.060f, h * 0.170f, d * 0.145f, detailSeg, 6);
    SculptEllipsoid(mesh,  w * 0.380f, h * 1.340f, -d * 0.020f, w * 0.060f, h * 0.170f, d * 0.145f, detailSeg, 6);

    // Arms: shorter, thicker, and integrated into the body so they no longer read as sticks.
    for (float side : {-1.0f, 1.0f}) {
        SculptVec3 shoulder{side * w * 0.305f, h * 1.015f, d * 0.015f};
        SculptVec3 elbow{side * w * 0.375f, h * 0.805f, d * 0.070f};
        SculptVec3 wrist{side * w * 0.405f, h * 0.630f, d * 0.090f};
        SculptEllipsoid(mesh, shoulder.x, shoulder.y, shoulder.z, w * 0.085f, h * 0.080f, d * 0.095f, detailSeg, 6);
        SculptCapsule(mesh, shoulder, elbow, w * 0.062f, h * 0.055f, d * 0.072f, detailSeg);
        SculptCapsule(mesh, elbow, wrist, w * 0.058f, h * 0.052f, d * 0.068f, detailSeg);
        SculptEllipsoid(mesh, wrist.x, wrist.y, wrist.z + d * 0.015f, w * 0.105f, h * 0.075f, d * 0.105f, detailSeg, 7);
        SculptEllipsoid(mesh, wrist.x - side * w * 0.020f, wrist.y - h * 0.020f, wrist.z + d * 0.070f, w * 0.070f, h * 0.045f, d * 0.045f, detailSeg, 4); // mitten front
        SculptEllipsoid(mesh, side * w * 0.315f, h * 0.900f, d * 0.150f, w * 0.040f, h * 0.120f, d * 0.040f, detailSeg, 5); // sleeve front seam
    }

    // Legs: thick short boots with knees and cuffs, not two thin rods.
    for (float side : {-1.0f, 1.0f}) {
        SculptVec3 hip{side * w * 0.135f, h * 0.520f, 0.0f};
        SculptVec3 knee{side * w * 0.145f, h * 0.335f, d * 0.005f};
        SculptVec3 ankle{side * w * 0.155f, h * 0.165f, d * 0.035f};
        SculptEllipsoid(mesh, hip.x, hip.y, hip.z, w * 0.080f, h * 0.070f, d * 0.085f, detailSeg, 5);
        SculptCapsule(mesh, hip, knee, w * 0.066f, h * 0.055f, d * 0.075f, detailSeg);
        SculptCapsule(mesh, knee, ankle, w * 0.064f, h * 0.055f, d * 0.075f, detailSeg);
        SculptEllipsoid(mesh, knee.x, knee.y, knee.z + d * 0.055f, w * 0.075f, h * 0.040f, d * 0.052f, detailSeg, 4);
        SculptEllipsoid(mesh, ankle.x, h * 0.085f, d * 0.165f, w * 0.150f, h * 0.085f, d * 0.255f, detailSeg, 8);
        SculptEllipsoid(mesh, ankle.x + side * w * 0.035f, h * 0.060f, d * 0.235f, w * 0.110f, h * 0.050f, d * 0.130f, detailSeg, 5);
    }

    // Raised facial features for clay preview readability.
    SculptEllipsoid(mesh, -w * 0.115f, h * 1.405f, d * 0.405f, w * 0.040f, h * 0.045f, d * 0.018f, 12, 5);
    SculptEllipsoid(mesh,  w * 0.115f, h * 1.405f, d * 0.405f, w * 0.040f, h * 0.045f, d * 0.018f, 12, 5);
    SculptEllipsoid(mesh, 0.0f, h * 1.330f, d * 0.420f, w * 0.025f, h * 0.020f, d * 0.014f, 10, 4);
    SculptBox(mesh, 0.0f, h * 1.270f, d * 0.425f, w * 0.165f, h * 0.014f, d * 0.014f);

    SculptApplyProjectionUVs(mesh);
    return mesh;
}

} // namespace

StructuredAssetBuildResult BuildSculptedStructuredAssetMesh(
    const ImageRGBA& image,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& mask,
    const StructuredAssetOptions& options) {

    StructuredAssetBuildResult result = BuildStructuredAssetMesh(image, depth, mask, options);
    if (!result.ok) return result;
    if (result.plan.assetType != GameAssetType::Character && result.plan.assetType != GameAssetType::Creature) return result;

    MeshData sculpted = BuildSculptedCharacterMesh(result.plan, options);
    if (sculpted.positions.empty() || sculpted.indices.empty()) return result;

    RecomputeNormals(sculpted);
    if (options.normalizeOutput) NormalizeMesh(sculpted, SculptMax(0.25f, options.targetHeight));
    GameAssetValidationReport validation = ValidateGameAssetMesh(sculpted);
    if (!validation.ok) return result;

    result.mesh = sculpted;
    result.validation = validation;
    result.message = "Sculpted stable character mesh generated.";
    result.warnings.push_back("Stable GUI uses a sculpted character builder: thicker limbs, layered hair, raised face, clothing, cuffs, hands and boots.");
    result.warnings.push_back("This is still a procedural proxy; exact production-quality reconstruction from one image requires a learned 3D prior or artist-authored model data.");
    return result;
}

} // namespace make3d
