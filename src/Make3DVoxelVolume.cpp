#include "Make3DVoxelVolume.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>

namespace make3d {

namespace {

constexpr float kPi = 3.14159265358979323846f;

static float Clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static int Index(int x, int y, int w) { return y * w + x; }

static float MeanDepthOnRow(const DepthImage& depth, const std::vector<std::uint8_t>& mask, int w, int row, int x0, int x1) {
    float sum = 0.0f;
    int count = 0;
    for (int x = x0; x <= x1; ++x) {
        int p = Index(x, row, w);
        if (!mask[static_cast<size_t>(p)]) continue;
        sum += depth.values[static_cast<size_t>(p)];
        ++count;
    }
    return count > 0 ? sum / static_cast<float>(count) : 0.5f;
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

MeshData BuildVoxelVolumeMesh(
    const ImageRGBA& image,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& mask,
    const VoxelVolumeOptions& options,
    VoxelVolumeReport* report) {

    MeshData mesh;
    const int w = image.width;
    const int h = image.height;
    if (w <= 0 || h <= 0 || mask.size() != static_cast<size_t>(w * h) || depth.values.size() != static_cast<size_t>(w * h)) {
        if (report) report->warnings.push_back("Invalid image, mask, or depth dimensions.");
        return mesh;
    }

    int samples = std::max(8, options.verticalSamples);
    int segments = std::max(8, options.radialSegments);
    std::vector<float> centerX;
    std::vector<float> centerY;
    std::vector<float> radiusX;
    std::vector<float> radiusZ;

    for (int s = 0; s < samples; ++s) {
        float yf = samples == 1 ? 0.0f : static_cast<float>(s) / static_cast<float>(samples - 1);
        int y = std::clamp(static_cast<int>(yf * static_cast<float>(h - 1)), 0, h - 1);
        int bestY = -1;
        int bestX0 = w;
        int bestX1 = -1;
        int bestCount = 0;

        for (int dy = -options.rowSearchRadius; dy <= options.rowSearchRadius; ++dy) {
            int yy = y + dy;
            if (yy < 0 || yy >= h) continue;
            int x0 = w;
            int x1 = -1;
            int count = 0;
            for (int x = 0; x < w; ++x) {
                if (mask[static_cast<size_t>(Index(x, yy, w))]) {
                    x0 = std::min(x0, x);
                    x1 = std::max(x1, x);
                    ++count;
                }
            }
            if (count > bestCount) {
                bestCount = count;
                bestY = yy;
                bestX0 = x0;
                bestX1 = x1;
            }
        }

        if (bestY < 0 || bestX1 < bestX0) continue;
        float cx = (static_cast<float>(bestX0 + bestX1) * 0.5f / static_cast<float>(w - 1) - 0.5f) * options.xyScale;
        float cy = (0.5f - static_cast<float>(bestY) / static_cast<float>(h - 1)) * options.xyScale;
        float rx = std::max(options.minRadius, (static_cast<float>(bestX1 - bestX0 + 1) / static_cast<float>(w)) * options.xyScale * 0.5f);
        float meanD = MeanDepthOnRow(depth, mask, w, bestY, bestX0, bestX1);
        float rz = std::max(options.minRadius, rx * (0.35f + Clamp01(meanD) * options.zInflation) * options.depthScale);
        centerX.push_back(cx);
        centerY.push_back(cy);
        radiusX.push_back(rx);
        radiusZ.push_back(rz);
    }

    if (radiusX.size() < 2) {
        if (report) report->warnings.push_back("Not enough mask rows to build voxel volume.");
        return mesh;
    }

    for (int it = 0; it < options.radiusSmoothingIterations; ++it) {
        std::vector<float> nx = radiusX;
        std::vector<float> nz = radiusZ;
        for (size_t i = 1; i + 1 < radiusX.size(); ++i) {
            nx[i] = (radiusX[i - 1] + radiusX[i] * 2.0f + radiusX[i + 1]) * 0.25f;
            nz[i] = (radiusZ[i - 1] + radiusZ[i] * 2.0f + radiusZ[i + 1]) * 0.25f;
        }
        radiusX.swap(nx);
        radiusZ.swap(nz);
    }

    for (size_t r = 0; r < radiusX.size(); ++r) {
        float v = radiusX.size() == 1 ? 0.0f : static_cast<float>(r) / static_cast<float>(radiusX.size() - 1);
        for (int s = 0; s < segments; ++s) {
            float a = static_cast<float>(s) / static_cast<float>(segments) * kPi * 2.0f;
            float x = centerX[r] + std::cos(a) * radiusX[r];
            float y = centerY[r];
            float z = std::sin(a) * radiusZ[r];
            mesh.positions.insert(mesh.positions.end(), {x, y, z});
            mesh.normals.insert(mesh.normals.end(), {std::cos(a), 0.0f, std::sin(a)});
            mesh.uvs.insert(mesh.uvs.end(), {static_cast<float>(s) / static_cast<float>(segments), v});
        }
    }

    auto vid = [&](size_t r, int s) -> std::uint32_t {
        return static_cast<std::uint32_t>(r * static_cast<size_t>(segments) + static_cast<size_t>((s + segments) % segments));
    };

    for (size_t r = 0; r + 1 < radiusX.size(); ++r) {
        for (int s = 0; s < segments; ++s) {
            std::uint32_t a = vid(r, s);
            std::uint32_t b = vid(r, s + 1);
            std::uint32_t c = vid(r + 1, s);
            std::uint32_t d = vid(r + 1, s + 1);
            mesh.indices.insert(mesh.indices.end(), {a, c, b, b, c, d});
        }
    }

    if (options.closeCaps) {
        std::uint32_t topCenter = static_cast<std::uint32_t>(mesh.positions.size() / 3);
        mesh.positions.insert(mesh.positions.end(), {centerX.front(), centerY.front(), 0.0f});
        mesh.normals.insert(mesh.normals.end(), {0.0f, 1.0f, 0.0f});
        mesh.uvs.insert(mesh.uvs.end(), {0.5f, 0.0f});
        for (int s = 0; s < segments; ++s) mesh.indices.insert(mesh.indices.end(), {topCenter, vid(0, s), vid(0, s + 1)});

        std::uint32_t bottomCenter = static_cast<std::uint32_t>(mesh.positions.size() / 3);
        mesh.positions.insert(mesh.positions.end(), {centerX.back(), centerY.back(), 0.0f});
        mesh.normals.insert(mesh.normals.end(), {0.0f, -1.0f, 0.0f});
        mesh.uvs.insert(mesh.uvs.end(), {0.5f, 1.0f});
        size_t last = radiusX.size() - 1;
        for (int s = 0; s < segments; ++s) mesh.indices.insert(mesh.indices.end(), {bottomCenter, vid(last, s + 1), vid(last, s)});
    }

    RecomputeNormals(mesh);

    if (report) {
        report->sourceWidth = w;
        report->sourceHeight = h;
        report->sampledRows = samples;
        report->generatedRings = static_cast<int>(radiusX.size());
        report->radialSegments = segments;
        report->vertices = static_cast<int>(mesh.positions.size() / 3);
        report->triangles = static_cast<int>(mesh.indices.size() / 3);
        report->closedCaps = options.closeCaps;
        report->minRadiusX = *std::min_element(radiusX.begin(), radiusX.end());
        report->maxRadiusX = *std::max_element(radiusX.begin(), radiusX.end());
        report->minRadiusZ = *std::min_element(radiusZ.begin(), radiusZ.end());
        report->maxRadiusZ = *std::max_element(radiusZ.begin(), radiusZ.end());
    }
    return mesh;
}

std::string VoxelVolumeReport::ToMarkdown() const {
    std::ostringstream o;
    o << "# Voxel Volume Report\n\n";
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| Source width | " << sourceWidth << " |\n";
    o << "| Source height | " << sourceHeight << " |\n";
    o << "| Sampled rows | " << sampledRows << " |\n";
    o << "| Generated rings | " << generatedRings << " |\n";
    o << "| Radial segments | " << radialSegments << " |\n";
    o << "| Vertices | " << vertices << " |\n";
    o << "| Triangles | " << triangles << " |\n";
    o << "| Min radius X | " << minRadiusX << " |\n";
    o << "| Max radius X | " << maxRadiusX << " |\n";
    o << "| Min radius Z | " << minRadiusZ << " |\n";
    o << "| Max radius Z | " << maxRadiusZ << " |\n";
    o << "| Closed caps | " << (closedCaps ? "yes" : "no") << " |\n";
    if (!warnings.empty()) {
        o << "\n## Warnings\n";
        for (const auto& w : warnings) o << "- " << w << "\n";
    }
    return o.str();
}

std::string VoxelVolumeReport::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"sourceWidth\": " << sourceWidth << ",\n";
    o << "  \"sourceHeight\": " << sourceHeight << ",\n";
    o << "  \"sampledRows\": " << sampledRows << ",\n";
    o << "  \"generatedRings\": " << generatedRings << ",\n";
    o << "  \"radialSegments\": " << radialSegments << ",\n";
    o << "  \"vertices\": " << vertices << ",\n";
    o << "  \"triangles\": " << triangles << ",\n";
    o << "  \"minRadiusX\": " << minRadiusX << ",\n";
    o << "  \"maxRadiusX\": " << maxRadiusX << ",\n";
    o << "  \"minRadiusZ\": " << minRadiusZ << ",\n";
    o << "  \"maxRadiusZ\": " << maxRadiusZ << ",\n";
    o << "  \"closedCaps\": " << (closedCaps ? "true" : "false") << ",\n";
    o << "  \"warnings\": [";
    for (size_t i = 0; i < warnings.size(); ++i) {
        if (i) o << ", ";
        o << "\"" << JsonEscape(warnings[i]) << "\"";
    }
    o << "]\n}";
    return o.str();
}

} // namespace make3d
