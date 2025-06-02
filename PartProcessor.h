#pragma once

#include <map>
#include <string>
#include "common.h"

// パーツ名一覧
std::vector<std::string> GetPartNames();

// 視点名一覧
std::vector<std::string> GetAvailableViews();

// パーツ別のVolume構築
std::map<PartType, Volume> BuildPartVolumes(
    const std::map<PartType, std::map<ViewType, Image2D>>& allImages);