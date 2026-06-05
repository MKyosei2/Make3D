#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace make3d {

struct MaskRefineOptions {
    int smoothingIterations = 3;
    int minComponentPixels = 64;
    bool keepLargestComponentOnly = true;
    bool fillInteriorHoles = true;
    bool smoothJaggedEdges = true;
};

struct MaskRefineReport {
    int width = 0;
    int height = 0;
    int foregroundBefore = 0;
    int foregroundAfter = 0;
    int componentsBefore = 0;
    int componentsAfter = 0;
    int removedComponents = 0;
    int filledHolePixels = 0;
    int smoothingIterations = 0;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

MaskRefineReport RefineForegroundMask(
    std::vector<std::uint8_t>& mask,
    int width,
    int height,
    const MaskRefineOptions& options = MaskRefineOptions{});

} // namespace make3d
