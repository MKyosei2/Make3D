#include "Make3DAdvancedCore.h"
#include "Make3DGameAssetGenerator.h"
#include "Make3DMaskRefiner.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>

static make3d::ImageRGBA MakeInput() {
    constexpr int W = 96;
    constexpr int H = 128;
    make3d::ImageRGBA img;
    img.width = W;
    img.height = H;
    img.pixels.assign(static_cast<size_t>(W) * H * 4, 0);
    auto set = [&](int x, int y, unsigned char r, unsigned char g, unsigned char b) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        size_t p = (static_cast<size_t>(y) * W + x) * 4;
        img.pixels[p + 0] = r;
        img.pixels[p + 1] = g;
        img.pixels[p + 2] = b;
        img.pixels[p + 3] = 255;
    };
    for (int y = 24; y < 116; ++y) for (int x = 26; x < 70; ++x) set(x, y, 150, 155, 165);
    for (int y = 16; y < 28; ++y) for (int x = 20; x < 76; ++x) set(x, y, 110, 95, 85);
    return img;
}

int main() {
    make3d::ImageRGBA img = MakeInput();
    make3d::ReconstructionReport rr;
    std::vector<std::uint8_t> mask = make3d::BuildForegroundMask(img, &rr);
    make3d::MaskRefineOptions mo;
    mo.keepLargestComponentOnly = true;
    make3d::RefineForegroundMask(mask, img.width, img.height, mo);
    make3d::AdvancedOptions ao;
    make3d::DepthImage depth = make3d::PrepareDepth(img, std::nullopt, mask, ao, &rr);
    make3d::GameAssetGeneratorOptions go;
    auto result = make3d::BuildGenericGameAsset(img, depth, mask, std::filesystem::current_path() / "game_asset_test", go);
    if (!result.ok) return 1;
    if (result.classification.assetType != make3d::GameAssetType::Building) return 2;
    if (!result.validation.ok) return 3;
    if (!std::filesystem::exists(result.objPath)) return 4;
    if (!std::filesystem::exists(result.gltfPath)) return 5;
    if (!std::filesystem::exists(result.manifestPath)) return 6;
    std::cout << "[PASS] Make3D generic asset test\n";
    return 0;
}
