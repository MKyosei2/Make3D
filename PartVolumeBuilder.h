#pragma once
#include "common.h"
#include <string>
#include <vector>
#include <map>

struct LabeledImage {
    std::string part;
    std::string view;
    Image2D image;
};

std::map<PartType, Volume> BuildVolumesFromImages(
    const std::map<PartType, std::map<ViewType, std::vector<std::string>>>& inputMap);
