#pragma once

#include "Make3DAdvancedCore.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace make3d {

struct StageTiming {
    std::string name;
    double milliseconds = 0.0;
    int vertices = 0;
    int triangles = 0;
    std::string note;
};

struct ProfiledBuildOutput {
    bool ok = false;
    std::string message;

    // Lightweight mesh for interactive previews.
    MeshData previewMesh;

    // Full-quality mesh for saved/exported assets.
    MeshData finalMesh;

    ReconstructionReport report;
    std::vector<StageTiming> stages;

    fs::path objPath;
    fs::path gltfPath;
    fs::path reportPath;
    fs::path benchmarkJsonPath;
    fs::path benchmarkMarkdownPath;
};

AdvancedOptions MakePreviewOptions(const AdvancedOptions& finalOptions);

MeshData BuildPreviewMeshForDisplay(
    const ImageRGBA& color,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& mask,
    const AdvancedOptions& finalOptions,
    ReconstructionReport* report = nullptr);

ProfiledBuildOutput BuildProfiledModelFromImage(
    const fs::path& colorPath,
    const std::optional<fs::path>& depthPath,
    const fs::path& outputDir,
    const AdvancedOptions& options);

std::string StageTimingsToMarkdown(const std::vector<StageTiming>& stages);
std::string StageTimingsToJson(const std::vector<StageTiming>& stages);

} // namespace make3d
