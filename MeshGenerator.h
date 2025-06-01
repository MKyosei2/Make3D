#pragma once
#include "GUIState.h"
#include "PartVolumeBuilder.h"

Mesh3D GenerateMeshFromVolume(const Volume3D& volume, int polygonCount);
