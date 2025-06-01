#pragma once
#include <string>
#include <map>
#include <vector>
#include "PartTypes.h"

struct GUIState {
    std::map<PartType, std::map<ViewType, std::string>> imagePaths;
    PartType selectedPart = PartType::Head;
    ViewType selectedView = ViewType::Front;
    int polygonCount = 1000;
    bool exportCombined = true;
};

struct Vertex {
    float x, y, z;
};

struct Mesh3D {
    std::vector<Vertex> vertices;
    std::vector<int> indices;
};

struct Image2D {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels; // RGBA

    bool IsOpaque(int x, int y) const {
        return pixels[(y * width + x) * 4 + 3] > 128;
    }
};
