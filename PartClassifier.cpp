#include "PartClassifier.h"
#include <cmath>

PartType ClassifyPart(int x, int y, int w, int h, int imageW, int imageH) {
    float cx = x + w / 2.0f;
    float cy = y + h / 2.0f;

    if (cy < imageH * 0.4 && h < w) return PartType::Head;
    if (cy > imageH * 0.8 && h > w * 1.5) return PartType::Leg;
    if (std::abs(cx - imageW / 2) > imageW * 0.25) return PartType::Arm;
    return PartType::Body;
}
