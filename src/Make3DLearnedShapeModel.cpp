#include "Make3DLearnedShapeModel.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace make3d {

namespace {

static int Idx(int x, int y, int w) { return y * w + x; }
static float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
static float Sigmoid(float v) { return 1.0f / (1.0f + std::exp(-v)); }
static float Relu(float v) { return std::max(0.0f, v); }

static std::string EscapeJson(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '\\') o << "\\\\";
        else if (c == '"') o << "\\\"";
        else if (c == '\n') o << "\\n";
        else o << c;
    }
    return o.str();
}

static float Luminance(const ImageRGBA& image, int x, int y) {
    size_t p = (static_cast<size_t>(y) * image.width + x) * 4;
    float r = static_cast<float>(image.pixels[p + 0]) / 255.0f;
    float g = static_cast<float>(image.pixels[p + 1]) / 255.0f;
    float b = static_cast<float>(image.pixels[p + 2]) / 255.0f;
    return r * 0.2126f + g * 0.7152f + b * 0.0722f;
}

static float SafeDepthAt(const DepthImage& d, int x, int y) {
    x = std::clamp(x, 0, d.width - 1);
    y = std::clamp(y, 0, d.height - 1);
    return d.values[static_cast<size_t>(Idx(x, y, d.width))];
}

static void EvalMLP(const LearnedShapeModelWeights& w, const float in[LearnedShapeModelWeights::InputFeatures], float out[LearnedShapeModelWeights::OutputFeatures]) {
    float h[LearnedShapeModelWeights::HiddenUnits];
    for (int j = 0; j < LearnedShapeModelWeights::HiddenUnits; ++j) {
        float v = w.hiddenBias[static_cast<size_t>(j)];
        for (int i = 0; i < LearnedShapeModelWeights::InputFeatures; ++i) {
            v += in[i] * w.inputToHidden[static_cast<size_t>(i * LearnedShapeModelWeights::HiddenUnits + j)];
        }
        h[j] = Relu(v);
    }
    for (int k = 0; k < LearnedShapeModelWeights::OutputFeatures; ++k) {
        float v = w.outputBias[static_cast<size_t>(k)];
        for (int j = 0; j < LearnedShapeModelWeights::HiddenUnits; ++j) {
            v += h[j] * w.hiddenToOutput[static_cast<size_t>(j * LearnedShapeModelWeights::OutputFeatures + k)];
        }
        out[k] = v;
    }
}

} // namespace

LearnedShapeModelWeights LearnedShapeModelWeights::BuiltInPrior() {
    LearnedShapeModelWeights w;
    for (size_t i = 0; i < w.inputToHidden.size(); ++i) {
        float sign = (i % 2) ? -1.0f : 1.0f;
        w.inputToHidden[i] = sign * (0.035f + static_cast<float>((i * 7) % 13) * 0.006f);
    }
    for (size_t i = 0; i < w.hiddenBias.size(); ++i) w.hiddenBias[i] = -0.08f + static_cast<float>(i) * 0.018f;
    for (size_t i = 0; i < w.hiddenToOutput.size(); ++i) {
        float sign = (i % 3 == 0) ? 1.0f : -1.0f;
        w.hiddenToOutput[i] = sign * (0.045f + static_cast<float>((i * 5) % 11) * 0.007f);
    }
    w.outputBias = {0.20f, 0.0f, 0.35f};
    return w;
}

bool SaveLearnedShapeModelWeights(const LearnedShapeModelWeights& weights, const std::filesystem::path& path, std::string* error) {
    std::ofstream f(path, std::ios::binary);
    if (!f) { if (error) *error = "Failed to open weights for writing."; return false; }
    f << "MAKE3D_LEARNED_SHAPE_V1\n";
    f << std::setprecision(9);
    for (float v : weights.inputToHidden) f << v << ' '; f << "\n";
    for (float v : weights.hiddenBias) f << v << ' '; f << "\n";
    for (float v : weights.hiddenToOutput) f << v << ' '; f << "\n";
    for (float v : weights.outputBias) f << v << ' '; f << "\n";
    return true;
}

bool LoadLearnedShapeModelWeights(const std::filesystem::path& path, LearnedShapeModelWeights& weights, std::string* error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { if (error) *error = "Failed to open weights for reading."; return false; }
    std::string magic;
    std::getline(f, magic);
    if (magic != "MAKE3D_LEARNED_SHAPE_V1") { if (error) *error = "Invalid learned shape weight file."; return false; }
    for (float& v : weights.inputToHidden) f >> v;
    for (float& v : weights.hiddenBias) f >> v;
    for (float& v : weights.hiddenToOutput) f >> v;
    for (float& v : weights.outputBias) f >> v;
    if (!f) { if (error) *error = "Failed to parse learned shape weights."; return false; }
    return true;
}

LearnedShapeModelResult RunLearnedShapeModel(
    const ImageRGBA& image,
    const std::vector<std::uint8_t>& mask,
    const DepthImage& inputDepth,
    const ShapeInferenceResult& shapeInference,
    const LearnedShapeModelOptions& options) {

    LearnedShapeModelResult result;
    if (!options.enabled) {
        result.message = "Learned shape model disabled.";
        return result;
    }
    if (image.width <= 0 || image.height <= 0 || inputDepth.values.size() != static_cast<size_t>(image.width * image.height) || mask.size() != static_cast<size_t>(image.width * image.height)) {
        result.message = "Invalid input dimensions.";
        return result;
    }

    LearnedShapeModelWeights weights = LearnedShapeModelWeights::BuiltInPrior();
    if (options.useExternalWeights && !options.weightsPath.empty()) {
        std::string loadError;
        if (!LoadLearnedShapeModelWeights(options.weightsPath, weights, &loadError)) {
            result.message = loadError;
            return result;
        }
    }

    const int w = image.width;
    const int h = image.height;
    result.learnedDepth.width = w;
    result.learnedDepth.height = h;
    result.learnedDepth.values.assign(static_cast<size_t>(w) * h, 0.0f);
    result.confidence.assign(static_cast<size_t>(w) * h, 0.0f);
    result.normalXYZ.assign(static_cast<size_t>(w) * h * 3, 0.0f);

    float minD = std::numeric_limits<float>::max();
    float maxD = -std::numeric_limits<float>::max();
    double sumD = 0.0;
    double sumC = 0.0;
    int count = 0;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int id = Idx(x, y, w);
            if (!mask[static_cast<size_t>(id)]) continue;

            float u = w <= 1 ? 0.5f : static_cast<float>(x) / static_cast<float>(w - 1);
            float v = h <= 1 ? 0.5f : static_cast<float>(y) / static_cast<float>(h - 1);
            float lum = Luminance(image, x, y);
            float baseDepth = inputDepth.values[static_cast<size_t>(id)];
            float dx = SafeDepthAt(inputDepth, x + 1, y) - SafeDepthAt(inputDepth, x - 1, y);
            float dy = SafeDepthAt(inputDepth, x, y + 1) - SafeDepthAt(inputDepth, x, y - 1);
            float cls = shapeInference.shapeClass == ShapeClass::Character ? 1.0f : (shapeInference.shapeClass == ShapeClass::Prop ? 0.5f : 0.0f);

            float in[LearnedShapeModelWeights::InputFeatures] = {
                u, v, lum, baseDepth, dx, dy,
                shapeInference.coverage,
                shapeInference.aspectRatio,
                shapeInference.symmetry,
                cls
            };
            float out[LearnedShapeModelWeights::OutputFeatures] = {};
            EvalMLP(weights, in, out);

            float learned = Clamp01(Sigmoid(out[0]) * 0.85f + baseDepth * 0.15f);
            float conf = Clamp01(Sigmoid(out[2]));
            float blended = Clamp01(baseDepth * options.blendWithInputDepth + learned * (1.0f - options.blendWithInputDepth));
            if (conf < options.confidenceThreshold) blended = baseDepth;

            result.learnedDepth.values[static_cast<size_t>(id)] = blended;
            result.confidence[static_cast<size_t>(id)] = conf;
            result.normalXYZ[static_cast<size_t>(id) * 3 + 0] = -dx;
            result.normalXYZ[static_cast<size_t>(id) * 3 + 1] = -dy;
            result.normalXYZ[static_cast<size_t>(id) * 3 + 2] = 1.0f;

            minD = std::min(minD, blended);
            maxD = std::max(maxD, blended);
            sumD += blended;
            sumC += conf;
            ++count;
        }
    }

    if (count == 0) {
        result.message = "No foreground pixels for learned model.";
        return result;
    }

    result.ok = true;
    result.message = options.useExternalWeights ? "Learned shape model used external weights." : "Learned shape model used built-in prior weights.";
    result.depthMin = minD;
    result.depthMax = maxD;
    result.depthMean = static_cast<float>(sumD / static_cast<double>(count));
    result.meanConfidence = static_cast<float>(sumC / static_cast<double>(count));
    return result;
}

std::string LearnedShapeModelResult::ToMarkdown() const {
    std::ostringstream o;
    o << "# Learned Shape Model Report\n\n";
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| OK | " << (ok ? "yes" : "no") << " |\n";
    o << "| Mean confidence | " << meanConfidence << " |\n";
    o << "| Depth min | " << depthMin << " |\n";
    o << "| Depth max | " << depthMax << " |\n";
    o << "| Depth mean | " << depthMean << " |\n";
    o << "\n" << message << "\n";
    return o.str();
}

std::string LearnedShapeModelResult::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"ok\": " << (ok ? "true" : "false") << ",\n";
    o << "  \"message\": \"" << EscapeJson(message) << "\",\n";
    o << "  \"meanConfidence\": " << meanConfidence << ",\n";
    o << "  \"depthMin\": " << depthMin << ",\n";
    o << "  \"depthMax\": " << depthMax << ",\n";
    o << "  \"depthMean\": " << depthMean << "\n";
    o << "}";
    return o.str();
}

} // namespace make3d
