#pragma once
#include <map>
#include "common.h"
#include "PartTypes.h"

Volume3D BuildVolumeFromImages(const std::map<ViewType, Image2D>& images);