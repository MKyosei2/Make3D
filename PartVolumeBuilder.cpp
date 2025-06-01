#include "PartVolumeBuilder.h"
#include "VolumeBuilder.h"
#include "PNGLoader.h"
#include "VolumeMerger.h"

static ViewType ViewFromString(const std::string& s) {
    if (s == "Front") return ViewType::Front;
    if (s == "Back") return ViewType::Back;
    if (s == "Left") return ViewType::Left;
    if (s == "Right") return ViewType::Right;
    if (s == "Top") return ViewType::Top;
    if (s == "Bottom") return ViewType::Bottom;
    return ViewType::Front;
}

static std::string ViewToString(ViewType v) {
    static const char* names[] = { "Front", "Back", "Left", "Right", "Top", "Bottom" };
    return names[(int)v];
}

static std::string PartToString(PartType p) {
    static const char* names[] = { "Head", "Body", "Arm", "Leg", "Tail", "Wing", "Unknown" };
    return names[(int)p];
}

std::map<PartType, Volume> BuildVolumesFromImages(
    const std::map<PartType, std::map<ViewType, std::vector<std::string>>>& inputMap) {

    std::map<PartType, Volume> result;

    for (const auto& [part, viewMap] : inputMap) {
        std::vector<Volume> partials;

        for (const auto& [view, paths] : viewMap) {
            for (const auto& path : paths) {
                Image2D img = LoadPNG(path.c_str());
                Volume vol = BuildVolumeFromSilhouette(img, view);
                partials.push_back(vol);
            }
        }

        Volume merged = MergeVolumes(partials);
        result[part] = merged;
    }

    return result;
}
