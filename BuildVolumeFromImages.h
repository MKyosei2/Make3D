#pragma once

#include "common.h"
#include "AppState.h"
#include "ImageUtils.h"
#include "VolumeUtils.h"

VolumeData BuildVolumeFromSingleImage(const Image& img);

struct MaskImage {
    std::vector<std::vector<bool>> pixels;
    int width, height;
};

bool generateVolumeFromImages(const AppState& appState, VolumeData& volume);