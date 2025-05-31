#include "PartVolumeBuilder.h"
#include "VolumeBuilder.h"

std::map<PartType, Volume> BuildVolumesFromImages(const std::map<PartType, std::string>& imagePaths) {
    std::map<PartType, Volume> volumes;

    for (const auto& [part, path] : imagePaths) {
        PNGImage image = LoadPNG(path.c_str());
        Volume vol = BuildVolumeFromSilhouette(image);  // ’Pƒ‚È2D¨3D•ÏŠ·
        volumes[part] = vol;
    }

    return volumes;
}
