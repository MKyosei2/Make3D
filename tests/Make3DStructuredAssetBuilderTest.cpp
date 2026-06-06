#include "Make3DStructuredAssetBuilder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

make3d::ImageRGBA MakeImage(int w, int h) {
    make3d::ImageRGBA image;
    image.width = w;
    image.height = h;
    image.pixels.assign(static_cast<size_t>(w) * h * 4, 255);
    return image;
}

void PaintRect(make3d::ImageRGBA& image, std::vector<std::uint8_t>& mask, int x0, int y0, int x1, int y1, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    x0 = std::max(0, x0); y0 = std::max(0, y0);
    x1 = std::min(image.width - 1, x1); y1 = std::min(image.height - 1, y1);
    for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
        const size_t id = static_cast<size_t>(y) * image.width + x;
        mask[id] = 255;
        const size_t p = id * 4;
        image.pixels[p + 0] = r;
        image.pixels[p + 1] = g;
        image.pixels[p + 2] = b;
        image.pixels[p + 3] = 255;
    }
}

make3d::DepthImage MakeDepth(int w, int h, const std::vector<std::uint8_t>& mask) {
    make3d::DepthImage depth;
    depth.width = w;
    depth.height = h;
    depth.values.assign(static_cast<size_t>(w) * h, 0.0f);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const size_t id = static_cast<size_t>(y) * w + x;
        if (!mask[id]) continue;
        const float u = w > 1 ? static_cast<float>(x) / static_cast<float>(w - 1) : 0.5f;
        const float v = h > 1 ? static_cast<float>(y) / static_cast<float>(h - 1) : 0.5f;
        const float dx = (u - 0.5f) * 2.0f;
        const float dy = (v - 0.5f) * 2.0f;
        depth.values[id] = 0.35f + 0.55f * std::max(0.0f, 1.0f - (dx * dx + dy * dy));
    }
    return depth;
}

bool ValidateResult(const char* name, const make3d::StructuredAssetBuildResult& result) {
    if (!result.ok) {
        std::cerr << name << " failed: " << result.message << "\n" << result.ToMarkdown() << "\n";
        return false;
    }
    if (result.mesh.positions.empty() || result.mesh.indices.empty()) {
        std::cerr << name << " produced empty mesh\n";
        return false;
    }
    if (!result.validation.ok) {
        std::cerr << name << " failed validation\n" << result.validation.ToMarkdown() << "\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    make3d::StructuredAssetOptions options;
    options.targetHeight = 2.0f;
    options.radialSegments = 18;

    {
        auto image = MakeImage(128, 192);
        std::vector<std::uint8_t> mask(static_cast<size_t>(image.width) * image.height, 0);
        PaintRect(image, mask, 48, 8, 80, 45, 220, 180, 140);   // head
        PaintRect(image, mask, 42, 48, 86, 110, 70, 120, 220);  // torso
        PaintRect(image, mask, 25, 54, 42, 125, 220, 170, 120); // arm
        PaintRect(image, mask, 86, 54, 103, 125, 220, 170, 120);
        PaintRect(image, mask, 46, 110, 60, 182, 80, 80, 80);   // legs
        PaintRect(image, mask, 68, 110, 82, 182, 80, 80, 80);
        auto depth = MakeDepth(image.width, image.height, mask);
        auto result = make3d::BuildStructuredAssetMesh(image, depth, mask, options);
        if (!ValidateResult("character", result)) return 1;
    }

    {
        auto image = MakeImage(160, 160);
        std::vector<std::uint8_t> mask(static_cast<size_t>(image.width) * image.height, 0);
        PaintRect(image, mask, 30, 58, 130, 76, 130, 90, 60);   // tabletop/seat
        PaintRect(image, mask, 35, 35, 125, 58, 120, 80, 50);   // back
        PaintRect(image, mask, 35, 76, 45, 145, 90, 70, 55);    // legs
        PaintRect(image, mask, 115, 76, 125, 145, 90, 70, 55);
        auto depth = MakeDepth(image.width, image.height, mask);
        auto result = make3d::BuildStructuredAssetMesh(image, depth, mask, options);
        if (!ValidateResult("furniture", result)) return 2;
    }

    {
        auto image = MakeImage(160, 180);
        std::vector<std::uint8_t> mask(static_cast<size_t>(image.width) * image.height, 0);
        PaintRect(image, mask, 35, 30, 125, 165, 160, 170, 180); // facade
        PaintRect(image, mask, 28, 12, 132, 35, 100, 100, 110);  // roof
        auto depth = MakeDepth(image.width, image.height, mask);
        auto result = make3d::BuildStructuredAssetMesh(image, depth, mask, options);
        if (!ValidateResult("building", result)) return 3;
    }

    return 0;
}
