#include "Make3DGameAssetGenerator.h"
#include "Make3DGltfMaterialExporter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace make3d {

namespace {

constexpr float Pi = 3.14159265358979323846f;

struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };

static int VertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
static int TriangleCount(const MeshData& mesh) { return static_cast<int>(mesh.indices.size() / 3); }
static int Idx(int x, int y, int w) { return y * w + x; }
static float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

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

static bool ComputeMaskBounds(const std::vector<std::uint8_t>& mask, int w, int h, int& minX, int& minY, int& maxX, int& maxY) {
    minX = w; minY = h; maxX = -1; maxY = -1;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!mask[static_cast<size_t>(Idx(x, y, w))]) continue;
            minX = std::min(minX, x); minY = std::min(minY, y);
            maxX = std::max(maxX, x); maxY = std::max(maxY, y);
        }
    }
    return maxX >= minX && maxY >= minY;
}

static void AppendMesh(MeshData& dst, const MeshData& src) {
    std::uint32_t base = static_cast<std::uint32_t>(VertexCount(dst));
    dst.positions.insert(dst.positions.end(), src.positions.begin(), src.positions.end());
    dst.normals.insert(dst.normals.end(), src.normals.begin(), src.normals.end());
    dst.uvs.insert(dst.uvs.end(), src.uvs.begin(), src.uvs.end());
    for (std::uint32_t i : src.indices) dst.indices.push_back(base + i);
}

static void AddBox(MeshData& mesh, float cx, float cy, float cz, float sx, float sy, float sz) {
    int base = VertexCount(mesh);
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

static void AddCylinder(MeshData& mesh, float cx, float cy0, float cy1, float cz, float rx, float rz, int segments) {
    segments = std::max(8, segments);
    int base = VertexCount(mesh);
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
    int bottom = VertexCount(mesh); AddVertex(mesh, cx, cy0, cz, 0.5f, 0.5f);
    int top = VertexCount(mesh); AddVertex(mesh, cx, cy1, cz, 0.5f, 0.5f);
    for (int s = 0; s < segments; ++s) {
        int n = (s + 1) % segments;
        AddTri(mesh, bottom, base + n, base + s);
        AddTri(mesh, top, base + segments + s, base + segments + n);
    }
}

static MeshData BuildReliefAsset(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const GameAssetGeneratorOptions& options) {
    AdvancedOptions adv;
    adv.mode = ReconstructionMode::HybridVolume;
    adv.quality = QualityPreset::Detailed;
    adv.maxGridResolution = std::max(24, options.gridResolution);
    adv.volumeRadialSegments = std::max(8, options.radialSegments);
    adv.depthScale = options.extrusionDepth;
    MeshData mesh = ReconstructMesh(color, depth, mask, adv, nullptr);
    NormalizeMesh(mesh, options.targetHeight);
    return mesh;
}

static MeshData BuildBuildingAsset(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const AssetClassificationResult& classification, const GameAssetGeneratorOptions& options) {
    (void)depth;
    MeshData mesh;
    int minX, minY, maxX, maxY;
    if (!ComputeMaskBounds(mask, color.width, color.height, minX, minY, maxX, maxY)) return mesh;
    float height = options.targetHeight;
    float width = height / std::max(0.25f, classification.aspectRatio);
    width = std::clamp(width, 0.65f, 2.8f);
    float depthSize = std::max(0.30f, options.buildingDepth);
    AddBox(mesh, 0.0f, height * 0.5f, 0.0f, width, height, depthSize);

    if (options.addBuildingDetails) {
        float roofH = std::max(0.12f, height * 0.12f);
        AddBox(mesh, 0.0f, height + roofH * 0.30f, 0.0f, width * 1.10f, roofH, depthSize * 1.12f);
        int columns = std::clamp(static_cast<int>(width * 3.0f + 1.0f), 2, 6);
        int rows = std::clamp(static_cast<int>(height * 2.2f), 2, 7);
        for (int row = 0; row < rows; ++row) {
            float y = height * (0.18f + 0.66f * (static_cast<float>(row) + 0.5f) / static_cast<float>(rows));
            for (int col = 0; col < columns; ++col) {
                float x = -width * 0.38f + width * 0.76f * (static_cast<float>(col) + 0.5f) / static_cast<float>(columns);
                AddBox(mesh, x, y, depthSize * 0.505f, width * 0.10f, height * 0.065f, depthSize * 0.035f);
            }
        }
        AddBox(mesh, 0.0f, height * 0.075f, depthSize * 0.515f, width * 0.18f, height * 0.15f, depthSize * 0.05f);
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

static MeshData BuildGenericPrimitiveAsset(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const AssetClassificationResult& classification, const GameAssetGeneratorOptions& options) {
    MeshData mesh = BuildReliefAsset(color, depth, mask, options);
    if (options.addPropDetailBands && classification.assetType != GameAssetType::FlatRelief) {
        MeshData bands;
        float bandW = 1.05f;
        AddBox(bands, 0.0f, 0.45f, options.extrusionDepth * 0.55f, bandW, 0.035f, 0.035f);
        AddBox(bands, 0.0f, 0.95f, options.extrusionDepth * 0.55f, bandW * 0.82f, 0.030f, 0.035f);
        AppendMesh(mesh, bands);
    }
    RecomputeNormals(mesh);
    NormalizeMesh(mesh, options.targetHeight);
    return mesh;
}

static MeshData BuildColliderBox(const GameAssetValidationReport& validation) {
    MeshData mesh;
    float sx = std::max(0.001f, validation.boundsMaxX - validation.boundsMinX);
    float sy = std::max(0.001f, validation.boundsMaxY - validation.boundsMinY);
    float sz = std::max(0.001f, validation.boundsMaxZ - validation.boundsMinZ);
    float cx = (validation.boundsMinX + validation.boundsMaxX) * 0.5f;
    float cy = (validation.boundsMinY + validation.boundsMaxY) * 0.5f;
    float cz = (validation.boundsMinZ + validation.boundsMaxZ) * 0.5f;
    AddBox(mesh, cx, cy, cz, sx, sy, sz);
    RecomputeNormals(mesh);
    return mesh;
}

static MeshData BuildLodProxy(const GameAssetValidationReport& validation, GameAssetType type) {
    MeshData mesh;
    float sx = std::max(0.001f, validation.boundsMaxX - validation.boundsMinX);
    float sy = std::max(0.001f, validation.boundsMaxY - validation.boundsMinY);
    float sz = std::max(0.001f, validation.boundsMaxZ - validation.boundsMinZ);
    float cx = (validation.boundsMinX + validation.boundsMaxX) * 0.5f;
    float cy = (validation.boundsMinY + validation.boundsMaxY) * 0.5f;
    float cz = (validation.boundsMinZ + validation.boundsMaxZ) * 0.5f;
    if (type == GameAssetType::Vehicle) {
        AddBox(mesh, cx, cy, cz, sx, sy * 0.82f, sz);
    } else if (type == GameAssetType::Building || type == GameAssetType::ArchitecturalPart) {
        AddBox(mesh, cx, cy, cz, sx, sy, sz);
    } else {
        AddCylinder(mesh, cx, validation.boundsMinY, validation.boundsMaxY, cz, sx * 0.38f, sz * 0.38f, 12);
    }
    RecomputeNormals(mesh);
    return mesh;
}

static std::string MakeMarkdownReport(const GameAssetGenerationResult& result) {
    std::ostringstream o;
    o << "# Make3D Generic Game Asset Report\n\n";
    o << result.classification.ToMarkdown() << "\n";
    o << "## Mesh validation\n\n" << result.validation.ToMarkdown() << "\n";
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
    o << "  \"classification\": " << result.classification.ToJson() << ",\n";
    o << "  \"validation\": " << result.validation.ToJson() << ",\n";
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

    if (mesh.positions.empty()) {
        r.warnings.push_back("Mesh has no positions.");
        return r;
    }
    r.boundsMinX = r.boundsMinY = r.boundsMinZ = std::numeric_limits<float>::max();
    r.boundsMaxX = r.boundsMaxY = r.boundsMaxZ = -std::numeric_limits<float>::max();
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        float x = mesh.positions[i + 0], y = mesh.positions[i + 1], z = mesh.positions[i + 2];
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) { ++r.nonFinitePositions; continue; }
        r.boundsMinX = std::min(r.boundsMinX, x); r.boundsMinY = std::min(r.boundsMinY, y); r.boundsMinZ = std::min(r.boundsMinZ, z);
        r.boundsMaxX = std::max(r.boundsMaxX, x); r.boundsMaxY = std::max(r.boundsMaxY, y); r.boundsMaxZ = std::max(r.boundsMaxZ, z);
    }

    auto pos = [&](std::uint32_t idx) -> Vec3 {
        size_t p = static_cast<size_t>(idx) * 3;
        return {mesh.positions[p], mesh.positions[p + 1], mesh.positions[p + 2]};
    };
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::uint32_t a = mesh.indices[i + 0], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
        if (a >= static_cast<std::uint32_t>(r.vertices) || b >= static_cast<std::uint32_t>(r.vertices) || c >= static_cast<std::uint32_t>(r.vertices)) {
            ++r.invalidIndices;
            continue;
        }
        Vec3 pa = pos(a), pb = pos(b), pc = pos(c);
        Vec3 ab{pb.x - pa.x, pb.y - pa.y, pb.z - pa.z};
        Vec3 ac{pc.x - pa.x, pc.y - pa.y, pc.z - pa.z};
        Vec3 cross{ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};
        float area2 = cross.x * cross.x + cross.y * cross.y + cross.z * cross.z;
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

GameAssetGenerationResult BuildGenericGameAsset(
    const ImageRGBA& color,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& mask,
    const std::filesystem::path& outputDir,
    const GameAssetGeneratorOptions& options) {

    GameAssetGenerationResult result;
    result.classification = InferGameAssetType(color, mask, depth);

    switch (result.classification.assetType) {
        case GameAssetType::Building:
        case GameAssetType::ArchitecturalPart:
            result.mesh = BuildBuildingAsset(color, depth, mask, result.classification, options);
            break;
        case GameAssetType::Vehicle:
            result.mesh = BuildVehicleLikeAsset(options);
            break;
        default:
            result.mesh = BuildGenericPrimitiveAsset(color, depth, mask, result.classification, options);
            break;
    }

    RecomputeNormals(result.mesh);
    result.validation = ValidateGameAssetMesh(result.mesh);
    if (!result.validation.ok) {
        result.message = "Generic game asset generation produced invalid mesh.";
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
    material.baseColorFactor = {0.72f, 0.76f, 0.82f, 1.0f};
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

    result.metadata.assetName = "Make3D_" + std::string(ToString(result.classification.assetType));
    result.metadata.assetType = result.classification.assetType;
    result.metadata.pivotX = 0.0f;
    result.metadata.pivotY = result.validation.boundsMinY;
    result.metadata.pivotZ = 0.0f;
    result.metadata.colliderMinX = result.validation.boundsMinX;
    result.metadata.colliderMinY = result.validation.boundsMinY;
    result.metadata.colliderMinZ = result.validation.boundsMinZ;
    result.metadata.colliderMaxX = result.validation.boundsMaxX;
    result.metadata.colliderMaxY = result.validation.boundsMaxY;
    result.metadata.colliderMaxZ = result.validation.boundsMaxZ;
    result.metadata.lod0Vertices = result.validation.vertices;
    result.metadata.lod0Triangles = result.validation.triangles;
    result.metadata.lodProxyVertices = VertexCount(result.lodProxyMesh);
    result.metadata.lodProxyTriangles = TriangleCount(result.lodProxyMesh);

    std::ofstream report(result.reportPath, std::ios::binary);
    report << MakeMarkdownReport(result);
    std::ofstream manifest(result.manifestPath, std::ios::binary);
    result.ok = true;
    result.message = "Generic game asset generation finished.";
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
    if (!warnings.empty()) {
        o << "### Warnings\n\n";
        for (const auto& w : warnings) o << "- " << w << "\n";
    }
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
