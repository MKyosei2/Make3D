#include "Make3DBenchmark.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace make3d {
namespace {

using Clock = std::chrono::steady_clock;

int VertexCount(const MeshData& mesh) {
    return static_cast<int>(mesh.positions.size() / 3);
}

int TriangleCount(const MeshData& mesh) {
    return static_cast<int>(mesh.indices.size() / 3);
}

std::string SafePathString(const fs::path& path) {
    try { return path.u8string(); } catch (...) { return "<path>"; }
}

std::string EscapeJson(const std::string& value) {
    std::ostringstream oss;
    for (char c : value) {
        switch (c) {
            case '\\': oss << "\\\\"; break;
            case '"': oss << "\\\""; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default: oss << c; break;
        }
    }
    return oss.str();
}

void AddStage(std::vector<StageTiming>& stages,
              const std::string& name,
              const Clock::time_point& start,
              const Clock::time_point& end,
              const MeshData* mesh = nullptr,
              const std::string& note = std::string()) {
    StageTiming stage;
    stage.name = name;
    stage.milliseconds = std::chrono::duration<double, std::milli>(end - start).count();
    if (mesh) {
        stage.vertices = VertexCount(*mesh);
        stage.triangles = TriangleCount(*mesh);
    }
    stage.note = note;
    stages.push_back(stage);
}

} // namespace

AdvancedOptions MakePreviewOptions(const AdvancedOptions& finalOptions) {
    AdvancedOptions preview = finalOptions;
    preview.maxGridResolution = std::clamp(finalOptions.maxGridResolution / 2, 32, 96);
    preview.volumeRadialSegments = std::clamp(finalOptions.volumeRadialSegments / 2, 8, 16);
    preview.exportObj = false;
    preview.exportGltf = false;
    preview.writeDebugImages = false;
    return preview;
}

MeshData BuildPreviewMeshForDisplay(
    const ImageRGBA& color,
    const DepthImage& depth,
    const std::vector<std::uint8_t>& mask,
    const AdvancedOptions& finalOptions,
    ReconstructionReport* report) {
    AdvancedOptions previewOptions = MakePreviewOptions(finalOptions);
    MeshData mesh = ReconstructMesh(color, depth, mask, previewOptions, report);
    RecomputeNormals(mesh);
    NormalizeMesh(mesh, 2.0f);
    if (report) {
        report->reconstructionMode = "ImageDrivenPreviewMesh";
        report->vertices = VertexCount(mesh);
        report->triangles = TriangleCount(mesh);
        report->watertightCandidate = true;
    }
    return mesh;
}

std::string StageTimingsToMarkdown(const std::vector<StageTiming>& stages) {
    std::ostringstream oss;
    oss << "## Stage profiler\n\n";
    oss << "| Stage | Milliseconds | Vertices | Triangles | Note |\n";
    oss << "|---|---:|---:|---:|---|\n";
    oss << std::fixed << std::setprecision(3);
    for (const StageTiming& s : stages) {
        oss << "| " << s.name << " | " << s.milliseconds << " | " << s.vertices << " | " << s.triangles << " | " << s.note << " |\n";
    }
    return oss.str();
}

std::string StageTimingsToJson(const std::vector<StageTiming>& stages) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "[\n";
    for (size_t i = 0; i < stages.size(); ++i) {
        const StageTiming& s = stages[i];
        oss << "  { \"name\": \"" << EscapeJson(s.name) << "\", \"milliseconds\": " << s.milliseconds
            << ", \"vertices\": " << s.vertices << ", \"triangles\": " << s.triangles
            << ", \"note\": \"" << EscapeJson(s.note) << "\" }";
        if (i + 1 < stages.size()) oss << ",";
        oss << "\n";
    }
    oss << "]";
    return oss.str();
}

ProfiledBuildOutput BuildProfiledModelFromImage(
    const fs::path& colorPath,
    const std::optional<fs::path>& depthPath,
    const fs::path& outputDir,
    const AdvancedOptions& options) {
    ProfiledBuildOutput out;
    std::string error;
    auto totalStart = Clock::now();

    auto start = Clock::now();
    auto color = LoadImageRGBA(colorPath, &error);
    auto end = Clock::now();
    AddStage(out.stages, "load_color", start, end, nullptr, SafePathString(colorPath));
    if (!color) { out.message = error; return out; }

    start = Clock::now();
    std::optional<DepthImage> providedDepth;
    if (depthPath) providedDepth = LoadDepthImage(*depthPath, &error);
    end = Clock::now();
    AddStage(out.stages, "load_depth", start, end, nullptr, depthPath ? SafePathString(*depthPath) : "not provided");

    out.report.imageWidth = color->width;
    out.report.imageHeight = color->height;

    start = Clock::now();
    std::vector<std::uint8_t> mask = BuildForegroundMask(*color, &out.report);
    end = Clock::now();
    AddStage(out.stages, "foreground_mask", start, end, nullptr, "foregroundPixels=" + std::to_string(out.report.foregroundPixels));
    if (out.report.foregroundPixels == 0) { out.message = "Foreground extraction failed."; return out; }

    start = Clock::now();
    DepthImage depth = PrepareDepth(*color, providedDepth, mask, options, &out.report);
    end = Clock::now();
    AddStage(out.stages, "depth_prepare", start, end, nullptr, providedDepth ? "provided" : "estimated");

    ReconstructionReport previewReport = out.report;
    start = Clock::now();
    out.previewMesh = BuildPreviewMeshForDisplay(*color, depth, mask, options, &previewReport);
    end = Clock::now();
    AddStage(out.stages, "preview_mesh", start, end, &out.previewMesh, "reduced grid for interactive preview");

    start = Clock::now();
    out.finalMesh = ReconstructMesh(*color, depth, mask, options, &out.report);
    RecomputeNormals(out.finalMesh);
    NormalizeMesh(out.finalMesh, 2.0f);
    end = Clock::now();
    AddStage(out.stages, "final_mesh", start, end, &out.finalMesh, "full-quality export mesh");

    if (out.finalMesh.positions.empty() || out.finalMesh.indices.empty()) {
        out.message = "Mesh reconstruction failed.";
        return out;
    }

    out.report.reconstructionMode = "ImageDrivenDetailedMesh";
    out.report.vertices = VertexCount(out.finalMesh);
    out.report.triangles = TriangleCount(out.finalMesh);
    out.report.watertightCandidate = true;

    std::error_code ec;
    fs::create_directories(outputDir, ec);
    out.objPath = outputDir / "make3d_advanced.obj";
    out.gltfPath = outputDir / "make3d_advanced.gltf";
    out.reportPath = outputDir / "make3d_report.md";
    out.benchmarkJsonPath = outputDir / "make3d_benchmark.json";
    out.benchmarkMarkdownPath = outputDir / "make3d_benchmark.md";

    start = Clock::now();
    if (options.exportObj && !ExportOBJ(out.finalMesh, out.objPath, "", &error)) { out.message = error; return out; }
    end = Clock::now();
    AddStage(out.stages, "export_obj", start, end, &out.finalMesh, options.exportObj ? SafePathString(out.objPath) : "disabled");

    start = Clock::now();
    if (options.exportGltf && !ExportGLTF(out.finalMesh, out.gltfPath, &error)) { out.message = error; return out; }
    end = Clock::now();
    AddStage(out.stages, "export_gltf", start, end, &out.finalMesh, options.exportGltf ? SafePathString(out.gltfPath) : "disabled");

    out.report.objPath = out.objPath;
    out.report.gltfPath = out.gltfPath;
    out.report.reportPath = out.reportPath;

    start = Clock::now();
    {
        std::ofstream md(out.reportPath, std::ios::binary);
        md << out.report.ToMarkdown() << "\n" << StageTimingsToMarkdown(out.stages);
        std::ofstream benchMd(out.benchmarkMarkdownPath, std::ios::binary);
        benchMd << "# Make3D benchmark\n\n" << StageTimingsToMarkdown(out.stages);
        std::ofstream benchJson(out.benchmarkJsonPath, std::ios::binary);
        benchJson << "{\n"
                  << "  \"imageWidth\": " << out.report.imageWidth << ",\n"
                  << "  \"imageHeight\": " << out.report.imageHeight << ",\n"
                  << "  \"previewVertices\": " << VertexCount(out.previewMesh) << ",\n"
                  << "  \"previewTriangles\": " << TriangleCount(out.previewMesh) << ",\n"
                  << "  \"finalVertices\": " << VertexCount(out.finalMesh) << ",\n"
                  << "  \"finalTriangles\": " << TriangleCount(out.finalMesh) << ",\n"
                  << "  \"stages\": " << StageTimingsToJson(out.stages) << "\n"
                  << "}\n";
    }
    end = Clock::now();
    AddStage(out.stages, "write_reports", start, end, nullptr, "report + benchmark files");

    if (options.writeDebugImages) {
        start = Clock::now();
        std::vector<std::uint8_t> maskRgb(static_cast<size_t>(color->width) * color->height * 3, 0);
        std::vector<std::uint8_t> depthRgb(static_cast<size_t>(color->width) * color->height * 3, 0);
        for (int i = 0; i < color->width * color->height; ++i) {
            std::uint8_t m = mask[static_cast<size_t>(i)] ? 255 : 0;
            float dv = depth.values.empty() ? 0.0f : std::clamp(depth.values[static_cast<size_t>(i)], 0.0f, 1.0f);
            std::uint8_t d = static_cast<std::uint8_t>(dv * 255.0f);
            maskRgb[static_cast<size_t>(i) * 3 + 0] = m;
            maskRgb[static_cast<size_t>(i) * 3 + 1] = m;
            maskRgb[static_cast<size_t>(i) * 3 + 2] = m;
            depthRgb[static_cast<size_t>(i) * 3 + 0] = d;
            depthRgb[static_cast<size_t>(i) * 3 + 1] = d;
            depthRgb[static_cast<size_t>(i) * 3 + 2] = d;
        }
        SaveDebugPPM(outputDir / "debug_mask.ppm", color->width, color->height, maskRgb, nullptr);
        SaveDebugPPM(outputDir / "debug_depth.ppm", color->width, color->height, depthRgb, nullptr);
        end = Clock::now();
        AddStage(out.stages, "debug_images", start, end, nullptr, "mask/depth ppm");
    }

    auto totalEnd = Clock::now();
    AddStage(out.stages, "total", totalStart, totalEnd, &out.finalMesh, "end-to-end profiled build");

    // Rewrite benchmark after late stages have been appended.
    {
        std::ofstream benchJson(out.benchmarkJsonPath, std::ios::binary);
        benchJson << "{\n"
                  << "  \"imageWidth\": " << out.report.imageWidth << ",\n"
                  << "  \"imageHeight\": " << out.report.imageHeight << ",\n"
                  << "  \"previewVertices\": " << VertexCount(out.previewMesh) << ",\n"
                  << "  \"previewTriangles\": " << TriangleCount(out.previewMesh) << ",\n"
                  << "  \"finalVertices\": " << VertexCount(out.finalMesh) << ",\n"
                  << "  \"finalTriangles\": " << TriangleCount(out.finalMesh) << ",\n"
                  << "  \"stages\": " << StageTimingsToJson(out.stages) << "\n"
                  << "}\n";
        std::ofstream benchMd(out.benchmarkMarkdownPath, std::ios::binary);
        benchMd << "# Make3D benchmark\n\n" << StageTimingsToMarkdown(out.stages);
    }

    out.ok = true;
    out.message = "Advanced reconstruction finished with separated preview/final meshes and benchmark output.";
    return out;
}

} // namespace make3d
