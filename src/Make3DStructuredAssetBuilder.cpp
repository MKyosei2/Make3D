#include "Make3DStructuredAssetBuilder.h"

#include <cmath>
#include <cstdint>
#include <sstream>

namespace make3d {
namespace {

constexpr float Pi = 3.14159265358979323846f;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

static int VertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
static int MaxInt(int a, int b) { return a > b ? a : b; }
static float MaxFloat(float a, float b) { return a > b ? a : b; }
static float MinFloat(float a, float b) { return a < b ? a : b; }
static float ClampFloat(float value, float lo, float hi) { return MaxFloat(lo, MinFloat(hi, value)); }
static float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 Mul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static float Length(Vec3 v) { return std::sqrt(MaxFloat(0.0f, Dot(v, v))); }
static Vec3 Normalize(Vec3 v) {
    const float len = Length(v);
    if (len <= 1.0e-6f) return {0.0f, 1.0f, 0.0f};
    return Mul(v, 1.0f / len);
}

static std::string EscapeJson(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '\\') o << "\\\\";
        else if (c == '"') o << "\\\"";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else o << c;
    }
    return o.str();
}

static void AddVertex(MeshData& mesh, float x, float y, float z, float u, float v) {
    mesh.positions.push_back(x); mesh.positions.push_back(y); mesh.positions.push_back(z);
    mesh.normals.push_back(0.0f); mesh.normals.push_back(1.0f); mesh.normals.push_back(0.0f);
    mesh.uvs.push_back(u); mesh.uvs.push_back(v);
}

static void AddVertex(MeshData& mesh, Vec3 p, float u, float v) { AddVertex(mesh, p.x, p.y, p.z, u, v); }

static void AddTri(MeshData& mesh, int a, int b, int c) {
    if (a < 0 || b < 0 || c < 0 || a == b || b == c || c == a) return;
    mesh.indices.push_back(static_cast<std::uint32_t>(a));
    mesh.indices.push_back(static_cast<std::uint32_t>(b));
    mesh.indices.push_back(static_cast<std::uint32_t>(c));
}

static void AddQuad(MeshData& mesh, int a, int b, int c, int d) {
    AddTri(mesh, a, b, c);
    AddTri(mesh, a, c, d);
}

static void AddBox(MeshData& mesh, float cx, float cy, float cz, float sx, float sy, float sz) {
    const int base = VertexCount(mesh);
    const float x0 = cx - sx * 0.5f, x1 = cx + sx * 0.5f;
    const float y0 = cy - sy * 0.5f, y1 = cy + sy * 0.5f;
    const float z0 = cz - sz * 0.5f, z1 = cz + sz * 0.5f;
    AddVertex(mesh, x0, y0, z0, 0.0f, 0.0f); AddVertex(mesh, x1, y0, z0, 1.0f, 0.0f); AddVertex(mesh, x1, y1, z0, 1.0f, 1.0f); AddVertex(mesh, x0, y1, z0, 0.0f, 1.0f);
    AddVertex(mesh, x0, y0, z1, 0.0f, 0.0f); AddVertex(mesh, x1, y0, z1, 1.0f, 0.0f); AddVertex(mesh, x1, y1, z1, 1.0f, 1.0f); AddVertex(mesh, x0, y1, z1, 0.0f, 1.0f);
    AddQuad(mesh, base + 0, base + 1, base + 2, base + 3);
    AddQuad(mesh, base + 5, base + 4, base + 7, base + 6);
    AddQuad(mesh, base + 4, base + 0, base + 3, base + 7);
    AddQuad(mesh, base + 1, base + 5, base + 6, base + 2);
    AddQuad(mesh, base + 3, base + 2, base + 6, base + 7);
    AddQuad(mesh, base + 4, base + 5, base + 1, base + 0);
}

static void AddEllipsoid(MeshData& mesh, float cx, float cy, float cz, float rx, float ry, float rz, int segments, int rings) {
    segments = MaxInt(10, segments);
    rings = MaxInt(5, rings);
    const int base = VertexCount(mesh);
    for (int y = 0; y <= rings; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(rings);
        const float theta = -Pi * 0.5f + v * Pi;
        const float sy = std::sin(theta);
        const float cr = std::cos(theta);
        for (int x = 0; x <= segments; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(segments);
            const float a = u * Pi * 2.0f;
            AddVertex(mesh, cx + std::cos(a) * cr * rx, cy + sy * ry, cz + std::sin(a) * cr * rz, u, v);
        }
    }
    const int stride = segments + 1;
    for (int y = 0; y < rings; ++y) {
        for (int x = 0; x < segments; ++x) {
            const int a = base + y * stride + x;
            const int b = a + 1;
            const int c = a + stride + 1;
            const int d = a + stride;
            AddQuad(mesh, a, b, c, d);
        }
    }
}

static void AddCylinder(MeshData& mesh, float cx, float cy0, float cy1, float cz, float rx, float rz, int segments) {
    segments = MaxInt(8, segments);
    const int base = VertexCount(mesh);
    for (int y = 0; y < 2; ++y) {
        const float cy = y == 0 ? cy0 : cy1;
        for (int s = 0; s < segments; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(segments);
            const float a = t * 2.0f * Pi;
            AddVertex(mesh, cx + std::cos(a) * rx, cy, cz + std::sin(a) * rz, t, static_cast<float>(y));
        }
    }
    for (int s = 0; s < segments; ++s) {
        const int n = (s + 1) % segments;
        AddQuad(mesh, base + s, base + n, base + segments + n, base + segments + s);
    }
    const int bottom = VertexCount(mesh); AddVertex(mesh, cx, cy0, cz, 0.5f, 0.5f);
    const int top = VertexCount(mesh); AddVertex(mesh, cx, cy1, cz, 0.5f, 0.5f);
    for (int s = 0; s < segments; ++s) {
        const int n = (s + 1) % segments;
        AddTri(mesh, bottom, base + n, base + s);
        AddTri(mesh, top, base + segments + s, base + segments + n);
    }
}

static void AddOrientedCylinder(MeshData& mesh, Vec3 a, Vec3 b, float radiusX, float radiusZ, int segments) {
    segments = MaxInt(8, segments);
    const Vec3 axis = Normalize(Sub(b, a));
    Vec3 ref = std::fabs(axis.y) < 0.88f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
    const Vec3 right = Normalize(Cross(ref, axis));
    const Vec3 forward = Normalize(Cross(axis, right));
    const int base = VertexCount(mesh);
    for (int ring = 0; ring < 2; ++ring) {
        const Vec3 center = ring == 0 ? a : b;
        for (int s = 0; s < segments; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(segments);
            const float angle = t * 2.0f * Pi;
            Vec3 offset = Add(Mul(right, std::cos(angle) * radiusX), Mul(forward, std::sin(angle) * radiusZ));
            AddVertex(mesh, Add(center, offset), t, static_cast<float>(ring));
        }
    }
    for (int s = 0; s < segments; ++s) {
        const int n = (s + 1) % segments;
        AddQuad(mesh, base + s, base + n, base + segments + n, base + segments + s);
    }
    const int capA = VertexCount(mesh); AddVertex(mesh, a, 0.5f, 0.0f);
    const int capB = VertexCount(mesh); AddVertex(mesh, b, 0.5f, 1.0f);
    for (int s = 0; s < segments; ++s) {
        const int n = (s + 1) % segments;
        AddTri(mesh, capA, base + n, base + s);
        AddTri(mesh, capB, base + segments + s, base + segments + n);
    }
}

static void AddCapsuleSegment(MeshData& mesh, Vec3 a, Vec3 b, float radiusX, float radiusY, float radiusZ, int segments) {
    AddOrientedCylinder(mesh, a, b, radiusX, radiusZ, segments);
    AddEllipsoid(mesh, a.x, a.y, a.z, radiusX, radiusY, radiusZ, segments, 6);
    AddEllipsoid(mesh, b.x, b.y, b.z, radiusX, radiusY, radiusZ, segments, 6);
}

static void AddFoot(MeshData& mesh, float x, float y, float z, float sx, float sy, float sz, int segments) {
    AddEllipsoid(mesh, x, y, z + sz * 0.18f, sx * 0.55f, sy * 0.38f, sz * 0.58f, segments, 6);
}

static void BuildCharacterStructured(MeshData& mesh, const AssetPlanResult& plan, const StructuredAssetOptions& options) {
    const float h = plan.targetHeight;
    const float w = ClampFloat(plan.targetWidth, 0.76f, 1.34f);
    const float d = MaxFloat(0.50f, plan.targetDepth);
    const int seg = MaxInt(28, options.radialSegments);
    const int limbSeg = MaxInt(16, seg / 2);

    // Body volumes: chibi/proxy proportions, rounded instead of raw boxes.
    AddEllipsoid(mesh, 0.0f, h * 0.88f, 0.0f, w * 0.255f, h * 0.210f, d * 0.265f, seg, 10); // upper torso
    AddEllipsoid(mesh, 0.0f, h * 0.66f, 0.0f, w * 0.275f, h * 0.145f, d * 0.255f, seg, 8);  // pelvis / lower body
    AddEllipsoid(mesh, 0.0f, h * 1.07f, 0.0f, w * 0.310f, h * 0.065f, d * 0.235f, seg, 5); // shoulders
    AddEllipsoid(mesh, 0.0f, h * 1.135f, 0.0f, w * 0.115f, h * 0.070f, d * 0.115f, limbSeg, 5); // neck

    // Head, back-of-head, hair cap, and bangs.
    AddEllipsoid(mesh, 0.0f, h * 1.36f, 0.0f, w * 0.330f, h * 0.235f, d * 0.360f, seg, 14);
    AddEllipsoid(mesh, 0.0f, h * 1.49f, -d * 0.030f, w * 0.350f, h * 0.105f, d * 0.385f, seg, 7);
    AddEllipsoid(mesh, -w * 0.140f, h * 1.405f, d * 0.300f, w * 0.075f, h * 0.085f, d * 0.040f, limbSeg, 5);
    AddEllipsoid(mesh,  0.000f, h * 1.400f, d * 0.315f, w * 0.085f, h * 0.095f, d * 0.045f, limbSeg, 5);
    AddEllipsoid(mesh,  w * 0.140f, h * 1.405f, d * 0.300f, w * 0.075f, h * 0.085f, d * 0.040f, limbSeg, 5);
    AddEllipsoid(mesh, -w * 0.325f, h * 1.330f, -d * 0.030f, w * 0.045f, h * 0.135f, d * 0.115f, limbSeg, 5);
    AddEllipsoid(mesh,  w * 0.325f, h * 1.330f, -d * 0.030f, w * 0.045f, h * 0.135f, d * 0.115f, limbSeg, 5);

    // Clothes / belt / front panel.
    AddEllipsoid(mesh, 0.0f, h * 0.610f, d * 0.205f, w * 0.315f, h * 0.045f, d * 0.050f, limbSeg, 4);
    AddBox(mesh, -w * 0.145f, h * 0.615f, d * 0.240f, w * 0.250f, h * 0.050f, d * 0.035f);
    AddBox(mesh,  w * 0.145f, h * 0.615f, d * 0.240f, w * 0.250f, h * 0.050f, d * 0.035f);
    AddEllipsoid(mesh, 0.0f, h * 0.875f, d * 0.245f, w * 0.190f, h * 0.025f, d * 0.030f, limbSeg, 4);

    // Arms as angled capsules, not vertical sticks.
    for (float side : {-1.0f, 1.0f}) {
        Vec3 shoulder{side * w * 0.295f, h * 1.020f, 0.0f};
        Vec3 elbow{side * w * 0.410f, h * 0.785f, d * 0.020f};
        Vec3 hand{side * w * 0.455f, h * 0.585f, d * 0.060f};
        AddCapsuleSegment(mesh, shoulder, elbow, w * 0.045f, h * 0.040f, d * 0.058f, limbSeg);
        AddCapsuleSegment(mesh, elbow, hand, w * 0.043f, h * 0.038f, d * 0.054f, limbSeg);
        AddEllipsoid(mesh, hand.x, hand.y, hand.z, w * 0.083f, h * 0.060f, d * 0.090f, limbSeg, 6);
    }

    // Legs and shoes with visible feet instead of thin rods.
    for (float side : {-1.0f, 1.0f}) {
        Vec3 hip{side * w * 0.120f, h * 0.540f, 0.0f};
        Vec3 knee{side * w * 0.135f, h * 0.345f, 0.0f};
        Vec3 ankle{side * w * 0.140f, h * 0.150f, d * 0.010f};
        AddCapsuleSegment(mesh, hip, knee, w * 0.052f, h * 0.045f, d * 0.065f, limbSeg);
        AddCapsuleSegment(mesh, knee, ankle, w * 0.049f, h * 0.043f, d * 0.060f, limbSeg);
        AddFoot(mesh, side * w * 0.145f, h * 0.070f, d * 0.115f, w * 0.215f, h * 0.075f, d * 0.260f, limbSeg);
    }

    // Face details are raised geometry so the clay preview reads as a model, not a silhouette.
    AddEllipsoid(mesh, -w * 0.095f, h * 1.390f, d * 0.355f, w * 0.030f, h * 0.034f, d * 0.014f, 10, 5);
    AddEllipsoid(mesh,  w * 0.095f, h * 1.390f, d * 0.355f, w * 0.030f, h * 0.034f, d * 0.014f, 10, 5);
    AddEllipsoid(mesh, 0.0f, h * 1.320f, d * 0.365f, w * 0.020f, h * 0.020f, d * 0.012f, 8, 4);
    AddBox(mesh, 0.0f, h * 1.265f, d * 0.370f, w * 0.155f, h * 0.012f, d * 0.012f);
}

static void BuildBuildingStructured(MeshData& mesh, const AssetPlanResult& plan, const StructuredAssetOptions& options) {
    const float h = plan.targetHeight;
    const float w = ClampFloat(plan.targetWidth, 0.55f, 2.80f);
    const float d = MaxFloat(0.65f, plan.targetDepth);
    AddBox(mesh, 0.0f, h * 0.48f, 0.0f, w, h * 0.92f, d);
    AddBox(mesh, 0.0f, h * 0.98f, 0.0f, w * 1.08f, h * 0.14f, d * 1.08f);
    AddBox(mesh, 0.0f, h * 0.10f, d * 0.52f, w * 0.16f, h * 0.20f, d * 0.08f);
    if (options.addDetailParts) {
        const int cols = MaxInt(2, static_cast<int>(w * 3.0f));
        const int rows = MaxInt(2, static_cast<int>(h * 2.2f));
        for (int r = 0; r < rows; ++r) for (int c = 0; c < cols; ++c) {
            const float x = -w * 0.35f + (c + 0.5f) * (w * 0.70f / static_cast<float>(cols));
            const float y = h * 0.25f + (r + 0.5f) * (h * 0.55f / static_cast<float>(rows));
            AddBox(mesh, x, y, d * 0.54f, w * 0.08f, h * 0.07f, d * 0.045f);
        }
    }
}

static void BuildFurnitureStructured(MeshData& mesh, const AssetPlanResult& plan, const StructuredAssetOptions& options) {
    const float h = plan.targetHeight;
    const float w = ClampFloat(plan.targetWidth, 0.70f, 2.60f);
    const float d = MaxFloat(0.55f, plan.targetDepth);
    const int seg = MaxInt(10, options.radialSegments / 2);
    AddBox(mesh, 0.0f, h * 0.46f, 0.0f, w, h * 0.12f, d);
    AddBox(mesh, 0.0f, h * 0.78f, -d * 0.42f, w * 0.92f, h * 0.48f, d * 0.12f);
    for (float sx : {-1.0f, 1.0f}) for (float sz : {-1.0f, 1.0f}) {
        AddCylinder(mesh, sx * w * 0.38f, h * 0.06f, h * 0.44f, sz * d * 0.34f, w * 0.025f, d * 0.025f, seg);
    }
    if (options.addDetailParts) {
        AddBox(mesh, -w * 0.55f, h * 0.58f, 0.0f, w * 0.08f, h * 0.28f, d * 0.90f);
        AddBox(mesh,  w * 0.55f, h * 0.58f, 0.0f, w * 0.08f, h * 0.28f, d * 0.90f);
    }
}

static void BuildVehicleStructured(MeshData& mesh, const AssetPlanResult& plan, const StructuredAssetOptions& options) {
    const float h = plan.targetHeight;
    const float w = MaxFloat(1.50f, plan.targetWidth);
    const float d = MaxFloat(0.70f, plan.targetDepth);
    const int seg = MaxInt(16, options.radialSegments);
    AddBox(mesh, 0.0f, h * 0.34f, 0.0f, w, h * 0.36f, d);
    AddBox(mesh, w * 0.08f, h * 0.62f, 0.0f, w * 0.50f, h * 0.30f, d * 0.82f);
    for (float sx : {-1.0f, 1.0f}) for (float sz : {-1.0f, 1.0f}) {
        AddCylinder(mesh, sx * w * 0.33f, h * 0.06f, h * 0.20f, sz * d * 0.48f, h * 0.12f, h * 0.12f, seg);
    }
}

static void BuildGenericStructured(MeshData& mesh, const AssetPlanResult& plan, const StructuredAssetOptions& options) {
    const float h = plan.targetHeight;
    const float w = ClampFloat(plan.targetWidth, 0.45f, 2.40f);
    const float d = MaxFloat(0.42f, plan.targetDepth);
    AddBox(mesh, 0.0f, h * 0.50f, 0.0f, w, h, d);
    if (options.addDetailParts) {
        AddBox(mesh, 0.0f, h * 0.62f, d * 0.52f, w * 0.82f, h * 0.08f, d * 0.08f);
        AddBox(mesh, 0.0f, h * 0.34f, d * 0.52f, w * 0.68f, h * 0.06f, d * 0.08f);
    }
}

static void BuildFromPlanParts(MeshData& mesh, const AssetPlanResult& plan, const StructuredAssetOptions& options) {
    switch (plan.assetType) {
        case GameAssetType::Character:
        case GameAssetType::Creature:
            BuildCharacterStructured(mesh, plan, options);
            break;
        case GameAssetType::Building:
        case GameAssetType::ArchitecturalPart:
            BuildBuildingStructured(mesh, plan, options);
            break;
        case GameAssetType::Furniture:
            BuildFurnitureStructured(mesh, plan, options);
            break;
        case GameAssetType::Vehicle:
            BuildVehicleStructured(mesh, plan, options);
            break;
        default:
            BuildGenericStructured(mesh, plan, options);
            break;
    }
}

struct AutoTypeDecision {
    GameAssetType type = GameAssetType::GenericProp;
    std::string reason;
    float characterScore = 0.0f;
    float buildingScore = 0.0f;
    float furnitureScore = 0.0f;
    float vehicleScore = 0.0f;
    float genericScore = 0.0f;
};

static void AddIf(float& score, bool condition, float value) {
    if (condition) score += value;
}

static AutoTypeDecision ResolveStructuredAssetType(const AssetAnalysisResult& analysis) {
    AutoTypeDecision d;
    const ShapeDescriptorReport& s = analysis.shape;
    const GameAssetType base = analysis.classification.assetType;

    d.characterScore = 0.10f;
    d.buildingScore = 0.05f;
    d.furnitureScore = 0.05f;
    d.vehicleScore = 0.05f;
    d.genericScore = 0.18f;

    AddIf(d.characterScore, base == GameAssetType::Character || base == GameAssetType::Creature, 0.35f);
    AddIf(d.buildingScore, base == GameAssetType::Building || base == GameAssetType::ArchitecturalPart, 0.35f);
    AddIf(d.furnitureScore, base == GameAssetType::Furniture, 0.22f);
    AddIf(d.vehicleScore, base == GameAssetType::Vehicle, 0.32f);
    AddIf(d.genericScore, base == GameAssetType::GenericProp || base == GameAssetType::ComplexObject, 0.12f);

    const bool upright = s.aspectRatio > 0.72f && s.aspectRatio < 1.95f;
    const bool tall = s.aspectRatio >= 1.20f && s.aspectRatio < 2.35f;
    const bool compactHumanoid = s.aspectRatio > 0.78f && s.aspectRatio < 1.65f && s.symmetryX > 0.34f;
    const bool centeredMass = s.massCenterY > 0.25f && s.massCenterY < 0.82f;
    const bool roundedOrArticulated = s.straightEdgeScore < 0.52f || s.rowStability < 0.50f || s.columnStability < 0.50f || s.contourComplexity > 0.30f;
    const bool strongBox = s.rectangularity > 0.72f && s.rowStability > 0.55f && s.columnStability > 0.50f && s.straightEdgeScore > 0.28f;
    const bool wideLow = s.aspectRatio < 0.68f && s.rectangularity > 0.34f;

    AddIf(d.characterScore, upright, 0.16f);
    AddIf(d.characterScore, tall, 0.12f);
    AddIf(d.characterScore, compactHumanoid, 0.20f);
    AddIf(d.characterScore, s.symmetryX > 0.38f, 0.16f);
    AddIf(d.characterScore, centeredMass, 0.12f);
    AddIf(d.characterScore, roundedOrArticulated, 0.22f);
    AddIf(d.characterScore, s.coverage > 0.040f && s.coverage < 0.78f, 0.08f);
    AddIf(d.characterScore, strongBox, -0.18f);

    AddIf(d.buildingScore, strongBox, 0.34f);
    AddIf(d.buildingScore, s.aspectRatio > 0.80f, 0.12f);
    AddIf(d.buildingScore, s.rectangularity > 0.62f, 0.16f);
    AddIf(d.buildingScore, s.rowStability > 0.55f, 0.15f);
    AddIf(d.buildingScore, s.columnStability > 0.55f, 0.15f);
    AddIf(d.buildingScore, s.straightEdgeScore > 0.32f, 0.15f);

    AddIf(d.furnitureScore, s.aspectRatio > 0.45f && s.aspectRatio < 1.35f, 0.14f);
    AddIf(d.furnitureScore, s.rectangularity > 0.42f, 0.12f);
    AddIf(d.furnitureScore, s.rowStability > 0.42f, 0.12f);
    AddIf(d.furnitureScore, s.columnStability > 0.30f, 0.10f);
    AddIf(d.furnitureScore, s.straightEdgeScore > 0.22f, 0.12f);
    AddIf(d.furnitureScore, roundedOrArticulated && compactHumanoid, -0.22f);

    AddIf(d.vehicleScore, wideLow, 0.38f);
    AddIf(d.vehicleScore, s.aspectRatio < 0.72f, 0.16f);
    AddIf(d.vehicleScore, s.symmetryX > 0.34f, 0.10f);
    AddIf(d.vehicleScore, s.massCenterY > 0.46f, 0.08f);

    AddIf(d.genericScore, s.contourComplexity > 0.58f, 0.14f);
    AddIf(d.genericScore, s.edgeDensity > 0.45f, 0.10f);

    float best = d.genericScore;
    d.type = GameAssetType::GenericProp;
    if (d.vehicleScore > best) { best = d.vehicleScore; d.type = GameAssetType::Vehicle; }
    if (d.furnitureScore > best) { best = d.furnitureScore; d.type = GameAssetType::Furniture; }
    if (d.buildingScore > best) { best = d.buildingScore; d.type = GameAssetType::Building; }
    if (d.characterScore >= best - 0.06f && roundedOrArticulated && centeredMass && !wideLow) {
        best = d.characterScore;
        d.type = GameAssetType::Character;
    } else if (d.characterScore > best) {
        best = d.characterScore;
        d.type = GameAssetType::Character;
    }

    std::ostringstream reason;
    reason << "Auto structured resolver selected " << ToString(d.type)
           << " from shape scores: character=" << d.characterScore
           << ", building=" << d.buildingScore
           << ", furniture=" << d.furnitureScore
           << ", vehicle=" << d.vehicleScore
           << ", generic=" << d.genericScore
           << ".";
    d.reason = reason.str();
    return d;
}

} // namespace

StructuredAssetBuildResult BuildStructuredAssetMesh(
    const ImageRGBA& image,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& mask,
    const StructuredAssetOptions& options) {

    StructuredAssetBuildResult result;
    AssetAnalysisOptions analysisOptions;
    analysisOptions.enablePartHints = true;
    result.analysis = AnalyzeAsset(image, mask, depth, analysisOptions);
    if (!result.analysis.ok) {
        result.message = "Structured asset analysis failed.";
        result.warnings = result.analysis.warnings;
        return result;
    }

    AutoTypeDecision decision = ResolveStructuredAssetType(result.analysis);
    if (options.forcedAssetType != GameAssetType::Unknown) {
        decision.type = options.forcedAssetType;
        decision.reason = std::string("Asset type overridden by caller: ") + ToString(options.forcedAssetType);
    }

    AssetAnalysisResult plannedAnalysis = result.analysis;
    if (plannedAnalysis.classification.assetType != decision.type) {
        plannedAnalysis.classification.assetType = decision.type;
        plannedAnalysis.classification.reasons.push_back(decision.reason);
        plannedAnalysis.partHints.clear();
    } else {
        plannedAnalysis.classification.reasons.push_back(decision.reason);
    }
    result.analysis = plannedAnalysis;

    AssetPlanOptions planOptions;
    planOptions.targetHeight = MaxFloat(0.25f, options.targetHeight);
    planOptions.defaultDepth = MaxFloat(0.12f, options.defaultDepth);
    planOptions.includeEditFriendlyParts = true;
    result.plan = BuildAssetPlanFromAnalysis(result.analysis, planOptions);
    if (!result.plan.ok) {
        result.message = "Structured asset plan failed.";
        result.warnings = result.plan.warnings;
        return result;
    }

    BuildFromPlanParts(result.mesh, result.plan, options);
    if (result.mesh.positions.empty() || result.mesh.indices.empty()) {
        result.message = "Structured asset builder produced empty mesh.";
        return result;
    }

    RecomputeNormals(result.mesh);
    if (options.normalizeOutput) NormalizeMesh(result.mesh, options.targetHeight);
    result.validation = ValidateGameAssetMesh(result.mesh);
    result.ok = result.validation.ok;
    result.message = result.ok ? "Structured asset mesh generated." : "Structured asset mesh failed validation.";
    result.warnings = result.validation.warnings;
    result.validation.warnings.push_back("Generated with auto structured multi-asset resolver instead of raw silhouette extrusion.");
    return result;
}

std::string StructuredAssetBuildResult::ToMarkdown() const {
    std::ostringstream o;
    o << "# Make3D Structured Asset Build Report\n\n";
    o << "Success: `" << (ok ? "true" : "false") << "`\n\n";
    o << "Message: " << message << "\n\n";
    o << "## Analysis\n\n" << analysis.ToMarkdown() << "\n";
    o << "## Plan\n\n" << plan.ToMarkdown() << "\n";
    o << "## Mesh validation\n\n" << validation.ToMarkdown() << "\n";
    if (!warnings.empty()) {
        o << "## Warnings\n\n";
        for (const auto& warning : warnings) o << "- " << warning << "\n";
    }
    return o.str();
}

std::string StructuredAssetBuildResult::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"ok\": " << (ok ? "true" : "false") << ",\n";
    o << "  \"message\": \"" << EscapeJson(message) << "\",\n";
    o << "  \"analysis\": " << analysis.ToJson() << ",\n";
    o << "  \"plan\": " << plan.ToJson() << ",\n";
    o << "  \"validation\": " << validation.ToJson() << "\n";
    o << "}\n";
    return o.str();
}

} // namespace make3d
