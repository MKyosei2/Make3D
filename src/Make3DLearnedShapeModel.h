#pragma once

#include "Make3DAdvancedCore.h"
#include "Make3DShapeInference.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace make3d {

struct LearnedShapeModelOptions {
    bool enabled = true;
    bool useExternalWeights = false;
    std::filesystem::path weightsPath;
    float blendWithInputDepth = 0.35f;
    float confidenceThreshold = 0.10f;
    int hiddenUnits = 8;
};

struct LearnedShapeModelWeights {
    static constexpr int InputFeatures = 10;
    static constexpr int HiddenUnits = 8;
    static constexpr int OutputFeatures = 3;

    std::array<float, InputFeatures * HiddenUnits> inputToHidden{};
    std::array<float, HiddenUnits> hiddenBias{};
    std::array<float, HiddenUnits * OutputFeatures> hiddenToOutput{};
    std::array<float, OutputFeatures> outputBias{};

    static LearnedShapeModelWeights BuiltInPrior();
};

struct LearnedShapeModelResult {
    bool ok = false;
    std::string message;
    DepthImage learnedDepth;
    std::vector<float> confidence;
    std::vector<float> normalXYZ;
    float meanConfidence = 0.0f;
    float depthMean = 0.0f;
    float depthMin = 0.0f;
    float depthMax = 0.0f;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

bool SaveLearnedShapeModelWeights(const LearnedShapeModelWeights& weights, const std::filesystem::path& path, std::string* error = nullptr);
bool LoadLearnedShapeModelWeights(const std::filesystem::path& path, LearnedShapeModelWeights& weights, std::string* error = nullptr);

LearnedShapeModelResult RunLearnedShapeModel(
    const ImageRGBA& image,
    const std::vector<std::uint8_t>& mask,
    const DepthImage& inputDepth,
    const ShapeInferenceResult& shapeInference,
    const LearnedShapeModelOptions& options = LearnedShapeModelOptions{});

} // namespace make3d
