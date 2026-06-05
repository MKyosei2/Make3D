#include "Make3DGameAssetGenerator.h"
#include "Make3DGltfMaterialExporter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>

namespace make3d {
namespace {

constexpr float Pi = 3.14159265358979323846f;
struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };

static int VertexCount(const MeshData& mesh) { return static_cast<int>(mesh.positions.size() / 3); }
static int TriangleCount(const MeshData& mesh) { return static_cast<int>(mesh.indices.size() / 3); }
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
    const size_t p = (static_cast<size_t>(y) * image.width + x) * 4;
    return (0.2126f * image.pixels[p + 0] + 0.7152f * image.pixels[p + 1] + 0.0722f * image.pixels[p + 2]) / 255.0f;
}

static bool IsBoundary(const std::vector<std::uint8_t>& mask, int x, int y, int w, int h) {
    if (!mask[static_cast<size_t>(Idx(x, y, w))]) return false;
    if (x <= 0 || y <= 0 || x >= w - 1 || y >= h - 1) return true;
    return !mask[static_cast<size_t>(Idx(x - 1, y, w))] ||
           !mask[static_cast<size_t>(Idx(x + 1, y, w))] ||
           !mask[static_cast<size_t>(Idx(x, y - 1, w))] ||
           !mask[static_cast<size_t>(Idx(x, y + 1, w))];
}

static void AddVertex(MeshData& mesh, float x, float y, float z, float u, float v) {
    mesh.positions.push_back(x); mesh.positions.push_back(y); mesh.positions.push_back(z);
    mesh.normals.push_back(0.0f); mesh.normals.push_back(1.0f); mesh.normals.push_back(0.0f);
    mesh.uvs.push_back(u); mesh.uvs.push_back(v);
}

static void AddTri(MeshData& mesh, int a, int b, int c) {
    if (a < 0 || b < 0 || c < 0 || a == b || b == c || c == a) return;
    mesh.indices.push_back(static_cast<std::uint32_t>(a));
    mesh.indices.push_back(static_cast<std::uint32_t>(b));
    mesh.indices.push_back(static_cast<std::uint32_t>(c));
}

static void AddQuad(MeshData& mesh, int a, int b, int c, int d) {
    AddTri(mesh, a, b, c);
    AddTri(mesh, a, c, d);
}

static void AddBox(MeshData& mesh, float cx, float cy, float cz, float sx, float sy, float sz) {
    const int base = VertexCount(mesh);
    const float x0 = cx - sx * 0.5f, x1 = cx + sx * 0.5f;
    const float y0 = cy - sy * 0.5f, y1 = cy + sy * 0.5f;
    const float z0 = cz - sz * 0.5f, z1 = cz + sz * 0.5f;
    AddVertex(mesh, x0, y0, z0, 0, 0); AddVertex(mesh, x1, y0, z0, 1, 0); AddVertex(mesh, x1, y1, z0, 1, 1); AddVertex(mesh, x0, y1, z0, 0, 1);
    AddVertex(mesh, x0, y0, z1, 0, 0); AddVertex(mesh, x1, y0, z1, 1, 0); AddVertex(mesh, x1, y1, z1, 1, 1); AddVertex(mesh, x0, y1, z1, 0, 1);
    AddQuad(mesh, base + 0, base + 1, base + 2, base + 3);
    AddQuad(mesh, base + 5, base + 4, base + 7, base + 6);
    AddQuad(mesh, base + 4, base + 0, base + 3, base + 7);
    AddQuad(mesh, base + 1, base + 5, base + 6, base + 2);
    AddQuad(mesh, base + 3, base + 2, base + 6, base + 7);
    AddQuad(mesh, base + 4, base + 5, base + 1, base + 0);
}

static void AddCylinder(MeshData& mesh, float cx, float cy0, float cy1, float cz, float rx, float rz, int segments) {
    segments = std::max(8, segments);
    const int base = VertexCount(mesh);
    for (int y = 0; y < 2; ++y) {
        const float cy = y == 0 ? cy0 : cy1;
        for (int s = 0; s < segments; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(segments);
            const float a = t * 2.0f * Pi;
            AddVertex(mesh, cx + std::cos(a) * rx, cy, cz + std::sin(a) * rz, t, static_cast<float>(y));
        }
    }
    for (int s = 0; s < segments; ++s) {
        const int n = (s + 1) % segments;
        AddQuad(mesh, base + s, base + n, base + segments + n, base + segments + s);
    }
    const int bottom = VertexCount(mesh); AddVertex(mesh, cx, cy0, cz, 0.5f, 0.5f);
    const int top = VertexCount(mesh); AddVertex(mesh, cx, cy1, cz, 0.5f, 0.5f);
    for (int s = 0; s < segments; ++s) {
        const int n = (s + 1) % segments;
        AddTri(mesh, bottom, base + n, base + s);
        AddTri(mesh, top, base + segments + s, base + segments + n);
    }
}

static void AppendMesh(MeshData& dst, const MeshData& src) {
    const std::uint32_t base = static_cast<std::uint32_t>(VertexCount(dst));
    dst.positions.insert(dst.positions.end(), src.positions.begin(), src.positions.end());
    dst.normals.insert(dst.normals.end(), src.normals.begin(), src.normals.end());
    dst.uvs.insert(dst.uvs.end(), src.uvs.begin(), src.uvs.end());
    for (std::uint32_t i : src.indices) dst.indices.push_back(base + i);
}

static ShapeDescriptorReport BuildShapeDescriptors(const ImageRGBA& image, const std::vector<std::uint8_t>& mask) {
    ShapeDescriptorReport r;
    const int w = image.width, h = image.height;
    r.minX = w; r.minY = h; r.maxX = -1; r.maxY = -1;
    double sumX = 0.0, sumY = 0.0;
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        if (!mask[static_cast<size_t>(Idx(x, y, w))]) continue;
        ++r.foregroundPixels;
        r.minX = std::min(r.minX, x); r.minY = std::min(r.minY, y);
        r.maxX = std::max(r.maxX, x); r.maxY = std::max(r.maxY, y);
        sumX += x; sumY += y;
    }
    if (r.foregroundPixels <= 0) return r;
    const int bw = std::max(1, r.maxX - r.minX + 1);
    const int bh = std::max(1, r.maxY - r.minY + 1);
    r.coverage = static_cast<float>(r.foregroundPixels) / static_cast<float>(std::max(1, w * h));
    r.aspectRatio = static_cast<float>(bh) / static_cast<float>(bw);
    r.rectangularity = static_cast<float>(r.foregroundPixels) / static_cast<float>(std::max(1, bw * bh));
    r.massCenterX = w > 1 ? static_cast<float>(sumX / r.foregroundPixels / (w - 1)) : 0.5f;
    r.massCenterY = h > 1 ? static_cast<float>(sumY / r.foregroundPixels / (h - 1)) : 0.5f;
    int boundary = 0;
    for (int y = r.minY; y <= r.maxY; ++y) for (int x = r.minX; x <= r.maxX; ++x) if (IsBoundary(mask, x, y, w, h)) ++boundary;
    r.perimeterRatio = static_cast<float>(boundary) / static_cast<float>(std::max(1, bw + bh));
    r.contourComplexity = Clamp01(static_cast<float>(boundary) / static_cast<float>(std::max(1, bw + bh) * 2));

    const int cx = (r.minX + r.maxX) / 2, cy = (r.minY + r.maxY) / 2;
    int sxHit = 0, sxTotal = 0, syHit = 0, syTotal = 0;
    for (int y = r.minY; y <= r.maxY; ++y) for (int x = r.minX; x <= r.maxX; ++x) {
        const bool a = mask[static_cast<size_t>(Idx(x, y, w))] != 0;
        const int mx = cx - (x - cx);
        if (mx >= 0 && mx < w) { const bool b = mask[static_cast<size_t>(Idx(mx, y, w))] != 0; if (a || b) { ++sxTotal; if (a == b) ++sxHit; } }
        const int my = cy - (y - cy);
        if (my >= 0 && my < h) { const bool b = mask[static_cast<size_t>(Idx(x, my, w))] != 0; if (a || b) { ++syTotal; if (a == b) ++syHit; } }
    }
    r.symmetryX = sxTotal ? static_cast<float>(sxHit) / sxTotal : 0.0f;
    r.symmetryY = syTotal ? static_cast<float>(syHit) / syTotal : 0.0f;

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

    int fgSamples = 0, edgeHits = 0, boundarySamples = 0, straightHits = 0;
    for (int y = std::max(1, r.minY); y <= std::min(h - 2, r.maxY); ++y) for (int x = std::max(1, r.minX); x <= std::min(w - 2, r.maxX); ++x) {
        if (!mask[static_cast<size_t>(Idx(x, y, w))]) continue;
        ++fgSamples;
        const float gx = Luminance(image, x + 1, y) - Luminance(image, x - 1, y);
        const float gy = Luminance(image, x, y + 1) - Luminance(image, x, y - 1);
        if (std::sqrt(gx * gx + gy * gy) > 0.12f) ++edgeHits;
        if (IsBoundary(mask, x, y, w, h)) {
            ++boundarySamples;
            const bool horizontal = mask[static_cast<size_t>(Idx(x - 1, y, w))] == mask[static_cast<size_t>(Idx(x + 1, y, w))];
            const bool vertical = mask[static_cast<size_t>(Idx(x, y - 1, w))] == mask[static_cast<size_t>(Idx(x, y + 1, w))];
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
        if (IsBoundary(mask, x, y, w, h)) all.push_back({x, y, w > 1 ? static_cast<float>(x) / (w - 1) : 0.0f, h > 1 ? static_cast<float>(y) / (h - 1) : 0.0f});
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
    struct Bin { int count = 0, r = 0, g = 0, b = 0; };
    std::vector<Bin> hist(static_cast<size_t>(bins * bins * bins));
    for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
        const int id = Idx(x, y, image.width);
        if (!mask[static_cast<size_t>(id)]) continue;
        const size_t p = static_cast<size_t>(id) * 4;
        const int br = std::min(bins - 1, image.pixels[p + 0] * bins / 256);
        const int bg = std::min(bins - 1, image.pixels[p + 1] * bins / 256);
        const int bb = std::min(bins - 1, image.pixels[p + 2] * bins / 256);
        Bin& bin = hist[static_cast<size_t>((br * bins + bg) * bins + bb)];
        ++bin.count; bin.r += image.pixels[p + 0]; bin.g += image.pixels[p + 1]; bin.b += image.pixels[p + 2];
    }
    std::vector<int> order(hist.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return hist[static_cast<size_t>(a)].count > hist[static_cast<size_t>(b)].count; });
    std::vector<PaletteColor> out;
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

static void AddHint(std::vector<PartHint2D>& hints, const char* name, const char* role, float conf, int minX, int minY, int maxX, int maxY) {
    hints.push_back({name, minX, minY, maxX, maxY, Clamp01(conf), role});
}

static std::vector<PartHint2D> GeneratePartHints(GameAssetType type, const ShapeDescriptorReport& s) {
    std::vector<PartHint2D> hints;
    if (s.foregroundPixels <= 0) return hints;
    const int w = std::max(1, s.maxX - s.minX + 1), h = std::max(1, s.maxY - s.minY + 1);
    auto X = [&](float t) { return s.minX + static_cast<int>(w * t); };
    auto Y = [&](float t) { return s.minY + static_cast<int>(h * t); };
    AddHint(hints, "main_volume", "main", 0.95f, s.minX, s.minY, s.maxX, s.maxY);
    if (type == GameAssetType::Character || type == GameAssetType::Creature) {
        AddHint(hints, "head", "head", 0.80f * s.symmetryX, X(0.35f), Y(0.00f), X(0.65f), Y(0.22f));
        AddHint(hints, "torso", "torso", 0.78f, X(0.28f), Y(0.22f), X(0.72f), Y(0.58f));
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
    } else {
        AddHint(hints, "front_panel", "panel", 0.55f, X(0.12f), Y(0.18f), X(0.88f), Y(0.86f));
        AddHint(hints, "detail_band", "detail", 0.45f, X(0.08f), Y(0.44f), X(0.92f), Y(0.56f));
    }
    return hints;
}

static PlannedPartKind KindFromHint(const PartHint2D& h, GameAssetType type) {
    const std::string& r = h.role;
    if (type == GameAssetType::Character || type == GameAssetType::Creature) {
        if (r == "head") return PlannedPartKind::CharacterHead;
        if (r == "torso") return PlannedPartKind::CharacterTorso;
        if (r == "arm") return PlannedPartKind::CharacterArm;
        if (r == "leg") return PlannedPartKind::CharacterLeg;
    }
    if (type == GameAssetType::Building || type == GameAssetType::ArchitecturalPart) {
        if (r == "wall") return PlannedPartKind::BuildingFacade;
        if (r == "roof") return PlannedPartKind::BuildingRoof;
        if (r == "door") return PlannedPartKind::BuildingDoor;
        if (r == "windows") return PlannedPartKind::BuildingWindowGrid;
    }
    if (type == GameAssetType::Vehicle) {
        if (r == "body") return PlannedPartKind::VehicleBody;
        if (r == "cabin") return PlannedPartKind::VehicleCabin;
        if (r == "wheel") return PlannedPartKind::VehicleWheel;
    }
    if (type == GameAssetType::ComplexObject || type == GameAssetType::Machine) {
        if (r == "core") return PlannedPartKind::ComplexCore;
        if (r == "protrusion") return PlannedPartKind::ComplexProtrusion;
    }
    if (r == "panel") return PlannedPartKind::DetailPanel;
    if (r == "detail") return PlannedPartKind::DetailBand;
    return PlannedPartKind::MainVolume;
}

static PlannedAssetPart PartFromHint(const PartHint2D& h, const ShapeDescriptorReport& s, GameAssetType type, const AssetPlanResult& plan) {
    PlannedAssetPart p;
    p.kind = KindFromHint(h, type);
    p.name = h.name.empty() ? std::string(ToString(p.kind)) : h.name;
    p.confidence = Clamp01(h.confidence);
    const float bw = static_cast<float>(std::max(1, s.maxX - s.minX + 1));
    const float bh = static_cast<float>(std::max(1, s.maxY - s.minY + 1));
    const float nx0 = (static_cast<float>(h.minX - s.minX) / bw) - 0.5f;
    const float nx1 = (static_cast<float>(h.maxX - s.minX) / bw) - 0.5f;
    const float ny0 = static_cast<float>(h.minY - s.minY) / bh;
    const float ny1 = static_cast<float>(h.maxY - s.minY) / bh;
    p.centerX = (nx0 + nx1) * 0.5f * plan.targetWidth;
    p.centerY = plan.targetHeight * (1.0f - (ny0 + ny1) * 0.5f);
    p.centerZ = 0.0f;
    p.sizeX = std::max(0.04f, std::abs(nx1 - nx0) * plan.targetWidth);
    p.sizeY = std::max(0.04f, std::abs(ny1 - ny0) * plan.targetHeight);
    p.sizeZ = std::max(0.04f, plan.targetDepth * 0.70f);
    if (p.kind == PlannedPartKind::BuildingRoof) p.sizeZ = plan.targetDepth * 1.10f;
    if (p.kind == PlannedPartKind::VehicleWheel) { p.sizeY = std::max(p.sizeY, plan.targetHeight * 0.18f); p.sizeZ = p.sizeY; }
    if (p.kind == PlannedPartKind::CharacterHead) p.sizeZ = std::max(p.sizeZ, p.sizeX * 0.75f);
    if (p.kind == PlannedPartKind::ComplexProtrusion) p.sizeZ = plan.targetDepth * 0.45f;
    return p;
}

static MeshData BuildReliefFallbackAsset(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const GameAssetGeneratorOptions& options) {
    AdvancedOptions adv;
    adv.mode = ReconstructionMode::HybridVolume;
    adv.quality = QualityPreset::Detailed;
    adv.maxGridResolution = std::max(24, options.gridResolution);
    adv.volumeRadialSegments = std::max(8, options.radialSegments);
    adv.depthScale = options.extrusionDepth;
    MeshData mesh = ReconstructMesh(color, depth, mask, adv, nullptr);
    if (options.addPropDetailBands) {
        MeshData bands;
        AddBox(bands, 0.0f, 0.45f, options.extrusionDepth * 0.55f, 1.05f, 0.035f, 0.035f);
        AddBox(bands, 0.0f, 0.95f, options.extrusionDepth * 0.55f, 0.86f, 0.030f, 0.035f);
        AppendMesh(mesh, bands);
    }
    RecomputeNormals(mesh);
    NormalizeMesh(mesh, options.targetHeight);
    return mesh;
}

static MeshData BuildVehicleLikeAsset(const GameAssetGeneratorOptions& options) {
    MeshData mesh;
    AddBox(mesh, 0.0f, 0.45f, 0.0f, 1.75f, 0.52f, 0.72f);
    AddBox(mesh, 0.10f, 0.86f, 0.0f, 0.85f, 0.38f, 0.62f);
    AddCylinder(mesh, -0.62f, 0.12f, 0.24f, 0.38f, 0.16f, 0.16f, options.radialSegments);
    AddCylinder(mesh,  0.62f, 0.12f, 0.24f, 0.38f, 0.16f, 0.16f, options.radialSegments);
    AddCylinder(mesh, -0.62f, 0.12f, 0.24f, -0.38f, 0.16f, 0.16f, options.radialSegments);
    AddCylinder(mesh,  0.62f, 0.12f, 0.24f, -0.38f, 0.16f, 0.16f, options.radialSegments);
    RecomputeNormals(mesh);
    NormalizeMesh(mesh, options.targetHeight * 0.65f);
    return mesh;
}

static MeshData BuildSafeProxyAsset(GameAssetType type, const GameAssetGeneratorOptions& options) {
    MeshData mesh;
    if (type == GameAssetType::Building || type == GameAssetType::ArchitecturalPart) AddBox(mesh, 0.0f, options.targetHeight * 0.5f, 0.0f, 1.0f, options.targetHeight, std::max(0.35f, options.buildingDepth));
    else if (type == GameAssetType::Vehicle) mesh = BuildVehicleLikeAsset(options);
    else if (type == GameAssetType::Character || type == GameAssetType::Creature) {
        AddCylinder(mesh, 0.0f, 0.0f, options.targetHeight * 0.72f, 0.0f, 0.28f, 0.18f, std::max(12, options.radialSegments));
        AddCylinder(mesh, 0.0f, options.targetHeight * 0.74f, options.targetHeight, 0.0f, 0.20f, 0.20f, std::max(12, options.radialSegments));
    } else AddBox(mesh, 0.0f, options.targetHeight * 0.5f, 0.0f, 0.85f, options.targetHeight, 0.55f);
    RecomputeNormals(mesh);
    NormalizeMesh(mesh, options.targetHeight);
    return mesh;
}

static void AddWindowGrid(MeshData& mesh, const PlannedAssetPart& p) {
    const int cols = std::max(2, static_cast<int>(p.sizeX * 4.0f));
    const int rows = std::max(2, static_cast<int>(p.sizeY * 3.0f));
    const float cellW = p.sizeX / static_cast<float>(cols);
    const float cellH = p.sizeY / static_cast<float>(rows);
    for (int r = 0; r < rows; ++r) for (int c = 0; c < cols; ++c) {
        const float x = p.centerX - p.sizeX * 0.5f + cellW * (static_cast<float>(c) + 0.5f);
        const float y = p.centerY - p.sizeY * 0.5f + cellH * (static_cast<float>(r) + 0.5f);
        AddBox(mesh, x, y, p.sizeZ * 0.58f, cellW * 0.45f, cellH * 0.42f, p.sizeZ * 0.06f);
    }
}

static MeshData BuildPlannedAssetMesh(const AssetPlanResult& plan, const GameAssetGeneratorOptions& options) {
    MeshData mesh;
    for (const PlannedAssetPart& p : plan.parts) {
        const float sx = std::max(0.03f, p.sizeX), sy = std::max(0.03f, p.sizeY), sz = std::max(0.03f, p.sizeZ);
        switch (p.kind) {
            case PlannedPartKind::CharacterHead: AddCylinder(mesh, p.centerX, p.centerY - sy * 0.5f, p.centerY + sy * 0.5f, p.centerZ, sx * 0.42f, sz * 0.50f, std::max(14, options.radialSegments)); break;
            case PlannedPartKind::CharacterArm:
            case PlannedPartKind::CharacterLeg: AddCylinder(mesh, p.centerX, p.centerY - sy * 0.5f, p.centerY + sy * 0.5f, p.centerZ, sx * 0.25f, sz * 0.28f, std::max(10, options.radialSegments / 2)); break;
            case PlannedPartKind::VehicleWheel: AddCylinder(mesh, p.centerX, p.centerY - sy * 0.5f, p.centerY + sy * 0.5f, p.centerZ + sz * 0.30f, sx * 0.45f, sz * 0.45f, std::max(16, options.radialSegments)); break;
            case PlannedPartKind::BuildingWindowGrid: AddWindowGrid(mesh, p); break;
            case PlannedPartKind::BuildingRoof: AddBox(mesh, p.centerX, p.centerY, p.centerZ, sx * 1.08f, sy, sz * 1.10f); break;
            case PlannedPartKind::BuildingDoor:
            case PlannedPartKind::DetailPanel:
            case PlannedPartKind::DetailBand: AddBox(mesh, p.centerX, p.centerY, p.centerZ + sz * 0.52f, sx, sy, std::max(0.02f, sz * 0.08f)); break;
            case PlannedPartKind::ComplexProtrusion: AddBox(mesh, p.centerX, p.centerY, p.centerZ, sx, sy, sz * 1.25f); break;
            default: AddBox(mesh, p.centerX, p.centerY, p.centerZ, sx, sy, sz); break;
        }
    }
    if (!mesh.positions.empty()) { RecomputeNormals(mesh); NormalizeMesh(mesh, plan.targetHeight); }
    return mesh;
}

static MeshData BuildColliderBox(const GameAssetValidationReport& v) {
    MeshData mesh;
    const float sx = std::max(0.001f, v.boundsMaxX - v.boundsMinX), sy = std::max(0.001f, v.boundsMaxY - v.boundsMinY), sz = std::max(0.001f, v.boundsMaxZ - v.boundsMinZ);
    AddBox(mesh, (v.boundsMinX + v.boundsMaxX) * 0.5f, (v.boundsMinY + v.boundsMaxY) * 0.5f, (v.boundsMinZ + v.boundsMaxZ) * 0.5f, sx, sy, sz);
    RecomputeNormals(mesh);
    return mesh;
}

static MeshData BuildLodProxy(const GameAssetValidationReport& v, GameAssetType type) {
    MeshData mesh;
    const float sx = std::max(0.001f, v.boundsMaxX - v.boundsMinX), sy = std::max(0.001f, v.boundsMaxY - v.boundsMinY), sz = std::max(0.001f, v.boundsMaxZ - v.boundsMinZ);
    const float cx = (v.boundsMinX + v.boundsMaxX) * 0.5f, cy = (v.boundsMinY + v.boundsMaxY) * 0.5f, cz = (v.boundsMinZ + v.boundsMaxZ) * 0.5f;
    if (type == GameAssetType::Vehicle || type == GameAssetType::Building || type == GameAssetType::ArchitecturalPart) AddBox(mesh, cx, cy, cz, sx, sy, sz);
    else AddCylinder(mesh, cx, v.boundsMinY, v.boundsMaxY, cz, sx * 0.38f, sz * 0.38f, 12);
    RecomputeNormals(mesh);
    return mesh;
}

static std::string MakeMarkdownReport(const GameAssetGenerationResult& r) {
    std::ostringstream o;
    o << "# Make3D Generic Game Asset Report\n\n";
    o << "## Asset analysis\n\n" << r.analysis.ToMarkdown() << "\n";
    o << "## Asset plan\n\n" << r.plan.ToMarkdown() << "\n";
    o << "## Classification\n\n" << r.classification.ToMarkdown() << "\n";
    o << "## Mesh validation\n\n" << r.validation.ToMarkdown() << "\n";
    o << "## Phase 0 safe-output quality gate\n\n" << r.qualityGate.ToMarkdown() << "\n";
    o << "## Game metadata\n\n" << r.metadata.ToMarkdown() << "\n";
    o << "## Output files\n\n- " << r.objPath.generic_string() << "\n- " << r.gltfPath.generic_string() << "\n";
    if (!r.colliderObjPath.empty()) o << "- " << r.colliderObjPath.generic_string() << "\n";
    if (!r.lodObjPath.empty()) o << "- " << r.lodObjPath.generic_string() << "\n";
    return o.str();
}

static std::string MakeManifestJson(const GameAssetGenerationResult& r) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"ok\": " << (r.ok ? "true" : "false") << ",\n";
    o << "  \"message\": \"" << EscapeJson(r.message) << "\",\n";
    o << "  \"analysis\": " << r.analysis.ToJson() << ",\n";
    o << "  \"plan\": " << r.plan.ToJson() << ",\n";
    o << "  \"classification\": " << r.classification.ToJson() << ",\n";
    o << "  \"validation\": " << r.validation.ToJson() << ",\n";
    o << "  \"qualityGate\": " << r.qualityGate.ToJson() << ",\n";
    o << "  \"metadata\": " << r.metadata.ToJson() << ",\n";
    o << "  \"obj\": \"" << EscapeJson(r.objPath.generic_string()) << "\",\n";
    o << "  \"gltf\": \"" << EscapeJson(r.gltfPath.generic_string()) << "\",\n";
    o << "  \"colliderObj\": \"" << EscapeJson(r.colliderObjPath.generic_string()) << "\",\n";
    o << "  \"lodProxyObj\": \"" << EscapeJson(r.lodObjPath.generic_string()) << "\"\n}\n";
    return o.str();
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

AssetAnalysisResult AnalyzeAsset(const ImageRGBA& image, const std::vector<std::uint8_t>& mask, const DepthImage& depth, const AssetAnalysisOptions& options) {
    AssetAnalysisResult r;
    if (image.width <= 0 || image.height <= 0 || mask.size() != static_cast<size_t>(image.width * image.height)) {
        r.warnings.push_back("Invalid image or mask for asset analysis.");
        return r;
    }
    r.shape = BuildShapeDescriptors(image, mask);
    if (r.shape.foregroundPixels <= 0) { r.warnings.push_back("No foreground pixels for asset analysis."); return r; }
    r.contour = ExtractContour(mask, image.width, image.height, options.maxContourPoints);
    r.palette = ExtractPalette(image, mask, options, r.shape.foregroundPixels);
    r.classification = InferGameAssetType(image, mask, depth);
    r.classification.edgeDensity = std::max(r.classification.edgeDensity, r.shape.edgeDensity);
    r.classification.straightEdgeScore = std::max(r.classification.straightEdgeScore, r.shape.straightEdgeScore);
    if (r.shape.contourComplexity > 0.72f && r.classification.assetType == GameAssetType::GenericProp) {
        r.classification.assetType = GameAssetType::ComplexObject;
        r.classification.reasons.push_back("Contour complexity upgraded generic prop to complex object.");
    }
    if (options.enablePartHints) r.partHints = GeneratePartHints(r.classification.assetType, r.shape);
    r.ok = true;
    return r;
}

AssetPlanResult BuildAssetPlanFromAnalysis(const AssetAnalysisResult& a, const AssetPlanOptions& options) {
    AssetPlanResult plan;
    if (!a.ok || a.shape.foregroundPixels <= 0) { plan.warnings.push_back("Cannot build asset plan because analysis is invalid."); return plan; }
    plan.ok = true;
    plan.assetType = a.classification.assetType;
    plan.assetName = "Make3D_" + std::string(ToString(plan.assetType)) + "_Planned";
    plan.targetHeight = std::max(0.10f, options.targetHeight);
    plan.targetWidth = std::clamp(plan.targetHeight / std::max(0.25f, a.shape.aspectRatio), 0.35f, 3.25f);
    plan.targetDepth = std::max(0.08f, options.defaultDepth);
    plan.wantsCollider = options.includeCollider;
    plan.wantsLod = options.includeLod;
    if (plan.assetType == GameAssetType::Building || plan.assetType == GameAssetType::ArchitecturalPart) plan.targetDepth = std::max(plan.targetDepth, 0.70f);
    else if (plan.assetType == GameAssetType::Vehicle) { plan.targetHeight *= 0.70f; plan.targetWidth = std::max(plan.targetWidth, 1.65f); plan.targetDepth = std::max(plan.targetDepth, 0.75f); }
    else if (plan.assetType == GameAssetType::Character || plan.assetType == GameAssetType::Creature) { plan.targetWidth = std::clamp(plan.targetWidth, 0.75f, 1.45f); plan.targetDepth = std::max(plan.targetDepth, 0.45f); }
    else if (plan.assetType == GameAssetType::ComplexObject || plan.assetType == GameAssetType::Machine) plan.targetDepth = std::max(plan.targetDepth, 0.70f);
    for (size_t i = 0; i < a.palette.size(); ++i) {
        PlannedMaterialSlot m;
        m.name = "mat_" + std::to_string(i);
        m.r = a.palette[i].r; m.g = a.palette[i].g; m.b = a.palette[i].b; m.coverage = a.palette[i].coverage;
        plan.materials.push_back(m);
    }
    if (plan.materials.empty()) plan.materials.push_back({"mat_default", 180, 180, 180, 1.0f});
    if (options.includeEditFriendlyParts) {
        for (const auto& h : a.partHints) {
            PlannedAssetPart p = PartFromHint(h, a.shape, plan.assetType, plan);
            p.materialSlot = plan.materials.empty() ? 0 : static_cast<int>(plan.parts.size() % plan.materials.size());
            plan.parts.push_back(p);
        }
    }
    if (plan.parts.empty()) plan.parts.push_back({PlannedPartKind::MainVolume, "main_volume", 1.0f, 0.0f, plan.targetHeight * 0.5f, 0.0f, plan.targetWidth, plan.targetHeight, plan.targetDepth, 0});
    return plan;
}

GameAssetValidationReport ValidateGameAssetMesh(const MeshData& mesh) {
    GameAssetValidationReport r;
    r.vertices = VertexCount(mesh);
    r.triangles = TriangleCount(mesh);
    r.missingNormals = mesh.normals.size() == mesh.positions.size() ? 0 : 1;
    r.missingUvs = mesh.uvs.size() == static_cast<size_t>(r.vertices) * 2 ? 0 : 1;
    if (mesh.positions.empty()) { r.warnings.push_back("Mesh has no positions."); return r; }
    r.boundsMinX = r.boundsMinY = r.boundsMinZ = std::numeric_limits<float>::max();
    r.boundsMaxX = r.boundsMaxY = r.boundsMaxZ = -std::numeric_limits<float>::max();
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        const float x = mesh.positions[i + 0], y = mesh.positions[i + 1], z = mesh.positions[i + 2];
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) { ++r.nonFinitePositions; continue; }
        r.boundsMinX = std::min(r.boundsMinX, x); r.boundsMinY = std::min(r.boundsMinY, y); r.boundsMinZ = std::min(r.boundsMinZ, z);
        r.boundsMaxX = std::max(r.boundsMaxX, x); r.boundsMaxY = std::max(r.boundsMaxY, y); r.boundsMaxZ = std::max(r.boundsMaxZ, z);
    }
    auto pos = [&](std::uint32_t idx) -> Vec3 { size_t p = static_cast<size_t>(idx) * 3; return {mesh.positions[p], mesh.positions[p + 1], mesh.positions[p + 2]}; };
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const std::uint32_t a = mesh.indices[i + 0], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
        if (a >= static_cast<std::uint32_t>(r.vertices) || b >= static_cast<std::uint32_t>(r.vertices) || c >= static_cast<std::uint32_t>(r.vertices)) { ++r.invalidIndices; continue; }
        const Vec3 pa = pos(a), pb = pos(b), pc = pos(c);
        const Vec3 ab{pb.x - pa.x, pb.y - pa.y, pb.z - pa.z};
        const Vec3 ac{pc.x - pa.x, pc.y - pa.y, pc.z - pa.z};
        const Vec3 cross{ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};
        if (cross.x * cross.x + cross.y * cross.y + cross.z * cross.z < 1.0e-12f) ++r.degenerateTriangles;
    }
    if (r.vertices == 0) r.warnings.push_back("Mesh has zero vertices.");
    if (r.triangles == 0) r.warnings.push_back("Mesh has zero triangles.");
    if (r.invalidIndices > 0) r.warnings.push_back("Mesh contains invalid face indices.");
    if (r.degenerateTriangles > 0) r.warnings.push_back("Mesh contains degenerate triangles.");
    if (r.nonFinitePositions > 0) r.warnings.push_back("Mesh contains NaN or infinity positions.");
    if (r.missingNormals) r.warnings.push_back("Mesh normals are missing or mismatched.");
    if (r.missingUvs) r.warnings.push_back("Mesh UVs are missing or mismatched.");
    r.ok = r.vertices > 0 && r.triangles > 0 && r.invalidIndices == 0 && r.nonFinitePositions == 0;
    return r;
}

GameAssetGenerationResult BuildGenericGameAsset(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const std::filesystem::path& outputDir, const GameAssetGeneratorOptions& options) {
    GameAssetGenerationResult result;
    result.analysis = AnalyzeAsset(color, mask, depth, options.analysis);
    if (result.analysis.ok) {
        AssetPlanOptions planOptions = options.plan;
        planOptions.targetHeight = options.targetHeight;
        planOptions.defaultDepth = std::max(options.extrusionDepth, options.buildingDepth);
        result.plan = BuildAssetPlanFromAnalysis(result.analysis, planOptions);
        result.classification = result.analysis.classification;
    } else {
        result.classification = InferGameAssetType(color, mask, depth);
    }
    if (options.useAssetAnalysisPlan && result.plan.ok) result.mesh = BuildPlannedAssetMesh(result.plan, options);
    if (result.mesh.positions.empty()) result.mesh = result.classification.assetType == GameAssetType::Vehicle ? BuildVehicleLikeAsset(options) : BuildReliefFallbackAsset(color, depth, mask, options);
    RecomputeNormals(result.mesh);
    result.validation = ValidateGameAssetMesh(result.mesh);
    result.qualityGate = AnalyzeMeshQualityGate(result.mesh, options.qualityGate);
    if (options.enforceSafeMeshQuality && (!result.validation.ok || !result.qualityGate.ok)) {
        result.mesh = BuildSafeProxyAsset(result.classification.assetType, options);
        RecomputeNormals(result.mesh);
        result.validation = ValidateGameAssetMesh(result.mesh);
        result.qualityGate = AnalyzeMeshQualityGate(result.mesh, options.qualityGate);
        result.qualityGate.warnings.push_back("Original generated mesh failed Phase 0 safe-output gate; exported stable typed proxy instead.");
    }
    if (!result.validation.ok || (options.enforceSafeMeshQuality && !result.qualityGate.ok)) { result.message = "Generic game asset generation produced unsafe mesh."; return result; }

    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    result.objPath = outputDir / "make3d_game_asset.obj";
    result.gltfPath = outputDir / "make3d_game_asset.gltf";
    result.reportPath = outputDir / "make3d_game_asset_report.md";
    result.manifestPath = outputDir / "make3d_game_asset_manifest.json";
    std::string error;
    if (!ExportOBJ(result.mesh, result.objPath, "", &error)) { result.message = error; return result; }
    GltfMaterialOptions material;
    material.materialName = "Make3DGameAssetMaterial";
    if (!result.plan.materials.empty()) { const auto& m = result.plan.materials.front(); material.baseColorFactor = {m.r / 255.0f, m.g / 255.0f, m.b / 255.0f, 1.0f}; }
    else material.baseColorFactor = {0.72f, 0.76f, 0.82f, 1.0f};
    if (!ExportGLTFWithMaterial(result.mesh, result.gltfPath, material, &error)) { result.message = error; return result; }
    if (options.generateColliderProxy) { result.colliderMesh = BuildColliderBox(result.validation); result.colliderValidation = ValidateGameAssetMesh(result.colliderMesh); result.colliderObjPath = outputDir / "make3d_game_asset_collider.obj"; if (!ExportOBJ(result.colliderMesh, result.colliderObjPath, "", &error)) { result.message = error; return result; } }
    if (options.generateLodProxy) { result.lodProxyMesh = BuildLodProxy(result.validation, result.classification.assetType); result.lodValidation = ValidateGameAssetMesh(result.lodProxyMesh); result.lodObjPath = outputDir / "make3d_game_asset_lod_proxy.obj"; if (!ExportOBJ(result.lodProxyMesh, result.lodObjPath, "", &error)) { result.message = error; return result; } }
    result.metadata.assetName = result.plan.ok ? result.plan.assetName : "Make3D_" + std::string(ToString(result.classification.assetType));
    result.metadata.assetType = result.classification.assetType;
    result.metadata.pivotX = 0.0f; result.metadata.pivotY = result.validation.boundsMinY; result.metadata.pivotZ = 0.0f;
    result.metadata.colliderMinX = result.validation.boundsMinX; result.metadata.colliderMinY = result.validation.boundsMinY; result.metadata.colliderMinZ = result.validation.boundsMinZ;
    result.metadata.colliderMaxX = result.validation.boundsMaxX; result.metadata.colliderMaxY = result.validation.boundsMaxY; result.metadata.colliderMaxZ = result.validation.boundsMaxZ;
    result.metadata.lod0Vertices = result.validation.vertices; result.metadata.lod0Triangles = result.validation.triangles;
    result.metadata.lodProxyVertices = VertexCount(result.lodProxyMesh); result.metadata.lodProxyTriangles = TriangleCount(result.lodProxyMesh);
    std::ofstream report(result.reportPath, std::ios::binary); report << MakeMarkdownReport(result);
    result.ok = true;
    result.message = result.plan.ok ? "Generic game asset generation finished with compact analysis/plan typed mesh." : "Generic game asset generation finished with fallback mesh.";
    std::ofstream manifest(result.manifestPath, std::ios::binary); manifest << MakeManifestJson(result);
    return result;
}

std::string ShapeDescriptorReport::ToMarkdown() const { std::ostringstream o; o << "| Metric | Value |\n|---|---:|\n" << "| Foreground pixels | " << foregroundPixels << " |\n" << "| Bounds | " << minX << ", " << minY << " - " << maxX << ", " << maxY << " |\n" << "| Coverage | " << coverage << " |\n" << "| Aspect ratio | " << aspectRatio << " |\n" << "| Rectangularity | " << rectangularity << " |\n" << "| Perimeter ratio | " << perimeterRatio << " |\n" << "| Symmetry X | " << symmetryX << " |\n" << "| Symmetry Y | " << symmetryY << " |\n" << "| Row stability | " << rowStability << " |\n" << "| Column stability | " << columnStability << " |\n" << "| Straight edge score | " << straightEdgeScore << " |\n" << "| Edge density | " << edgeDensity << " |\n" << "| Mass center | " << massCenterX << ", " << massCenterY << " |\n" << "| Contour complexity | " << contourComplexity << " |\n"; return o.str(); }
std::string ShapeDescriptorReport::ToJson() const { std::ostringstream o; o << "{\"foregroundPixels\": " << foregroundPixels << ", \"bounds\": [" << minX << ", " << minY << ", " << maxX << ", " << maxY << "], \"coverage\": " << coverage << ", \"aspectRatio\": " << aspectRatio << ", \"rectangularity\": " << rectangularity << ", \"perimeterRatio\": " << perimeterRatio << ", \"symmetryX\": " << symmetryX << ", \"symmetryY\": " << symmetryY << ", \"rowStability\": " << rowStability << ", \"columnStability\": " << columnStability << ", \"straightEdgeScore\": " << straightEdgeScore << ", \"edgeDensity\": " << edgeDensity << ", \"massCenter\": [" << massCenterX << ", " << massCenterY << "], \"contourComplexity\": " << contourComplexity << "}"; return o.str(); }
std::string AssetAnalysisResult::ToMarkdown() const { std::ostringstream o; o << "# Make3D Asset Analysis Report\n\n## Classification\n\n" << classification.ToMarkdown() << "\n## Shape descriptors\n\n" << shape.ToMarkdown() << "\n## Contour\n\n| Metric | Value |\n|---|---:|\n| Sampled contour points | " << contour.size() << " |\n\n## Palette\n\n| Index | RGB | Coverage |\n|---:|---|---:|\n"; for (size_t i = 0; i < palette.size(); ++i) o << "| " << i << " | " << static_cast<int>(palette[i].r) << ", " << static_cast<int>(palette[i].g) << ", " << static_cast<int>(palette[i].b) << " | " << palette[i].coverage << " |\n"; o << "\n## Part hints\n\n| Name | Role | Confidence | Bounds |\n|---|---|---:|---|\n"; for (const auto& h : partHints) o << "| " << h.name << " | " << h.role << " | " << h.confidence << " | " << h.minX << "," << h.minY << " - " << h.maxX << "," << h.maxY << " |\n"; return o.str(); }
std::string AssetAnalysisResult::ToJson() const { std::ostringstream o; o << "{\"ok\": " << (ok ? "true" : "false") << ", \"classification\": " << classification.ToJson() << ", \"shape\": " << shape.ToJson() << ", \"contourPoints\": " << contour.size() << ", \"palette\": ["; for (size_t i = 0; i < palette.size(); ++i) { if (i) o << ", "; o << "{\"rgb\": [" << static_cast<int>(palette[i].r) << ", " << static_cast<int>(palette[i].g) << ", " << static_cast<int>(palette[i].b) << "], \"coverage\": " << palette[i].coverage << "}"; } o << "], \"partHints\": ["; for (size_t i = 0; i < partHints.size(); ++i) { if (i) o << ", "; const auto& h = partHints[i]; o << "{\"name\": \"" << EscapeJson(h.name) << "\", \"role\": \"" << EscapeJson(h.role) << "\", \"confidence\": " << h.confidence << ", \"bounds\": [" << h.minX << ", " << h.minY << ", " << h.maxX << ", " << h.maxY << "]}"; } o << "]}"; return o.str(); }
std::string PlannedAssetPart::ToMarkdownRow() const { std::ostringstream o; o << "| " << name << " | " << ToString(kind) << " | " << confidence << " | " << centerX << ", " << centerY << ", " << centerZ << " | " << sizeX << ", " << sizeY << ", " << sizeZ << " | " << materialSlot << " |"; return o.str(); }
std::string PlannedAssetPart::ToJson() const { std::ostringstream o; o << "{\"name\": \"" << EscapeJson(name) << "\", \"kind\": \"" << ToString(kind) << "\", \"confidence\": " << confidence << ", \"center\": [" << centerX << ", " << centerY << ", " << centerZ << "], \"size\": [" << sizeX << ", " << sizeY << ", " << sizeZ << "], \"materialSlot\": " << materialSlot << "}"; return o.str(); }
std::string PlannedMaterialSlot::ToJson() const { std::ostringstream o; o << "{\"name\": \"" << EscapeJson(name) << "\", \"rgb\": [" << static_cast<int>(r) << ", " << static_cast<int>(g) << ", " << static_cast<int>(b) << "], \"coverage\": " << coverage << "}"; return o.str(); }
std::string AssetPlanResult::ToMarkdown() const { std::ostringstream o; o << "# Make3D Asset Plan\n\n| Field | Value |\n|---|---:|\n" << "| OK | " << (ok ? "yes" : "no") << " |\n" << "| Asset type | " << ToString(assetType) << " |\n" << "| Asset name | " << assetName << " |\n" << "| Target size | " << targetWidth << ", " << targetHeight << ", " << targetDepth << " |\n" << "| Pivot | " << pivotX << ", " << pivotY << ", " << pivotZ << " |\n" << "| Wants collider | " << (wantsCollider ? "yes" : "no") << " |\n" << "| Wants LOD | " << (wantsLod ? "yes" : "no") << " |\n\n## Parts\n\n| Name | Kind | Confidence | Center | Size | Material |\n|---|---|---:|---|---|---:|\n"; for (const auto& p : parts) o << p.ToMarkdownRow() << "\n"; return o.str(); }
std::string AssetPlanResult::ToJson() const { std::ostringstream o; o << "{\"ok\": " << (ok ? "true" : "false") << ", \"assetType\": \"" << ToString(assetType) << "\", \"assetName\": \"" << EscapeJson(assetName) << "\", \"targetSize\": [" << targetWidth << ", " << targetHeight << ", " << targetDepth << "], \"pivot\": [" << pivotX << ", " << pivotY << ", " << pivotZ << "], \"wantsCollider\": " << (wantsCollider ? "true" : "false") << ", \"wantsLod\": " << (wantsLod ? "true" : "false") << ", \"materials\": ["; for (size_t i = 0; i < materials.size(); ++i) { if (i) o << ", "; o << materials[i].ToJson(); } o << "], \"parts\": ["; for (size_t i = 0; i < parts.size(); ++i) { if (i) o << ", "; o << parts[i].ToJson(); } o << "]}"; return o.str(); }
std::string GameAssetValidationReport::ToMarkdown() const { std::ostringstream o; o << "| Metric | Value |\n|---|---:|\n| OK | " << (ok ? "yes" : "no") << " |\n| Vertices | " << vertices << " |\n| Triangles | " << triangles << " |\n| Invalid indices | " << invalidIndices << " |\n| Degenerate triangles | " << degenerateTriangles << " |\n| Non-finite positions | " << nonFinitePositions << " |\n| Missing normals | " << missingNormals << " |\n| Missing UVs | " << missingUvs << " |\n| Bounds min | " << boundsMinX << ", " << boundsMinY << ", " << boundsMinZ << " |\n| Bounds max | " << boundsMaxX << ", " << boundsMaxY << ", " << boundsMaxZ << " |\n\n"; if (!warnings.empty()) { o << "### Warnings\n\n"; for (const auto& w : warnings) o << "- " << w << "\n"; } return o.str(); }
std::string GameAssetValidationReport::ToJson() const { std::ostringstream o; o << "{\"ok\": " << (ok ? "true" : "false") << ", \"vertices\": " << vertices << ", \"triangles\": " << triangles << ", \"invalidIndices\": " << invalidIndices << ", \"degenerateTriangles\": " << degenerateTriangles << ", \"nonFinitePositions\": " << nonFinitePositions << ", \"missingNormals\": " << missingNormals << ", \"missingUvs\": " << missingUvs << ", \"boundsMin\": [" << boundsMinX << ", " << boundsMinY << ", " << boundsMinZ << "], \"boundsMax\": [" << boundsMaxX << ", " << boundsMaxY << ", " << boundsMaxZ << "], \"warnings\": ["; for (size_t i = 0; i < warnings.size(); ++i) { if (i) o << ", "; o << "\"" << EscapeJson(warnings[i]) << "\""; } o << "]}"; return o.str(); }
std::string GameAssetMetadata::ToMarkdown() const { std::ostringstream o; o << "| Field | Value |\n|---|---:|\n| Asset name | " << assetName << " |\n| Asset type | " << ToString(assetType) << " |\n| Pivot | " << pivotX << ", " << pivotY << ", " << pivotZ << " |\n| Unit scale meters | " << unitScaleMeters << " |\n| Collider min | " << colliderMinX << ", " << colliderMinY << ", " << colliderMinZ << " |\n| Collider max | " << colliderMaxX << ", " << colliderMaxY << ", " << colliderMaxZ << " |\n| LOD0 vertices | " << lod0Vertices << " |\n| LOD0 triangles | " << lod0Triangles << " |\n| LOD proxy vertices | " << lodProxyVertices << " |\n| LOD proxy triangles | " << lodProxyTriangles << " |\n"; return o.str(); }
std::string GameAssetMetadata::ToJson() const { std::ostringstream o; o << "{\"assetName\": \"" << EscapeJson(assetName) << "\", \"assetType\": \"" << EscapeJson(ToString(assetType)) << "\", \"pivot\": [" << pivotX << ", " << pivotY << ", " << pivotZ << "], \"unitScaleMeters\": " << unitScaleMeters << ", \"colliderMin\": [" << colliderMinX << ", " << colliderMinY << ", " << colliderMinZ << "], \"colliderMax\": [" << colliderMaxX << ", " << colliderMaxY << ", " << colliderMaxZ << "], \"lod0Vertices\": " << lod0Vertices << ", \"lod0Triangles\": " << lod0Triangles << ", \"lodProxyVertices\": " << lodProxyVertices << ", \"lodProxyTriangles\": " << lodProxyTriangles << "}"; return o.str(); }

} // namespace make3d
