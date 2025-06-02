#pragma once
#include <map>
#include <string>
#include "PartTypes.h"
#include "common.h"

struct GUIState {
    std::map<PartType, std::map<ViewType, std::string>> imagePaths;
    PartType selectedPart = PartType::Head;
    ViewType selectedView = ViewType::Front;
    int polygonCount = 1000;
    bool exportCombined = true;
    ExportScaleUnit scaleUnit = ExportScaleUnit::UnityMeters;
};