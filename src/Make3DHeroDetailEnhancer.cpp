#include "Make3DHeroDetailEnhancer.h"

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

static void AddCapsule(MeshData& mesh, float x0, float y0, float z0, float x1, float y1, float z1, float rx, float rz, int segs) {
    int base = VertexCount(mesh);
    int rings = 6;
    segs = std::max(8, segs);
    for (int r = 0; r < rings; ++r) {
        float t = static_cast<float>(r) / static_cast<float>(rings - 1);
        float cx = x0 + (x1 - x0) * t;
        float cy = y0 + (y1 - y0) * t;
        float cz = z0 + (z1 - z0) * t;
        float taper = std::sin(Pi * (0.12f + 0.76f * t));
        float rrX = rx * std::max(0.35f, taper);
        float rrZ = rz * std::max(0.35f, taper);
        for (int s = 0; s < segs; ++s) {
            float u = static_cast<float>(s) / static_cast<float>(segs);
            float a = 2.0f * Pi * u;
            mesh.positions.push_back(cx + std::cos(a) * rrX);
            mesh.positions.push_back(cy);
            mesh.positions.push_back(cz + std::sin(a) * rrZ);
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

static void AddFaceAccents(MeshData& mesh, float h, float w, float d, int segs) {
    const float yTop = h * 0.5f;
    const float headCenter = yTop - h * 0.12f;
    const float faceZ = d * 0.18f;
    AddEllipsoid(mesh, -w * 0.055f, headCenter + h * 0.030f, faceZ, w * 0.020f, h * 0.012f, d * 0.010f, segs / 2, 4);
    AddEllipsoid(mesh,  w * 0.055f, headCenter + h * 0.030f, faceZ, w * 0.020f, h * 0.012f, d * 0.010f, segs / 2, 4);
    AddEllipsoid(mesh, 0.0f, headCenter - h * 0.005f, faceZ + d * 0.010f, w * 0.020f, h * 0.030f, d * 0.018f, segs / 2, 4);
    AddCapsule(mesh, -w * 0.045f, headCenter - h * 0.055f, faceZ + d * 0.006f, w * 0.045f, headCenter - h * 0.055f, faceZ + d * 0.006f, w * 0.010f, d * 0.006f, segs / 2);
}

static void AddFingerHints(MeshData& mesh, float h, float w, float d, int segs) {
    const float yBottom = -h * 0.5f;
    const float hip = yBottom + h * 0.33f;
    const float handY = hip - h * 0.145f;
    for (int side = -1; side <= 1; side += 2) {
        float baseX = static_cast<float>(side) * w * 0.42f;
        for (int i = 0; i < 3; ++i) {
            float offset = static_cast<float>(i - 1) * w * 0.018f;
            AddCapsule(mesh,
                       baseX + offset,
                       handY,
                       d * 0.055f,
                       baseX + offset + static_cast<float>(side) * w * 0.030f,
                       handY - h * 0.025f,
                       d * 0.070f,
                       w * 0.006f,
                       d * 0.006f,
                       segs / 3);
        }
    }
}

static void AddHairStrands(MeshData& mesh, float h, float w, float d, int segs) {
    const float yTop = h * 0.5f;
    const float headCenter = yTop - h * 0.12f;
    for (int i = -2; i <= 2; ++i) {
        float x = static_cast<float>(i) * w * 0.045f;
        AddCapsule(mesh,
                   x,
                   headCenter + h * 0.085f,
                   -d * 0.155f,
                   x * 1.18f,
                   headCenter - h * (0.055f + 0.010f * std::abs(i)),
                   -d * 0.185f,
                   w * 0.018f,
                   d * 0.025f,
                   segs / 2);
    }
}

static void AddBootDetails(MeshData& mesh, float h, float w, float d, int segs) {
    const float yBottom = -h * 0.5f;
    const float foot = yBottom + h * 0.03f;
    AddCapsule(mesh, -w * 0.16f, foot + h * 0.025f, d * 0.045f, -w * 0.16f, foot - h * 0.010f, d * 0.115f, w * 0.075f, d * 0.090f, segs / 2);
    AddCapsule(mesh,  w * 0.16f, foot + h * 0.025f, d * 0.045f,  w * 0.16f, foot - h * 0.010f, d * 0.115f, w * 0.075f, d * 0.090f, segs / 2);
}

} // namespace

void AddHeroDetailVolumes(MeshData& mesh, const HeroCharacterOptions& options, HeroCharacterReport* report) {
    if (mesh.positions.empty()) return;
    const int start = VertexCount(mesh);
    const float h = options.heightScale;
    const float w = options.widthScale;
    const float d = options.depthScale;
    const int segs = std::max(12, options.radialSegments);

    const float yTop = h * 0.5f;
    const float yBottom = -h * 0.5f;
    const float headCenter = yTop - h * 0.12f;
    const float neck = yTop - h * 0.25f;
    const float shoulderY = neck - h * 0.03f;
    const float hip = yBottom + h * 0.33f;
    const float foot = yBottom + h * 0.03f;

    if (options.buildNeckAndShoulders) {
        AddCapsule(mesh, 0.0f, neck + h * 0.030f, 0.015f, 0.0f, shoulderY, 0.020f, w * 0.105f, d * 0.095f, segs);
        AddEllipsoid(mesh, -w * 0.29f, shoulderY, 0.005f, w * 0.105f, h * 0.040f, d * 0.080f, segs / 2, 5);
        AddEllipsoid(mesh,  w * 0.29f, shoulderY, 0.005f, w * 0.105f, h * 0.040f, d * 0.080f, segs / 2, 5);
    }
    const int connectorEnd = VertexCount(mesh);

    if (options.buildHair) {
        AddEllipsoid(mesh, 0.0f, headCenter + h * 0.06f, -d * 0.02f, w * 0.23f * options.hairScale, h * 0.17f * options.hairScale, d * 0.23f * options.hairScale, segs, 8);
        AddEllipsoid(mesh, -w * 0.12f, headCenter - h * 0.03f, -d * 0.05f, w * 0.09f * options.hairScale, h * 0.16f * options.hairScale, d * 0.12f * options.hairScale, segs / 2, 6);
        AddEllipsoid(mesh,  w * 0.12f, headCenter - h * 0.03f, -d * 0.05f, w * 0.09f * options.hairScale, h * 0.16f * options.hairScale, d * 0.12f * options.hairScale, segs / 2, 6);
        AddHairStrands(mesh, h, w, d, segs);
    }
    const int hairEnd = VertexCount(mesh);

    if (options.buildClothing) {
        AddCapsule(mesh, 0.0f, h * 0.18f, 0.035f, 0.0f, -h * 0.18f, 0.055f, w * 0.37f * options.clothingScale, d * 0.235f * options.clothingScale, segs);
        AddCapsule(mesh, 0.0f, -h * 0.15f, 0.060f, 0.0f, -h * 0.34f, 0.070f, w * 0.39f * options.clothingScale, d * 0.240f * options.clothingScale, segs);
        AddCapsule(mesh, -w * 0.30f, h * 0.15f, d * 0.070f, w * 0.30f, h * 0.15f, d * 0.070f, w * 0.020f, d * 0.014f, segs / 2);
    }
    const int clothingEnd = VertexCount(mesh);

    if (options.buildHands) {
        AddEllipsoid(mesh, -w * 0.42f, hip - h * 0.13f, 0.03f, w * 0.058f, h * 0.045f, d * 0.052f, segs / 2, 5);
        AddEllipsoid(mesh,  w * 0.42f, hip - h * 0.13f, 0.03f, w * 0.058f, h * 0.045f, d * 0.052f, segs / 2, 5);
        AddFingerHints(mesh, h, w, d, segs);
    }
    if (options.buildFeet) {
        AddEllipsoid(mesh, -w * 0.16f, foot - h * 0.015f, d * 0.09f, w * 0.078f, h * 0.032f, d * 0.118f, segs / 2, 5);
        AddEllipsoid(mesh,  w * 0.16f, foot - h * 0.015f, d * 0.09f, w * 0.078f, h * 0.032f, d * 0.118f, segs / 2, 5);
        AddBootDetails(mesh, h, w, d, segs);
    }

    AddFaceAccents(mesh, h, w, d, segs);

    RecomputeNormals(mesh);
    if (report) {
        report->connectorVertices += connectorEnd - start;
        report->hairVertices += hairEnd - connectorEnd;
        report->clothingVertices += clothingEnd - hairEnd;
        report->vertices = VertexCount(mesh);
        report->triangles = static_cast<int>(mesh.indices.size() / 3);
    }
}

} // namespace make3d
