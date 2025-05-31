#pragma once
#include "common.h"

enum class PartType {
    Head,
    Body,
    Arm,
    Leg,
    Unknown
};

struct Volume {
    int width, height, depth;
    std::vector<uint8_t> data; // 0 = empty, 1 = filled

    uint8_t get(int x, int y, int z) const {
        return data[x + y * width + z * width * height];
    }

    void set(int x, int y, int z, uint8_t value) {
        data[x + y * width + z * width * height] = value;
    }
};

struct LabeledVolume {
    Volume volume;
    PartType part;
};

std::vector<LabeledVolume> BuildVolumesFromLabeledImages(const std::vector<std::tuple<std::string, std::string, Image2D>>& labeledImages);
