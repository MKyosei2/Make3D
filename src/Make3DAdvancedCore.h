#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace make3d {

namespace fs = std::filesystem;

struct ImageRGBA {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels; // RGBA, 8-bit per channel
};

struct DepthImage {
    int width = 0;
    int height = 0;
    std::vector<float> values; // normalized 0..1, foreground only
};

struct MeshData {
    std::vector<float> positions; // xyz
    std::vector<float> normals;   // xyz
    std::vector<float> uvs;       // uv
    std::vector<std::uint32_t> indices;
};

enum class ReconstructionMode {
    Auto,
    ReliefSurface,
    SilhouetteVolume,
    HybridVolume
};

enum class QualityPreset {
    Draft,
    Standard,
    Detailed
};

struct AdvancedOptions {
    ReconstructionMode mode = ReconstructionMode::Auto;
    QualityPreset quality = QualityPreset::Standard;
    int maxGridResolution = 192;
    int volumeRadialSegments = 20;
    float xyScale = 2.2f;
    float depthScale = 1.15f;
    float baseThickness = 0.10f;
    bool exportObj = true;
    bool exportGltf = true;
    bool writeDebugImages = true;
};

struct ReconstructionReport {
    int imageWidth = 0;
    int imageHeight = 0;
    int foregroundPixels = 0;
    float foregroundCoverage = 0.0f;
    float depthMin = 0.0f;
    float depthMax = 0.0f;
    float depthMean = 0.0f;
    int vertices = 0;
    int triangles = 0;
    bool usedProvidedDepth = false;
    bool watertightCandidate = false;
    std::string reconstructionMode;
    fs::path objPath;
    fs::path gltfPath;
    fs::path reportPath;
    std::vector<std::string> warnings;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

struct BuildOutput {
    bool ok = false;
    std::string message;
    MeshData mesh;
    ReconstructionReport report;
};

std::optional<ImageRGBA> LoadImageRGBA(const fs::path& path, std::string* error = nullptr);
std::optional<DepthImage> LoadDepthImage(const fs::path& path, std::string* error = nullptr);

std::vector<std::uint8_t> BuildForegroundMask(const ImageRGBA& image, ReconstructionReport* report = nullptr);
DepthImage EstimateDepthFromSingleImage(const ImageRGBA& image, const std::vector<std::uint8_t>& mask, ReconstructionReport* report = nullptr);
DepthImage PrepareDepth(const ImageRGBA& color, const std::optional<DepthImage>& providedDepth, const std::vector<std::uint8_t>& mask, const AdvancedOptions& options, ReconstructionReport* report = nullptr);

MeshData ReconstructMesh(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const AdvancedOptions& options, ReconstructionReport* report = nullptr);
void RecomputeNormals(MeshData& mesh);
void NormalizeMesh(MeshData& mesh, float targetHeight = 2.0f);

bool ExportOBJ(const MeshData& mesh, const fs::path& objPath, const std::string& materialTextureName, std::string* error = nullptr);
bool ExportGLTF(const MeshData& mesh, const fs::path& gltfPath, std::string* error = nullptr);
bool SaveDebugPPM(const fs::path& path, int width, int height, const std::vector<std::uint8_t>& rgb, std::string* error = nullptr);

BuildOutput BuildModelFromImage(const fs::path& colorPath, const std::optional<fs::path>& depthPath, const fs::path& outputDir, const AdvancedOptions& options);

const char* ToString(ReconstructionMode mode);
const char* ToString(QualityPreset preset);

} // namespace make3d
