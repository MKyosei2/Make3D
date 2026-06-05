#pragma once

#include "Make3DAdvancedCore.h"
#include "Make3DAssetTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace make3d {

struct AssetAnalysisOptions {
    int maxContourPoints = 256;
    int paletteBinsPerChannel = 4;
    int maxPaletteColors = 6;
    bool enablePartHints = true;
};

struct ContourPoint2D {
    int x = 0;
    int y = 0;
    float nx = 0.0f;
    float ny = 0.0f;
};

struct ShapeDescriptorReport {
    int foregroundPixels = 0;
    int minX = 0;
    int minY = 0;
    int maxX = -1;
    int maxY = -1;
    float coverage = 0.0f;
    float aspectRatio = 1.0f;
    float rectangularity = 0.0f;
    float perimeterRatio = 0.0f;
    float symmetryX = 0.0f;
    float symmetryY = 0.0f;
    float rowStability = 0.0f;
    float columnStability = 0.0f;
    float straightEdgeScore = 0.0f;
    float edgeDensity = 0.0f;
    float massCenterX = 0.5f;
    float massCenterY = 0.5f;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

struct PaletteColor {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    int pixels = 0;
    float coverage = 0.0f;
};

struct PartHint2D {
    std::string name;
    int minX = 0;
    int minY = 0;
    int maxX = -1;
    int maxY = -1;
    float confidence = 0.0f;
    std::string role;
};

struct AssetAnalysisResult {
    bool ok = false;
    AssetClassificationResult classification;
    ShapeDescriptorReport shape;
    std::vector<ContourPoint2D> contour;
    std::vector<PaletteColor> palette;
    std::vector<PartHint2D> partHints;
    std::vector<std::string> warnings;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

AssetAnalysisResult AnalyzeAsset(
    const ImageRGBA& image,
    const std::vector<std::uint8_t>& mask,
    const DepthImage& depth,
    const AssetAnalysisOptions& options = AssetAnalysisOptions{});

} // namespace make3d
