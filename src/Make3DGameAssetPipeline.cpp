#include "Make3DGameAssetPipeline.h"
#include "Make3DGltfMaterialExporter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <tuple>
#include <vector>

namespace make3d {

namespace fs = std::filesystem;

namespace {

constexpr float Pi = 3.14159265358979323846f;

struct MaskStats {
    int pixels = 0;
    int minX = 0;
    int minY = 0;
    int maxX = -1;
    int maxY = -1;
    float coverage = 0.0f;
    float aspect = 1.0f;
    float rectangularity = 0.0f;
    float symmetry = 0.0f;
    float horizontalLineScore = 0.0f;
    float verticalLineScore = 0.0f;
};

static int Idx(int x, int y, int w) { return y * w + x; }

static int VertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
static int TriangleCount(const MeshData& mesh) { return static_cast<int>(mesh.indices.size() / 3); }

static std::string EscapeJson(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '\\') o << "\\\\";
        else if (c == '"') o << "\\\"";
        else if (c == '\n') o << "\\n";
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

static void AddBox(MeshData& mesh, float cx, float cy, float cz, float sx, float sy, float sz) {
    int base = VertexCount(mesh);
    float x0 = cx - sx * 0.5f, x1 = cx + sx * 0.5f;
    float y0 = cy - sy * 0.5f, y1 = cy + sy * 0.5f;
    float z0 = cz - sz * 0.5f, z1 = cz + sz * 0.5f;
    const float p[8][3] = {{x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}};
    for (int i = 0; i < 8; ++i) AddVertex(mesh, p[i][0], p[i][1], p[i][2], (i & 1) ? 1.0f : 0.0f, (i & 2) ? 1.0f : 0.0f);
    const int f[12][3] = {{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{3,7,6},{3,6,2},{1,2,6},{1,6,5},{0,4,7},{0,7,3}};
    for (auto& t : f) AddTri(mesh, base + t[0], base + t[1], base + t[2]);
}

static void AddCylinder(MeshData& mesh, float cx, float cy, float cz, float rx, float rz, float height, int segments) {
    int base = VertexCount(mesh);
    segments = std::max(8, segments);
    for (int y = 0; y < 2; ++y) {
        float yy = cy + (y == 0 ? -height * 0.5f : height * 0.5f);
        for (int s = 0; s < segments; ++s) {
            float a = 2.0f * Pi * static_cast<float>(s) / static_cast<float>(segments);
            AddVertex(mesh, cx + std::cos(a) * rx, yy, cz + std::sin(a) * rz, static_cast<float>(s) / segments, static_cast<float>(y));
        }
    }
    int bottom = VertexCount(mesh); AddVertex(mesh, cx, cy - height * 0.5f, cz, 0.5f, 0.5f);
    int top = VertexCount(mesh); AddVertex(mesh, cx, cy + height * 0.5f, cz, 0.5f, 0.5f);
    for (int s = 0; s < segments; ++s) {
        int n = (s + 1) % segments;
        AddTri(mesh, base + s, base + segments + s, base + n);
        AddTri(mesh, base + n, base + segments + s, base + segments + n);
        AddTri(mesh, bottom, base + n, base + s);
        AddTri(mesh, top, base + segments + s, base + segments + n);
    }
}

static MaskStats AnalyzeMaskForAsset(const ImageRGBA& image, const std::vector<std::uint8_t>& mask) {
    MaskStats s;
    int w = image.width, h = image.height;
    if (w <= 0 || h <= 0 || mask.size() != static_cast<size_t>(w * h)) return s;
    s.minX = w; s.minY = h;
    std::vector<int> rowCount(static_cast<size_t>(h), 0), colCount(static_cast<size_t>(w), 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!mask[static_cast<size_t>(Idx(x, y, w))]) continue;
            ++s.pixels; ++rowCount[static_cast<size_t>(y)]; ++colCount[static_cast<size_t>(x)];
            s.minX = std::min(s.minX, x); s.minY = std::min(s.minY, y);
            s.maxX = std::max(s.maxX, x); s.maxY = std::max(s.maxY, y);
        }
    }
    if (s.pixels <= 0) return s;
    int bw = std::max(1, s.maxX - s.minX + 1);
    int bh = std::max(1, s.maxY - s.minY + 1);
    s.coverage = static_cast<float>(s.pixels) / static_cast<float>(w * h);
    s.aspect = static_cast<float>(bh) / static_cast<float>(bw);
    s.rectangularity = static_cast<float>(s.pixels) / static_cast<float>(bw * bh);
    int cx = (s.minX + s.maxX) / 2;
    int hit = 0, total = 0;
    for (int y = s.minY; y <= s.maxY; ++y) for (int x = s.minX; x <= s.maxX; ++x) {
        int mx = cx - (x - cx);
        if (mx < 0 || mx >= w) continue;
        bool a = mask[static_cast<size_t>(Idx(x, y, w))] != 0;
        bool b = mask[static_cast<size_t>(Idx(mx, y, w))] != 0;
        if (a || b) { ++total; if (a == b) ++hit; }
    }
    s.symmetry = total > 0 ? static_cast<float>(hit) / static_cast<float>(total) : 0.0f;
    int strongRows = 0, strongCols = 0;
    for (int y = s.minY; y <= s.maxY; ++y) if (rowCount[static_cast<size_t>(y)] > bw * 70 / 100) ++strongRows;
    for (int x = s.minX; x <= s.maxX; ++x) if (colCount[static_cast<size_t>(x)] > bh * 70 / 100) ++strongCols;
    s.horizontalLineScore = static_cast<float>(strongRows) / static_cast<float>(bh);
    s.verticalLineScore = static_cast<float>(strongCols) / static_cast<float>(bw);
    return s;
}

static void AppendPart(std::vector<GameAssetPart>& parts, const std::string& name, const std::string& material, int v0, int t0, const MeshData& mesh) {
    GameAssetPart p;
    p.name = name;
    p.semanticMaterial = material;
    p.firstVertex = v0;
    p.vertexCount = VertexCount(mesh) - v0;
    p.firstTriangle = t0;
    p.triangleCount = TriangleCount(mesh) - t0;
    if (p.vertexCount > 0 && p.triangleCount > 0) parts.push_back(p);
}

static MeshData BuildBuildingMesh(const MaskStats& stats, std::vector<GameAssetPart>& parts) {
    MeshData mesh;
    float height = 2.2f;
    float width = std::clamp(1.25f / std::max(0.4f, stats.aspect), 0.85f, 2.2f);
    float depth = std::clamp(width * 0.55f, 0.55f, 1.2f);
    int v0 = VertexCount(mesh), t0 = TriangleCount(mesh);
    AddBox(mesh, 0.0f, height * 0.5f, 0.0f, width, height, depth);
    AppendPart(parts, "building_body", "wall", v0, t0, mesh);

    v0 = VertexCount(mesh); t0 = TriangleCount(mesh);
    AddBox(mesh, 0.0f, height + 0.18f, 0.0f, width * 1.12f, 0.32f, depth * 1.10f);
    AppendPart(parts, "roof_or_top_mass", "roof", v0, t0, mesh);

    int columns = std::max(2, std::min(5, static_cast<int>(width * 2.5f)));
    int rows = std::max(2, std::min(6, static_cast<int>(height * 2.0f)));
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < columns; ++c) {
            float x = -width * 0.36f + width * 0.72f * (columns == 1 ? 0.5f : static_cast<float>(c) / static_cast<float>(columns - 1));
            float y = height * 0.25f + height * 0.55f * static_cast<float>(r) / static_cast<float>(std::max(1, rows - 1));
            v0 = VertexCount(mesh); t0 = TriangleCount(mesh);
            AddBox(mesh, x, y, depth * 0.51f, width * 0.12f, height * 0.08f, 0.035f);
            AppendPart(parts, "window_" + std::to_string(r) + "_" + std::to_string(c), "window", v0, t0, mesh);
        }
    }
    v0 = VertexCount(mesh); t0 = TriangleCount(mesh);
    AddBox(mesh, 0.0f, height * 0.11f, depth * 0.53f, width * 0.18f, height * 0.22f, 0.05f);
    AppendPart(parts, "door", "door", v0, t0, mesh);
    RecomputeNormals(mesh);
    return mesh;
}

static MeshData BuildPropMesh(const MaskStats& stats, std::vector<GameAssetPart>& parts) {
    MeshData mesh;
    float width = 1.35f;
    float height = std::clamp(stats.aspect * 1.1f, 0.7f, 2.1f);
    float depth = 0.8f;
    int v0 = VertexCount(mesh), t0 = TriangleCount(mesh);
    if (stats.symmetry > 0.62f && stats.rectangularity < 0.62f) {
        AddCylinder(mesh, 0.0f, height * 0.5f, 0.0f, width * 0.34f, depth * 0.34f, height, 24);
        AppendPart(parts, "prop_rotational_body", "primary", v0, t0, mesh);
    } else {
        AddBox(mesh, 0.0f, height * 0.5f, 0.0f, width, height, depth);
        AppendPart(parts, "prop_box_body", "primary", v0, t0, mesh);
    }
    v0 = VertexCount(mesh); t0 = TriangleCount(mesh);
    AddBox(mesh, 0.0f, height + 0.06f, 0.0f, width * 0.65f, 0.12f, depth * 0.65f);
    AppendPart(parts, "prop_detail_cap", "secondary", v0, t0, mesh);
    RecomputeNormals(mesh);
    return mesh;
}

static MeshData BuildEnvironmentPieceMesh(const MaskStats& stats, std::vector<GameAssetPart>& parts) {
    MeshData mesh;
    float width = std::clamp(2.0f / std::max(0.4f, stats.aspect), 1.4f, 3.4f);
    int v0 = VertexCount(mesh), t0 = TriangleCount(mesh);
    AddBox(mesh, 0.0f, 0.06f, 0.0f, width, 0.12f, 1.2f);
    AppendPart(parts, "environment_base", "ground_or_platform", v0, t0, mesh);
    v0 = VertexCount(mesh); t0 = TriangleCount(mesh);
    AddBox(mesh, 0.0f, 0.55f, -0.50f, width * 0.95f, 1.0f, 0.12f);
    AppendPart(parts, "environment_backing", "wall_or_backdrop", v0, t0, mesh);
    RecomputeNormals(mesh);
    return mesh;
}

static MeshData BuildCollisionFromMeshBounds(const MeshData& mesh) {
    MeshData collision;
    if (mesh.positions.empty()) return collision;
    float minX = std::numeric_limits<float>::max(), minY = minX, minZ = minX;
    float maxX = -minX, maxY = -minX, maxZ = -minX;
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        minX = std::min(minX, mesh.positions[i + 0]); maxX = std::max(maxX, mesh.positions[i + 0]);
        minY = std::min(minY, mesh.positions[i + 1]); maxY = std::max(maxY, mesh.positions[i + 1]);
        minZ = std::min(minZ, mesh.positions[i + 2]); maxZ = std::max(maxZ, mesh.positions[i + 2]);
    }
    AddBox(collision, (minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f, std::max(0.01f, maxX - minX), std::max(0.01f, maxY - minY), std::max(0.01f, maxZ - minZ));
    RecomputeNormals(collision);
    return collision;
}

static MeshData BuildLodFromMesh(const MeshData& mesh) {
    MeshData lod;
    if (mesh.positions.empty() || mesh.indices.empty()) return lod;
    lod.positions = mesh.positions;
    lod.normals = mesh.normals;
    lod.uvs = mesh.uvs;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 6) {
        lod.indices.push_back(mesh.indices[i + 0]);
        lod.indices.push_back(mesh.indices[i + 1]);
        lod.indices.push_back(mesh.indices[i + 2]);
    }
    if (lod.indices.empty() && mesh.indices.size() >= 3) lod.indices.assign(mesh.indices.begin(), mesh.indices.begin() + 3);
    RecomputeNormals(lod);
    return lod;
}

static std::string MakeGameAssetReport(const GameAssetResult& r) {
    std::ostringstream o;
    o << "# Make3D Game Asset Report\n\n";
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| Asset type | " << ToString(r.assetType) << " |\n";
    o << "| OK | " << (r.ok ? "yes" : "no") << " |\n";
    o << "| Parts | " << r.parts.size() << " |\n";
    o << "| Vertices | " << VertexCount(r.mesh) << " |\n";
    o << "| Triangles | " << TriangleCount(r.mesh) << " |\n";
    o << "\n## Parts\n\n";
    o << "| Name | Semantic material | Vertices | Triangles |\n|---|---|---:|---:|\n";
    for (const auto& p : r.parts) o << "| " << p.name << " | " << p.semanticMaterial << " | " << p.vertexCount << " | " << p.triangleCount << " |\n";
    o << "\n## Validation\n\n" << r.validation.ToMarkdown() << "\n";
    o << "## Output files\n\n";
    if (!r.objPath.empty()) o << "- " << r.objPath.generic_string() << "\n";
    if (!r.gltfPath.empty()) o << "- " << r.gltfPath.generic_string() << "\n";
    if (!r.collisionObjPath.empty()) o << "- " << r.collisionObjPath.generic_string() << "\n";
    if (!r.lodObjPath.empty()) o << "- " << r.lodObjPath.generic_string() << "\n";
    return o.str();
}

} // namespace

const char* ToString(GameAssetType type) {
    switch (type) {
        case GameAssetType::Character: return "Character";
        case GameAssetType::Building: return "Building";
        case GameAssetType::Prop: return "Prop";
        case GameAssetType::EnvironmentPiece: return "EnvironmentPiece";
        case GameAssetType::Unknown: return "Unknown";
    }
    return "Unknown";
}

GameAssetType ClassifyGameAssetType(const ImageRGBA& image, const std::vector<std::uint8_t>& refinedMask, const ShapeInferenceResult& shapeInference) {
    MaskStats stats = AnalyzeMaskForAsset(image, refinedMask);
    if (stats.pixels <= 0) return GameAssetType::Unknown;
    if (shapeInference.shapeClass == ShapeClass::Character && stats.aspect > 1.15f && stats.symmetry > 0.48f) return GameAssetType::Character;
    bool buildingLike = stats.rectangularity > 0.58f && stats.verticalLineScore > 0.12f && stats.horizontalLineScore > 0.12f && stats.aspect > 0.55f;
    if (buildingLike || (stats.coverage > 0.18f && stats.rectangularity > 0.70f && stats.aspect > 0.65f)) return GameAssetType::Building;
    if (stats.aspect < 0.45f || (stats.coverage > 0.28f && stats.aspect < 0.75f)) return GameAssetType::EnvironmentPiece;
    return GameAssetType::Prop;
}

GameAssetValidationReport ValidateGameAsset(const MeshData& mesh, const std::vector<GameAssetPart>& parts, const MeshData& collisionMesh, const MeshData& lodMesh, int triangleBudget) {
    GameAssetValidationReport r;
    r.vertices = VertexCount(mesh);
    r.triangles = TriangleCount(mesh);
    r.parts = static_cast<int>(parts.size());
    r.materialSlots = std::max(1, r.parts);
    r.estimatedDrawCalls = r.materialSlots;
    r.hasNormals = mesh.normals.size() == mesh.positions.size() && !mesh.normals.empty();
    r.hasUvs = mesh.uvs.size() / 2 == mesh.positions.size() / 3 && !mesh.uvs.empty();
    r.hasCollisionMesh = !collisionMesh.positions.empty() && !collisionMesh.indices.empty();
    r.hasLodMesh = !lodMesh.positions.empty() && !lodMesh.indices.empty();
    r.withinTriangleBudget = r.triangles > 0 && r.triangles <= triangleBudget;
    r.editableParts = r.parts >= 1;
    std::map<std::tuple<int, int, int>, int> buckets;
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        auto key = std::make_tuple(static_cast<int>(std::round(mesh.positions[i + 0] * 1000.0f)), static_cast<int>(std::round(mesh.positions[i + 1] * 1000.0f)), static_cast<int>(std::round(mesh.positions[i + 2] * 1000.0f)));
        int& count = buckets[key];
        ++count;
        if (count == 2) ++r.duplicateVertexBuckets;
    }
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::uint32_t a = mesh.indices[i + 0], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
        if (a == b || b == c || c == a) { ++r.degenerateTriangles; continue; }
        if ((static_cast<size_t>(a) + 1) * 3 > mesh.positions.size() || (static_cast<size_t>(b) + 1) * 3 > mesh.positions.size() || (static_cast<size_t>(c) + 1) * 3 > mesh.positions.size()) { ++r.degenerateTriangles; continue; }
    }
    float score = 0.0f;
    if (r.vertices > 0 && r.triangles > 0) score += 0.22f;
    if (r.hasNormals) score += 0.14f;
    if (r.hasUvs) score += 0.12f;
    if (r.editableParts) score += 0.16f;
    if (r.hasCollisionMesh) score += 0.14f;
    if (r.hasLodMesh) score += 0.10f;
    if (r.withinTriangleBudget) score += 0.08f;
    if (r.degenerateTriangles == 0) score += 0.04f;
    r.gameReadyScore = std::min(1.0f, score);
    r.gameReadyCandidate = r.gameReadyScore >= 0.76f;
    if (!r.hasCollisionMesh) r.warnings.push_back("Missing collision mesh.");
    if (!r.hasLodMesh) r.warnings.push_back("Missing LOD mesh.");
    if (!r.withinTriangleBudget) r.warnings.push_back("Triangle count is outside the configured game budget.");
    if (r.degenerateTriangles > 0) r.warnings.push_back("Degenerate or invalid triangles detected.");
    if (r.duplicateVertexBuckets > std::max(16, r.vertices / 8)) r.warnings.push_back("Many duplicate vertex buckets detected; cleanup/merge pass should be improved.");
    return r;
}

GameAssetResult BuildGameAssetFromImage(const fs::path& colorPath, const std::optional<fs::path>& depthPath, const fs::path& outputDir, const GameAssetGenerationOptions& options) {
    GameAssetResult result;
    std::string error;
    auto color = LoadImageRGBA(colorPath, &error);
    if (!color) { result.message = error; return result; }
    std::optional<DepthImage> providedDepth;
    if (depthPath) providedDepth = LoadDepthImage(*depthPath, &error);
    fs::create_directories(outputDir);
    std::vector<std::uint8_t> mask = BuildForegroundMask(*color, &result.reconstructionReport);
    if (result.reconstructionReport.foregroundPixels <= 0) { result.message = "Foreground extraction failed."; return result; }
    result.maskReport = RefineForegroundMask(mask, color->width, color->height, options.maskRefine);
    DepthImage depth = PrepareDepth(*color, providedDepth, mask, options.reconstruction, &result.reconstructionReport);
    result.shapeInference = RunShapeInference(*color, mask, depth, ShapeInferenceOptions{});
    result.assetType = options.enableAutoTypeClassification ? ClassifyGameAssetType(*color, mask, result.shapeInference) : options.forcedType;
    if (options.forcedType != GameAssetType::Unknown) result.assetType = options.forcedType;
    MaskStats stats = AnalyzeMaskForAsset(*color, mask);
    if (result.assetType == GameAssetType::Character) {
        result.mesh = BuildHeroCharacterMesh(*color, depth, mask, options.heroCharacter, nullptr);
        result.parts.push_back({"character_hero_body", "character_semantic", 0, VertexCount(result.mesh), 0, TriangleCount(result.mesh)});
    } else if (result.assetType == GameAssetType::Building) {
        result.mesh = BuildBuildingMesh(stats, result.parts);
    } else if (result.assetType == GameAssetType::EnvironmentPiece) {
        result.mesh = BuildEnvironmentPieceMesh(stats, result.parts);
    } else {
        result.assetType = GameAssetType::Prop;
        result.mesh = BuildPropMesh(stats, result.parts);
    }
    if (result.mesh.positions.empty() || result.mesh.indices.empty()) { result.message = "Game asset mesh generation failed."; return result; }
    result.collisionMesh = BuildCollisionFromMeshBounds(result.mesh);
    result.lodMesh = BuildLodFromMesh(result.mesh);
    result.validation = ValidateGameAsset(result.mesh, result.parts, result.collisionMesh, result.lodMesh, options.triangleBudget);
    std::string stem = std::string("make3d_") + ToString(result.assetType);
    std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (options.exportObj) {
        result.objPath = outputDir / (stem + ".obj");
        if (!ExportOBJ(result.mesh, result.objPath, "", &error)) { result.message = error; return result; }
    }
    if (options.exportGltf) {
        result.gltfPath = outputDir / (stem + ".gltf");
        GltfMaterialOptions material;
        material.materialName = std::string("Make3D") + ToString(result.assetType) + "GameAssetMaterial";
        material.baseColorFactor = {0.72f, 0.76f, 0.78f, 1.0f};
        if (!ExportGLTFWithMaterial(result.mesh, result.gltfPath, material, &error)) { result.message = error; return result; }
    }
    if (options.exportCollisionObj) {
        result.collisionObjPath = outputDir / (stem + "_collision.obj");
        if (!ExportOBJ(result.collisionMesh, result.collisionObjPath, "", &error)) { result.message = error; return result; }
    }
    if (options.exportLodObj) {
        result.lodObjPath = outputDir / (stem + "_lod1.obj");
        if (!ExportOBJ(result.lodMesh, result.lodObjPath, "", &error)) { result.message = error; return result; }
    }
    if (options.writeReport) {
        result.reportPath = outputDir / "game_asset_report.md";
        std::ofstream report(result.reportPath, std::ios::binary);
        report << MakeGameAssetReport(result);
    }
    result.ok = true;
    result.message = "Game asset pipeline finished.";
    return result;
}

std::string GameAssetValidationReport::ToMarkdown() const {
    std::ostringstream o;
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| Vertices | " << vertices << " |\n";
    o << "| Triangles | " << triangles << " |\n";
    o << "| Parts | " << parts << " |\n";
    o << "| Material slots | " << materialSlots << " |\n";
    o << "| Estimated draw calls | " << estimatedDrawCalls << " |\n";
    o << "| Has normals | " << (hasNormals ? "yes" : "no") << " |\n";
    o << "| Has UVs | " << (hasUvs ? "yes" : "no") << " |\n";
    o << "| Has collision mesh | " << (hasCollisionMesh ? "yes" : "no") << " |\n";
    o << "| Has LOD mesh | " << (hasLodMesh ? "yes" : "no") << " |\n";
    o << "| Within triangle budget | " << (withinTriangleBudget ? "yes" : "no") << " |\n";
    o << "| Game-ready candidate | " << (gameReadyCandidate ? "yes" : "no") << " |\n";
    o << "| Game-ready score | " << std::fixed << std::setprecision(2) << gameReadyScore << " |\n";
    if (!warnings.empty()) {
        o << "\nWarnings:\n";
        for (const auto& w : warnings) o << "- " << w << "\n";
    }
    return o.str();
}

std::string GameAssetValidationReport::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"vertices\": " << vertices << ",\n";
    o << "  \"triangles\": " << triangles << ",\n";
    o << "  \"parts\": " << parts << ",\n";
    o << "  \"materialSlots\": " << materialSlots << ",\n";
    o << "  \"estimatedDrawCalls\": " << estimatedDrawCalls << ",\n";
    o << "  \"hasNormals\": " << (hasNormals ? "true" : "false") << ",\n";
    o << "  \"hasUvs\": " << (hasUvs ? "true" : "false") << ",\n";
    o << "  \"hasCollisionMesh\": " << (hasCollisionMesh ? "true" : "false") << ",\n";
    o << "  \"hasLodMesh\": " << (hasLodMesh ? "true" : "false") << ",\n";
    o << "  \"gameReadyCandidate\": " << (gameReadyCandidate ? "true" : "false") << ",\n";
    o << "  \"gameReadyScore\": " << gameReadyScore << ",\n";
    o << "  \"warnings\": [";
    for (size_t i = 0; i < warnings.size(); ++i) {
        if (i) o << ", ";
        o << "\"" << EscapeJson(warnings[i]) << "\"";
    }
    o << "]\n}";
    return o.str();
}

} // namespace make3d
