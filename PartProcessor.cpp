#include "PartProcessor.h"

std::vector<std::string> GetPartNames() {
    return {
        "Head",
        "Body",
        "ArmL",
        "ArmR",
        "LegL",
        "LegR"
    };
}

std::vector<std::string> GetAvailableViews() {
    return {
        "Front",
        "Back",
        "Left",
        "Right",
        "Top",
        "Bottom"
    };
}

std::map<PartType, Volume> BuildPartVolumes(
    const std::map<PartType, std::map<ViewType, Image2D>>& allImages)
{
    std::map<PartType, Volume> result;
    for (const auto& [part, views] : allImages) {
        Volume vol = BuildVolumeFromImages(views);
        result[part] = vol;
    }
    return result;
}