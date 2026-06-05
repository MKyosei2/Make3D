#include "Make3DAdvancedCore.h"
#include "Make3DLearnedShapeModel.h"
#include "Make3DShapeInference.h"

#include <filesystem>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

static make3d::ImageRGBA BuildImage() {
    constexpr int W = 48;
    constexpr int H = 64;
    make3d::ImageRGBA image;
    image.width = W;
    image.height = H;
    image.pixels.assign(static_cast<size_t>(W) * H * 4, 0);
    for (int y = 8; y < 56; ++y) {
        for (int x = 12; x < 36; ++x) {
            float dx = (static_cast<float>(x) - 24.0f) / 12.0f;
            float dy = (static_cast<float>(y) - 32.0f) / 24.0f;
            if (dx * dx + dy * dy <= 1.0f) {
                size_t p = (static_cast<size_t>(y) * W + x) * 4;
                image.pixels[p + 0] = static_cast<unsigned char>(120 + x);
                image.pixels[p + 1] = static_cast<unsigned char>(130 + y);
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
    make3d::ShapeInferenceResult shape = make3d::RunShapeInference(image, mask, depth);

    make3d::LearnedShapeModelResult result = make3d::RunLearnedShapeModel(image, mask, depth, shape);
    if (!result.ok) return Fail("learned shape model failed");
    if (result.learnedDepth.values.size() != depth.values.size()) return Fail("learned depth size mismatch");
    if (result.confidence.size() != depth.values.size()) return Fail("confidence size mismatch");
    if (result.normalXYZ.size() != depth.values.size() * 3) return Fail("normal size mismatch");
    if (result.meanConfidence <= 0.0f) return Fail("confidence was not computed");

    make3d::LearnedShapeModelWeights weights = make3d::LearnedShapeModelWeights::BuiltInPrior();
    fs::path weightsPath = fs::current_path() / "learned_shape_test.weights";
    std::string error;
    if (!make3d::SaveLearnedShapeModelWeights(weights, weightsPath, &error)) {
        std::cerr << error << "\n";
        return 1;
    }
    make3d::LearnedShapeModelWeights loaded;
    if (!make3d::LoadLearnedShapeModelWeights(weightsPath, loaded, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    std::cout << "[PASS] Make3D learned shape model test\n";
    std::cout << result.ToMarkdown() << "\n";
    return 0;
}
