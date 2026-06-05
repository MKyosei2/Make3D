#include "Make3DLearnedShapeTrainer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>

namespace make3d {

namespace {

struct TrainingRow {
    std::array<float, LearnedShapeModelWeights::InputFeatures> x{};
    std::array<float, LearnedShapeModelWeights::OutputFeatures> y{};
};

static float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
static float Sigmoid(float v) { return 1.0f / (1.0f + std::exp(-v)); }
static float Relu(float v) { return std::max(0.0f, v); }
static float ReluGrad(float v) { return v > 0.0f ? 1.0f : 0.0f; }

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

static float TeacherDepth(float u, float v, float lum, float baseDepth, float coverage, float aspect, float symmetry, float cls) {
    float cx = u - 0.5f;
    float cy = v - 0.5f;
    float radial = std::sqrt(cx * cx * 1.8f + cy * cy * 1.2f);
    float dome = Clamp01(1.0f - radial * 1.55f);
    float torso = 1.0f - std::min(1.0f, std::abs(v - 0.50f) * 3.0f);
    float head = 1.0f - std::min(1.0f, std::abs(v - 0.20f) * 8.0f);
    float shape = cls > 0.75f ? (0.25f * head + 0.35f * torso + 0.40f * dome) : (0.15f * symmetry + 0.85f * dome);
    return Clamp01(baseDepth * 0.25f + shape * 0.55f + lum * 0.12f + coverage * 0.08f + std::min(aspect, 2.0f) * 0.025f);
}

static std::vector<TrainingRow> BuildSyntheticRows(int count) {
    std::vector<TrainingRow> rows;
    rows.reserve(static_cast<size_t>(count));
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    for (int i = 0; i < count; ++i) {
        TrainingRow r;
        float u = unit(rng);
        float v = unit(rng);
        float lum = unit(rng);
        float baseDepth = unit(rng);
        float dx = (unit(rng) - 0.5f) * 0.35f;
        float dy = (unit(rng) - 0.5f) * 0.35f;
        float coverage = 0.05f + unit(rng) * 0.55f;
        float aspect = 0.45f + unit(rng) * 2.3f;
        float symmetry = unit(rng);
        float cls = unit(rng) > 0.52f ? 1.0f : 0.5f;
        r.x = {u, v, lum, baseDepth, dx, dy, coverage, aspect, symmetry, cls};
        float targetDepth = TeacherDepth(u, v, lum, baseDepth, coverage, aspect, symmetry, cls);
        float targetNormalX = Clamp01(0.5f - dx * 1.5f);
        float targetConfidence = Clamp01(0.28f + symmetry * 0.35f + coverage * 0.25f + (cls > 0.75f ? 0.12f : 0.02f));
        r.y = {targetDepth, targetNormalX, targetConfidence};
        rows.push_back(r);
    }
    return rows;
}

static void Forward(const LearnedShapeModelWeights& w, const TrainingRow& row, float hiddenRaw[LearnedShapeModelWeights::HiddenUnits], float hidden[LearnedShapeModelWeights::HiddenUnits], float out[LearnedShapeModelWeights::OutputFeatures]) {
    for (int j = 0; j < LearnedShapeModelWeights::HiddenUnits; ++j) {
        float v = w.hiddenBias[static_cast<size_t>(j)];
        for (int i = 0; i < LearnedShapeModelWeights::InputFeatures; ++i) {
            v += row.x[static_cast<size_t>(i)] * w.inputToHidden[static_cast<size_t>(i * LearnedShapeModelWeights::HiddenUnits + j)];
        }
        hiddenRaw[j] = v;
        hidden[j] = Relu(v);
    }
    for (int k = 0; k < LearnedShapeModelWeights::OutputFeatures; ++k) {
        float v = w.outputBias[static_cast<size_t>(k)];
        for (int j = 0; j < LearnedShapeModelWeights::HiddenUnits; ++j) {
            v += hidden[j] * w.hiddenToOutput[static_cast<size_t>(j * LearnedShapeModelWeights::OutputFeatures + k)];
        }
        out[k] = Sigmoid(v);
    }
}

static float Loss(const LearnedShapeModelWeights& w, const std::vector<TrainingRow>& rows, size_t begin, size_t end) {
    if (begin >= end) return 0.0f;
    double sum = 0.0;
    for (size_t n = begin; n < end; ++n) {
        float raw[LearnedShapeModelWeights::HiddenUnits];
        float hidden[LearnedShapeModelWeights::HiddenUnits];
        float out[LearnedShapeModelWeights::OutputFeatures];
        Forward(w, rows[n], raw, hidden, out);
        for (int k = 0; k < LearnedShapeModelWeights::OutputFeatures; ++k) {
            float e = out[k] - rows[n].y[static_cast<size_t>(k)];
            sum += e * e;
        }
    }
    return static_cast<float>(sum / static_cast<double>((end - begin) * LearnedShapeModelWeights::OutputFeatures));
}

static void TrainOneRow(LearnedShapeModelWeights& w, const TrainingRow& row, float lr) {
    float hiddenRaw[LearnedShapeModelWeights::HiddenUnits];
    float hidden[LearnedShapeModelWeights::HiddenUnits];
    float out[LearnedShapeModelWeights::OutputFeatures];
    Forward(w, row, hiddenRaw, hidden, out);

    float dOut[LearnedShapeModelWeights::OutputFeatures];
    for (int k = 0; k < LearnedShapeModelWeights::OutputFeatures; ++k) {
        float e = out[k] - row.y[static_cast<size_t>(k)];
        dOut[k] = 2.0f * e * out[k] * (1.0f - out[k]);
    }

    float dHidden[LearnedShapeModelWeights::HiddenUnits] = {};
    for (int j = 0; j < LearnedShapeModelWeights::HiddenUnits; ++j) {
        float s = 0.0f;
        for (int k = 0; k < LearnedShapeModelWeights::OutputFeatures; ++k) {
            s += dOut[k] * w.hiddenToOutput[static_cast<size_t>(j * LearnedShapeModelWeights::OutputFeatures + k)];
        }
        dHidden[j] = s * ReluGrad(hiddenRaw[j]);
    }

    for (int j = 0; j < LearnedShapeModelWeights::HiddenUnits; ++j) {
        for (int k = 0; k < LearnedShapeModelWeights::OutputFeatures; ++k) {
            size_t idx = static_cast<size_t>(j * LearnedShapeModelWeights::OutputFeatures + k);
            w.hiddenToOutput[idx] -= lr * dOut[k] * hidden[j];
        }
    }
    for (int k = 0; k < LearnedShapeModelWeights::OutputFeatures; ++k) {
        w.outputBias[static_cast<size_t>(k)] -= lr * dOut[k];
    }
    for (int i = 0; i < LearnedShapeModelWeights::InputFeatures; ++i) {
        for (int j = 0; j < LearnedShapeModelWeights::HiddenUnits; ++j) {
            size_t idx = static_cast<size_t>(i * LearnedShapeModelWeights::HiddenUnits + j);
            w.inputToHidden[idx] -= lr * dHidden[j] * row.x[static_cast<size_t>(i)];
        }
    }
    for (int j = 0; j < LearnedShapeModelWeights::HiddenUnits; ++j) {
        w.hiddenBias[static_cast<size_t>(j)] -= lr * dHidden[j];
    }
}

} // namespace

LearnedShapeTrainingReport TrainLearnedShapeModel(
    const LearnedShapeTrainingOptions& options,
    LearnedShapeModelWeights* trainedWeights) {

    LearnedShapeTrainingReport report;
    int sampleCount = std::max(32, options.syntheticSamples);
    std::vector<TrainingRow> rows = BuildSyntheticRows(sampleCount);
    size_t validationCount = static_cast<size_t>(std::clamp(options.validationSplit, 0.05f, 0.45f) * static_cast<float>(rows.size()));
    size_t trainEnd = rows.size() - validationCount;

    LearnedShapeModelWeights weights = LearnedShapeModelWeights::BuiltInPrior();
    report.syntheticSamples = static_cast<int>(rows.size());
    report.trainingSamples = static_cast<int>(trainEnd);
    report.validationSamples = static_cast<int>(validationCount);
    report.epochs = std::max(1, options.epochs);
    report.initialLoss = Loss(weights, rows, 0, trainEnd);

    for (int e = 0; e < report.epochs; ++e) {
        float lr = options.learningRate * (1.0f - 0.65f * static_cast<float>(e) / static_cast<float>(report.epochs));
        for (size_t i = 0; i < trainEnd; ++i) TrainOneRow(weights, rows[i], lr);
    }

    report.finalTrainingLoss = Loss(weights, rows, 0, trainEnd);
    report.finalValidationLoss = Loss(weights, rows, trainEnd, rows.size());
    report.outputWeightsPath = options.outputWeightsPath;

    if (trainedWeights) *trainedWeights = weights;
    if (!options.outputWeightsPath.empty()) {
        std::string error;
        std::filesystem::create_directories(options.outputWeightsPath.parent_path());
        if (!SaveLearnedShapeModelWeights(weights, options.outputWeightsPath, &error)) {
            report.message = error;
            return report;
        }
    }

    report.ok = true;
    report.message = "Trained local learned shape model on synthetic teacher data.";
    return report;
}

std::string LearnedShapeTrainingReport::ToMarkdown() const {
    std::ostringstream o;
    o << "# Learned Shape Training Report\n\n";
    o << "| Metric | Value |\n|---|---:|\n";
    o << "| OK | " << (ok ? "yes" : "no") << " |\n";
    o << "| Synthetic samples | " << syntheticSamples << " |\n";
    o << "| Training samples | " << trainingSamples << " |\n";
    o << "| Validation samples | " << validationSamples << " |\n";
    o << "| Epochs | " << epochs << " |\n";
    o << "| Initial loss | " << initialLoss << " |\n";
    o << "| Final training loss | " << finalTrainingLoss << " |\n";
    o << "| Final validation loss | " << finalValidationLoss << " |\n";
    o << "| Output weights | " << outputWeightsPath.u8string() << " |\n";
    o << "\n" << message << "\n";
    return o.str();
}

std::string LearnedShapeTrainingReport::ToJson() const {
    std::ostringstream o;
    o << "{\n";
    o << "  \"ok\": " << (ok ? "true" : "false") << ",\n";
    o << "  \"message\": \"" << EscapeJson(message) << "\",\n";
    o << "  \"syntheticSamples\": " << syntheticSamples << ",\n";
    o << "  \"trainingSamples\": " << trainingSamples << ",\n";
    o << "  \"validationSamples\": " << validationSamples << ",\n";
    o << "  \"epochs\": " << epochs << ",\n";
    o << "  \"initialLoss\": " << initialLoss << ",\n";
    o << "  \"finalTrainingLoss\": " << finalTrainingLoss << ",\n";
    o << "  \"finalValidationLoss\": " << finalValidationLoss << ",\n";
    o << "  \"outputWeightsPath\": \"" << EscapeJson(outputWeightsPath.u8string()) << "\"\n";
    o << "}";
    return o.str();
}

} // namespace make3d
