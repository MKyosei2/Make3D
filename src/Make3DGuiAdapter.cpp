#include "Make3DGuiAdapter.h"
#include "Make3DGltfMaterialExporter.h"

#include <sstream>

namespace make3d::gui {

AdvancedOptions OptionsFromGuiSelection(int reconstructionIndex, int qualityIndex) {
    AdvancedOptions options;

    switch (qualityIndex) {
        case 0:
            options.quality = QualityPreset::Draft;
            options.maxGridResolution = 96;
            options.volumeRadialSegments = 12;
            options.depthScale = 0.95f;
            break;
        case 2:
            options.quality = QualityPreset::Detailed;
            options.maxGridResolution = 224;
            options.volumeRadialSegments = 28;
            options.depthScale = 1.35f;
            break;
        case 1:
        default:
            options.quality = QualityPreset::Standard;
            options.maxGridResolution = 160;
            options.volumeRadialSegments = 20;
            options.depthScale = 1.15f;
            break;
    }

    switch (reconstructionIndex) {
        case 1:
            options.mode = ReconstructionMode::ReliefSurface;
            break;
        case 2:
            options.mode = ReconstructionMode::SilhouetteVolume;
            break;
        case 3:
            options.mode = ReconstructionMode::HybridVolume;
            break;
        case 0:
        default:
            options.mode = ReconstructionMode::Auto;
            break;
    }

    return options;
}

GuiBuildSummary BuildAdvancedFromGuiRequest(const GuiBuildRequest& request) {
    GuiBuildSummary summary;
    AdvancedOptions options = OptionsFromGuiSelection(request.guiReconstructionIndex, request.guiQualityIndex);
    options.exportObj = request.exportObj;
    options.exportGltf = request.exportGltf;
    options.writeDebugImages = request.writeDebugImages;

    BuildOutput output = BuildModelFromImage(request.colorPath, request.depthPath, request.outputDir, options);
    summary.ok = output.ok;
    summary.objPath = output.report.objPath;
    summary.gltfPath = output.report.gltfPath;
    summary.reportPath = output.report.reportPath;
    summary.vertices = output.report.vertices;
    summary.triangles = output.report.triangles;

    if (!output.ok) {
        summary.message = output.message;
        return summary;
    }

    if (request.exportGltf) {
        GltfMaterialOptions material;
        material.materialName = "Make3DGeneratedMaterial";
        std::filesystem::path materialGltf = request.outputDir / "make3d_advanced_material.gltf";
        std::string materialError;
        if (ExportGLTFWithMaterial(output.mesh, materialGltf, material, &materialError)) {
            summary.gltfPath = materialGltf;
        }
    }

    std::ostringstream oss;
    oss << "Advanced reconstruction finished. "
        << "vertices=" << output.report.vertices
        << ", triangles=" << output.report.triangles
        << ", mode=" << output.report.reconstructionMode;
    if (!summary.gltfPath.empty()) oss << ", glTF=" << summary.gltfPath.u8string();
    if (!output.report.reportPath.empty()) oss << ", report=" << output.report.reportPath.u8string();
    if (!output.report.warnings.empty()) oss << ", warnings=" << output.report.warnings.size();
    summary.message = oss.str();
    return summary;
}

} // namespace make3d::gui
