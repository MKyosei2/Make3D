#pragma once
#include <windows.h>
#include <vector>

struct PartRegion {
    std::vector<POINT> outline;
    RECT boundingBox;
    int label = 0;
};
