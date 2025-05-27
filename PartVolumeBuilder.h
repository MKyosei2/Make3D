#pragma once
#include "common.h"
#include "PartClassifier.h"

struct PartRegion {
    int x, y, width, height;
};

struct PartVolume {
    PartType type;
    Volume volume;
};

std::vector<PartVolume> BuildPartsFromImage(const Image2D& image, const std::vector<PartRegion>& regions, int depth);
