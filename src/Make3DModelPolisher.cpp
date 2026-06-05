#include "Make3DModelPolisher.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace make3d {

namespace {

struct EdgeKey {
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    bool operator==(const EdgeKey& other) const { return a == other.a && b == other.b; }
};

struct EdgeHash {
    size_t operator()(const EdgeKey& e) const {
        return (static_cast<size_t>(e.a) << 32u) ^ static_cast<size_t>(e.b);
    }
};

static EdgeKey MakeEdge(std::uint32_t a, std::uint32_t b) {
    if (a > b) std::swap(a, b);
    return {a, b};
}

static int VertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
static int TriangleCount(const MeshData& mesh) { return static_cast<int>(mesh.indices.size() / 3); }

static void CopyVertex(MeshData& dst, const MeshData& src, std::uint32_t i) {
    size_t p = static_cast<size_t>(i) * 3;
    size_t t = static_cast<size_t>(i) * 2;
    dst.positions.insert(dst.positions.end(), {src.positions[p], src.positions[p + 1], src.positions[p + 2]});
    if (p + 2 < src.normals.size()) dst.normals.insert(dst.normals.end(), {src.normals[p], src.normals[p + 1], src.normals[p + 2]});
    else dst.normals.insert(dst.normals.end(), {0.0f, 1.0f, 0.0f});
    if (t + 1 < src.uvs.size()) dst.uvs.insert(dst.uvs.end(), {src.uvs[t], src.uvs[t + 1]});
    else dst.uvs.insert(dst.uvs.end(), {0.0f, 0.0f});
}

static std::vector<std::vector<int>> BuildTriangleComponents(const MeshData& mesh) {
    int triCount = TriangleCount(mesh);
    std::unordered_map<EdgeKey, std::vector<int>, EdgeHash> edgeToTriangles;
    for (int ti = 0; ti < triCount; ++ti) {
        std::uint32_t a = mesh.indices[static_cast<size_t>(ti) * 3 + 0];
        std::uint32_t b = mesh.indices[static_cast<size_t>(ti) * 3 + 1];
        std::uint32_t c = mesh.indices[static_cast<size_t>(ti) * 3 + 2];
        edgeToTriangles[MakeEdge(a, b)].push_back(ti);
        edgeToTriangles[MakeEdge(b, c)].push_back(ti);
        edgeToTriangles[MakeEdge(c, a)].push_back(ti);
    }

    std::vector<std::vector<int>> adjacency(static_cast<size_t>(triCount));
    for (const auto& entry : edgeToTriangles) {
        const auto& faces = entry.second;
        for (size_t i = 0; i < faces.size(); ++i) {
            for (size_t j = i + 1; j < faces.size(); ++j) {
                adjacency[static_cast<size_t>(faces[i])].push_back(faces[j]);
                adjacency[static_cast<size_t>(faces[j])].push_back(faces[i]);
            }
        }
    }

    std::vector<int> visited(static_cast<size_t>(triCount), 0);
    std::vector<std::vector<int>> components;
    for (int start = 0; start < triCount; ++start) {
        if (visited[static_cast<size_t>(start)]) continue;
        std::vector<int> component;
        std::queue<int> q;
        q.push(start);
        visited[static_cast<size_t>(start)] = 1;
        while (!q.empty()) {
            int t = q.front();
            q.pop();
            component.push_back(t);
            for (int n : adjacency[static_cast<size_t>(t)]) {
                if (!visited[static_cast<size_t>(n)]) {
                    visited[static_cast<size_t>(n)] = 1;
                    q.push(n);
                }
            }
        }
        components.push_back(std::move(component));
    }
    return components;
}

static int RemoveSmallComponents(MeshData& mesh, const MeshPolishOptions& options) {
    auto components = BuildTriangleComponents(mesh);
    if (components.empty()) return 0;

    size_t largestIndex = 0;
    for (size_t i = 1; i < components.size(); ++i) {
        if (components[i].size() > components[largestIndex].size()) largestIndex = i;
    }

    std::unordered_set<int> keepTriangles;
    int removed = 0;
    for (size_t ci = 0; ci < components.size(); ++ci) {
        bool keep = options.keepLargestComponentOnly ? (ci == largestIndex) : (static_cast<int>(components[ci].size()) >= options.minComponentTriangles);
        if (keep) {
            for (int t : components[ci]) keepTriangles.insert(t);
        } else {
            removed += static_cast<int>(components[ci].size());
        }
    }

    MeshData out;
    std::vector<std::uint32_t> remap(static_cast<size_t>(VertexCount(mesh)), std::numeric_limits<std::uint32_t>::max());
    auto mapVertex = [&](std::uint32_t oldIndex) -> std::uint32_t {
        if (oldIndex >= remap.size()) return std::numeric_limits<std::uint32_t>::max();
        if (remap[oldIndex] != std::numeric_limits<std::uint32_t>::max()) return remap[oldIndex];
        std::uint32_t ni = static_cast<std::uint32_t>(out.positions.size() / 3);
        CopyVertex(out, mesh, oldIndex);
        remap[oldIndex] = ni;
        return ni;
    };

    int triCount = TriangleCount(mesh);
    for (int ti = 0; ti < triCount; ++ti) {
        if (keepTriangles.find(ti) == keepTriangles.end()) continue;
        std::uint32_t a = mapVertex(mesh.indices[static_cast<size_t>(ti) * 3 + 0]);
        std::uint32_t b = mapVertex(mesh.indices[static_cast<size_t>(ti) * 3 + 1]);
        std::uint32_t c = mapVertex(mesh.indices[static_cast<size_t>(ti) * 3 + 2]);
        if (a == std::numeric_limits<std::uint32_t>::max() || b == std::numeric_limits<std::uint32_t>::max() || c == std::numeric_limits<std::uint32_t>::max()) continue;
        out.indices.insert(out.indices.end(), {a, b, c});
    }

    mesh = std::move(out);
    RecomputeNormals(mesh);
    return removed;
}

static std::unordered_set<std::uint32_t> FindBoundaryVertices(const MeshData& mesh) {
    std::unordered_map<EdgeKey, int, EdgeHash> edgeCounts;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::uint32_t a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
        ++edgeCounts[MakeEdge(a, b)];
        ++edgeCounts[MakeEdge(b, c)];
        ++edgeCounts[MakeEdge(c, a)];
    }
    std::unordered_set<std::uint32_t> boundary;
    for (const auto& e : edgeCounts) {
        if (e.second == 1) {
            boundary.insert(e.first.a);
            boundary.insert(e.first.b);
        }
    }
    return boundary;
}

static void SmoothMesh(MeshData& mesh, const MeshPolishOptions& options) {
    int vCount = VertexCount(mesh);
    if (vCount <= 0 || mesh.indices.empty()) return;

    std::vector<std::vector<std::uint32_t>> neighbors(static_cast<size_t>(vCount));
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::uint32_t a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
        if (a >= static_cast<std::uint32_t>(vCount) || b >= static_cast<std::uint32_t>(vCount) || c >= static_cast<std::uint32_t>(vCount)) continue;
        neighbors[a].push_back(b); neighbors[a].push_back(c);
        neighbors[b].push_back(a); neighbors[b].push_back(c);
        neighbors[c].push_back(a); neighbors[c].push_back(b);
    }
    for (auto& list : neighbors) {
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
    }

    auto boundary = FindBoundaryVertices(mesh);
    std::vector<float> next = mesh.positions;
    for (int iter = 0; iter < options.smoothingIterations; ++iter) {
        next = mesh.positions;
        for (int vi = 0; vi < vCount; ++vi) {
            const auto& ns = neighbors[static_cast<size_t>(vi)];
            if (ns.empty()) continue;
            float ax = 0.0f, ay = 0.0f, az = 0.0f;
            for (std::uint32_t n : ns) {
                size_t p = static_cast<size_t>(n) * 3;
                ax += mesh.positions[p + 0];
                ay += mesh.positions[p + 1];
                az += mesh.positions[p + 2];
            }
            float inv = 1.0f / static_cast<float>(ns.size());
            ax *= inv; ay *= inv; az *= inv;
            float strength = boundary.count(static_cast<std::uint32_t>(vi)) ? options.preserveBoundaryStrength : options.smoothingStrength;
            size_t p = static_cast<size_t>(vi) * 3;
            next[p + 0] = mesh.positions[p + 0] + (ax - mesh.positions[p + 0]) * strength;
            next[p + 1] = mesh.positions[p + 1] + (ay - mesh.positions[p + 1]) * strength;
            next[p + 2] = mesh.positions[p + 2] + (az - mesh.positions[p + 2]) * strength;
        }
        mesh.positions.swap(next);
    }
    RecomputeNormals(mesh);
}

static std::string JsonEscape(const std::string& value) {
    std::ostringstream oss;
    for (char c : value) {
        if (c == '\\') oss << "\\\\";
        else if (c == '"') oss << "\\\"";
        else if (c == '\n') oss << "\\n";
        else oss << c;
    }
    return oss.str();
}

} // namespace

MeshPolishReport PolishMesh(MeshData& mesh, const MeshPolishOptions& options) {
    MeshPolishReport report;
    report.verticesBefore = VertexCount(mesh);
    report.trianglesBefore = TriangleCount(mesh);
    report.connectedComponentsBefore = static_cast<int>(BuildTriangleComponents(mesh).size());

    if (options.cleanupBeforePolish) CleanupMesh(mesh);
    int beforeRemove = TriangleCount(mesh);
    report.removedTriangles += RemoveSmallComponents(mesh, options);
    report.removedComponents = std::max(0, report.connectedComponentsBefore - static_cast<int>(BuildTriangleComponents(mesh).size()));
    if (beforeRemove > TriangleCount(mesh)) report.removedTriangles += beforeRemove - TriangleCount(mesh);

    SmoothMesh(mesh, options);
    report.smoothingIterations = options.smoothingIterations;

    if (options.cleanupAfterPolish) CleanupMesh(mesh);
    MeshValidationReport validation = ValidateMesh(mesh);
    report.verticesAfter = validation.verticesAfter;
    report.trianglesAfter = validation.trianglesAfter;
    report.connectedComponentsAfter = static_cast<int>(BuildTriangleComponents(mesh).size());
    report.validForExport = validation.validForExport;
    report.warnings = validation.warnings;
    return report;
}

std::string MeshPolishReport::ToMarkdown() const {
    std::ostringstream o;
    o << "# Mesh Polish Report\n\n";
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| Vertices before | " << verticesBefore << " |\n";
    o << "| Vertices after | " << verticesAfter << " |\n";
    o << "| Triangles before | " << trianglesBefore << " |\n";
    o << "| Triangles after | " << trianglesAfter << " |\n";
    o << "| Components before | " << connectedComponentsBefore << " |\n";
    o << "| Components after | " << connectedComponentsAfter << " |\n";
    o << "| Removed components | " << removedComponents << " |\n";
    o << "| Removed triangles | " << removedTriangles << " |\n";
    o << "| Smoothing iterations | " << smoothingIterations << " |\n";
    o << "| Valid for export | " << (validForExport ? "yes" : "no") << " |\n";
    if (!warnings.empty()) {
        o << "\n## Warnings\n";
        for (const auto& w : warnings) o << "- " << w << "\n";
    }
    return o.str();
}

std::string MeshPolishReport::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"verticesBefore\": " << verticesBefore << ",\n";
    o << "  \"verticesAfter\": " << verticesAfter << ",\n";
    o << "  \"trianglesBefore\": " << trianglesBefore << ",\n";
    o << "  \"trianglesAfter\": " << trianglesAfter << ",\n";
    o << "  \"connectedComponentsBefore\": " << connectedComponentsBefore << ",\n";
    o << "  \"connectedComponentsAfter\": " << connectedComponentsAfter << ",\n";
    o << "  \"removedComponents\": " << removedComponents << ",\n";
    o << "  \"removedTriangles\": " << removedTriangles << ",\n";
    o << "  \"smoothingIterations\": " << smoothingIterations << ",\n";
    o << "  \"validForExport\": " << (validForExport ? "true" : "false") << ",\n";
    o << "  \"warnings\": [";
    for (size_t i = 0; i < warnings.size(); ++i) {
        if (i) o << ", ";
        o << "\"" << JsonEscape(warnings[i]) << "\"";
    }
    o << "]\n}";
    return o.str();
}

} // namespace make3d
