#pragma once
#include <vector>
#include "common.h"
#include "ImageProcessor.h"
#include "MainApp.h"

struct VolumeData {
    int width, height, depth;
    std::vector<bool> voxels;
    VolumeData(int w = 0, int h = 0, int d = 0);
    void resize(int w, int h, int d);
    void set(int x, int y, int z, bool value);
    bool get(int x, int y, int z) const;
};

bool generateVolumeFromImages(const AppState& state, VolumeData& volume);
VolumeData BuildVolumeFromSingleImageWithDepthProfile(const Image& image, int resolution);
VolumeData AlignAndMergeMasks(const std::vector<Image>& masks, int resolution);