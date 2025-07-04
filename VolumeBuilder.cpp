#include "VolumeBuilder.h"
#include "ImageProcessor.h"

VolumeData::VolumeData(int w, int h, int d) : width(w), height(h), depth(d), voxels(w* h* d, false) {}

void VolumeData::resize(int w, int h, int d) {
    width = w; height = h; depth = d;
    voxels.assign(w * h * d, false);
}

void VolumeData::set(int x, int y, int z, bool value) {
    if (x < 0 || y < 0 || z < 0 || x >= width || y >= height || z >= depth) return;
    voxels[x + y * width + z * width * height] = value;
}

bool VolumeData::get(int x, int y, int z) const {
    if (x < 0 || y < 0 || z < 0 || x >= width || y >= height || z >= depth) return false;
    return voxels[x + y * width + z * width * height];
}

bool generateVolumeFromImages(const AppState& state, VolumeData& volume) {
    int res = state.voxelResolution;
    volume.resize(res, res, res);

    if (state.isSingleImage) {
        Image image = ConvertBitmapToImage(state.getImageBitmap(Front), state.silhouetteThreshold);
        if (image.width == 0 || image.height == 0) return false;
        volume = BuildVolumeFromSingleImageWithDepthProfile(image, res);
        return true;
    }

    std::vector<Image> masks(6);
    for (int i = 0; i < 6; ++i) {
        HBITMAP bmp = state.getImageBitmap((ViewDirection)i);
        if (!bmp) return false;
        masks[i] = ExtractMaskFromBitmap(bmp, state.silhouetteThreshold);
    }

    volume = AlignAndMergeMasks(masks, res);
    return true;
}

VolumeData BuildVolumeFromSingleImageWithDepthProfile(const Image& image, int resolution) {
    VolumeData volume(resolution, resolution, resolution);
    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            if (image.get(x, y)) {
                int depthStart = resolution / 3;
                int depthEnd = 2 * resolution / 3;
                for (int z = depthStart; z < depthEnd; ++z)
                    volume.set(x, y, z, true);
            }
        }
    }
    return volume;
}

VolumeData AlignAndMergeMasks(const std::vector<Image>& masks, int resolution) {
    VolumeData volume(resolution, resolution, resolution);
    for (int z = 0; z < resolution; ++z) {
        for (int y = 0; y < resolution; ++y) {
            for (int x = 0; x < resolution; ++x) {
                bool front = masks[Front].get(x, y);
                bool back = masks[Back].get(resolution - 1 - x, y);
                bool left = masks[Left].get(z, y);
                bool right = masks[Right].get(resolution - 1 - z, y);
                bool top = masks[Top].get(x, resolution - 1 - z);
                bool bottom = masks[Bottom].get(x, z);
                if (front && back && left && right && top && bottom)
                    volume.set(x, y, z, true);
            }
        }
    }
    return volume;
}