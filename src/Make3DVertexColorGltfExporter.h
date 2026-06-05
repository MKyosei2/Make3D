#pragma once

#include "Make3DAdvancedCore.h"

#include <array>
#include <filesystem>
#include <string>

namespace make3d {

struct VertexColorGltfOptions {
    std::string materialName = "Make3DVertexColorMaterial";
    std::array<float, 4> fallbackColor = {0.75f, 0.78f, 0.84f, 1.0f};
    float metallicFactor = 0.0f;
    float roughnessFactor = 0.82f;
    bool doubleSided = true;
};

bool ExportGLTFWithVertexColors(
    const MeshData& mesh,
    const ImageRGBA& sourceImage,
    const std::filesystem::path& gltfPath,
    const VertexColorGltfOptions& options = VertexColorGltfOptions{},
    std::string* error = nullptr);

} // namespace make3d
