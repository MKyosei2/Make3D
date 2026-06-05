#include "Make3DAdvancedCore.h"
#include "Make3DVoxelVolume.h"

#include <iostream>
#include <vector>

static make3d::ImageRGBA BuildImage() {
    constexpr int W = 64;
    constexpr int H = 96;
    make3d::ImageRGBA image;
    image.width = W;
    image.height = H;
    image.pixels.assign(static_cast<size_t>(W) * H * 4, 0);
    for (int y = 16; y < 80; ++y) {
        float t = static_cast<float>(y - 16) / 63.0f;
        float radius = 10.0f + 12.0f * (1.0f - (t - 0.5f) * (t - 0.5f) * 4.0f);
        for (int x = 0; x < W; ++x) {
            float dx = static_cast<float>(x - W / 2);
            if (dx * dx <= radius * radius) {
                size_t p = (static_cast<size_t>(y) * W + x) * 4;
                image.pixels[p + 0] = 180;
                image.pixels[p + 1] = 190;
                image.pixels[p + 2] = 220;
                image.pixels[p + 3] = 255;
            }
        }
    }
    return image;
}

static int Fail(const char* message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

int main() {
    make3d::ImageRGBA image = BuildImage();
    make3d::ReconstructionReport report;
    std::vector<std::uint8_t> mask = make3d::BuildForegroundMask(image, &report);
    make3d::DepthImage depth = make3d::EstimateDepthFromSingleImage(image, mask, &report);

    make3d::VoxelVolumeOptions options;
    options.verticalSamples = 32;
    options.radialSegments = 16;
    options.closeCaps = true;

    make3d::VoxelVolumeReport voxelReport;
    make3d::MeshData mesh = make3d::BuildVoxelVolumeMesh(image, depth, mask, options, &voxelReport);

    if (mesh.positions.empty()) return Fail("voxel volume positions are empty");
    if (mesh.indices.empty()) return Fail("voxel volume indices are empty");
    if (voxelReport.generatedRings < 2) return Fail("not enough generated rings");
    if (voxelReport.vertices <= 0) return Fail("invalid vertex report");
    if (voxelReport.triangles <= 0) return Fail("invalid triangle report");
    if (!voxelReport.closedCaps) return Fail("expected closed caps");

    std::cout << "[PASS] Make3D voxel volume test\n";
    std::cout << voxelReport.ToMarkdown() << "\n";
    return 0;
}
