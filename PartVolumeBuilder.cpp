#include "PartVolumeBuilder.h"
#include "VolumeBuilder.h"
#include "VolumeMerger.h"

std::map<PartType, Volume> BuildVolumesFromImages(const std::map<PartType, std::vector<std::string>>& imageGroups) {
    std::map<PartType, Volume> volumes;

    for (const auto& [part, paths] : imageGroups) {
        std::vector<Volume> partVolumes;

        for (const auto& path : paths) {
            PNGImage image = LoadPNG(path.c_str());
            Volume vol = BuildVolumeFromSilhouette(image);
            partVolumes.push_back(vol);
        }

        volumes[part] = MergeVolumes(partVolumes);
    }

    return volumes;
}