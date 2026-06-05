#pragma once

#include "Make3DAdvancedCore.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace make3d {

struct HeroSemanticPalette {
    std::array<float, 4> skin = {0.86f, 0.70f, 0.56f, 1.0f};
    std::array<float, 4> hair = {0.12f, 0.09f, 0.07f, 1.0f};
    std::array<float, 4> clothing = {0.25f, 0.42f, 0.78f, 1.0f};
    std::array<float, 4> lowerClothing = {0.18f, 0.22f, 0.55f, 1.0f};
    std::array<float, 4> shoes = {0.08f, 0.08f, 0.10f, 1.0f};
    std::array<float, 4> face = {0.95f, 0.78f, 0.66f, 1.0f};
    std::array<float, 4> accent = {0.95f, 0.85f, 0.25f, 1.0f};
};

struct HeroSemanticGltfOptions {
    std::string materialName = "Make3DHeroSemanticMaterial";
    float metallicFactor = 0.02f;
    float roughnessFactor = 0.72f;
    bool doubleSided = true;
};

HeroSemanticPalette ExtractHeroSemanticPalette(
    const ImageRGBA& image,
    const std::vector<std::uint8_t>& mask);

bool ExportHeroSemanticGLTF(
    const MeshData& mesh,
    const HeroSemanticPalette& palette,
    const std::filesystem::path& gltfPath,
    const HeroSemanticGltfOptions& options = HeroSemanticGltfOptions{},
    std::string* error = nullptr);

} // namespace make3d
