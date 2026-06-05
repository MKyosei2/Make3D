#pragma once

#include "Make3DAdvancedCore.h"

#include <cstdint>
#include <string>
#include <vector>

namespace make3d {

enum class ShapeClass {
    Unknown,
    Character,
    Prop,
    Flat
};

struct ShapeInferenceOptions {
    bool useShapePrior = true;
    bool useSymmetry = true;
    int profileSamples = 64;
    float backDepthScale = 0.72f;
    float sideDepthScale = 0.82f;
};

struct ShapeInferenceResult {
    ShapeClass shapeClass = ShapeClass::Unknown;
    int foregroundPixels = 0;
    float coverage = 0.0f;
    float aspectRatio = 1.0f;
    float symmetry = 0.0f;
    float massCenterY = 0.5f;
    std::vector<float> radiusProfile;
    std::vector<float> depthProfile;
    DepthImage adjustedDepth;
    DepthImage backDepth;
    DepthImage sideDepth;
    std::string ToMarkdown() const;
    std::string ToJson() const;
};

const char* ToString(ShapeClass value);

ShapeInferenceResult RunShapeInference(
    const ImageRGBA& image,
    const std::vector<std::uint8_t>& mask,
    const DepthImage& depth,
    const ShapeInferenceOptions& options = ShapeInferenceOptions{});

} // namespace make3d
