#include "Make3DAdvancedCore.h"
#include "Make3DGameAssetGenerator.h"
#include "Make3DMaskRefiner.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

namespace fs = std::filesystem;

static make3d::ImageRGBA CreateInput() {
    constexpr int W = 128;
    constexpr int H = 128;
    make3d::ImageRGBA img;
    img.width = W;
    img.height = H;
    img.pixels.assign(static_cast<size_t>(W) * H * 4, 0);
    auto set = [&](int x, int y, unsigned char r, unsigned char g, unsigned char b) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        size_t p = (static_cast<size_t>(y) * W + x) * 4;
        img.pixels[p + 0] = r; img.pixels[p + 1] = g; img.pixels[p + 2] = b; img.pixels[p + 3] = 255;
    };
    for (int y = 24; y < 116; ++y) for (int x = 34; x < 94; ++x) set(x, y, 150, 155, 165);
    for (int y = 16; y < 28; ++y) for (int x = 28; x < 100; ++x) set(x, y, 105, 90, 80);
    for (int row = 0; row < 3; ++row) for (int col = 0; col < 3; ++col) {
        int x0 = 44 + col * 17;
        int y0 = 38 + row * 20;
        for (int y = y0; y < y0 + 9; ++y) for (int x = x0; x < x0 + 9; ++x) set(x, y, 80, 120, 170);
    }
    for (int y = 96; y < 116; ++y) for (int x = 59; x < 69; ++x) set(x, y, 80, 65, 55);
    return img;
}

static bool SaveTga(const fs::path& path, const make3d::ImageRGBA& img) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    unsigned char h[18] = {};
    h[2] = 2; h[12] = static_cast<unsigned char>(img.width & 255); h[13] = static_cast<unsigned char>((img.width >> 8) & 255);
    h[14] = static_cast<unsigned char>(img.height & 255); h[15] = static_cast<unsigned char>((img.height >> 8) & 255); h[16] = 32; h[17] = 8 | 0x20;
    f.write(reinterpret_cast<const char*>(h), sizeof(h));
    for (int i = 0; i < img.width * img.height; ++i) {
        size_t p = static_cast<size_t>(i) * 4;
        unsigned char bgra[4] = {img.pixels[p + 2], img.pixels[p + 1], img.pixels[p + 0], img.pixels[p + 3]};
        f.write(reinterpret_cast<const char*>(bgra), 4);
    }
    return true;
}

int main(int argc, char** argv) {
    fs::path out = argc >= 2 ? fs::path(argv[1]) : fs::path("generic_asset_sample");
    make3d::ImageRGBA img = CreateInput();
    if (!SaveTga(out / "input_building.tga", img)) return 1;
    make3d::ReconstructionReport rr;
    auto mask = make3d::BuildForegroundMask(img, &rr);
    make3d::MaskRefineOptions mo;
    mo.keepLargestComponentOnly = true;
    make3d::RefineForegroundMask(mask, img.width, img.height, mo);
    make3d::AdvancedOptions ao;
    make3d::DepthImage depth = make3d::PrepareDepth(img, std::nullopt, mask, ao, &rr);
    make3d::GameAssetGeneratorOptions go;
    auto result = make3d::BuildGenericGameAsset(img, depth, mask, out / "output", go);
    if (!result.ok) { std::cerr << result.message << "\n"; return 2; }
    std::ofstream guide(out / "OPEN_THIS_FIRST.txt", std::ios::binary);
    guide << "Open: " << result.gltfPath.u8string() << "\n";
    guide << "Type: " << make3d::ToString(result.classification.assetType) << "\n";
    guide << "Report: " << result.reportPath.u8string() << "\n";
    std::cout << result.message << "\n";
    std::cout << "Type: " << make3d::ToString(result.classification.assetType) << "\n";
    std::cout << "glTF: " << result.gltfPath.u8string() << "\n";
    return 0;
}
