#pragma once

#include "VolumeUtils.h"
#include "GUIState.h"
#include <vector>

class PartProcessor {
public:
    PartProcessor();

    void setSelectionRegions(const std::vector<SelectionRegion>& regions);

    // 指定されたパーツ領域だけを抽出した VolumeData を返す
    VolumeData* process(VolumeData* inputVolume);

private:
    std::vector<SelectionRegion> regions;
};
