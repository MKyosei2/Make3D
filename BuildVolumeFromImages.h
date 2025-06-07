#pragma once

#include "common.h"
#include "AppState.h"

struct MaskImage {
    std::vector<std::vector<bool>> pixels;
    int width, height;
};

bool generateVolumeFromImages(const AppState& appState, VolumeData& volume);
