#include "Make3DMeshTools.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <sstream>

namespace make3d {

namespace {

struct V3 { float x, y, z; };

static bool HasVertex(const MeshData& mesh, std::uint32_t i) {
    return (static_cast<size_t>(i) + 1) * 3 <= mesh.positions.size();
}

static V3 Pos(const MeshData& mesh, std::uint32_t i) {
    size_t p = static_cast<size_t>(i) * 3;
    return {mesh.positions[p], mesh.positions[p + 1], mesh.positions[p + 2]};
}

static V3 Sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static V3 Cross(V3 a, V3 b) { return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x}; }
static float Len(V3 v) { return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }
static float Area(V3 a, V3 b, V3 c) { return 0.5f * Len(Cross(Sub(b, a), Sub(c, a))); }

static std::string JsonEscape(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '\\') o << "\\\\";
        else if (c == '"') o << "\\\"";
        else if (c == '\n') o << "\\n";
        else o << c;
    }
    return o.str();
}

} // namespace

MeshValidationReport ValidateMesh(const MeshData& mesh) {
    MeshValidationReport r;
    r.verticesBefore = static_cast<int>(mesh.positions.size() / 3);
    r.verticesAfter = r.verticesBefore;
    r.trianglesBefore = static_cast<int>(mesh.indices.size() / 3);
    r.trianglesAfter = r.trianglesBefore;

    if (r.verticesBefore == 0) r.warnings.push_back("Mesh has no vertices.");
    if (r.trianglesBefore == 0) r.warnings.push_back("Mesh has no triangles.");

    if (r.verticesBefore > 0) {
        r.minX = r.minY = r.minZ = std::numeric_limits<float>::max();
        r.maxX = r.maxY = r.maxZ = -std::numeric_limits<float>::max();
        for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
            r.minX = std::min(r.minX, mesh.positions[i]);
            r.minY = std::min(r.minY, mesh.positions[i + 1]);
            r.minZ = std::min(r.minZ, mesh.positions[i + 2]);
            r.maxX = std::max(r.maxX, mesh.positions[i]);
            r.maxY = std::max(r.maxY, mesh.positions[i + 1]);
            r.maxZ = std::max(r.maxZ, mesh.positions[i + 2]);
        }
    }

    std::map<std::pair<std::uint32_t, std::uint32_t>, int> edges;
    auto addEdge = [&](std::uint32_t a, std::uint32_t b) {
        if (a > b) std::swap(a, b);
        ++edges[{a, b}];
    };

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::uint32_t a = mesh.indices[i];
        std::uint32_t b = mesh.indices[i + 1];
        std::uint32_t c = mesh.indices[i + 2];
        if (!HasVertex(mesh, a) || !HasVertex(mesh, b) || !HasVertex(mesh, c)) { ++r.invalidTriangles; continue; }
        if (a == b || b == c || c == a) { ++r.degenerateTriangles; continue; }
        float area = Area(Pos(mesh, a), Pos(mesh, b), Pos(mesh, c));
        if (area <= 1.0e-10f) { ++r.degenerateTriangles; continue; }
        r.surfaceArea += area;
        addEdge(a, b); addEdge(b, c); addEdge(c, a);
    }

    for (const auto& e : edges) {
        if (e.second == 1) ++r.boundaryEdges;
        else if (e.second > 2) ++r.nonManifoldEdges;
    }

    if (r.invalidTriangles) r.warnings.push_back("Mesh contains invalid triangle indices.");
    if (r.degenerateTriangles) r.warnings.push_back("Mesh contains degenerate triangles.");
    if (r.boundaryEdges) r.warnings.push_back("Mesh has boundary edges and may not be watertight.");
    if (r.nonManifoldEdges) r.warnings.push_back("Mesh has non-manifold edges.");

    r.validForExport = r.verticesBefore > 0 && r.trianglesBefore > 0 && r.invalidTriangles == 0;
    r.watertightCandidate = r.validForExport && r.boundaryEdges == 0 && r.nonManifoldEdges == 0;
    return r;
}

MeshValidationReport CleanupMesh(MeshData& mesh, const MeshCleanupOptions& options) {
    MeshValidationReport before = ValidateMesh(mesh);
    MeshData out;
    std::vector<std::uint32_t> remap(static_cast<size_t>(before.verticesBefore), std::numeric_limits<std::uint32_t>::max());

    auto copyVertex = [&](std::uint32_t oldIndex) -> std::uint32_t {
        if (!HasVertex(mesh, oldIndex)) return std::numeric_limits<std::uint32_t>::max();
        if (oldIndex < remap.size() && remap[oldIndex] != std::numeric_limits<std::uint32_t>::max()) return remap[oldIndex];
        std::uint32_t ni = static_cast<std::uint32_t>(out.positions.size() / 3);
        size_t p = static_cast<size_t>(oldIndex) * 3;
        size_t t = static_cast<size_t>(oldIndex) * 2;
        out.positions.insert(out.positions.end(), {mesh.positions[p], mesh.positions[p + 1], mesh.positions[p + 2]});
        if (p + 2 < mesh.normals.size()) out.normals.insert(out.normals.end(), {mesh.normals[p], mesh.normals[p + 1], mesh.normals[p + 2]});
        else out.normals.insert(out.normals.end(), {0.0f, 1.0f, 0.0f});
        if (t + 1 < mesh.uvs.size()) out.uvs.insert(out.uvs.end(), {mesh.uvs[t], mesh.uvs[t + 1]});
        else out.uvs.insert(out.uvs.end(), {0.0f, 0.0f});
        if (oldIndex < remap.size()) remap[oldIndex] = ni;
        return ni;
    };

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::uint32_t a0 = mesh.indices[i], b0 = mesh.indices[i + 1], c0 = mesh.indices[i + 2];
        if (!HasVertex(mesh, a0) || !HasVertex(mesh, b0) || !HasVertex(mesh, c0)) { if (options.removeInvalidTriangles) continue; }
        std::uint32_t a = copyVertex(a0), b = copyVertex(b0), c = copyVertex(c0);
        if (a == std::numeric_limits<std::uint32_t>::max() || b == std::numeric_limits<std::uint32_t>::max() || c == std::numeric_limits<std::uint32_t>::max()) continue;
        if ((a == b || b == c || c == a) && options.removeDegenerateTriangles) continue;
        if (Area(Pos(out, a), Pos(out, b), Pos(out, c)) <= 1.0e-10f && options.removeDegenerateTriangles) continue;
        out.indices.insert(out.indices.end(), {a, b, c});
    }

    mesh = std::move(out);
    RecomputeNormals(mesh);
    MeshValidationReport after = ValidateMesh(mesh);
    after.verticesBefore = before.verticesBefore;
    after.trianglesBefore = before.trianglesBefore;
    after.unusedVerticesRemoved = std::max(0, before.verticesBefore - after.verticesAfter);
    return after;
}

std::string MeshValidationReport::ToMarkdown() const {
    std::ostringstream o;
    o << "# Mesh Validation Report\n\n";
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| Vertices before | " << verticesBefore << " |\n";
    o << "| Vertices after | " << verticesAfter << " |\n";
    o << "| Triangles before | " << trianglesBefore << " |\n";
    o << "| Triangles after | " << trianglesAfter << " |\n";
    o << "| Invalid triangles | " << invalidTriangles << " |\n";
    o << "| Degenerate triangles | " << degenerateTriangles << " |\n";
    o << "| Boundary edges | " << boundaryEdges << " |\n";
    o << "| Non-manifold edges | " << nonManifoldEdges << " |\n";
    o << "| Surface area | " << surfaceArea << " |\n";
    o << "| Valid for export | " << (validForExport ? "yes" : "no") << " |\n";
    o << "| Watertight candidate | " << (watertightCandidate ? "yes" : "no") << " |\n";
    if (!warnings.empty()) { o << "\n## Warnings\n"; for (const auto& w : warnings) o << "- " << w << "\n"; }
    return o.str();
}

std::string MeshValidationReport::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"verticesBefore\": " << verticesBefore << ",\n";
    o << "  \"verticesAfter\": " << verticesAfter << ",\n";
    o << "  \"trianglesBefore\": " << trianglesBefore << ",\n";
    o << "  \"trianglesAfter\": " << trianglesAfter << ",\n";
    o << "  \"invalidTriangles\": " << invalidTriangles << ",\n";
    o << "  \"degenerateTriangles\": " << degenerateTriangles << ",\n";
    o << "  \"boundaryEdges\": " << boundaryEdges << ",\n";
    o << "  \"nonManifoldEdges\": " << nonManifoldEdges << ",\n";
    o << "  \"surfaceArea\": " << surfaceArea << ",\n";
    o << "  \"validForExport\": " << (validForExport ? "true" : "false") << ",\n";
    o << "  \"watertightCandidate\": " << (watertightCandidate ? "true" : "false") << ",\n";
    o << "  \"warnings\": [";
    for (size_t i = 0; i < warnings.size(); ++i) { if (i) o << ", "; o << "\"" << JsonEscape(warnings[i]) << "\""; }
    o << "]\n}";
    return o.str();
}

} // namespace make3d
