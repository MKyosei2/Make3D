#pragma once
#include "common.h"

// 画像はnullptr可。depthは出力Volumeの奥行き。
Volume BuildVolumeFromMultipleSilhouettes(
    const Image2D* front,
    const Image2D* back,
    const Image2D* right,
    const Image2D* left,
    const Image2D* top,
    const Image2D* bottom,
    int depth);
