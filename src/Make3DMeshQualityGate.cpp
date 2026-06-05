#include "Make3DMeshQualityGate.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>

namespace make3d {

namespace {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

static int VertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
static int TriangleCount(const MeshData& mesh) { return static_cast<int>(mesh.indices.size() / 3); }

static Vec3 Pos(const MeshData& mesh, std::uint32_t i) {
    size_t p = static_cast<size_t>(i) * 3;
    return {mesh.positions[p + 0], mesh.positions[p + 1], mesh.positions[p + 2]};
}

static float Dist(Vec3 a, Vec3 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static float Area2(Vec3 a, Vec3 b, Vec3 c) {
    Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
    Vec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
    Vec3 cr{ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};
    return std::sqrt(cr.x * cr.x + cr.y * cr.y + cr.z * cr.z);
}

static std::string EscapeJson(const std::string& value) {
    std::ostringstream o;
    for (char c : value) {
        if (c == '\\') o << "\\\\";
        else if (c == '"') o << "\\\"";
        else if (c == '\n') o << "\\n";
        else o << c;
    }
    return o.str();
}

} // namespace

MeshQualityGateReport AnalyzeMeshQualityGate(const MeshData& mesh, const MeshQualityGateOptions& options) {
    MeshQualityGateReport r;
    r.vertices = VertexCount(mesh);
    r.triangles = TriangleCount(mesh);

    if (r.vertices <= 0 || r.triangles < options.minimumTriangles) {
        r.warnings.push_back("Mesh does not have enough geometry for a safe output asset.");
        return r;
    }

    if (mesh.positions.size() != static_cast<size_t>(r.vertices) * 3) {
        r.warnings.push_back("Mesh position buffer is malformed.");
        return r;
    }

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();

    for (int i = 0; i < r.vertices; ++i) {
        float x = mesh.positions[static_cast<size_t>(i) * 3 + 0];
        float y = mesh.positions[static_cast<size_t>(i) * 3 + 1];
        float z = mesh.positions[static_cast<size_t>(i) * 3 + 2];
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
            r.warnings.push_back("Mesh contains non-finite vertex positions.");
            return r;
        }
        minX = std::min(minX, x); minY = std::min(minY, y); minZ = std::min(minZ, z);
        maxX = std::max(maxX, x); maxY = std::max(maxY, y); maxZ = std::max(maxZ, z);
    }

    float sx = maxX - minX;
    float sy = maxY - minY;
    float sz = maxZ - minZ;
    r.boundsDiagonal = std::sqrt(sx * sx + sy * sy + sz * sz);
    if (!(r.boundsDiagonal > 1.0e-6f)) {
        r.warnings.push_back("Mesh bounds are collapsed.");
        return r;
    }

    const float longEdgeLimit = r.boundsDiagonal * std::max(0.05f, options.maxEdgeToBoundsDiagonal);

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::uint32_t ia = mesh.indices[i + 0];
        std::uint32_t ib = mesh.indices[i + 1];
        std::uint32_t ic = mesh.indices[i + 2];
        if (ia >= static_cast<std::uint32_t>(r.vertices) || ib >= static_cast<std::uint32_t>(r.vertices) || ic >= static_cast<std::uint32_t>(r.vertices)) {
            r.warnings.push_back("Mesh contains invalid triangle indices.");
            return r;
        }
        Vec3 a = Pos(mesh, ia);
        Vec3 b = Pos(mesh, ib);
        Vec3 c = Pos(mesh, ic);
        float e0 = Dist(a, b);
        float e1 = Dist(b, c);
        float e2 = Dist(c, a);
        float longest = std::max(e0, std::max(e1, e2));
        float shortest = std::max(1.0e-8f, std::min(e0, std::min(e1, e2)));
        float aspect = longest / shortest;
        float area2 = Area2(a, b, c);
        float altitude = area2 / std::max(1.0e-8f, longest);

        r.maxEdgeLength = std::max(r.maxEdgeLength, longest);
        r.maxTriangleAspectRatio = std::max(r.maxTriangleAspectRatio, aspect);
        if (longest > longEdgeLimit) ++r.longEdgeTriangles;
        if (aspect > options.maxTriangleAspectRatio) ++r.thinTriangles;
        if (longest > longEdgeLimit && altitude < longest * 0.03f) ++r.spikeLikeTriangles;
    }

    float triCount = static_cast<float>(std::max(1, r.triangles));
    float longRatio = static_cast<float>(r.longEdgeTriangles) / triCount;
    float thinRatio = static_cast<float>(r.thinTriangles) / triCount;
    float spikeRatio = static_cast<float>(r.spikeLikeTriangles) / triCount;

    if (longRatio > options.maxLongEdgeTriangleRatio) r.warnings.push_back("Mesh has too many long-edge triangles; likely shredded fallback output.");
    if (thinRatio > options.maxThinTriangleRatio) r.warnings.push_back("Mesh has too many thin triangles; likely spike-like output.");
    if (spikeRatio > options.maxSpikeTriangleRatio) r.warnings.push_back("Mesh contains spike-like triangles above the safe-output threshold.");

    r.ok = r.warnings.empty();
    return r;
}

std::string MeshQualityGateReport::ToMarkdown() const {
    std::ostringstream o;
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| OK | " << (ok ? "yes" : "no") << " |\n";
    o << "| Vertices | " << vertices << " |\n";
    o << "| Triangles | " << triangles << " |\n";
    o << "| Long-edge triangles | " << longEdgeTriangles << " |\n";
    o << "| Thin triangles | " << thinTriangles << " |\n";
    o << "| Spike-like triangles | " << spikeLikeTriangles << " |\n";
    o << "| Bounds diagonal | " << boundsDiagonal << " |\n";
    o << "| Max edge length | " << maxEdgeLength << " |\n";
    o << "| Max triangle aspect ratio | " << maxTriangleAspectRatio << " |\n";
    if (!warnings.empty()) {
        o << "\n### Warnings\n\n";
        for (const auto& w : warnings) o << "- " << w << "\n";
    }
    return o.str();
}

std::string MeshQualityGateReport::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"ok\": " << (ok ? "true" : "false") << ",\n";
    o << "  \"vertices\": " << vertices << ",\n";
    o << "  \"triangles\": " << triangles << ",\n";
    o << "  \"longEdgeTriangles\": " << longEdgeTriangles << ",\n";
    o << "  \"thinTriangles\": " << thinTriangles << ",\n";
    o << "  \"spikeLikeTriangles\": " << spikeLikeTriangles << ",\n";
    o << "  \"boundsDiagonal\": " << boundsDiagonal << ",\n";
    o << "  \"maxEdgeLength\": " << maxEdgeLength << ",\n";
    o << "  \"maxTriangleAspectRatio\": " << maxTriangleAspectRatio << ",\n";
    o << "  \"warnings\": [";
    for (size_t i = 0; i < warnings.size(); ++i) {
        if (i) o << ", ";
        o << "\"" << EscapeJson(warnings[i]) << "\"";
    }
    o << "]\n}";
    return o.str();
}

} // namespace make3d
