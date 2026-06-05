#include "Make3DAdvancedCore.h"
#include "Make3DShapeInference.h"

#include <iostream>
#include <vector>

static make3d::ImageRGBA BuildCharacterImage() {
    constexpr int W = 64;
    constexpr int H = 96;
    make3d::ImageRGBA image;
    image.width = W;
    image.height = H;
    image.pixels.assign(static_cast<size_t>(W) * H * 4, 0);
    auto set = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        size_t p = (static_cast<size_t>(y) * W + x) * 4;
        image.pixels[p + 0] = 180;
        image.pixels[p + 1] = 190;
        image.pixels[p + 2] = 230;
        image.pixels[p + 3] = 255;
    };
    for (int y = 8; y < 28; ++y) for (int x = 23; x < 41; ++x) if ((x - 32) * (x - 32) + (y - 18) * (y - 18) < 95) set(x, y);
    for (int y = 30; y < 68; ++y) for (int x = 18; x < 46; ++x) set(x, y);
    for (int y = 68; y < 92; ++y) for (int x = 22; x < 30; ++x) set(x, y);
    for (int y = 68; y < 92; ++y) for (int x = 34; x < 42; ++x) set(x, y);
    return image;
}

static int Fail(const char* message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

int main() {
    make3d::ImageRGBA image = BuildCharacterImage();
    make3d::ReconstructionReport report;
    std::vector<std::uint8_t> mask = make3d::BuildForegroundMask(image, &report);
    make3d::DepthImage depth = make3d::EstimateDepthFromSingleImage(image, mask, &report);
    make3d::ShapeInferenceResult result = make3d::RunShapeInference(image, mask, depth);

    if (result.foregroundPixels <= 0) return Fail("foreground was not measured");
    if (result.radiusProfile.empty()) return Fail("radius profile missing");
    if (result.depthProfile.empty()) return Fail("depth profile missing");
    if (result.adjustedDepth.values.size() != depth.values.size()) return Fail("adjusted depth size mismatch");
    if (result.backDepth.values.size() != depth.values.size()) return Fail("back depth size mismatch");
    if (result.sideDepth.values.size() != depth.values.size()) return Fail("side depth size mismatch");

    std::cout << "[PASS] Make3D shape inference test\n";
    std::cout << result.ToMarkdown() << "\n";
    return 0;
}
