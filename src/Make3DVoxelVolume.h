#pragma once

#include "Make3DAdvancedCore.h"

#include <cstdint>
#include <string>
#include <vector>

namespace make3d {

struct VoxelVolumeOptions {
    int verticalSamples = 96;
    int radialSegments = 28;
    int rowSearchRadius = 2;
    int radiusSmoothingIterations = 3;
    float xyScale = 2.2f;
    float depthScale = 1.25f;
    float minRadius = 0.025f;
    float zInflation = 0.55f;
    bool closeCaps = true;
};

struct VoxelVolumeReport {
    int sourceWidth = 0;
    int sourceHeight = 0;
    int sampledRows = 0;
    int generatedRings = 0;
    int radialSegments = 0;
    int vertices = 0;
    int triangles = 0;
    float minRadiusX = 0.0f;
    float maxRadiusX = 0.0f;
    float minRadiusZ = 0.0f;
    float maxRadiusZ = 0.0f;
    bool closedCaps = false;
    std::vector<std::string> warnings;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

MeshData BuildVoxelVolumeMesh(
    const ImageRGBA& image,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& mask,
    const VoxelVolumeOptions& options = VoxelVolumeOptions{},
    VoxelVolumeReport* report = nullptr);

} // namespace make3d
