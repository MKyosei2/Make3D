#pragma once
#include <windows.h>

class VolumeBuilder
{
public:
    VolumeBuilder();
    ~VolumeBuilder();

    bool buildVolume(HBITMAP images[6], int silhouetteThreshold);
};
