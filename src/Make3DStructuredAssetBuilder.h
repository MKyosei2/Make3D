#pragma once

#include "Make3DGameAssetGenerator.h"

#include <string>
#include <vector>

namespace make3d {

struct StructuredAssetOptions {
    float targetHeight = 2.0f;
    float defaultDepth = 0.62f;
    int radialSegments = 24;
    bool addDetailParts = true;
    bool preferSmoothCharacters = true;
    bool normalizeOutput = true;

    // Unknown means auto-classify from the image. Any other value forces the structured
    // builder to use that category template. This is important because single-image
    // classification is ambiguous: a front-facing chibi character can look box-like to
    // purely geometric heuristics, and furniture can look character-like when viewed head-on.
    GameAssetType forcedAssetType = GameAssetType::Unknown;
};

struct StructuredAssetBuildResult {
    bool ok = false;
    std::string message;
    MeshData mesh;
    AssetAnalysisResult analysis;
    AssetPlanResult plan;
    GameAssetValidationReport validation;
    std::vector<std::string> warnings;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

StructuredAssetBuildResult BuildStructuredAssetMesh(
    const ImageRGBA& image,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& mask,
    const StructuredAssetOptions& options = StructuredAssetOptions{});

} // namespace make3d
