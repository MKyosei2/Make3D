#include "Make3DAssetAnalysis.h"

#include <algorithm>
#include <array>
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

static bool IsBoundary(const std::vector<std::uint8_t>& mask, int x, int y, int w, int h) {
    if (!mask[static_cast<size_t>(Idx(x, y, w))]) return false;
    if (x <= 0 || y <= 0 || x >= w - 1 || y >= h - 1) return true;
    return !mask[static_cast<size_t>(Idx(x - 1, y, w))] ||
           !mask[static_cast<size_t>(Idx(x + 1, y, w))] ||
           !mask[static_cast<size_t>(Idx(x, y - 1, w))] ||
           !mask[static_cast<size_t>(Idx(x, y + 1, w))];
}

static ShapeDescriptorReport BuildShape(const ImageRGBA& image, const std::vector<std::uint8_t>& mask) {
    ShapeDescriptorReport r;
    const int w = image.width;
    const int h = image.height;
    r.minX = w; r.minY = h; r.maxX = -1; r.maxY = -1;
    double sx = 0.0, sy = 0.0;
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        if (!mask[static_cast<size_t>(Idx(x, y, w))]) continue;
        ++r.foregroundPixels;
        r.minX = std::min(r.minX, x); r.minY = std::min(r.minY, y);
        r.maxX = std::max(r.maxX, x); r.maxY = std::max(r.maxY, y);
        sx += x; sy += y;
    }
    if (r.foregroundPixels <= 0) return r;
    const int bw = std::max(1, r.maxX - r.minX + 1);
    const int bh = std::max(1, r.maxY - r.minY + 1);
    r.coverage = static_cast<float>(r.foregroundPixels) / static_cast<float>(std::max(1, w * h));
    r.aspectRatio = static_cast<float>(bh) / static_cast<float>(bw);
    r.rectangularity = static_cast<float>(r.foregroundPixels) / static_cast<float>(std::max(1, bw * bh));
    r.massCenterX = w > 1 ? static_cast<float>(sx / r.foregroundPixels / (w - 1)) : 0.5f;
    r.massCenterY = h > 1 ? static_cast<float>(sy / r.foregroundPixels / (h - 1)) : 0.5f;

    int boundary = 0;
    for (int y = r.minY; y <= r.maxY; ++y) for (int x = r.minX; x <= r.maxX; ++x) if (IsBoundary(mask, x, y, w, h)) ++boundary;
    r.perimeterRatio = static_cast<float>(boundary) / static_cast<float>(std::max(1, bw + bh));

    int sxHit = 0, sxTotal = 0, syHit = 0, syTotal = 0;
    int cx = (r.minX + r.maxX) / 2;
    int cy = (r.minY + r.maxY) / 2;
    for (int y = r.minY; y <= r.maxY; ++y) for (int x = r.minX; x <= r.maxX; ++x) {
        bool a = mask[static_cast<size_t>(Idx(x, y, w))] != 0;
        int mx = cx - (x - cx);
        if (mx >= 0 && mx < w) { bool b = mask[static_cast<size_t>(Idx(mx, y, w))] != 0; if (a || b) { ++sxTotal; if (a == b) ++sxHit; } }
        int my = cy - (y - cy);
        if (my >= 0 && my < h) { bool b = mask[static_cast<size_t>(Idx(x, my, w))] != 0; if (a || b) { ++syTotal; if (a == b) ++syHit; } }
    }
    r.symmetryX = sxTotal ? static_cast<float>(sxHit) / static_cast<float>(sxTotal) : 0.0f;
    r.symmetryY = syTotal ? static_cast<float>(syHit) / static_cast<float>(syTotal) : 0.0f;

    int stableRows = 0, rows = 0, stableCols = 0, cols = 0;
    for (int y = r.minY; y <= r.maxY; ++y) {
        int l = w, rr = -1;
        for (int x = r.minX; x <= r.maxX; ++x) if (mask[static_cast<size_t>(Idx(x, y, w))]) { l = std::min(l, x); rr = std::max(rr, x); }
        if (rr >= l) { ++rows; if (static_cast<float>(rr - l + 1) / bw > 0.72f) ++stableRows; }
    }
    for (int x = r.minX; x <= r.maxX; ++x) {
        int t = h, b = -1;
        for (int y = r.minY; y <= r.maxY; ++y) if (mask[static_cast<size_t>(Idx(x, y, w))]) { t = std::min(t, y); b = std::max(b, y); }
        if (b >= t) { ++cols; if (static_cast<float>(b - t + 1) / bh > 0.72f) ++stableCols; }
    }
    r.rowStability = rows ? static_cast<float>(stableRows) / rows : 0.0f;
    r.columnStability = cols ? static_cast<float>(stableCols) / cols : 0.0f;

    int boundarySamples = 0, straightHits = 0, edgeHits = 0, fgSamples = 0;
    auto lum = [&](int x, int y) {
        size_t p = (static_cast<size_t>(y) * w + x) * 4;
        return (0.2126f * image.pixels[p] + 0.7152f * image.pixels[p + 1] + 0.0722f * image.pixels[p + 2]) / 255.0f;
    };
    for (int y = std::max(1, r.minY); y <= std::min(h - 2, r.maxY); ++y) for (int x = std::max(1, r.minX); x <= std::min(w - 2, r.maxX); ++x) {
        if (!mask[static_cast<size_t>(Idx(x, y, w))]) continue;
        ++fgSamples;
        float gx = lum(x + 1, y) - lum(x - 1, y);
        float gy = lum(x, y + 1) - lum(x, y - 1);
        if (std::sqrt(gx * gx + gy * gy) > 0.12f) ++edgeHits;
        if (IsBoundary(mask, x, y, w, h)) {
            ++boundarySamples;
            bool horizontal = mask[static_cast<size_t>(Idx(x - 1, y, w))] == mask[static_cast<size_t>(Idx(x + 1, y, w))];
            bool vertical = mask[static_cast<size_t>(Idx(x, y - 1, w))] == mask[static_cast<size_t>(Idx(x, y + 1, w))];
            if (horizontal || vertical) ++straightHits;
        }
    }
    r.edgeDensity = fgSamples ? static_cast<float>(edgeHits) / fgSamples : 0.0f;
    r.straightEdgeScore = boundarySamples ? Clamp01(static_cast<float>(straightHits) / boundarySamples) : 0.0f;
    return r;
}

static std::vector<ContourPoint2D> ExtractContour(const std::vector<std::uint8_t>& mask, int w, int h, int maxPoints) {
    std::vector<ContourPoint2D> all;
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        if (!IsBoundary(mask, x, y, w, h)) continue;
        all.push_back({x, y, w > 1 ? static_cast<float>(x) / (w - 1) : 0.0f, h > 1 ? static_cast<float>(y) / (h - 1) : 0.0f});
    }
    if (maxPoints > 0 && static_cast<int>(all.size()) > maxPoints) {
        std::vector<ContourPoint2D> sampled;
        for (int i = 0; i < maxPoints; ++i) sampled.push_back(all[static_cast<size_t>(i) * all.size() / static_cast<size_t>(maxPoints)]);
        return sampled;
    }
    return all;
}

static std::vector<PaletteColor> ExtractPalette(const ImageRGBA& image, const std::vector<std::uint8_t>& mask, const AssetAnalysisOptions& options, int foregroundPixels) {
    const int bins = std::max(2, options.paletteBinsPerChannel);
    struct Bin { int count = 0; int r = 0; int g = 0; int b = 0; };
    std::vector<Bin> hist(static_cast<size_t>(bins * bins * bins));
    for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
        int id = Idx(x, y, image.width);
        if (!mask[static_cast<size_t>(id)]) continue;
        size_t p = static_cast<size_t>(id) * 4;
        int br = std::min(bins - 1, image.pixels[p] * bins / 256);
        int bg = std::min(bins - 1, image.pixels[p + 1] * bins / 256);
        int bb = std::min(bins - 1, image.pixels[p + 2] * bins / 256);
        Bin& bin = hist[static_cast<size_t>((br * bins + bg) * bins + bb)];
        ++bin.count; bin.r += image.pixels[p]; bin.g += image.pixels[p + 1]; bin.b += image.pixels[p + 2];
    }
    std::vector<PaletteColor> out;
    std::vector<int> order(hist.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return hist[static_cast<size_t>(a)].count > hist[static_cast<size_t>(b)].count; });
    for (int idx : order) {
        const Bin& bin = hist[static_cast<size_t>(idx)];
        if (bin.count <= 0) continue;
        PaletteColor c;
        c.r = static_cast<std::uint8_t>(bin.r / bin.count);
        c.g = static_cast<std::uint8_t>(bin.g / bin.count);
        c.b = static_cast<std::uint8_t>(bin.b / bin.count);
        c.pixels = bin.count;
        c.coverage = foregroundPixels ? static_cast<float>(bin.count) / foregroundPixels : 0.0f;
        out.push_back(c);
        if (static_cast<int>(out.size()) >= std::max(1, options.maxPaletteColors)) break;
    }
    return out;
}

static void AddHint(std::vector<PartHint2D>& hints, const char* name, const char* role, float confidence, int minX, int minY, int maxX, int maxY) {
    PartHint2D h;
    h.name = name; h.role = role; h.confidence = Clamp01(confidence); h.minX = minX; h.minY = minY; h.maxX = maxX; h.maxY = maxY;
    hints.push_back(h);
}

static std::vector<PartHint2D> GenerateHints(GameAssetType type, const ShapeDescriptorReport& s) {
    std::vector<PartHint2D> hints;
    if (s.foregroundPixels <= 0) return hints;
    int w = std::max(1, s.maxX - s.minX + 1);
    int h = std::max(1, s.maxY - s.minY + 1);
    auto X = [&](float t) { return s.minX + static_cast<int>(w * t); };
    auto Y = [&](float t) { return s.minY + static_cast<int>(h * t); };
    AddHint(hints, "main_volume", "main", 0.95f, s.minX, s.minY, s.maxX, s.maxY);
    if (type == GameAssetType::Character || type == GameAssetType::Creature) {
        AddHint(hints, "head", "head", 0.78f * s.symmetryX, X(0.36f), Y(0.00f), X(0.64f), Y(0.22f));
        AddHint(hints, "torso", "torso", 0.76f, X(0.28f), Y(0.22f), X(0.72f), Y(0.58f));
        AddHint(hints, "left_arm", "arm", 0.58f, X(0.02f), Y(0.25f), X(0.32f), Y(0.66f));
        AddHint(hints, "right_arm", "arm", 0.58f, X(0.68f), Y(0.25f), X(0.98f), Y(0.66f));
        AddHint(hints, "left_leg", "leg", 0.66f, X(0.30f), Y(0.58f), X(0.50f), Y(1.00f));
        AddHint(hints, "right_leg", "leg", 0.66f, X(0.50f), Y(0.58f), X(0.70f), Y(1.00f));
    } else if (type == GameAssetType::Building || type == GameAssetType::ArchitecturalPart) {
        AddHint(hints, "facade", "wall", 0.90f, X(0.05f), Y(0.15f), X(0.95f), Y(1.00f));
        AddHint(hints, "roof", "roof", 0.72f, X(0.00f), Y(0.00f), X(1.00f), Y(0.18f));
        AddHint(hints, "window_grid", "windows", 0.70f * s.straightEdgeScore + 0.20f, X(0.18f), Y(0.26f), X(0.82f), Y(0.78f));
        AddHint(hints, "door", "door", 0.70f, X(0.42f), Y(0.76f), X(0.58f), Y(1.00f));
    } else if (type == GameAssetType::Vehicle) {
        AddHint(hints, "body", "body", 0.86f, X(0.05f), Y(0.32f), X(0.95f), Y(0.82f));
        AddHint(hints, "cabin", "cabin", 0.58f, X(0.35f), Y(0.12f), X(0.72f), Y(0.45f));
        AddHint(hints, "left_wheel", "wheel", 0.68f, X(0.18f), Y(0.74f), X(0.34f), Y(1.00f));
        AddHint(hints, "right_wheel", "wheel", 0.68f, X(0.66f), Y(0.74f), X(0.82f), Y(1.00f));
    } else if (type == GameAssetType::ComplexObject || type == GameAssetType::Machine) {
        AddHint(hints, "core", "core", 0.82f, X(0.16f), Y(0.18f), X(0.84f), Y(0.86f));
        AddHint(hints, "left_protrusion", "protrusion", 0.50f + s.contourComplexity * 0.30f, X(0.00f), Y(0.30f), X(0.25f), Y(0.70f));
        AddHint(hints, "right_protrusion", "protrusion", 0.50f + s.contourComplexity * 0.30f, X(0.75f), Y(0.30f), X(1.00f), Y(0.70f));
        AddHint(hints, "top_detail", "detail", 0.50f, X(0.30f), Y(0.00f), X(0.70f), Y(0.22f));
    } else {
        AddHint(hints, "front_panel", "panel", 0.55f, X(0.12f), Y(0.18f), X(0.88f), Y(0.86f));
        AddHint(hints, "detail_band", "detail", 0.45f, X(0.08f), Y(0.44f), X(0.92f), Y(0.56f));
    }
    return hints;
}

} // namespace

AssetAnalysisResult AnalyzeAsset(const ImageRGBA& image, const std::vector<std::uint8_t>& mask, const DepthImage& depth, const AssetAnalysisOptions& options) {
    AssetAnalysisResult r;
    if (image.width <= 0 || image.height <= 0 || mask.size() != static_cast<size_t>(image.width * image.height)) {
        r.warnings.push_back("Invalid image or mask for asset analysis.");
        return r;
    }
    r.shape = BuildShape(image, mask);
    if (r.shape.foregroundPixels <= 0) {
        r.warnings.push_back("No foreground pixels for asset analysis.");
        return r;
    }
    r.contour = ExtractContour(mask, image.width, image.height, options.maxContourPoints);
    int bw = std::max(1, r.shape.maxX - r.shape.minX + 1);
    int bh = std::max(1, r.shape.maxY - r.shape.minY + 1);
    r.shape.contourComplexity = Clamp01(static_cast<float>(r.contour.size()) / static_cast<float>(std::max(1, bw + bh)));
    r.palette = ExtractPalette(image, mask, options, r.shape.foregroundPixels);
    AssetTypeClassifierOptions classifierOptions;
    r.classification = InferGameAssetType(image, mask, depth, classifierOptions);
    r.classification.edgeDensity = std::max(r.classification.edgeDensity, r.shape.edgeDensity);
    r.classification.straightEdgeScore = std::max(r.classification.straightEdgeScore, r.shape.straightEdgeScore);
    if (r.shape.contourComplexity > 0.72f && r.classification.assetType == GameAssetType::GenericProp) {
        r.classification.assetType = GameAssetType::ComplexObject;
        r.classification.reasons.push_back("Phase 1 contour complexity upgraded generic prop to complex object.");
    }
    if (options.enablePartHints) r.partHints = GenerateHints(r.classification.assetType, r.shape);
    r.ok = true;
    return r;
}

std::string ShapeDescriptorReport::ToMarkdown() const {
    std::ostringstream o;
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| Foreground pixels | " << foregroundPixels << " |\n";
    o << "| Bounds | " << minX << ", " << minY << " - " << maxX << ", " << maxY << " |\n";
    o << "| Coverage | " << coverage << " |\n";
    o << "| Aspect ratio | " << aspectRatio << " |\n";
    o << "| Rectangularity | " << rectangularity << " |\n";
    o << "| Perimeter ratio | " << perimeterRatio << " |\n";
    o << "| Symmetry X | " << symmetryX << " |\n";
    o << "| Symmetry Y | " << symmetryY << " |\n";
    o << "| Row stability | " << rowStability << " |\n";
    o << "| Column stability | " << columnStability << " |\n";
    o << "| Straight edge score | " << straightEdgeScore << " |\n";
    o << "| Edge density | " << edgeDensity << " |\n";
    o << "| Mass center | " << massCenterX << ", " << massCenterY << " |\n";
    o << "| Contour complexity | " << contourComplexity << " |\n";
    return o.str();
}

std::string ShapeDescriptorReport::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"foregroundPixels\": " << foregroundPixels << ",\n";
    o << "  \"bounds\": [" << minX << ", " << minY << ", " << maxX << ", " << maxY << "],\n";
    o << "  \"coverage\": " << coverage << ",\n";
    o << "  \"aspectRatio\": " << aspectRatio << ",\n";
    o << "  \"rectangularity\": " << rectangularity << ",\n";
    o << "  \"perimeterRatio\": " << perimeterRatio << ",\n";
    o << "  \"symmetryX\": " << symmetryX << ",\n";
    o << "  \"symmetryY\": " << symmetryY << ",\n";
    o << "  \"rowStability\": " << rowStability << ",\n";
    o << "  \"columnStability\": " << columnStability << ",\n";
    o << "  \"straightEdgeScore\": " << straightEdgeScore << ",\n";
    o << "  \"edgeDensity\": " << edgeDensity << ",\n";
    o << "  \"massCenter\": [" << massCenterX << ", " << massCenterY << "],\n";
    o << "  \"contourComplexity\": " << contourComplexity << "\n}";
    return o.str();
}

std::string AssetAnalysisResult::ToMarkdown() const {
    std::ostringstream o;
    o << "# Make3D Asset Analysis Report\n\n";
    o << "## Classification\n\n" << classification.ToMarkdown() << "\n";
    o << "## Shape descriptors\n\n" << shape.ToMarkdown() << "\n";
    o << "## Contour\n\n";
    o << "| Metric | Value |\n|---|---:|\n| Sampled contour points | " << contour.size() << " |\n\n";
    o << "## Palette\n\n| Index | RGB | Coverage |\n|---:|---|---:|\n";
    for (size_t i = 0; i < palette.size(); ++i) o << "| " << i << " | " << static_cast<int>(palette[i].r) << ", " << static_cast<int>(palette[i].g) << ", " << static_cast<int>(palette[i].b) << " | " << palette[i].coverage << " |\n";
    o << "\n## Part hints\n\n| Name | Role | Confidence | Bounds |\n|---|---|---:|---|\n";
    for (const auto& h : partHints) o << "| " << h.name << " | " << h.role << " | " << h.confidence << " | " << h.minX << "," << h.minY << " - " << h.maxX << "," << h.maxY << " |\n";
    return o.str();
}

std::string AssetAnalysisResult::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"ok\": " << (ok ? "true" : "false") << ",\n";
    o << "  \"classification\": " << classification.ToJson() << ",\n";
    o << "  \"shape\": " << shape.ToJson() << ",\n";
    o << "  \"contourPoints\": " << contour.size() << ",\n";
    o << "  \"palette\": [";
    for (size_t i = 0; i < palette.size(); ++i) { if (i) o << ", "; o << "{\"rgb\": [" << static_cast<int>(palette[i].r) << ", " << static_cast<int>(palette[i].g) << ", " << static_cast<int>(palette[i].b) << "], \"coverage\": " << palette[i].coverage << "}"; }
    o << "],\n  \"partHints\": [";
    for (size_t i = 0; i < partHints.size(); ++i) {
        const auto& h = partHints[i];
        if (i) o << ", ";
        o << "{\"name\": \"" << EscapeJson(h.name) << "\", \"role\": \"" << EscapeJson(h.role) << "\", \"confidence\": " << h.confidence << ", \"bounds\": [" << h.minX << ", " << h.minY << ", " << h.maxX << ", " << h.maxY << "]}";
    }
    o << "]\n}";
    return o.str();
}

} // namespace make3d
