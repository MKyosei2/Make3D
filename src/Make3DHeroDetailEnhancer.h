#pragma once

#include "Make3DHeroCharacterModel.h"

namespace make3d {

void AddHeroDetailVolumes(
    MeshData& mesh,
    const HeroCharacterOptions& options,
    HeroCharacterReport* report = nullptr);

} // namespace make3d
