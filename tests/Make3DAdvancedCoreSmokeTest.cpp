#include "Make3DAdvancedCore.h"

#include <filesystem>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

static make3d::ImageRGBA BuildSyntheticCharacterImage() {
    constexpr int W = 96;
    constexpr int H = 128;
    make3d::ImageRGBA image;
    image.width = W;
    image.height = H;
    image.pixels.assign(static_cast<size_t>(W) * H * 4, 0);

    auto setPixel = [&](int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        size_t p = (static_cast<size_t>(y) * W + x) * 4;
        image.pixels[p + 0] = r;
        image.pixels[p + 1] = g;
        image.pixels[p + 2] = b;
        image.pixels[p + 3] = a;
    };

    auto ellipse = [&](float cx, float cy, float rx, float ry, unsigned char r, unsigned char g, unsigned char b) {
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                float dx = (x - cx) / rx;
                float dy = (y - cy) / ry;
                if (dx * dx + dy * dy <= 1.0f) setPixel(x, y, r, g, b, 255);
            }
        }
    };

    auto capsule = [&](float x0, float y0, float x1, float y1, float radius, unsigned char r, unsigned char g, unsigned char b) {
        float vx = x1 - x0;
        float vy = y1 - y0;
        float len2 = vx * vx + vy * vy;
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                float t = len2 > 0.0f ? ((x - x0) * vx + (y - y0) * vy) / len2 : 0.0f;
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
                float px = x0 + vx * t;
                float py = y0 + vy * t;
                float dx = x - px;
                float dy = y - py;
                if (dx * dx + dy * dy <= radius * radius) setPixel(x, y, r, g, b, 255);
            }
        }
    };

    ellipse(48, 22, 13, 15, 230, 210, 180);      // head
    capsule(48, 37, 48, 72, 16, 80, 130, 210);   // torso
    capsule(31, 43, 15, 72, 6, 80, 130, 210);    // left arm
    capsule(65, 43, 81, 72, 6, 80, 130, 210);    // right arm
    capsule(41, 72, 35, 118, 7, 60, 80, 190);    // left leg
    capsule(55, 72, 61, 118, 7, 60, 80, 190);    // right leg
    return image;
}

static int Fail(const char* message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

int main() {
    make3d::ImageRGBA image = BuildSyntheticCharacterImage();
    make3d::ReconstructionReport report;
    auto mask = make3d::BuildForegroundMask(image, &report);
    if (report.foregroundPixels <= 0) return Fail("foreground mask is empty");
    if (report.foregroundCoverage <= 0.02f || report.foregroundCoverage >= 0.80f) return Fail("foreground coverage is outside expected range");

    make3d::AdvancedOptions options;
    options.mode = make3d::ReconstructionMode::HybridVolume;
    options.quality = make3d::QualityPreset::Detailed;
    options.maxGridResolution = 96;
    options.volumeRadialSegments = 18;

    make3d::DepthImage depth = make3d::PrepareDepth(image, std::nullopt, mask, options, &report);
    if (depth.values.empty()) return Fail("depth is empty");
    if (report.depthMax <= report.depthMin) return Fail("depth statistics are invalid");

    make3d::MeshData mesh = make3d::ReconstructMesh(image, depth, mask, options, &report);
    if (mesh.positions.empty()) return Fail("mesh has no vertices");
    if (mesh.indices.empty()) return Fail("mesh has no triangles");
    if (report.vertices <= 0 || report.triangles <= 0) return Fail("report mesh statistics are invalid");

    fs::path out = fs::current_path() / "test_output";
    std::string error;
    if (!make3d::ExportOBJ(mesh, out / "smoke.obj", "", &error)) return Fail(error.c_str());
    if (!make3d::ExportGLTF(mesh, out / "smoke.gltf", &error)) return Fail(error.c_str());
    if (!fs::exists(out / "smoke.obj")) return Fail("OBJ file was not written");
    if (!fs::exists(out / "smoke.gltf")) return Fail("glTF file was not written");
    if (!fs::exists(out / "smoke.bin")) return Fail("glTF binary buffer was not written");

    std::cout << "[PASS] Make3D advanced core smoke test\n";
    std::cout << "Foreground pixels: " << report.foregroundPixels << "\n";
    std::cout << "Vertices: " << report.vertices << "\n";
    std::cout << "Triangles: " << report.triangles << "\n";
    return 0;
}
