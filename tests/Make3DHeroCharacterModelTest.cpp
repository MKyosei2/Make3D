#include "Make3DAdvancedCore.h"
#include "Make3DHeroCharacterModel.h"

#include <iostream>
#include <vector>

static make3d::ImageRGBA BuildCharacterImage() {
    constexpr int W = 80;
    constexpr int H = 128;
    make3d::ImageRGBA image;
    image.width = W;
    image.height = H;
    image.pixels.assign(static_cast<size_t>(W) * H * 4, 0);
    auto set = [&](int x, int y, unsigned char r, unsigned char g, unsigned char b) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        size_t p = (static_cast<size_t>(y) * W + x) * 4;
        image.pixels[p + 0] = r;
        image.pixels[p + 1] = g;
        image.pixels[p + 2] = b;
        image.pixels[p + 3] = 255;
    };
    auto ellipse = [&](int cx, int cy, int rx, int ry, unsigned char r, unsigned char g, unsigned char b) {
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
            float dx = static_cast<float>(x - cx) / static_cast<float>(rx);
            float dy = static_cast<float>(y - cy) / static_cast<float>(ry);
            if (dx * dx + dy * dy <= 1.0f) set(x, y, r, g, b);
        }
    };
    ellipse(40, 18, 11, 14, 235, 210, 180);
    ellipse(40, 52, 17, 28, 90, 150, 230);
    ellipse(22, 60, 6, 26, 90, 150, 230);
    ellipse(58, 60, 6, 26, 90, 150, 230);
    ellipse(32, 100, 6, 26, 60, 80, 190);
    ellipse(48, 100, 6, 26, 60, 80, 190);
    return image;
}

static int Fail(const char* message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

int main() {
    make3d::ImageRGBA image = BuildCharacterImage();
    make3d::ReconstructionReport reconstructionReport;
    std::vector<std::uint8_t> mask = make3d::BuildForegroundMask(image, &reconstructionReport);
    make3d::DepthImage depth = make3d::EstimateDepthFromSingleImage(image, mask, &reconstructionReport);

    make3d::HeroCharacterReport report;
    make3d::HeroCharacterOptions options;
    options.radialSegments = 24;
    options.verticalSegments = 8;
    make3d::MeshData mesh = make3d::BuildHeroCharacterMesh(image, depth, mask, options, &report);

    if (!report.ok) return Fail("hero character report not ok");
    if (mesh.positions.empty()) return Fail("hero mesh has no positions");
    if (mesh.indices.empty()) return Fail("hero mesh has no triangles");
    if (report.vertices <= 0) return Fail("vertex count missing");
    if (report.triangles <= 0) return Fail("triangle count missing");
    if (report.detectedAspectRatio <= 1.0f) return Fail("character aspect not detected");

    std::cout << "[PASS] Make3D hero character model test\n";
    std::cout << report.ToMarkdown() << "\n";
    return 0;
}
