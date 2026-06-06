#include "Make3DStructuredAssetBuilder.h"

#include <cmath>
#include <cstdint>
#include <sstream>

namespace make3d {
namespace {

constexpr float Pi = 3.14159265358979323846f;

static int VertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
static int MaxInt(int a, int b) { return a > b ? a : b; }
static float MaxFloat(float a, float b) { return a > b ? a : b; }
static float MinFloat(float a, float b) { return a < b ? a : b; }
static float ClampFloat(float value, float lo, float hi) { return MaxFloat(lo, MinFloat(hi, value)); }

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

static void AddFoot(MeshData& mesh, float x, float y, float z, float sx, float sy, float sz, int segments) {
    AddEllipsoid(mesh, x, y, z + sz * 0.18f, sx * 0.55f, sy * 0.38f, sz * 0.58f, segments, 6);
}

static void BuildCharacterStructured(MeshData& mesh, const AssetPlanResult& plan, const StructuredAssetOptions& options) {
    const float h = plan.targetHeight;
    const float w = ClampFloat(plan.targetWidth, 0.72f, 1.35f);
    const float d = MaxFloat(0.42f, plan.targetDepth);
    const int seg = MaxInt(18, options.radialSegments);

    AddEllipsoid(mesh, 0.0f, h * 0.78f, 0.0f, w * 0.26f, h * 0.18f, d * 0.28f, seg, 10);
    AddEllipsoid(mesh, 0.0f, h * 0.95f, 0.0f, w * 0.19f, h * 0.08f, d * 0.20f, seg, 6);
    AddEllipsoid(mesh, 0.0f, h * 1.15f, 0.0f, w * 0.31f, h * 0.21f, d * 0.34f, seg, 12);
    AddEllipsoid(mesh, 0.0f, h * 1.28f, -d * 0.04f, w * 0.34f, h * 0.09f, d * 0.36f, seg, 6);
    AddBox(mesh, 0.0f, h * 0.63f, d * 0.25f, w * 0.56f, h * 0.06f, d * 0.06f);

    const int limbSeg = MaxInt(10, seg / 2);
    for (float side : {-1.0f, 1.0f}) {
        AddCylinder(mesh, side * w * 0.39f, h * 0.56f, h * 0.89f, 0.0f, w * 0.045f, d * 0.065f, limbSeg);
        AddEllipsoid(mesh, side * w * 0.43f, h * 0.52f, 0.0f, w * 0.075f, h * 0.045f, d * 0.085f, limbSeg, 5);
        AddCylinder(mesh, side * w * 0.13f, h * 0.15f, h * 0.53f, 0.0f, w * 0.055f, d * 0.070f, limbSeg);
        AddFoot(mesh, side * w * 0.13f, h * 0.07f, d * 0.08f, w * 0.18f, h * 0.055f, d * 0.22f, limbSeg);
    }

    AddEllipsoid(mesh, -w * 0.08f, h * 1.17f, d * 0.32f, w * 0.025f, h * 0.026f, d * 0.012f, 8, 4);
    AddEllipsoid(mesh,  w * 0.08f, h * 1.17f, d * 0.32f, w * 0.025f, h * 0.026f, d * 0.012f, 8, 4);
    AddBox(mesh, 0.0f, h * 1.10f, d * 0.335f, w * 0.16f, h * 0.012f, d * 0.010f);
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
    result.validation.warnings.push_back("Generated with structured multi-asset builder instead of raw silhouette extrusion.");
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
