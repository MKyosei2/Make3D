#include "Make3DGameAssetGenerator.h"
#include "Make3DGltfMaterialExporter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>

namespace make3d {
namespace {

constexpr float Pi = 3.14159265358979323846f;
struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };

static int VertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
static int TriangleCount(const MeshData& mesh) { return static_cast<int>(mesh.indices.size() / 3); }

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
    AddVertex(mesh, x0, y0, z0, 0, 0); AddVertex(mesh, x1, y0, z0, 1, 0); AddVertex(mesh, x1, y1, z0, 1, 1); AddVertex(mesh, x0, y1, z0, 0, 1);
    AddVertex(mesh, x0, y0, z1, 0, 0); AddVertex(mesh, x1, y0, z1, 1, 0); AddVertex(mesh, x1, y1, z1, 1, 1); AddVertex(mesh, x0, y1, z1, 0, 1);
    AddQuad(mesh, base + 0, base + 1, base + 2, base + 3);
    AddQuad(mesh, base + 5, base + 4, base + 7, base + 6);
    AddQuad(mesh, base + 4, base + 0, base + 3, base + 7);
    AddQuad(mesh, base + 1, base + 5, base + 6, base + 2);
    AddQuad(mesh, base + 3, base + 2, base + 6, base + 7);
    AddQuad(mesh, base + 4, base + 5, base + 1, base + 0);
}

static void AddCylinder(MeshData& mesh, float cx, float cy0, float cy1, float cz, float rx, float rz, int segments) {
    segments = std::max(8, segments);
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

static void AppendMesh(MeshData& dst, const MeshData& src) {
    const std::uint32_t base = static_cast<std::uint32_t>(VertexCount(dst));
    dst.positions.insert(dst.positions.end(), src.positions.begin(), src.positions.end());
    dst.normals.insert(dst.normals.end(), src.normals.begin(), src.normals.end());
    dst.uvs.insert(dst.uvs.end(), src.uvs.begin(), src.uvs.end());
    for (std::uint32_t i : src.indices) dst.indices.push_back(base + i);
}

static MeshData BuildReliefFallbackAsset(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const GameAssetGeneratorOptions& options) {
    AdvancedOptions adv;
    adv.mode = ReconstructionMode::HybridVolume;
    adv.quality = QualityPreset::Detailed;
    adv.maxGridResolution = std::max(24, options.gridResolution);
    adv.volumeRadialSegments = std::max(8, options.radialSegments);
    adv.depthScale = options.extrusionDepth;
    MeshData mesh = ReconstructMesh(color, depth, mask, adv, nullptr);
    if (options.addPropDetailBands) {
        MeshData bands;
        AddBox(bands, 0.0f, 0.45f, options.extrusionDepth * 0.55f, 1.05f, 0.035f, 0.035f);
        AddBox(bands, 0.0f, 0.95f, options.extrusionDepth * 0.55f, 0.86f, 0.030f, 0.035f);
        AppendMesh(mesh, bands);
    }
    RecomputeNormals(mesh);
    NormalizeMesh(mesh, options.targetHeight);
    return mesh;
}

static MeshData BuildVehicleLikeAsset(const GameAssetGeneratorOptions& options) {
    MeshData mesh;
    AddBox(mesh, 0.0f, 0.45f, 0.0f, 1.75f, 0.52f, 0.72f);
    AddBox(mesh, 0.10f, 0.86f, 0.0f, 0.85f, 0.38f, 0.62f);
    AddCylinder(mesh, -0.62f, 0.12f, 0.24f, 0.38f, 0.16f, 0.16f, options.radialSegments);
    AddCylinder(mesh,  0.62f, 0.12f, 0.24f, 0.38f, 0.16f, 0.16f, options.radialSegments);
    AddCylinder(mesh, -0.62f, 0.12f, 0.24f, -0.38f, 0.16f, 0.16f, options.radialSegments);
    AddCylinder(mesh,  0.62f, 0.12f, 0.24f, -0.38f, 0.16f, 0.16f, options.radialSegments);
    RecomputeNormals(mesh);
    NormalizeMesh(mesh, options.targetHeight * 0.65f);
    return mesh;
}

static MeshData BuildSafeProxyAsset(GameAssetType type, const GameAssetGeneratorOptions& options) {
    MeshData mesh;
    if (type == GameAssetType::Building || type == GameAssetType::ArchitecturalPart) {
        AddBox(mesh, 0.0f, options.targetHeight * 0.5f, 0.0f, 1.0f, options.targetHeight, std::max(0.35f, options.buildingDepth));
    } else if (type == GameAssetType::Vehicle) {
        mesh = BuildVehicleLikeAsset(options);
    } else if (type == GameAssetType::Character || type == GameAssetType::Creature) {
        AddCylinder(mesh, 0.0f, 0.0f, options.targetHeight * 0.72f, 0.0f, 0.28f, 0.18f, std::max(12, options.radialSegments));
        AddCylinder(mesh, 0.0f, options.targetHeight * 0.74f, options.targetHeight, 0.0f, 0.20f, 0.20f, std::max(12, options.radialSegments));
    } else {
        AddBox(mesh, 0.0f, options.targetHeight * 0.5f, 0.0f, 0.85f, options.targetHeight, 0.55f);
    }
    RecomputeNormals(mesh);
    NormalizeMesh(mesh, options.targetHeight);
    return mesh;
}

static void AddWindowGrid(MeshData& mesh, const PlannedAssetPart& p) {
    const int cols = std::max(2, static_cast<int>(p.sizeX * 4.0f));
    const int rows = std::max(2, static_cast<int>(p.sizeY * 3.0f));
    const float cellW = p.sizeX / static_cast<float>(cols);
    const float cellH = p.sizeY / static_cast<float>(rows);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const float x = p.centerX - p.sizeX * 0.5f + cellW * (static_cast<float>(c) + 0.5f);
            const float y = p.centerY - p.sizeY * 0.5f + cellH * (static_cast<float>(r) + 0.5f);
            AddBox(mesh, x, y, p.sizeZ * 0.58f, cellW * 0.45f, cellH * 0.42f, p.sizeZ * 0.06f);
        }
    }
}

static MeshData BuildPlannedAssetMesh(const AssetPlanResult& plan, const GameAssetGeneratorOptions& options) {
    MeshData mesh;
    for (const PlannedAssetPart& p : plan.parts) {
        const float sx = std::max(0.03f, p.sizeX);
        const float sy = std::max(0.03f, p.sizeY);
        const float sz = std::max(0.03f, p.sizeZ);
        switch (p.kind) {
            case PlannedPartKind::CharacterHead:
                AddCylinder(mesh, p.centerX, p.centerY - sy * 0.5f, p.centerY + sy * 0.5f, p.centerZ, sx * 0.42f, sz * 0.50f, std::max(14, options.radialSegments));
                break;
            case PlannedPartKind::CharacterArm:
            case PlannedPartKind::CharacterLeg:
                AddCylinder(mesh, p.centerX, p.centerY - sy * 0.5f, p.centerY + sy * 0.5f, p.centerZ, sx * 0.25f, sz * 0.28f, std::max(10, options.radialSegments / 2));
                break;
            case PlannedPartKind::VehicleWheel:
                AddCylinder(mesh, p.centerX, p.centerY - sy * 0.5f, p.centerY + sy * 0.5f, p.centerZ + sz * 0.30f, sx * 0.45f, sz * 0.45f, std::max(16, options.radialSegments));
                break;
            case PlannedPartKind::BuildingWindowGrid:
                AddWindowGrid(mesh, p);
                break;
            case PlannedPartKind::BuildingRoof:
                AddBox(mesh, p.centerX, p.centerY, p.centerZ, sx * 1.08f, sy, sz * 1.10f);
                break;
            case PlannedPartKind::BuildingDoor:
            case PlannedPartKind::DetailPanel:
            case PlannedPartKind::DetailBand:
                AddBox(mesh, p.centerX, p.centerY, p.centerZ + sz * 0.52f, sx, sy, std::max(0.02f, sz * 0.08f));
                break;
            case PlannedPartKind::ComplexProtrusion:
                AddBox(mesh, p.centerX, p.centerY, p.centerZ, sx, sy, sz * 1.25f);
                break;
            default:
                AddBox(mesh, p.centerX, p.centerY, p.centerZ, sx, sy, sz);
                break;
        }
    }
    if (mesh.positions.empty()) return mesh;
    RecomputeNormals(mesh);
    NormalizeMesh(mesh, plan.targetHeight);
    return mesh;
}

static MeshData BuildColliderBox(const GameAssetValidationReport& validation) {
    MeshData mesh;
    const float sx = std::max(0.001f, validation.boundsMaxX - validation.boundsMinX);
    const float sy = std::max(0.001f, validation.boundsMaxY - validation.boundsMinY);
    const float sz = std::max(0.001f, validation.boundsMaxZ - validation.boundsMinZ);
    const float cx = (validation.boundsMinX + validation.boundsMaxX) * 0.5f;
    const float cy = (validation.boundsMinY + validation.boundsMaxY) * 0.5f;
    const float cz = (validation.boundsMinZ + validation.boundsMaxZ) * 0.5f;
    AddBox(mesh, cx, cy, cz, sx, sy, sz);
    RecomputeNormals(mesh);
    return mesh;
}

static MeshData BuildLodProxy(const GameAssetValidationReport& validation, GameAssetType type) {
    MeshData mesh;
    const float sx = std::max(0.001f, validation.boundsMaxX - validation.boundsMinX);
    const float sy = std::max(0.001f, validation.boundsMaxY - validation.boundsMinY);
    const float sz = std::max(0.001f, validation.boundsMaxZ - validation.boundsMinZ);
    const float cx = (validation.boundsMinX + validation.boundsMaxX) * 0.5f;
    const float cy = (validation.boundsMinY + validation.boundsMaxY) * 0.5f;
    const float cz = (validation.boundsMinZ + validation.boundsMaxZ) * 0.5f;
    if (type == GameAssetType::Vehicle || type == GameAssetType::Building || type == GameAssetType::ArchitecturalPart) AddBox(mesh, cx, cy, cz, sx, sy, sz);
    else AddCylinder(mesh, cx, validation.boundsMinY, validation.boundsMaxY, cz, sx * 0.38f, sz * 0.38f, 12);
    RecomputeNormals(mesh);
    return mesh;
}

static std::string MakeMarkdownReport(const GameAssetGenerationResult& result) {
    std::ostringstream o;
    o << "# Make3D Generic Game Asset Report\n\n";
    o << "## Phase 1 Asset analysis\n\n" << result.analysis.ToMarkdown() << "\n";
    o << "## Phase 1 Asset plan\n\n" << result.plan.ToMarkdown() << "\n";
    o << "## Classification\n\n" << result.classification.ToMarkdown() << "\n";
    o << "## Mesh validation\n\n" << result.validation.ToMarkdown() << "\n";
    o << "## Phase 0 safe-output quality gate\n\n" << result.qualityGate.ToMarkdown() << "\n";
    o << "## Game metadata\n\n" << result.metadata.ToMarkdown() << "\n";
    o << "## Output files\n\n";
    o << "- " << result.objPath.generic_string() << "\n";
    o << "- " << result.gltfPath.generic_string() << "\n";
    if (!result.colliderObjPath.empty()) o << "- " << result.colliderObjPath.generic_string() << "\n";
    if (!result.lodObjPath.empty()) o << "- " << result.lodObjPath.generic_string() << "\n";
    return o.str();
}

static std::string MakeManifestJson(const GameAssetGenerationResult& result) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"ok\": " << (result.ok ? "true" : "false") << ",\n";
    o << "  \"message\": \"" << EscapeJson(result.message) << "\",\n";
    o << "  \"analysis\": " << result.analysis.ToJson() << ",\n";
    o << "  \"plan\": " << result.plan.ToJson() << ",\n";
    o << "  \"classification\": " << result.classification.ToJson() << ",\n";
    o << "  \"validation\": " << result.validation.ToJson() << ",\n";
    o << "  \"qualityGate\": " << result.qualityGate.ToJson() << ",\n";
    o << "  \"metadata\": " << result.metadata.ToJson() << ",\n";
    o << "  \"obj\": \"" << EscapeJson(result.objPath.generic_string()) << "\",\n";
    o << "  \"gltf\": \"" << EscapeJson(result.gltfPath.generic_string()) << "\",\n";
    o << "  \"colliderObj\": \"" << EscapeJson(result.colliderObjPath.generic_string()) << "\",\n";
    o << "  \"lodProxyObj\": \"" << EscapeJson(result.lodObjPath.generic_string()) << "\"\n";
    o << "}\n";
    return o.str();
}

} // namespace

GameAssetValidationReport ValidateGameAssetMesh(const MeshData& mesh) {
    GameAssetValidationReport r;
    r.vertices = VertexCount(mesh);
    r.triangles = TriangleCount(mesh);
    r.missingNormals = mesh.normals.size() == mesh.positions.size() ? 0 : 1;
    r.missingUvs = mesh.uvs.size() == static_cast<size_t>(r.vertices) * 2 ? 0 : 1;
    if (mesh.positions.empty()) { r.warnings.push_back("Mesh has no positions."); return r; }
    r.boundsMinX = r.boundsMinY = r.boundsMinZ = std::numeric_limits<float>::max();
    r.boundsMaxX = r.boundsMaxY = r.boundsMaxZ = -std::numeric_limits<float>::max();
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        const float x = mesh.positions[i + 0], y = mesh.positions[i + 1], z = mesh.positions[i + 2];
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) { ++r.nonFinitePositions; continue; }
        r.boundsMinX = std::min(r.boundsMinX, x); r.boundsMinY = std::min(r.boundsMinY, y); r.boundsMinZ = std::min(r.boundsMinZ, z);
        r.boundsMaxX = std::max(r.boundsMaxX, x); r.boundsMaxY = std::max(r.boundsMaxY, y); r.boundsMaxZ = std::max(r.boundsMaxZ, z);
    }
    auto pos = [&](std::uint32_t idx) -> Vec3 { size_t p = static_cast<size_t>(idx) * 3; return {mesh.positions[p], mesh.positions[p + 1], mesh.positions[p + 2]}; };
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const std::uint32_t a = mesh.indices[i + 0], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
        if (a >= static_cast<std::uint32_t>(r.vertices) || b >= static_cast<std::uint32_t>(r.vertices) || c >= static_cast<std::uint32_t>(r.vertices)) { ++r.invalidIndices; continue; }
        Vec3 pa = pos(a), pb = pos(b), pc = pos(c);
        Vec3 ab{pb.x - pa.x, pb.y - pa.y, pb.z - pa.z};
        Vec3 ac{pc.x - pa.x, pc.y - pa.y, pc.z - pa.z};
        Vec3 cross{ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};
        const float area2 = cross.x * cross.x + cross.y * cross.y + cross.z * cross.z;
        if (area2 < 1.0e-12f) ++r.degenerateTriangles;
    }
    if (r.vertices == 0) r.warnings.push_back("Mesh has zero vertices.");
    if (r.triangles == 0) r.warnings.push_back("Mesh has zero triangles.");
    if (r.invalidIndices > 0) r.warnings.push_back("Mesh contains invalid face indices.");
    if (r.degenerateTriangles > 0) r.warnings.push_back("Mesh contains degenerate triangles.");
    if (r.nonFinitePositions > 0) r.warnings.push_back("Mesh contains NaN or infinity positions.");
    if (r.missingNormals) r.warnings.push_back("Mesh normals are missing or mismatched.");
    if (r.missingUvs) r.warnings.push_back("Mesh UVs are missing or mismatched.");
    r.ok = r.vertices > 0 && r.triangles > 0 && r.invalidIndices == 0 && r.nonFinitePositions == 0;
    return r;
}

GameAssetGenerationResult BuildGenericGameAsset(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const std::filesystem::path& outputDir, const GameAssetGeneratorOptions& options) {
    GameAssetGenerationResult result;
    result.analysis = AnalyzeAsset(color, mask, depth, options.analysis);
    if (result.analysis.ok) {
        AssetPlanOptions planOptions = options.plan;
        planOptions.targetHeight = options.targetHeight;
        planOptions.defaultDepth = std::max(options.extrusionDepth, options.buildingDepth);
        result.plan = BuildAssetPlanFromAnalysis(result.analysis, planOptions);
        result.classification = result.analysis.classification;
    } else {
        result.classification = InferGameAssetType(color, mask, depth);
    }

    if (options.useAssetAnalysisPlan && result.plan.ok) {
        result.mesh = BuildPlannedAssetMesh(result.plan, options);
    }
    if (result.mesh.positions.empty()) {
        if (result.classification.assetType == GameAssetType::Vehicle) result.mesh = BuildVehicleLikeAsset(options);
        else result.mesh = BuildReliefFallbackAsset(color, depth, mask, options);
    }

    RecomputeNormals(result.mesh);
    result.validation = ValidateGameAssetMesh(result.mesh);
    result.qualityGate = AnalyzeMeshQualityGate(result.mesh, options.qualityGate);
    if (options.enforceSafeMeshQuality && (!result.validation.ok || !result.qualityGate.ok)) {
        result.mesh = BuildSafeProxyAsset(result.classification.assetType, options);
        RecomputeNormals(result.mesh);
        result.validation = ValidateGameAssetMesh(result.mesh);
        result.qualityGate = AnalyzeMeshQualityGate(result.mesh, options.qualityGate);
        result.qualityGate.warnings.push_back("Original generated mesh failed Phase 0 safe-output gate; exported stable typed proxy instead.");
    }
    if (!result.validation.ok || (options.enforceSafeMeshQuality && !result.qualityGate.ok)) {
        result.message = "Generic game asset generation produced unsafe mesh.";
        return result;
    }

    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    result.objPath = outputDir / "make3d_game_asset.obj";
    result.gltfPath = outputDir / "make3d_game_asset.gltf";
    result.reportPath = outputDir / "make3d_game_asset_report.md";
    result.manifestPath = outputDir / "make3d_game_asset_manifest.json";

    std::string error;
    if (!ExportOBJ(result.mesh, result.objPath, "", &error)) { result.message = error; return result; }
    GltfMaterialOptions material;
    material.materialName = "Make3DGameAssetMaterial";
    if (!result.plan.materials.empty()) {
        const PlannedMaterialSlot& m = result.plan.materials.front();
        material.baseColorFactor = {m.r / 255.0f, m.g / 255.0f, m.b / 255.0f, 1.0f};
    } else {
        material.baseColorFactor = {0.72f, 0.76f, 0.82f, 1.0f};
    }
    if (!ExportGLTFWithMaterial(result.mesh, result.gltfPath, material, &error)) { result.message = error; return result; }

    if (options.generateColliderProxy) {
        result.colliderMesh = BuildColliderBox(result.validation);
        result.colliderValidation = ValidateGameAssetMesh(result.colliderMesh);
        result.colliderObjPath = outputDir / "make3d_game_asset_collider.obj";
        if (!ExportOBJ(result.colliderMesh, result.colliderObjPath, "", &error)) { result.message = error; return result; }
    }
    if (options.generateLodProxy) {
        result.lodProxyMesh = BuildLodProxy(result.validation, result.classification.assetType);
        result.lodValidation = ValidateGameAssetMesh(result.lodProxyMesh);
        result.lodObjPath = outputDir / "make3d_game_asset_lod_proxy.obj";
        if (!ExportOBJ(result.lodProxyMesh, result.lodObjPath, "", &error)) { result.message = error; return result; }
    }

    result.metadata.assetName = result.plan.ok ? result.plan.assetName : "Make3D_" + std::string(ToString(result.classification.assetType));
    result.metadata.assetType = result.classification.assetType;
    result.metadata.pivotX = 0.0f; result.metadata.pivotY = result.validation.boundsMinY; result.metadata.pivotZ = 0.0f;
    result.metadata.colliderMinX = result.validation.boundsMinX; result.metadata.colliderMinY = result.validation.boundsMinY; result.metadata.colliderMinZ = result.validation.boundsMinZ;
    result.metadata.colliderMaxX = result.validation.boundsMaxX; result.metadata.colliderMaxY = result.validation.boundsMaxY; result.metadata.colliderMaxZ = result.validation.boundsMaxZ;
    result.metadata.lod0Vertices = result.validation.vertices; result.metadata.lod0Triangles = result.validation.triangles;
    result.metadata.lodProxyVertices = VertexCount(result.lodProxyMesh); result.metadata.lodProxyTriangles = TriangleCount(result.lodProxyMesh);

    std::ofstream report(result.reportPath, std::ios::binary);
    report << MakeMarkdownReport(result);
    result.ok = true;
    result.message = result.plan.ok ? "Generic game asset generation finished with Phase 1 analysis/plan typed mesh." : "Generic game asset generation finished with fallback mesh.";
    std::ofstream manifest(result.manifestPath, std::ios::binary);
    manifest << MakeManifestJson(result);
    return result;
}

std::string GameAssetValidationReport::ToMarkdown() const {
    std::ostringstream o;
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| OK | " << (ok ? "yes" : "no") << " |\n";
    o << "| Vertices | " << vertices << " |\n";
    o << "| Triangles | " << triangles << " |\n";
    o << "| Invalid indices | " << invalidIndices << " |\n";
    o << "| Degenerate triangles | " << degenerateTriangles << " |\n";
    o << "| Non-finite positions | " << nonFinitePositions << " |\n";
    o << "| Missing normals | " << missingNormals << " |\n";
    o << "| Missing UVs | " << missingUvs << " |\n";
    o << "| Bounds min | " << boundsMinX << ", " << boundsMinY << ", " << boundsMinZ << " |\n";
    o << "| Bounds max | " << boundsMaxX << ", " << boundsMaxY << ", " << boundsMaxZ << " |\n\n";
    if (!warnings.empty()) { o << "### Warnings\n\n"; for (const auto& w : warnings) o << "- " << w << "\n"; }
    return o.str();
}

std::string GameAssetValidationReport::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"ok\": " << (ok ? "true" : "false") << ",\n";
    o << "  \"vertices\": " << vertices << ",\n";
    o << "  \"triangles\": " << triangles << ",\n";
    o << "  \"invalidIndices\": " << invalidIndices << ",\n";
    o << "  \"degenerateTriangles\": " << degenerateTriangles << ",\n";
    o << "  \"nonFinitePositions\": " << nonFinitePositions << ",\n";
    o << "  \"missingNormals\": " << missingNormals << ",\n";
    o << "  \"missingUvs\": " << missingUvs << ",\n";
    o << "  \"boundsMin\": [" << boundsMinX << ", " << boundsMinY << ", " << boundsMinZ << "],\n";
    o << "  \"boundsMax\": [" << boundsMaxX << ", " << boundsMaxY << ", " << boundsMaxZ << "],\n";
    o << "  \"warnings\": [";
    for (size_t i = 0; i < warnings.size(); ++i) { if (i) o << ", "; o << "\"" << EscapeJson(warnings[i]) << "\""; }
    o << "]\n}";
    return o.str();
}

std::string GameAssetMetadata::ToMarkdown() const {
    std::ostringstream o;
    o << "| Field | Value |\n|---|---:|\n";
    o << "| Asset name | " << assetName << " |\n";
    o << "| Asset type | " << ToString(assetType) << " |\n";
    o << "| Pivot | " << pivotX << ", " << pivotY << ", " << pivotZ << " |\n";
    o << "| Unit scale meters | " << unitScaleMeters << " |\n";
    o << "| Collider min | " << colliderMinX << ", " << colliderMinY << ", " << colliderMinZ << " |\n";
    o << "| Collider max | " << colliderMaxX << ", " << colliderMaxY << ", " << colliderMaxZ << " |\n";
    o << "| LOD0 vertices | " << lod0Vertices << " |\n";
    o << "| LOD0 triangles | " << lod0Triangles << " |\n";
    o << "| LOD proxy vertices | " << lodProxyVertices << " |\n";
    o << "| LOD proxy triangles | " << lodProxyTriangles << " |\n";
    return o.str();
}

std::string GameAssetMetadata::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"assetName\": \"" << EscapeJson(assetName) << "\",\n";
    o << "  \"assetType\": \"" << EscapeJson(ToString(assetType)) << "\",\n";
    o << "  \"pivot\": [" << pivotX << ", " << pivotY << ", " << pivotZ << "],\n";
    o << "  \"unitScaleMeters\": " << unitScaleMeters << ",\n";
    o << "  \"colliderMin\": [" << colliderMinX << ", " << colliderMinY << ", " << colliderMinZ << "],\n";
    o << "  \"colliderMax\": [" << colliderMaxX << ", " << colliderMaxY << ", " << colliderMaxZ << "],\n";
    o << "  \"lod0Vertices\": " << lod0Vertices << ",\n";
    o << "  \"lod0Triangles\": " << lod0Triangles << ",\n";
    o << "  \"lodProxyVertices\": " << lodProxyVertices << ",\n";
    o << "  \"lodProxyTriangles\": " << lodProxyTriangles << "\n";
    o << "}";
    return o.str();
}

} // namespace make3d
