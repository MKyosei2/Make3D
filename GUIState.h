#pragma once
#include <map>
#include <vector>
#include <string>
#include "PartVolumeBuilder.h"

struct GUIState {
    std::map<PartType, std::vector<std::string>> partImages;
    PartType selectedPart = PartType::Unknown;
    int polygonCount = 1000;
};
