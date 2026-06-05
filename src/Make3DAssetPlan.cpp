#include "Make3DAssetPlan.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>

namespace make3d {
namespace {

static float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

static std::string EscapeJson(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '\\') o << "\\\\";
        else if (c == '"') o << "\\\"";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else o << c;
    }
    return o.str();
}

static PlannedPartKind KindFromHint(const PartHint2D& hint, GameAssetType type) {
    const std::string& role = hint.role;
    if (type == GameAssetType::Character || type == GameAssetType::Creature) {
        if (role == "head") return PlannedPartKind::CharacterHead;
        if (role == "torso") return PlannedPartKind::CharacterTorso;
        if (role == "arm") return PlannedPartKind::CharacterArm;
        if (role == "leg") return PlannedPartKind::CharacterLeg;
        if (role == "hair") return PlannedPartKind::CharacterHair;
        if (role == "clothing") return PlannedPartKind::CharacterClothing;
    }
    if (type == GameAssetType::Building || type == GameAssetType::ArchitecturalPart) {
        if (role == "wall") return PlannedPartKind::BuildingFacade;
        if (role == "roof") return PlannedPartKind::BuildingRoof;
        if (role == "door") return PlannedPartKind::BuildingDoor;
        if (role == "windows") return PlannedPartKind::BuildingWindowGrid;
    }
    if (type == GameAssetType::Vehicle) {
        if (role == "body") return PlannedPartKind::VehicleBody;
        if (role == "cabin") return PlannedPartKind::VehicleCabin;
        if (role == "wheel") return PlannedPartKind::VehicleWheel;
    }
    if (type == GameAssetType::ComplexObject || type == GameAssetType::Machine) {
        if (role == "core") return PlannedPartKind::ComplexCore;
        if (role == "protrusion") return PlannedPartKind::ComplexProtrusion;
        if (role == "detail") return PlannedPartKind::DetailPanel;
    }
    if (role == "detail") return PlannedPartKind::DetailBand;
    if (role == "panel") return PlannedPartKind::DetailPanel;
    return PlannedPartKind::MainVolume;
}

static PlannedAssetPart PartFromHint(const PartHint2D& hint, const ShapeDescriptorReport& shape, GameAssetType type, const AssetPlanResult& plan) {
    PlannedAssetPart part;
    part.kind = KindFromHint(hint, type);
    part.name = hint.name.empty() ? std::string(ToString(part.kind)) : hint.name;
    part.confidence = Clamp01(hint.confidence);
    const float bw = static_cast<float>(std::max(1, shape.maxX - shape.minX + 1));
    const float bh = static_cast<float>(std::max(1, shape.maxY - shape.minY + 1));
    const float nx0 = (static_cast<float>(hint.minX - shape.minX) / bw) - 0.5f;
    const float nx1 = (static_cast<float>(hint.maxX - shape.minX) / bw) - 0.5f;
    const float ny0 = static_cast<float>(hint.minY - shape.minY) / bh;
    const float ny1 = static_cast<float>(hint.maxY - shape.minY) / bh;
    part.centerX = (nx0 + nx1) * 0.5f * plan.targetWidth;
    part.centerY = plan.targetHeight * (1.0f - (ny0 + ny1) * 0.5f);
    part.centerZ = 0.0f;
    part.sizeX = std::max(0.04f, std::abs(nx1 - nx0) * plan.targetWidth);
    part.sizeY = std::max(0.04f, std::abs(ny1 - ny0) * plan.targetHeight);
    part.sizeZ = std::max(0.04f, plan.targetDepth * 0.70f);
    if (part.kind == PlannedPartKind::BuildingRoof) part.sizeZ = plan.targetDepth * 1.10f;
    if (part.kind == PlannedPartKind::VehicleWheel) { part.sizeY = std::max(part.sizeY, plan.targetHeight * 0.18f); part.sizeZ = part.sizeY; }
    if (part.kind == PlannedPartKind::CharacterHead) part.sizeZ = std::max(part.sizeZ, part.sizeX * 0.75f);
    if (part.kind == PlannedPartKind::ComplexProtrusion) part.sizeZ = plan.targetDepth * 0.45f;
    part.materialSlot = 0;
    return part;
}

static PlannedMaterialSlot MakeMaterial(const PaletteColor& color, size_t index) {
    PlannedMaterialSlot m;
    std::ostringstream name;
    name << "mat_" << index;
    m.name = name.str();
    m.r = color.r;
    m.g = color.g;
    m.b = color.b;
    m.coverage = color.coverage;
    return m;
}

} // namespace

const char* ToString(PlannedPartKind kind) {
    switch (kind) {
        case PlannedPartKind::MainVolume: return "MainVolume";
        case PlannedPartKind::CharacterHead: return "CharacterHead";
        case PlannedPartKind::CharacterTorso: return "CharacterTorso";
        case PlannedPartKind::CharacterArm: return "CharacterArm";
        case PlannedPartKind::CharacterLeg: return "CharacterLeg";
        case PlannedPartKind::CharacterHair: return "CharacterHair";
        case PlannedPartKind::CharacterClothing: return "CharacterClothing";
        case PlannedPartKind::BuildingFacade: return "BuildingFacade";
        case PlannedPartKind::BuildingRoof: return "BuildingRoof";
        case PlannedPartKind::BuildingDoor: return "BuildingDoor";
        case PlannedPartKind::BuildingWindowGrid: return "BuildingWindowGrid";
        case PlannedPartKind::VehicleBody: return "VehicleBody";
        case PlannedPartKind::VehicleCabin: return "VehicleCabin";
        case PlannedPartKind::VehicleWheel: return "VehicleWheel";
        case PlannedPartKind::ComplexCore: return "ComplexCore";
        case PlannedPartKind::ComplexProtrusion: return "ComplexProtrusion";
        case PlannedPartKind::DetailPanel: return "DetailPanel";
        case PlannedPartKind::DetailBand: return "DetailBand";
        default: return "Unknown";
    }
}

AssetPlanResult BuildAssetPlanFromAnalysis(const AssetAnalysisResult& analysis, const AssetPlanOptions& options) {
    AssetPlanResult plan;
    if (!analysis.ok || analysis.shape.foregroundPixels <= 0) {
        plan.warnings.push_back("Cannot build asset plan because analysis is invalid.");
        return plan;
    }

    plan.ok = true;
    plan.assetType = analysis.classification.assetType;
    plan.assetName = "Make3D_" + std::string(ToString(plan.assetType)) + "_Planned";
    plan.targetHeight = std::max(0.10f, options.targetHeight);
    plan.targetWidth = plan.targetHeight / std::max(0.25f, analysis.shape.aspectRatio);
    plan.targetWidth = std::clamp(plan.targetWidth, 0.35f, 3.25f);
    plan.targetDepth = std::max(0.08f, options.defaultDepth);
    plan.pivotX = 0.0f;
    plan.pivotY = 0.0f;
    plan.pivotZ = 0.0f;
    plan.wantsCollider = options.includeCollider;
    plan.wantsLod = options.includeLod;

    if (plan.assetType == GameAssetType::Building || plan.assetType == GameAssetType::ArchitecturalPart) {
        plan.targetDepth = std::max(plan.targetDepth, 0.70f);
    } else if (plan.assetType == GameAssetType::Vehicle) {
        plan.targetHeight *= 0.70f;
        plan.targetWidth = std::max(plan.targetWidth, 1.65f);
        plan.targetDepth = std::max(plan.targetDepth, 0.75f);
    } else if (plan.assetType == GameAssetType::Character || plan.assetType == GameAssetType::Creature) {
        plan.targetWidth = std::clamp(plan.targetWidth, 0.75f, 1.45f);
        plan.targetDepth = std::max(plan.targetDepth, 0.45f);
    } else if (plan.assetType == GameAssetType::ComplexObject || plan.assetType == GameAssetType::Machine) {
        plan.targetDepth = std::max(plan.targetDepth, 0.70f);
    }

    for (size_t i = 0; i < analysis.palette.size(); ++i) plan.materials.push_back(MakeMaterial(analysis.palette[i], i));
    if (plan.materials.empty()) {
        PlannedMaterialSlot material;
        material.name = "mat_default";
        plan.materials.push_back(material);
    }

    if (options.includeEditFriendlyParts) {
        for (const auto& hint : analysis.partHints) {
            PlannedAssetPart part = PartFromHint(hint, analysis.shape, plan.assetType, plan);
            if (!plan.materials.empty()) part.materialSlot = static_cast<int>(plan.parts.size() % plan.materials.size());
            plan.parts.push_back(part);
        }
    }

    if (plan.parts.empty()) {
        PlannedAssetPart main;
        main.kind = PlannedPartKind::MainVolume;
        main.name = "main_volume";
        main.confidence = 1.0f;
        main.centerX = 0.0f;
        main.centerY = plan.targetHeight * 0.5f;
        main.centerZ = 0.0f;
        main.sizeX = plan.targetWidth;
        main.sizeY = plan.targetHeight;
        main.sizeZ = plan.targetDepth;
        plan.parts.push_back(main);
    }

    if (analysis.contour.size() > 220) plan.warnings.push_back("Contour was heavily sampled; complex fine details will be represented as planned parts or material detail.");
    if (analysis.shape.rectangularity < 0.22f) plan.warnings.push_back("Source silhouette is sparse; generated asset should be treated as proxy geometry.");
    return plan;
}

std::string PlannedAssetPart::ToMarkdownRow() const {
    std::ostringstream o;
    o << "| " << name << " | " << ToString(kind) << " | " << confidence << " | "
      << centerX << ", " << centerY << ", " << centerZ << " | "
      << sizeX << ", " << sizeY << ", " << sizeZ << " | " << materialSlot << " |";
    return o.str();
}

std::string PlannedAssetPart::ToJson() const {
    std::ostringstream o;
    o << "{\"name\": \"" << EscapeJson(name) << "\", "
      << "\"kind\": \"" << ToString(kind) << "\", "
      << "\"confidence\": " << confidence << ", "
      << "\"center\": [" << centerX << ", " << centerY << ", " << centerZ << "], "
      << "\"size\": [" << sizeX << ", " << sizeY << ", " << sizeZ << "], "
      << "\"materialSlot\": " << materialSlot << "}";
    return o.str();
}

std::string PlannedMaterialSlot::ToJson() const {
    std::ostringstream o;
    o << "{\"name\": \"" << EscapeJson(name) << "\", \"rgb\": ["
      << static_cast<int>(r) << ", " << static_cast<int>(g) << ", " << static_cast<int>(b)
      << "], \"coverage\": " << coverage << "}";
    return o.str();
}

std::string AssetPlanResult::ToMarkdown() const {
    std::ostringstream o;
    o << "# Make3D Asset Plan\n\n";
    o << "| Field | Value |\n|---|---:|\n";
    o << "| OK | " << (ok ? "yes" : "no") << " |\n";
    o << "| Asset type | " << ToString(assetType) << " |\n";
    o << "| Asset name | " << assetName << " |\n";
    o << "| Target size | " << targetWidth << ", " << targetHeight << ", " << targetDepth << " |\n";
    o << "| Pivot | " << pivotX << ", " << pivotY << ", " << pivotZ << " |\n";
    o << "| Wants collider | " << (wantsCollider ? "yes" : "no") << " |\n";
    o << "| Wants LOD | " << (wantsLod ? "yes" : "no") << " |\n\n";
    o << "## Parts\n\n| Name | Kind | Confidence | Center | Size | Material |\n|---|---|---:|---|---|---:|\n";
    for (const auto& p : parts) o << p.ToMarkdownRow() << "\n";
    if (!warnings.empty()) {
        o << "\n## Warnings\n\n";
        for (const auto& w : warnings) o << "- " << w << "\n";
    }
    return o.str();
}

std::string AssetPlanResult::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"ok\": " << (ok ? "true" : "false") << ",\n";
    o << "  \"assetType\": \"" << ToString(assetType) << "\",\n";
    o << "  \"assetName\": \"" << EscapeJson(assetName) << "\",\n";
    o << "  \"targetSize\": [" << targetWidth << ", " << targetHeight << ", " << targetDepth << "],\n";
    o << "  \"pivot\": [" << pivotX << ", " << pivotY << ", " << pivotZ << "],\n";
    o << "  \"wantsCollider\": " << (wantsCollider ? "true" : "false") << ",\n";
    o << "  \"wantsLod\": " << (wantsLod ? "true" : "false") << ",\n";
    o << "  \"materials\": [";
    for (size_t i = 0; i < materials.size(); ++i) { if (i) o << ", "; o << materials[i].ToJson(); }
    o << "],\n  \"parts\": [";
    for (size_t i = 0; i < parts.size(); ++i) { if (i) o << ", "; o << parts[i].ToJson(); }
    o << "],\n  \"warnings\": [";
    for (size_t i = 0; i < warnings.size(); ++i) { if (i) o << ", "; o << "\"" << EscapeJson(warnings[i]) << "\""; }
    o << "]\n}";
    return o.str();
}

} // namespace make3d
