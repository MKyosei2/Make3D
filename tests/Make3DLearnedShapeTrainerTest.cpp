#include "Make3DLearnedShapeTrainer.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static int Fail(const char* message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

int main() {
    fs::path out = fs::current_path() / "learned_shape_trainer_test";
    fs::create_directories(out);

    make3d::LearnedShapeTrainingOptions options;
    options.syntheticSamples = 128;
    options.epochs = 12;
    options.learningRate = 0.025f;
    options.outputWeightsPath = out / "test.weights";

    make3d::LearnedShapeModelWeights weights;
    make3d::LearnedShapeTrainingReport report = make3d::TrainLearnedShapeModel(options, &weights);
    if (!report.ok) return Fail("training failed");
    if (!fs::exists(options.outputWeightsPath)) return Fail("weights were not written");
    if (report.finalTrainingLoss <= 0.0f) return Fail("invalid final loss");
    if (report.finalTrainingLoss > report.initialLoss * 1.25f) return Fail("training diverged too much");

    make3d::LearnedShapeModelWeights loaded;
    std::string error;
    if (!make3d::LoadLearnedShapeModelWeights(options.outputWeightsPath, loaded, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    std::cout << "[PASS] Make3D learned shape trainer test\n";
    std::cout << report.ToMarkdown() << "\n";
    return 0;
}
