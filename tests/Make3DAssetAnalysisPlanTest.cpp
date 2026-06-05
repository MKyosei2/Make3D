#include "Make3DGameAssetGenerator.h"

#include <iostream>
#include <vector>

namespace {

static int Idx(int x, int y, int w) { return y * w + x; }

static make3d::ImageRGBA MakeBuildingImage(int w, int h) {
    make3d::ImageRGBA image;
    image.width = w;
    image.height = h;
    image.pixels.assign(static_cast<size_t>(w) * h * 4, 0);
    auto set = [&](int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
        size_t p = static_cast<size_t>(Idx(x, y, w)) * 4;
        image.pixels[p + 0] = r;
        image.pixels[p + 1] = g;
        image.pixels[p + 2] = b;
        image.pixels[p + 3] = a;
    };
    for (int y = h / 5; y < h - 3; ++y) {
        for (int x = w / 4; x < 3 * w / 4; ++x) set(x, y, 160, 165, 172, 255);
    }
    for (int y = h / 5; y < h / 3; ++y) {
        for (int x = w / 5; x < 4 * w / 5; ++x) set(x, y, 110, 80, 65, 255);
    }
    return image;
}

static std::vector<std::uint8_t> BuildAlphaMask(const make3d::ImageRGBA& image) {
    std::vector<std::uint8_t> mask(static_cast<size_t>(image.width) * image.height, 0);
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            size_t p = static_cast<size_t>(Idx(x, y, image.width)) * 4;
            mask[static_cast<size_t>(Idx(x, y, image.width))] = image.pixels[p + 3] ? 255 : 0;
        }
    }
    return mask;
}

} // namespace

int main() {
    make3d::ImageRGBA image = MakeBuildingImage(96, 128);
    std::vector<std::uint8_t> mask = BuildAlphaMask(image);
    make3d::DepthImage depth;
    depth.width = image.width;
    depth.height = image.height;
    depth.values.assign(static_cast<size_t>(image.width) * image.height, 0.55f);

    make3d::AssetAnalysisResult analysis = make3d::AnalyzeAsset(image, mask, depth);
    if (!analysis.ok) {
        std::cerr << "Asset analysis failed\n" << analysis.ToMarkdown() << "\n";
        return 1;
    }
    if (analysis.contour.empty()) {
        std::cerr << "Contour was not extracted\n";
        return 2;
    }
    if (analysis.palette.empty()) {
        std::cerr << "Palette was not extracted\n";
        return 3;
    }
    if (analysis.partHints.empty()) {
        std::cerr << "Part hints were not generated\n";
        return 4;
    }

    make3d::AssetPlanOptions options;
    options.targetHeight = 3.0f;
    make3d::AssetPlanResult plan = make3d::BuildAssetPlanFromAnalysis(analysis, options);
    if (!plan.ok) {
        std::cerr << "Asset plan failed\n" << plan.ToMarkdown() << "\n";
        return 5;
    }
    if (plan.parts.empty()) {
        std::cerr << "Plan has no editable parts\n";
        return 6;
    }
    if (plan.materials.empty()) {
        std::cerr << "Plan has no material slots\n";
        return 7;
    }
    if (!plan.wantsCollider || !plan.wantsLod) {
        std::cerr << "Plan should request collider and LOD\n";
        return 8;
    }

    std::cout << "[PASS] compact Make3D asset analysis + plan test\n";
    std::cout << analysis.ToMarkdown() << "\n";
    std::cout << plan.ToMarkdown() << "\n";
    return 0;
}
