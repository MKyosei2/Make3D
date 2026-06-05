#include "Make3DCompleteGameAssetPipeline.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

namespace make3d {
namespace {
struct Vec3 { float x, y, z; };
static int VCount(const MeshData& m) { return static_cast<int>(m.positions.size() / 3); }
static int TCount(const MeshData& m) { return static_cast<int>(m.indices.size() / 3); }
static float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
static Vec3 P(const MeshData& m, std::uint32_t i) { size_t p = static_cast<size_t>(i) * 3; return {m.positions[p], m.positions[p + 1], m.positions[p + 2]}; }
static float Area2(Vec3 a, Vec3 b, Vec3 c) {
    Vec3 ab{b.x-a.x,b.y-a.y,b.z-a.z}; Vec3 ac{c.x-a.x,c.y-a.y,c.z-a.z};
    Vec3 cr{ab.y*ac.z-ab.z*ac.y, ab.z*ac.x-ab.x*ac.z, ab.x*ac.y-ab.y*ac.x};
    return cr.x*cr.x + cr.y*cr.y + cr.z*cr.z;
}
static void PlanarUv(MeshData& m, MeshRepairReport& r) {
    int n = VCount(m); if (n <= 0) return;
    float minX=std::numeric_limits<float>::max(), minY=minX, maxX=-minX, maxY=-minX;
    for (int i=0;i<n;++i) { float x=m.positions[(size_t)i*3], y=m.positions[(size_t)i*3+1]; minX=std::min(minX,x); maxX=std::max(maxX,x); minY=std::min(minY,y); maxY=std::max(maxY,y); }
    float sx=std::max(1e-5f,maxX-minX), sy=std::max(1e-5f,maxY-minY);
    m.uvs.assign((size_t)n*2,0.0f);
    for (int i=0;i<n;++i) { float x=m.positions[(size_t)i*3], y=m.positions[(size_t)i*3+1]; m.uvs[(size_t)i*2]=Clamp01((x-minX)/sx); m.uvs[(size_t)i*2+1]=Clamp01((y-minY)/sy); }
    r.regeneratedUvVertices = n;
}
}

MeshRepairReport RepairGameAssetMesh(MeshData& mesh, bool regeneratePlanarUv) {
    MeshRepairReport r;
    r.inputVertices = VCount(mesh); r.inputTriangles = TCount(mesh);
    int n = VCount(mesh); if (n <= 0) { r.warnings.push_back("Input mesh is empty."); return r; }
    for (float& v : mesh.positions) if (!std::isfinite(v)) { v = 0.0f; ++r.repairedNonFinitePositions; }
    std::vector<std::uint32_t> clean; clean.reserve(mesh.indices.size());
    for (size_t i=0;i+2<mesh.indices.size();i+=3) {
        std::uint32_t a=mesh.indices[i], b=mesh.indices[i+1], c=mesh.indices[i+2];
        if (a >= (std::uint32_t)n || b >= (std::uint32_t)n || c >= (std::uint32_t)n) { ++r.removedInvalidTriangles; continue; }
        if (a == b || b == c || c == a || Area2(P(mesh,a), P(mesh,b), P(mesh,c)) < 1e-12f) { ++r.removedDegenerateTriangles; continue; }
        clean.push_back(a); clean.push_back(b); clean.push_back(c);
    }
    mesh.indices.swap(clean);
    if (regeneratePlanarUv) PlanarUv(mesh, r);
    RecomputeNormals(mesh); r.normalsRecomputed = true;
    r.outputVertices = VCount(mesh); r.outputTriangles = TCount(mesh);
    if (r.outputTriangles == 0) r.warnings.push_back("Repair removed all triangles.");
    return r;
}

bool WriteProjectedTexturePPM(const ImageRGBA& color, const std::vector<std::uint8_t>& mask, const std::filesystem::path& texturePath, int textureSize, std::string* error) {
    int s = std::max(16, textureSize);
    if (color.width <= 0 || color.height <= 0 || mask.size() != (size_t)(color.width * color.height)) { if (error) *error = "Invalid image or mask for texture projection."; return false; }
    if (!texturePath.parent_path().empty()) std::filesystem::create_directories(texturePath.parent_path());
    std::ofstream f(texturePath, std::ios::binary); if (!f) { if (error) *error = "Failed to open projected texture."; return false; }
    f << "P6\n" << s << " " << s << "\n255\n";
    for (int y=0;y<s;++y) { int sy=std::clamp((int)(((float)y+0.5f)/(float)s*color.height),0,color.height-1); for (int x=0;x<s;++x) {
        int sx=std::clamp((int)(((float)x+0.5f)/(float)s*color.width),0,color.width-1); size_t ip=((size_t)sy*color.width+sx)*4; bool inside=mask[(size_t)sy*color.width+sx]!=0;
        unsigned char rgb[3] = {
            static_cast<unsigned char>(inside ? color.pixels[ip] : 96),
            static_cast<unsigned char>(inside ? color.pixels[ip + 1] : 96),
            static_cast<unsigned char>(inside ? color.pixels[ip + 2] : 96)
        };
        f.write(reinterpret_cast<const char*>(rgb),3);
    }}
    return true;
}

std::string MeshRepairReport::ToMarkdown() const {
    std::ostringstream o; o << "| Metric | Value |\n|---|---:|\n";
    o << "| Input vertices | " << inputVertices << " |\n| Input triangles | " << inputTriangles << " |\n| Output vertices | " << outputVertices << " |\n| Output triangles | " << outputTriangles << " |\n";
    o << "| Removed invalid triangles | " << removedInvalidTriangles << " |\n| Removed degenerate triangles | " << removedDegenerateTriangles << " |\n| Repaired non-finite positions | " << repairedNonFinitePositions << " |\n| Regenerated UV vertices | " << regeneratedUvVertices << " |\n| Normals recomputed | " << (normalsRecomputed ? "yes" : "no") << " |\n";
    if (!warnings.empty()) { o << "\n### Warnings\n\n"; for (const auto& w : warnings) o << "- " << w << "\n"; }
    return o.str();
}

std::string MeshRepairReport::ToJson() const {
    std::ostringstream o; o << "{\n";
    o << "  \"inputVertices\": " << inputVertices << ",\n  \"inputTriangles\": " << inputTriangles << ",\n  \"outputVertices\": " << outputVertices << ",\n  \"outputTriangles\": " << outputTriangles << ",\n";
    o << "  \"removedInvalidTriangles\": " << removedInvalidTriangles << ",\n  \"removedDegenerateTriangles\": " << removedDegenerateTriangles << ",\n  \"repairedNonFinitePositions\": " << repairedNonFinitePositions << ",\n  \"regeneratedUvVertices\": " << regeneratedUvVertices << ",\n  \"normalsRecomputed\": " << (normalsRecomputed ? "true" : "false") << "\n}";
    return o.str();
}

}