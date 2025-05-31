#include "VolumeBuilder.h"
#include <algorithm>
#include <unordered_map>

static PartType ClassifyPart(const std::string& label) {
    if (label == "head") return PartType::Head;
    if (label == "body") return PartType::Body;
    if (label == "arm") return PartType::Arm;
    if (label == "leg") return PartType::Leg;
    return PartType::Unknown;
}

std::vector<LabeledVolume> BuildVolumesFromLabeledImages(const std::vector<std::tuple<std::string, std::string, Image2D>>& labeledImages) {
    std::unordered_map<std::string, std::unordered_map<std::string, Image2D>> partViews;
    for (const auto& [part, view, img] : labeledImages) {
        partViews[part][view] = img;
    }

    std::vector<LabeledVolume> result;
    int size = 64;

    for (const auto& [partLabel, views] : partViews) {
        Volume vol;
        vol.width = vol.height = vol.depth = size;
        vol.data.resize(size * size * size, 0);

        for (const auto& [view, img] : views) {
            if (view == "front") {
                for (int y = 0; y < std::min(size, img.height); ++y) {
                    for (int x = 0; x < std::min(size, img.width); ++x) {
                        if (img.pixels[y * img.width + x] > 0) {
                            for (int z = 0; z < size; ++z) {
                                vol.set(x, size - y - 1, z, 1);
                            }
                        }
                    }
                }
            }
            else if (view == "side") {
                for (int y = 0; y < std::min(size, img.height); ++y) {
                    for (int z = 0; z < std::min(size, img.width); ++z) {
                        if (img.pixels[y * img.width + z] > 0) {
                            for (int x = 0; x < size; ++x) {
                                vol.set(x, size - y - 1, z, 1);
                            }
                        }
                    }
                }
            }
            else if (view == "top") {
                for (int z = 0; z < std::min(size, img.height); ++z) {
                    for (int x = 0; x < std::min(size, img.width); ++x) {
                        if (img.pixels[z * img.width + x] > 0) {
                            for (int y = 0; y < size; ++y) {
                                vol.set(x, y, size - z - 1, 1);
                            }
                        }
                    }
                }
            }
        }

        result.push_back({ vol, ClassifyPart(partLabel) });
    }

    return result;
}