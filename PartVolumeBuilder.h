#pragma once
#include "common.h"
#include "PNGLoader.h"
#include <map>
#include <vector>

enum class PartType {
    Head, Body, Arm, Leg, Tail, Wing, Unknown
};

std::map<PartType, Volume> BuildVolumesFromImages(const std::map<PartType, std::vector<std::string>>& imageGroups);
