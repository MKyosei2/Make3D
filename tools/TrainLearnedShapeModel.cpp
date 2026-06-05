#include "Make3DLearnedShapeTrainer.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    fs::path outDir = argc >= 2 ? fs::path(argv[1]) : fs::path("learned_shape_training");
    fs::create_directories(outDir);

    make3d::LearnedShapeTrainingOptions options;
    options.syntheticSamples = 768;
    options.epochs = 100;
    options.learningRate = 0.032f;
    options.validationSplit = 0.20f;
    options.outputWeightsPath = outDir / "learned_shape.weights";

    make3d::LearnedShapeTrainingReport report = make3d::TrainLearnedShapeModel(options);
    if (!report.ok) {
        std::cerr << report.message << "\n";
        return 1;
    }

    std::ofstream md(outDir / "learned_shape_training_report.md", std::ios::binary);
    md << report.ToMarkdown();
    std::ofstream js(outDir / "learned_shape_training_report.json", std::ios::binary);
    js << report.ToJson();

    std::cout << report.message << "\n";
    std::cout << "Weights: " << report.outputWeightsPath.u8string() << "\n";
    std::cout << "Initial loss: " << report.initialLoss << "\n";
    std::cout << "Final training loss: " << report.finalTrainingLoss << "\n";
    std::cout << "Final validation loss: " << report.finalValidationLoss << "\n";
    return 0;
}
