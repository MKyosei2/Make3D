#include "Make3DHeroCharacterModel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>

namespace make3d {

namespace {

constexpr float Pi = 3.14159265358979323846f;

struct PartSpec {
    float cx0, cy0, cz0;
    float cx1, cy1, cz1;
    float r0x, r0z;
    float r1x, r1z;
    int rings;
};

static int Idx(int x, int y, int w) { return y * w + x; }

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

static void AddEllipticPart(MeshData& mesh, const PartSpec& p, int radialSegments) {
    int baseVertex = static_cast<int>(mesh.positions.size() / 3);
    int rings = std::max(2, p.rings);
    int segs = std::max(8, radialSegments);
    for (int r = 0; r < rings; ++r) {
        float t = rings <= 1 ? 0.0f : static_cast<float>(r) / static_cast<float>(rings - 1);
        float cx = p.cx0 + (p.cx1 - p.cx0) * t;
        float cy = p.cy0 + (p.cy1 - p.cy0) * t;
        float cz = p.cz0 + (p.cz1 - p.cz0) * t;
        float rx = p.r0x + (p.r1x - p.r0x) * t;
        float rz = p.r0z + (p.r1z - p.r0z) * t;
        float taper = std::sin(Pi * (0.15f + 0.70f * t));
        rx *= std::max(0.25f, taper);
        rz *= std::max(0.25f, taper);
        for (int s = 0; s < segs; ++s) {
            float a = 2.0f * Pi * static_cast<float>(s) / static_cast<float>(segs);
            float x = cx + std::cos(a) * rx;
            float z = cz + std::sin(a) * rz;
            mesh.positions.push_back(x);
            mesh.positions.push_back(cy);
            mesh.positions.push_back(z);
            mesh.normals.push_back(std::cos(a));
            mesh.normals.push_back(0.0f);
            mesh.normals.push_back(std::sin(a));
            mesh.uvs.push_back(static_cast<float>(s) / static_cast<float>(segs));
            mesh.uvs.push_back(t);
        }
    }

    for (int r = 0; r < rings - 1; ++r) {
        for (int s = 0; s < segs; ++s) {
            int a = baseVertex + r * segs + s;
            int b = baseVertex + r * segs + (s + 1) % segs;
            int c = baseVertex + (r + 1) * segs + s;
            int d = baseVertex + (r + 1) * segs + (s + 1) % segs;
            mesh.indices.push_back(static_cast<std::uint32_t>(a));
            mesh.indices.push_back(static_cast<std::uint32_t>(c));
            mesh.indices.push_back(static_cast<std::uint32_t>(b));
            mesh.indices.push_back(static_cast<std::uint32_t>(b));
            mesh.indices.push_back(static_cast<std::uint32_t>(c));
            mesh.indices.push_back(static_cast<std::uint32_t>(d));
        }
    }

    int bottomCenter = static_cast<int>(mesh.positions.size() / 3);
    mesh.positions.push_back(p.cx0); mesh.positions.push_back(p.cy0); mesh.positions.push_back(p.cz0);
    mesh.normals.push_back(0.0f); mesh.normals.push_back(-1.0f); mesh.normals.push_back(0.0f);
    mesh.uvs.push_back(0.5f); mesh.uvs.push_back(0.0f);
    int topCenter = static_cast<int>(mesh.positions.size() / 3);
    mesh.positions.push_back(p.cx1); mesh.positions.push_back(p.cy1); mesh.positions.push_back(p.cz1);
    mesh.normals.push_back(0.0f); mesh.normals.push_back(1.0f); mesh.normals.push_back(0.0f);
    mesh.uvs.push_back(0.5f); mesh.uvs.push_back(1.0f);

    for (int s = 0; s < segs; ++s) {
        int a = baseVertex + s;
        int b = baseVertex + (s + 1) % segs;
        mesh.indices.push_back(static_cast<std::uint32_t>(bottomCenter));
        mesh.indices.push_back(static_cast<std::uint32_t>(b));
        mesh.indices.push_back(static_cast<std::uint32_t>(a));

        int topRing = baseVertex + (rings - 1) * segs;
        int c = topRing + s;
        int d = topRing + (s + 1) % segs;
        mesh.indices.push_back(static_cast<std::uint32_t>(topCenter));
        mesh.indices.push_back(static_cast<std::uint32_t>(c));
        mesh.indices.push_back(static_cast<std::uint32_t>(d));
    }
}

static void MeasureMask(const std::vector<std::uint8_t>& mask, int w, int h, int& pixels, int& minX, int& minY, int& maxX, int& maxY, float& symmetry) {
    pixels = 0; minX = w; minY = h; maxX = -1; maxY = -1;
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        if (!mask[static_cast<size_t>(Idx(x, y, w))]) continue;
        ++pixels;
        minX = std::min(minX, x); minY = std::min(minY, y);
        maxX = std::max(maxX, x); maxY = std::max(maxY, y);
    }
    int hit = 0, total = 0;
    if (pixels > 0) {
        int cx = (minX + maxX) / 2;
        for (int y = minY; y <= maxY; ++y) for (int x = minX; x <= maxX; ++x) {
            int m = cx - (x - cx);
            if (m < 0 || m >= w) continue;
            bool a = mask[static_cast<size_t>(Idx(x, y, w))] != 0;
            bool b = mask[static_cast<size_t>(Idx(m, y, w))] != 0;
            if (a || b) { ++total; if (a == b) ++hit; }
        }
    }
    symmetry = total > 0 ? static_cast<float>(hit) / static_cast<float>(total) : 0.0f;
}

} // namespace

MeshData BuildHeroCharacterMesh(
    const ImageRGBA& image,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& refinedMask,
    const HeroCharacterOptions& options,
    HeroCharacterReport* report) {

    MeshData mesh;
    HeroCharacterReport local;
    local.sourceWidth = image.width;
    local.sourceHeight = image.height;
    if (image.width <= 0 || image.height <= 0 || refinedMask.size() != static_cast<size_t>(image.width * image.height)) {
        local.warnings.push_back("Invalid hero character input dimensions.");
        if (report) *report = local;
        return mesh;
    }

    int minX, minY, maxX, maxY;
    float symmetry = 0.0f;
    MeasureMask(refinedMask, image.width, image.height, local.foregroundPixels, minX, minY, maxX, maxY, symmetry);
    local.detectedSymmetry = symmetry;
    if (local.foregroundPixels <= 0) {
        local.warnings.push_back("No foreground pixels for hero character reconstruction.");
        if (report) *report = local;
        return mesh;
    }

    int bw = std::max(1, maxX - minX + 1);
    int bh = std::max(1, maxY - minY + 1);
    local.detectedAspectRatio = static_cast<float>(bh) / static_cast<float>(bw);

    float h = options.heightScale;
    float w = options.widthScale;
    float d = options.depthScale;
    local.bodyHeight = h;
    local.bodyWidth = w;

    float yTop = h * 0.5f;
    float yBottom = -h * 0.5f;
    float headCenter = yTop - h * 0.12f;
    float neck = yTop - h * 0.25f;
    float hip = yBottom + h * 0.33f;
    float foot = yBottom + h * 0.03f;

    int segs = std::max(12, options.radialSegments);
    int rings = std::max(4, options.verticalSegments);

    if (options.buildHead) {
        PartSpec head{0.0f, headCenter - h * 0.11f, 0.02f, 0.0f, headCenter + h * 0.10f, 0.02f,
                      w * 0.18f * options.headScale, d * 0.18f * options.headScale,
                      w * 0.16f * options.headScale, d * 0.17f * options.headScale,
                      rings};
        AddEllipticPart(mesh, head, segs);
    }

    if (options.buildTorso) {
        PartSpec torso{0.0f, neck, 0.0f, 0.0f, hip, 0.03f,
                       w * 0.32f * options.torsoScale, d * 0.20f * options.torsoScale,
                       w * 0.24f * options.torsoScale, d * 0.17f * options.torsoScale,
                       rings + 4};
        AddEllipticPart(mesh, torso, segs);
    }

    if (options.buildArms) {
        PartSpec leftUpper{-w * 0.34f, neck - h * 0.04f, 0.0f, -w * 0.52f, hip + h * 0.13f, 0.01f,
                           w * 0.070f * options.limbScale, d * 0.070f * options.limbScale,
                           w * 0.055f * options.limbScale, d * 0.055f * options.limbScale,
                           rings};
        PartSpec rightUpper{w * 0.34f, neck - h * 0.04f, 0.0f, w * 0.52f, hip + h * 0.13f, 0.01f,
                            w * 0.070f * options.limbScale, d * 0.070f * options.limbScale,
                            w * 0.055f * options.limbScale, d * 0.055f * options.limbScale,
                            rings};
        PartSpec leftFore{-w * 0.52f, hip + h * 0.13f, 0.01f, -w * 0.42f, hip - h * 0.10f, 0.02f,
                          w * 0.055f * options.limbScale, d * 0.055f * options.limbScale,
                          w * 0.045f * options.limbScale, d * 0.045f * options.limbScale,
                          rings};
        PartSpec rightFore{w * 0.52f, hip + h * 0.13f, 0.01f, w * 0.42f, hip - h * 0.10f, 0.02f,
                           w * 0.055f * options.limbScale, d * 0.055f * options.limbScale,
                           w * 0.045f * options.limbScale, d * 0.045f * options.limbScale,
                           rings};
        AddEllipticPart(mesh, leftUpper, segs);
        AddEllipticPart(mesh, rightUpper, segs);
        AddEllipticPart(mesh, leftFore, segs);
        AddEllipticPart(mesh, rightFore, segs);
    }

    if (options.buildLegs) {
        PartSpec leftLeg{-w * 0.13f, hip, 0.02f, -w * 0.16f, foot, 0.00f,
                         w * 0.085f * options.limbScale, d * 0.075f * options.limbScale,
                         w * 0.060f * options.limbScale, d * 0.055f * options.limbScale,
                         rings + 2};
        PartSpec rightLeg{w * 0.13f, hip, 0.02f, w * 0.16f, foot, 0.00f,
                          w * 0.085f * options.limbScale, d * 0.075f * options.limbScale,
                          w * 0.060f * options.limbScale, d * 0.055f * options.limbScale,
                          rings + 2};
        AddEllipticPart(mesh, leftLeg, segs);
        AddEllipticPart(mesh, rightLeg, segs);
    }

    RecomputeNormals(mesh);
    local.vertices = static_cast<int>(mesh.positions.size() / 3);
    local.triangles = static_cast<int>(mesh.indices.size() / 3);
    local.ok = !mesh.positions.empty() && !mesh.indices.empty();
    if (local.detectedAspectRatio < 1.05f) local.warnings.push_back("Input does not look vertically character-like; hero prior was still forced.");
    if (local.detectedSymmetry < 0.35f) local.warnings.push_back("Input symmetry is low; hero character prior may dominate the source image.");
    if (report) *report = local;
    return mesh;
}

std::string HeroCharacterReport::ToMarkdown() const {
    std::ostringstream o;
    o << "# Hero Character Reconstruction Report\n\n";
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| OK | " << (ok ? "yes" : "no") << " |\n";
    o << "| Source width | " << sourceWidth << " |\n";
    o << "| Source height | " << sourceHeight << " |\n";
    o << "| Foreground pixels | " << foregroundPixels << " |\n";
    o << "| Vertices | " << vertices << " |\n";
    o << "| Triangles | " << triangles << " |\n";
    o << "| Detected aspect ratio | " << detectedAspectRatio << " |\n";
    o << "| Detected symmetry | " << detectedSymmetry << " |\n";
    o << "| Body height | " << bodyHeight << " |\n";
    o << "| Body width | " << bodyWidth << " |\n";
    if (!warnings.empty()) {
        o << "\n## Warnings\n";
        for (const auto& w : warnings) o << "- " << w << "\n";
    }
    return o.str();
}

std::string HeroCharacterReport::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"ok\": " << (ok ? "true" : "false") << ",\n";
    o << "  \"sourceWidth\": " << sourceWidth << ",\n";
    o << "  \"sourceHeight\": " << sourceHeight << ",\n";
    o << "  \"foregroundPixels\": " << foregroundPixels << ",\n";
    o << "  \"vertices\": " << vertices << ",\n";
    o << "  \"triangles\": " << triangles << ",\n";
    o << "  \"detectedAspectRatio\": " << detectedAspectRatio << ",\n";
    o << "  \"detectedSymmetry\": " << detectedSymmetry << ",\n";
    o << "  \"bodyHeight\": " << bodyHeight << ",\n";
    o << "  \"bodyWidth\": " << bodyWidth << ",\n";
    o << "  \"warnings\": [";
    for (size_t i = 0; i < warnings.size(); ++i) {
        if (i) o << ", ";
        o << "\"" << EscapeJson(warnings[i]) << "\"";
    }
    o << "]\n";
    o << "}";
    return o.str();
}

} // namespace make3d
