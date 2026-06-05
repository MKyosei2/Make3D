#include "Make3DShapeInference.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>

namespace make3d {

namespace {

static int Idx(int x, int y, int w) { return y * w + x; }
static float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

static std::string JsonEscape(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '\\') o << "\\\\";
        else if (c == '"') o << "\\\"";
        else if (c == '\n') o << "\\n";
        else o << c;
    }
    return o.str();
}

static ShapeClass Classify(float aspect, float coverage, float symmetry, float massCenterY) {
    if (coverage < 0.04f) return ShapeClass::Flat;
    if (aspect > 1.25f && symmetry > 0.55f && massCenterY > 0.38f && massCenterY < 0.68f) return ShapeClass::Character;
    if (aspect < 0.55f || coverage > 0.45f) return ShapeClass::Prop;
    if (symmetry > 0.45f) return ShapeClass::Character;
    return ShapeClass::Prop;
}

} // namespace

const char* ToString(ShapeClass value) {
    switch (value) {
        case ShapeClass::Character: return "Character";
        case ShapeClass::Prop: return "Prop";
        case ShapeClass::Flat: return "Flat";
        default: return "Unknown";
    }
}

ShapeInferenceResult RunShapeInference(
    const ImageRGBA& image,
    const std::vector<std::uint8_t>& mask,
    const DepthImage& depth,
    const ShapeInferenceOptions& options) {

    ShapeInferenceResult r;
    const int w = image.width;
    const int h = image.height;
    r.adjustedDepth = depth;
    r.backDepth = depth;
    r.sideDepth = depth;
    if (w <= 0 || h <= 0 || mask.size() != static_cast<size_t>(w * h) || depth.values.size() != static_cast<size_t>(w * h)) return r;

    int minX = w, minY = h, maxX = -1, maxY = -1;
    double sumY = 0.0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!mask[static_cast<size_t>(Idx(x, y, w))]) continue;
            ++r.foregroundPixels;
            minX = std::min(minX, x); minY = std::min(minY, y);
            maxX = std::max(maxX, x); maxY = std::max(maxY, y);
            sumY += static_cast<double>(y);
        }
    }
    if (r.foregroundPixels <= 0) return r;

    r.coverage = static_cast<float>(r.foregroundPixels) / static_cast<float>(w * h);
    int bw = std::max(1, maxX - minX + 1);
    int bh = std::max(1, maxY - minY + 1);
    r.aspectRatio = static_cast<float>(bh) / static_cast<float>(bw);
    r.massCenterY = static_cast<float>(sumY / static_cast<double>(r.foregroundPixels) / static_cast<double>(h - 1));

    int symHit = 0;
    int symTotal = 0;
    int cx = (minX + maxX) / 2;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            int mirror = cx - (x - cx);
            if (mirror < 0 || mirror >= w) continue;
            bool a = mask[static_cast<size_t>(Idx(x, y, w))] != 0;
            bool b = mask[static_cast<size_t>(Idx(mirror, y, w))] != 0;
            if (a || b) {
                ++symTotal;
                if (a == b) ++symHit;
            }
        }
    }
    r.symmetry = symTotal > 0 ? static_cast<float>(symHit) / static_cast<float>(symTotal) : 0.0f;
    r.shapeClass = Classify(r.aspectRatio, r.coverage, r.symmetry, r.massCenterY);

    int samples = std::max(8, options.profileSamples);
    r.radiusProfile.assign(static_cast<size_t>(samples), 0.0f);
    r.depthProfile.assign(static_cast<size_t>(samples), 0.0f);

    for (int s = 0; s < samples; ++s) {
        int y = minY + (bh <= 1 ? 0 : (s * (bh - 1)) / (samples - 1));
        int rowMin = w, rowMax = -1, count = 0;
        float depthSum = 0.0f;
        for (int x = 0; x < w; ++x) {
            int id = Idx(x, y, w);
            if (!mask[static_cast<size_t>(id)]) continue;
            rowMin = std::min(rowMin, x);
            rowMax = std::max(rowMax, x);
            depthSum += depth.values[static_cast<size_t>(id)];
            ++count;
        }
        if (count > 0) {
            r.radiusProfile[static_cast<size_t>(s)] = static_cast<float>(rowMax - rowMin + 1) / static_cast<float>(bw);
            r.depthProfile[static_cast<size_t>(s)] = depthSum / static_cast<float>(count);
        }
    }

    if (options.useShapePrior) {
        for (int y = 0; y < h; ++y) {
            float yf = h <= 1 ? 0.0f : static_cast<float>(y) / static_cast<float>(h - 1);
            float classBoost = 1.0f;
            if (r.shapeClass == ShapeClass::Character) {
                float torso = 1.0f - std::min(1.0f, std::abs(yf - 0.48f) * 3.2f);
                float head = 1.0f - std::min(1.0f, std::abs(yf - 0.18f) * 8.0f);
                classBoost = 0.78f + 0.28f * torso + 0.14f * head;
            } else if (r.shapeClass == ShapeClass::Prop) {
                classBoost = 0.90f + 0.12f * std::sin(3.14159265f * yf);
            }
            for (int x = 0; x < w; ++x) {
                int id = Idx(x, y, w);
                if (!mask[static_cast<size_t>(id)]) continue;
                r.adjustedDepth.values[static_cast<size_t>(id)] = Clamp01(depth.values[static_cast<size_t>(id)] * classBoost);
                r.backDepth.values[static_cast<size_t>(id)] = Clamp01(r.adjustedDepth.values[static_cast<size_t>(id)] * options.backDepthScale + 0.12f);
                r.sideDepth.values[static_cast<size_t>(id)] = Clamp01(r.adjustedDepth.values[static_cast<size_t>(id)] * options.sideDepthScale + 0.08f);
            }
        }
    }

    return r;
}

std::string ShapeInferenceResult::ToMarkdown() const {
    std::ostringstream o;
    o << "# Shape Inference Report\n\n";
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| Shape class | " << ToString(shapeClass) << " |\n";
    o << "| Foreground pixels | " << foregroundPixels << " |\n";
    o << "| Coverage | " << coverage << " |\n";
    o << "| Aspect ratio | " << aspectRatio << " |\n";
    o << "| Symmetry | " << symmetry << " |\n";
    o << "| Mass center Y | " << massCenterY << " |\n";
    o << "| Profile samples | " << radiusProfile.size() << " |\n";
    return o.str();
}

std::string ShapeInferenceResult::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"shapeClass\": \"" << JsonEscape(ToString(shapeClass)) << "\",\n";
    o << "  \"foregroundPixels\": " << foregroundPixels << ",\n";
    o << "  \"coverage\": " << coverage << ",\n";
    o << "  \"aspectRatio\": " << aspectRatio << ",\n";
    o << "  \"symmetry\": " << symmetry << ",\n";
    o << "  \"massCenterY\": " << massCenterY << ",\n";
    o << "  \"profileSamples\": " << radiusProfile.size() << "\n";
    o << "}";
    return o.str();
}

} // namespace make3d
