#pragma once

#include "Make3DHeroCharacterModel.h"

namespace make3d {

struct HeroFineDetailOptions {
    bool addFaceFeatures = true;
    bool addHairStrands = true;
    bool addClothingFolds = true;
    bool addFingerHints = true;
    bool addShoeSoles = true;
    float faceScale = 1.0f;
    float hairStrandScale = 1.0f;
    float clothingFoldScale = 1.0f;
};

struct HeroFineDetailReport {
    int faceFeatureVertices = 0;
    int hairStrandVertices = 0;
    int clothingFoldVertices = 0;
    int fingerVertices = 0;
    int shoeSoleVertices = 0;
    int verticesAfter = 0;
    int trianglesAfter = 0;
};

void AddHeroFineDetails(
    MeshData& mesh,
    const HeroCharacterOptions& heroOptions,
    const HeroFineDetailOptions& detailOptions = HeroFineDetailOptions{},
    HeroFineDetailReport* report = nullptr);

} // namespace make3d
