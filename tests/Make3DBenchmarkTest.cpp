#include "Make3DBenchmark.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

make3d::ImageRGBA CreateSyntheticCharacterImage() {
    make3d::ImageRGBA image;
    image.width = 96;
    image.height = 128;
    image.pixels.assign(static_cast<size_t>(image.width) * image.height * 4, 0);

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            float nx = (x - image.width * 0.5f) / (image.width * 0.32f);
            float ny = (y - image.height * 0.52f) / (image.height * 0.42f);
            bool insideBody = nx * nx + ny * ny < 1.0f;
            bool insideHead = (x - image.width * 0.5f) * (x - image.width * 0.5f) +
                              (y - image.height * 0.20f) * (y - image.height * 0.20f) < 15.0f * 15.0f;
            if (!insideBody && !insideHead) continue;

            size_t p = (static_cast<size_t>(y) * image.width + x) * 4;
            image.pixels[p + 0] = static_cast<std::uint8_t>(80 + (x % 80));
            image.pixels[p + 1] = static_cast<std::uint8_t>(110 + (y % 80));
            image.pixels[p + 2] = 210;
            image.pixels[p + 3] = 255;
        }
    }

    return image;
}

int TriangleCount(const make3d::MeshData& mesh) {
    return static_cast<int>(mesh.indices.size() / 3);
}

} // namespace

int main() {
    make3d::ImageRGBA image = CreateSyntheticCharacterImage();
    make3d::ReconstructionReport report;
    std::vector<std::uint8_t> mask = make3d::BuildForegroundMask(image, &report);
    if (report.foregroundPixels <= 0) {
        std::cerr << "Expected synthetic foreground pixels.\n";
        return 1;
    }

    make3d::AdvancedOptions finalOptions;
    finalOptions.maxGridResolution = 192;
    finalOptions.volumeRadialSegments = 20;

    make3d::DepthImage depth = make3d::EstimateDepthFromSingleImage(image, mask, &report);
    make3d::MeshData preview = make3d::BuildPreviewMeshForDisplay(image, depth, mask, finalOptions, &report);
    make3d::MeshData finalMesh = make3d::ReconstructMesh(image, depth, mask, finalOptions, &report);
    make3d::RecomputeNormals(finalMesh);
    make3d::NormalizeMesh(finalMesh, 2.0f);

    if (preview.positions.empty() || preview.indices.empty()) {
        std::cerr << "Preview mesh was empty.\n";
        return 2;
    }
    if (finalMesh.positions.empty() || finalMesh.indices.empty()) {
        std::cerr << "Final mesh was empty.\n";
        return 3;
    }
    if (TriangleCount(finalMesh) < TriangleCount(preview)) {
        std::cerr << "Expected final mesh to keep at least preview triangle detail. preview="
                  << TriangleCount(preview) << " final=" << TriangleCount(finalMesh) << "\n";
        return 4;
    }

    std::vector<make3d::StageTiming> stages;
    make3d::StageTiming s;
    s.name = "synthetic_stage";
    s.milliseconds = 1.25;
    s.vertices = static_cast<int>(finalMesh.positions.size() / 3);
    s.triangles = TriangleCount(finalMesh);
    s.note = "benchmark serialization smoke test";
    stages.push_back(s);

    std::string json = make3d::StageTimingsToJson(stages);
    if (json.find("synthetic_stage") == std::string::npos || json.find("milliseconds") == std::string::npos) {
        std::cerr << "Stage timing JSON did not contain expected fields.\n";
        return 5;
    }

    std::cout << "Make3DBenchmarkTest passed. previewTris=" << TriangleCount(preview)
              << " finalTris=" << TriangleCount(finalMesh) << "\n";
    return 0;
}
