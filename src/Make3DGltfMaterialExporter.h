#pragma once

#include "Make3DAdvancedCore.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>

namespace make3d {

struct GltfMaterialOptions {
    std::string materialName = "Make3DMaterial";
    std::array<float, 4> baseColorFactor = {0.82f, 0.82f, 0.82f, 1.0f};
    float metallicFactor = 0.0f;
    float roughnessFactor = 0.86f;
    bool doubleSided = true;
    std::optional<std::filesystem::path> textureUri;
};

bool ExportGLTFWithMaterial(
    const MeshData& mesh,
    const std::filesystem::path& gltfPath,
    const GltfMaterialOptions& material,
    std::string* error = nullptr);

} // namespace make3d
