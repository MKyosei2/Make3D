#include "Make3DHeroFineDetailPass.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace make3d {

namespace {

constexpr float Pi = 3.14159265358979323846f;

static int VertexCount(const MeshData& mesh) {
    return static_cast<int>(mesh.positions.size() / 3);
}

static void AddEllipsoid(MeshData& mesh, float cx, float cy, float cz, float rx, float ry, float rz, int segs, int rings) {
    int base = VertexCount(mesh);
    segs = std::max(8, segs);
    rings = std::max(4, rings);
    for (int r = 0; r <= rings; ++r) {
        float v = static_cast<float>(r) / static_cast<float>(rings);
        float phi = -0.5f * Pi + Pi * v;
        float cp = std::cos(phi);
        float sp = std::sin(phi);
        for (int s = 0; s < segs; ++s) {
            float u = static_cast<float>(s) / static_cast<float>(segs);
            float a = 2.0f * Pi * u;
            mesh.positions.push_back(cx + std::cos(a) * cp * rx);
            mesh.positions.push_back(cy + sp * ry);
            mesh.positions.push_back(cz + std::sin(a) * cp * rz);
            mesh.normals.push_back(std::cos(a) * cp);
            mesh.normals.push_back(sp);
            mesh.normals.push_back(std::sin(a) * cp);
            mesh.uvs.push_back(u);
            mesh.uvs.push_back(v);
        }
    }
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segs; ++s) {
            int a = base + r * segs + s;
            int b = base + r * segs + (s + 1) % segs;
            int c = base + (r + 1) * segs + s;
            int d = base + (r + 1) * segs + (s + 1) % segs;
            mesh.indices.push_back(static_cast<std::uint32_t>(a));
            mesh.indices.push_back(static_cast<std::uint32_t>(c));
            mesh.indices.push_back(static_cast<std::uint32_t>(b));
            mesh.indices.push_back(static_cast<std::uint32_t>(b));
            mesh.indices.push_back(static_cast<std::uint32_t>(c));
            mesh.indices.push_back(static_cast<std::uint32_t>(d));
        }
    }
}

static void AddCapsule(MeshData& mesh, float x0, float y0, float z0, float x1, float y1, float z1, float radius, int segs) {
    int base = VertexCount(mesh);
    int rings = 5;
    segs = std::max(8, segs);
    for (int r = 0; r < rings; ++r) {
        float t = static_cast<float>(r) / static_cast<float>(rings - 1);
        float cx = x0 + (x1 - x0) * t;
        float cy = y0 + (y1 - y0) * t;
        float cz = z0 + (z1 - z0) * t;
        float taper = std::sin(Pi * (0.10f + 0.80f * t));
        float rr = radius * std::max(0.30f, taper);
        for (int s = 0; s < segs; ++s) {
            float u = static_cast<float>(s) / static_cast<float>(segs);
            float a = 2.0f * Pi * u;
            mesh.positions.push_back(cx + std::cos(a) * rr);
            mesh.positions.push_back(cy);
            mesh.positions.push_back(cz + std::sin(a) * rr);
            mesh.normals.push_back(std::cos(a));
            mesh.normals.push_back(0.0f);
            mesh.normals.push_back(std::sin(a));
            mesh.uvs.push_back(u);
            mesh.uvs.push_back(t);
        }
    }
    for (int r = 0; r < rings - 1; ++r) {
        for (int s = 0; s < segs; ++s) {
            int a = base + r * segs + s;
            int b = base + r * segs + (s + 1) % segs;
            int c = base + (r + 1) * segs + s;
            int d = base + (r + 1) * segs + (s + 1) % segs;
            mesh.indices.push_back(static_cast<std::uint32_t>(a));
            mesh.indices.push_back(static_cast<std::uint32_t>(c));
            mesh.indices.push_back(static_cast<std::uint32_t>(b));
            mesh.indices.push_back(static_cast<std::uint32_t>(b));
            mesh.indices.push_back(static_cast<std::uint32_t>(c));
            mesh.indices.push_back(static_cast<std::uint32_t>(d));
        }
    }
}

static void AddFaceFeatures(MeshData& mesh, float h, float w, float d, float scale, int segs) {
    float yTop = h * 0.5f;
    float headCenter = yTop - h * 0.12f;
    float faceZ = d * 0.205f;
    AddEllipsoid(mesh, -w * 0.055f, headCenter + h * 0.025f, faceZ, w * 0.020f * scale, h * 0.010f * scale, d * 0.010f * scale, segs, 4);
    AddEllipsoid(mesh,  w * 0.055f, headCenter + h * 0.025f, faceZ, w * 0.020f * scale, h * 0.010f * scale, d * 0.010f * scale, segs, 4);
    AddEllipsoid(mesh, 0.0f, headCenter - h * 0.015f, faceZ + d * 0.010f, w * 0.018f * scale, h * 0.035f * scale, d * 0.020f * scale, segs, 5);
    AddCapsule(mesh, -w * 0.045f, headCenter - h * 0.070f, faceZ, w * 0.045f, headCenter - h * 0.070f, faceZ, w * 0.008f * scale, segs);
}

static void AddHairStrands(MeshData& mesh, float h, float w, float d, float scale, int segs) {
    float yTop = h * 0.5f;
    float headCenter = yTop - h * 0.12f;
    for (int i = -3; i <= 3; ++i) {
        float x = static_cast<float>(i) * w * 0.045f;
        float drop = h * (0.12f + 0.015f * static_cast<float>((i + 3) % 3));
        AddCapsule(mesh, x, headCenter + h * 0.115f, -d * 0.02f, x * 1.25f, headCenter + h * 0.03f - drop, d * 0.10f, w * 0.016f * scale, segs);
    }
}

static void AddClothingFolds(MeshData& mesh, float h, float w, float d, float scale, int segs) {
    for (int i = -3; i <= 3; ++i) {
        float x = static_cast<float>(i) * w * 0.065f;
        AddCapsule(mesh, x, h * 0.145f, d * 0.245f, x * 0.82f, -h * 0.120f, d * 0.260f, w * 0.010f * scale, segs);
    }
    AddCapsule(mesh, -w * 0.30f, h * 0.08f, d * 0.255f, w * 0.30f, h * 0.06f, d * 0.255f, w * 0.008f * scale, segs);
}

static void AddFingerHints(MeshData& mesh, float h, float w, float d, int segs) {
    float yBottom = -h * 0.5f;
    float hip = yBottom + h * 0.33f;
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < 4; ++i) {
            float offset = (static_cast<float>(i) - 1.5f) * w * 0.012f;
            float baseX = side * w * 0.42f;
            AddCapsule(mesh, baseX + offset, hip - h * 0.145f, d * 0.072f, baseX + offset + side * w * 0.020f, hip - h * 0.180f, d * 0.088f, w * 0.0055f, segs);
        }
    }
}

static void AddShoeSoles(MeshData& mesh, float h, float w, float d, int segs) {
    float yBottom = -h * 0.5f;
    float foot = yBottom + h * 0.03f;
    AddEllipsoid(mesh, -w * 0.16f, foot - h * 0.045f, d * 0.145f, w * 0.090f, h * 0.018f, d * 0.135f, segs, 4);
    AddEllipsoid(mesh,  w * 0.16f, foot - h * 0.045f, d * 0.145f, w * 0.090f, h * 0.018f, d * 0.135f, segs, 4);
}

} // namespace

void AddHeroFineDetails(
    MeshData& mesh,
    const HeroCharacterOptions& heroOptions,
    const HeroFineDetailOptions& detailOptions,
    HeroFineDetailReport* report) {

    if (mesh.positions.empty()) return;
    HeroFineDetailReport local;
    const float h = heroOptions.heightScale;
    const float w = heroOptions.widthScale;
    const float d = heroOptions.depthScale;
    const int segs = std::max(8, heroOptions.radialSegments / 2);

    int before = VertexCount(mesh);
    if (detailOptions.addFaceFeatures) AddFaceFeatures(mesh, h, w, d, detailOptions.faceScale, segs);
    local.faceFeatureVertices = VertexCount(mesh) - before;

    before = VertexCount(mesh);
    if (detailOptions.addHairStrands) AddHairStrands(mesh, h, w, d, detailOptions.hairStrandScale, segs);
    local.hairStrandVertices = VertexCount(mesh) - before;

    before = VertexCount(mesh);
    if (detailOptions.addClothingFolds) AddClothingFolds(mesh, h, w, d, detailOptions.clothingFoldScale, segs);
    local.clothingFoldVertices = VertexCount(mesh) - before;

    before = VertexCount(mesh);
    if (detailOptions.addFingerHints) AddFingerHints(mesh, h, w, d, segs);
    local.fingerVertices = VertexCount(mesh) - before;

    before = VertexCount(mesh);
    if (detailOptions.addShoeSoles) AddShoeSoles(mesh, h, w, d, segs);
    local.shoeSoleVertices = VertexCount(mesh) - before;

    RecomputeNormals(mesh);
    local.verticesAfter = VertexCount(mesh);
    local.trianglesAfter = static_cast<int>(mesh.indices.size() / 3);
    if (report) *report = local;
}

} // namespace make3d
