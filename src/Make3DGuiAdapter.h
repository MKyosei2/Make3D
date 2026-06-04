#pragma once

#include "Make3DAdvancedCore.h"

#include <filesystem>
#include <optional>
#include <string>

namespace make3d::gui {

struct GuiBuildRequest {
    std::filesystem::path colorPath;
    std::optional<std::filesystem::path> depthPath;
    std::filesystem::path outputDir;
    int guiReconstructionIndex = 0; // 0 Auto, 1 Relief, 2 PrimitiveBox, 3 HumanoidProxy
    int guiQualityIndex = 1;        // 0 Easy, 1 Recommended, 2 Detailed
    bool exportObj = true;
    bool exportGltf = true;
    bool writeDebugImages = true;
};

struct GuiBuildSummary {
    bool ok = false;
    std::string message;
    std::filesystem::path objPath;
    std::filesystem::path gltfPath;
    std::filesystem::path reportPath;
    int vertices = 0;
    int triangles = 0;
};

AdvancedOptions OptionsFromGuiSelection(int reconstructionIndex, int qualityIndex);
GuiBuildSummary BuildAdvancedFromGuiRequest(const GuiBuildRequest& request);

} // namespace make3d::gui
