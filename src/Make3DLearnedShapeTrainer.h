#pragma once

#include "Make3DLearnedShapeModel.h"

#include <filesystem>
#include <string>

namespace make3d {

struct LearnedShapeTrainingOptions {
    int syntheticSamples = 512;
    int epochs = 80;
    float learningRate = 0.035f;
    float validationSplit = 0.20f;
    std::filesystem::path outputWeightsPath;
};

struct LearnedShapeTrainingReport {
    bool ok = false;
    std::string message;
    int syntheticSamples = 0;
    int trainingSamples = 0;
    int validationSamples = 0;
    int epochs = 0;
    float initialLoss = 0.0f;
    float finalTrainingLoss = 0.0f;
    float finalValidationLoss = 0.0f;
    std::filesystem::path outputWeightsPath;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

LearnedShapeTrainingReport TrainLearnedShapeModel(
    const LearnedShapeTrainingOptions& options,
    LearnedShapeModelWeights* trainedWeights = nullptr);

} // namespace make3d
