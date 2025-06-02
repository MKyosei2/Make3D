#pragma once
#include <map>
#include "common.h"

enum class ViewType {
    Front,
    Back,
    Left,
    Right,
    Top,
    Bottom
};

Volume3D BuildVolumeFromImages(const std::map<ViewType, Image2D>& images);
