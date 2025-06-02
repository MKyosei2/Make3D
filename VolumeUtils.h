#pragma once

#include <map>
#include "common.h"

// ボリューム構築
Volume BuildVolumeFromImages(const std::map<ViewType, Image2D>& images);

// ボリューム合成
Volume MergeVolumes(const std::vector<Volume>& volumes);
