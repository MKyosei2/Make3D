#pragma once

#include "Make3DAdvancedCore.h"

#include <cstdint>
#include <string>
#include <vector>

namespace make3d {

struct HeroCharacterOptions {
    int radialSegments = 32;
    int verticalSegments = 12;
    float heightScale = 2.6f;
    float widthScale = 1.05f;
    float depthScale = 0.72f;
    float headScale = 1.0f;
    float torsoScale = 1.0f;
    float limbScale = 1.0f;
    float hairScale = 1.0f;
    float clothingScale = 1.0f;
    bool buildHead = true;
    bool buildTorso = true;
    bool buildArms = true;
    bool buildLegs = true;
    bool buildNeckAndShoulders = true;
    bool buildHands = true;
    bool buildFeet = true;
    bool buildHair = true;
    bool buildClothing = true;
    bool addBackVolume = true;
};

struct HeroCharacterReport {
    bool ok = false;
    int sourceWidth = 0;
    int sourceHeight = 0;
    int foregroundPixels = 0;
    int vertices = 0;
    int triangles = 0;
    int hairVertices = 0;
    int clothingVertices = 0;
    int connectorVertices = 0;
    float detectedAspectRatio = 1.0f;
    float detectedSymmetry = 0.0f;
    float bodyHeight = 0.0f;
    float bodyWidth = 0.0f;
    std::vector<std::string> warnings;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

MeshData BuildHeroCharacterMesh(
    const ImageRGBA& image,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& refinedMask,
    const HeroCharacterOptions& options = HeroCharacterOptions{},
    HeroCharacterReport* report = nullptr);

} // namespace make3d
