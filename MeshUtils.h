#pragma once
#include "common.h"
#include "PartTypes.h"

inline Vec3 ConvertVertexToVec3(const Vertex& v) {
    return Vec3(v.x, v.y, v.z);
}

float GetScaleFactor(ExportScaleUnit unit) {
    switch (unit) {
    case ExportScaleUnit::UnityMeters: return 1.0f;
    case ExportScaleUnit::MayaCentimeters: return 0.01f;
    default: return 1.0f;
    }
}
