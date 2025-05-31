#pragma once
#include <string>
#include <map>
#include <vector>

enum class ViewDirection {
    Front, Back, Left, Right, Top, Bottom
};

struct GUIState {
    std::string currentPartName = "part";
    ViewDirection selectedView = ViewDirection::Front;
    std::map<std::string, std::map<ViewDirection, std::vector<std::string>>> allParts;
    int polygonCount = 1000;
};
