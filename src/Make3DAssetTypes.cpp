#include "Make3DAssetTypes.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace make3d {

namespace {

static int Idx(int x, int y, int w) { return y * w + x; }
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

static float Luminance(const ImageRGBA& image, int x, int y) {
    size_t p = (static_cast<size_t>(y) * image.width + x) * 4;
    return (0.2126f * image.pixels[p + 0] + 0.7152f * image.pixels[p + 1] + 0.0722f * image.pixels[p + 2]) / 255.0f;
}

static void AddReason(AssetClassificationResult& r, const std::string& reason) {
    r.reasons.push_back(reason);
}

} // namespace

const char* ToString(GameAssetType type) {
    switch (type) {
        case GameAssetType::Character: return "Character";
        case GameAssetType::Building: return "Building";
        case GameAssetType::Vehicle: return "Vehicle";
        case GameAssetType::Furniture: return "Furniture";
        case GameAssetType::ToolOrWeapon: return "ToolOrWeapon";
        case GameAssetType::Machine: return "Machine";
        case GameAssetType::Creature: return "Creature";
        case GameAssetType::TerrainPiece: return "TerrainPiece";
        case GameAssetType::ArchitecturalPart: return "ArchitecturalPart";
        case GameAssetType::FlatRelief: return "FlatRelief";
        case GameAssetType::ComplexObject: return "ComplexObject";
        case GameAssetType::GenericProp: return "GenericProp";
        default: return "Unknown";
    }
}

AssetClassificationResult InferGameAssetType(
    const ImageRGBA& image,
    const std::vector<std::uint8_t>& mask,
    const DepthImage& depth,
    const AssetTypeClassifierOptions& options) {

    AssetClassificationResult r;
    const int w = image.width;
    const int h = image.height;
    if (w <= 0 || h <= 0 || mask.size() != static_cast<size_t>(w * h)) {
        r.warnings.push_back("Invalid dimensions for asset type classification.");
        return r;
    }

    int minX = w, minY = h, maxX = -1, maxY = -1;
    double sumY = 0.0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!mask[static_cast<size_t>(Idx(x, y, w))]) continue;
            ++r.foregroundPixels;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
            sumY += static_cast<double>(y);
        }
    }

    if (r.foregroundPixels == 0) {
        r.assetType = GameAssetType::Unknown;
        r.warnings.push_back("No foreground pixels for asset type classification.");
        return r;
    }

    const int bw = std::max(1, maxX - minX + 1);
    const int bh = std::max(1, maxY - minY + 1);
    const int bboxArea = std::max(1, bw * bh);
    r.coverage = static_cast<float>(r.foregroundPixels) / static_cast<float>(w * h);
    r.aspectRatio = static_cast<float>(bh) / static_cast<float>(bw);
    r.rectangularity = static_cast<float>(r.foregroundPixels) / static_cast<float>(bboxArea);
    r.massCenterY = h > 1 ? static_cast<float>(sumY / static_cast<double>(r.foregroundPixels) / static_cast<double>(h - 1)) : 0.5f;

    int symXHit = 0, symXTotal = 0;
    int symYHit = 0, symYTotal = 0;
    const int cx = (minX + maxX) / 2;
    const int cy = (minY + maxY) / 2;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const int id = Idx(x, y, w);
            const bool a = mask[static_cast<size_t>(id)] != 0;
            const int mx = cx - (x - cx);
            if (mx >= 0 && mx < w) {
                const bool b = mask[static_cast<size_t>(Idx(mx, y, w))] != 0;
                if (a || b) { ++symXTotal; if (a == b) ++symXHit; }
            }
            const int my = cy - (y - cy);
            if (my >= 0 && my < h) {
                const bool b = mask[static_cast<size_t>(Idx(x, my, w))] != 0;
                if (a || b) { ++symYTotal; if (a == b) ++symYHit; }
            }
        }
    }
    r.symmetryX = symXTotal > 0 ? static_cast<float>(symXHit) / static_cast<float>(symXTotal) : 0.0f;
    r.symmetryY = symYTotal > 0 ? static_cast<float>(symYHit) / static_cast<float>(symYTotal) : 0.0f;

    std::vector<float> rowWidths;
    std::vector<float> colHeights;
    rowWidths.reserve(static_cast<size_t>(bh));
    colHeights.reserve(static_cast<size_t>(bw));
    int strongRows = 0;
    int strongCols = 0;
    for (int y = minY; y <= maxY; ++y) {
        int left = w, right = -1;
        for (int x = minX; x <= maxX; ++x) {
            if (!mask[static_cast<size_t>(Idx(x, y, w))]) continue;
            left = std::min(left, x);
            right = std::max(right, x);
        }
        if (right >= left) {
            float width = static_cast<float>(right - left + 1) / static_cast<float>(bw);
            rowWidths.push_back(width);
            if (width > 0.72f) ++strongRows;
        }
    }
    for (int x = minX; x <= maxX; ++x) {
        int top = h, bottom = -1;
        for (int y = minY; y <= maxY; ++y) {
            if (!mask[static_cast<size_t>(Idx(x, y, w))]) continue;
            top = std::min(top, y);
            bottom = std::max(bottom, y);
        }
        if (bottom >= top) {
            float height = static_cast<float>(bottom - top + 1) / static_cast<float>(bh);
            colHeights.push_back(height);
            if (height > 0.72f) ++strongCols;
        }
    }
    r.rowStability = rowWidths.empty() ? 0.0f : static_cast<float>(strongRows) / static_cast<float>(rowWidths.size());
    r.columnStability = colHeights.empty() ? 0.0f : static_cast<float>(strongCols) / static_cast<float>(colHeights.size());

    int edgeCount = 0;
    int fgSampleCount = 0;
    int straightEdgeHits = 0;
    const int stride = std::max(1, options.edgeSampleStride);
    for (int y = std::max(1, minY); y <= std::min(h - 2, maxY); y += stride) {
        for (int x = std::max(1, minX); x <= std::min(w - 2, maxX); x += stride) {
            const int id = Idx(x, y, w);
            if (!mask[static_cast<size_t>(id)]) continue;
            ++fgSampleCount;
            float gx = Luminance(image, x + 1, y) - Luminance(image, x - 1, y);
            float gy = Luminance(image, x, y + 1) - Luminance(image, x, y - 1);
            float mag = std::sqrt(gx * gx + gy * gy);
            if (mag > 0.12f) ++edgeCount;
            bool boundary = !mask[static_cast<size_t>(Idx(x - 1, y, w))] || !mask[static_cast<size_t>(Idx(x + 1, y, w))] ||
                            !mask[static_cast<size_t>(Idx(x, y - 1, w))] || !mask[static_cast<size_t>(Idx(x, y + 1, w))];
            if (boundary) {
                bool horizontal = mask[static_cast<size_t>(Idx(x - 1, y, w))] == mask[static_cast<size_t>(Idx(x + 1, y, w))];
                bool vertical = mask[static_cast<size_t>(Idx(x, y - 1, w))] == mask[static_cast<size_t>(Idx(x, y + 1, w))];
                if (horizontal || vertical) ++straightEdgeHits;
            }
        }
    }
    r.edgeDensity = fgSampleCount > 0 ? static_cast<float>(edgeCount) / static_cast<float>(fgSampleCount) : 0.0f;
    r.straightEdgeScore = fgSampleCount > 0 ? Clamp01(static_cast<float>(straightEdgeHits) / static_cast<float>(fgSampleCount) * 3.0f) : 0.0f;

    const bool tallSymmetric = r.aspectRatio > 1.20f && r.symmetryX > 0.50f && r.massCenterY > 0.30f && r.massCenterY < 0.72f;
    const bool boxy = r.rectangularity >= options.buildingRectangularityThreshold && r.rowStability > 0.45f && r.columnStability > 0.40f;
    const bool wideLow = r.aspectRatio < 0.62f && r.rectangularity > 0.35f;
    const bool veryFlat = r.coverage < 0.035f || (r.aspectRatio < 0.25f && r.rectangularity < 0.45f);
    const bool complex = r.edgeDensity > options.complexEdgeDensityThreshold && r.rectangularity < 0.78f;

    if (options.preferCharacterForTallSymmetricSilhouettes && tallSymmetric && !boxy) {
        r.assetType = GameAssetType::Character;
        AddReason(r, "Tall symmetric silhouette classified as character.");
    } else if (options.preferBuildings && boxy && r.aspectRatio > 0.65f) {
        r.assetType = GameAssetType::Building;
        AddReason(r, "Boxy high-rectangularity silhouette classified as building.");
    } else if (boxy && r.aspectRatio <= 0.65f) {
        r.assetType = GameAssetType::ArchitecturalPart;
        AddReason(r, "Low boxy silhouette classified as architectural part.");
    } else if (wideLow && r.symmetryX > 0.35f) {
        r.assetType = GameAssetType::Vehicle;
        AddReason(r, "Wide low symmetric silhouette classified as vehicle-like asset.");
    } else if (veryFlat) {
        r.assetType = GameAssetType::FlatRelief;
        AddReason(r, "Very flat or sparse silhouette classified as flat relief.");
    } else if (complex) {
        r.assetType = GameAssetType::ComplexObject;
        AddReason(r, "High internal edge density classified as complex object.");
    } else if (r.symmetryX > 0.50f && r.aspectRatio > 0.80f && r.aspectRatio < 1.35f) {
        r.assetType = GameAssetType::Furniture;
        AddReason(r, "Medium box-like symmetric silhouette classified as furniture-like prop.");
    } else {
        r.assetType = GameAssetType::GenericProp;
        AddReason(r, "Fallback to generic prop reconstruction.");
    }

    if (depth.values.size() == static_cast<size_t>(w * h)) {
        float minD = 1.0f, maxD = 0.0f;
        for (int i = 0; i < w * h; ++i) {
            if (!mask[static_cast<size_t>(i)]) continue;
            minD = std::min(minD, depth.values[static_cast<size_t>(i)]);
            maxD = std::max(maxD, depth.values[static_cast<size_t>(i)]);
        }
        if (maxD - minD < 0.06f) r.warnings.push_back("Depth range is shallow; output may need manual sculpting or extrusion adjustment.");
    }

    return r;
}

std::string AssetClassificationResult::ToMarkdown() const {
    std::ostringstream o;
    o << "# Game Asset Classification Report\n\n";
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| Asset type | " << ToString(assetType) << " |\n";
    o << "| Foreground pixels | " << foregroundPixels << " |\n";
    o << "| Coverage | " << coverage << " |\n";
    o << "| Aspect ratio | " << aspectRatio << " |\n";
    o << "| Rectangularity | " << rectangularity << " |\n";
    o << "| Symmetry X | " << symmetryX << " |\n";
    o << "| Symmetry Y | " << symmetryY << " |\n";
    o << "| Row stability | " << rowStability << " |\n";
    o << "| Column stability | " << columnStability << " |\n";
    o << "| Straight edge score | " << straightEdgeScore << " |\n";
    o << "| Edge density | " << edgeDensity << " |\n";
    o << "| Mass center Y | " << massCenterY << " |\n\n";
    if (!reasons.empty()) {
        o << "## Reasons\n\n";
        for (const auto& reason : reasons) o << "- " << reason << "\n";
        o << "\n";
    }
    if (!warnings.empty()) {
        o << "## Warnings\n\n";
        for (const auto& warning : warnings) o << "- " << warning << "\n";
    }
    return o.str();
}

std::string AssetClassificationResult::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"assetType\": \"" << EscapeJson(ToString(assetType)) << "\",\n";
    o << "  \"foregroundPixels\": " << foregroundPixels << ",\n";
    o << "  \"coverage\": " << coverage << ",\n";
    o << "  \"aspectRatio\": " << aspectRatio << ",\n";
    o << "  \"rectangularity\": " << rectangularity << ",\n";
    o << "  \"symmetryX\": " << symmetryX << ",\n";
    o << "  \"symmetryY\": " << symmetryY << ",\n";
    o << "  \"rowStability\": " << rowStability << ",\n";
    o << "  \"columnStability\": " << columnStability << ",\n";
    o << "  \"straightEdgeScore\": " << straightEdgeScore << ",\n";
    o << "  \"edgeDensity\": " << edgeDensity << ",\n";
    o << "  \"massCenterY\": " << massCenterY << ",\n";
    o << "  \"reasons\": [";
    for (size_t i = 0; i < reasons.size(); ++i) {
        if (i) o << ", ";
        o << "\"" << EscapeJson(reasons[i]) << "\"";
    }
    o << "],\n  \"warnings\": [";
    for (size_t i = 0; i < warnings.size(); ++i) {
        if (i) o << ", ";
        o << "\"" << EscapeJson(warnings[i]) << "\"";
    }
    o << "]\n}";
    return o.str();
}

} // namespace make3d
