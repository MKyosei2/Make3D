#pragma once
#include <map>
#include <vector>
#include <string>

enum class PartType { Head, Body, Arm, Leg, Tail, Wing, Unknown };
enum class ViewType { Front, Back, Left, Right, Top, Bottom };
enum class ExportScaleUnit { UnityMeters, MayaCentimeters };

struct GUIState {
    std::map<PartType, std::map<ViewType, std::vector<std::string>>> partViewImages;
    PartType selectedPart = PartType::Unknown;
    ViewType selectedView = ViewType::Front;
    int polygonCount = 1000;
    bool exportCombined = false;
    ExportScaleUnit scaleUnit = ExportScaleUnit::UnityMeters;
};

extern GUIState guiState;
