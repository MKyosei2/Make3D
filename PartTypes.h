#pragma once
#include <string>
#include <map>
#include <vector>

enum class PartType {
    Head, Body, Arm, Leg, Other
};

enum class ViewType {
    Front, Back, Left, Right, Top, Bottom
};

inline const char* ToString(PartType type) {
    switch (type) {
    case PartType::Head: return "Head";
    case PartType::Body: return "Body";
    case PartType::Arm:  return "Arm";
    case PartType::Leg:  return "Leg";
    default: return "Other";
    }
}

inline const char* ToString(ViewType view) {
    switch (view) {
    case ViewType::Front:  return "Front";
    case ViewType::Back:   return "Back";
    case ViewType::Left:   return "Left";
    case ViewType::Right:  return "Right";
    case ViewType::Top:    return "Top";
    case ViewType::Bottom: return "Bottom";
    default: return "Unknown";
    }
}
