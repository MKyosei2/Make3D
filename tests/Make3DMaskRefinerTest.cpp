#include "Make3DMaskRefiner.h"

#include <iostream>
#include <vector>

static int Fail(const char* message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

int main() {
    constexpr int W = 16;
    constexpr int H = 16;
    std::vector<std::uint8_t> mask(static_cast<size_t>(W) * H, 0);

    for (int y = 4; y < 12; ++y) {
        for (int x = 4; x < 12; ++x) mask[static_cast<size_t>(y) * W + x] = 1;
    }
    mask[static_cast<size_t>(7) * W + 7] = 0;
    mask[static_cast<size_t>(1) * W + 1] = 1;
    mask[static_cast<size_t>(14) * W + 14] = 1;

    make3d::MaskRefineOptions options;
    options.keepLargestComponentOnly = true;
    options.fillInteriorHoles = true;
    options.smoothJaggedEdges = false;

    make3d::MaskRefineReport report = make3d::RefineForegroundMask(mask, W, H, options);
    if (report.componentsBefore < 3) return Fail("expected noisy components before refinement");
    if (report.componentsAfter != 1) return Fail("expected single component after refinement");
    if (report.removedComponents < 2) return Fail("small components were not removed");
    if (report.filledHolePixels < 1) return Fail("interior hole was not filled");

    std::cout << "[PASS] Make3D mask refiner test\n";
    std::cout << report.ToMarkdown() << "\n";
    return 0;
}
