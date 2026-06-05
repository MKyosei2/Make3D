#pragma once

#include "Make3DImageDetailPack.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace make3d {

struct FullProductionAssetOptions {
    FinalGameAssetOptions finalOptions;
    int textureSize = 128;
};

struct FullProductionAssetResult {
    bool ok = false;
    std::string message;
    FinalGameAssetResult finalAsset;
    DeliveryPackResult delivery;
    AssetAuditPackResult audit;
    ScenePackResult scene;
    QualityPackResult quality;
    GeometryRefinementPackResult geometry;
    ProductionQualityPackResult production;
    ImageDetailPackResult detail;
    std::filesystem::path acceptanceReportPath;
    std::filesystem::path outputIndexPath;
};

inline bool WriteFullPipelineText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return true;
}

inline bool WriteFullAcceptanceReport(const std::filesystem::path& path, const FullProductionAssetResult& r) {
    std::ostringstream o;
    o << "# Make3D Full Production Asset Acceptance\n\n";
    o << "- Final asset: " << (r.finalAsset.ok ? "ok" : "failed") << "\n";
    o << "- Delivery pack: " << (r.delivery.ok ? "ok" : "failed") << "\n";
    o << "- Audit pack: " << (r.audit.ok ? "ok" : "failed") << "\n";
    o << "- Scene pack: " << (r.scene.ok ? "ok" : "failed") << "\n";
    o << "- Quality pack: " << (r.quality.ok ? "ok" : "failed") << "\n";
    o << "- Geometry refinement: " << (r.geometry.ok ? "ok" : "failed") << "\n";
    o << "- Production quality: " << (r.production.ok ? "ok" : "failed") << "\n";
    o << "- Image detail: " << (r.detail.ok ? "ok" : "failed") << "\n";
    o << "- glTF: " << r.finalAsset.complete.asset.gltfPath.generic_string() << "\n";
    o << "- repaired mesh: " << r.geometry.repairedMeshPath.generic_string() << "\n";
    o << "- readiness score: " << r.production.readinessScorePath.generic_string() << "\n";
    o << "- detail quality: " << r.detail.detailQualityPath.generic_string() << "\n";
    o << "\nResult: full_production_asset_ready\n";
    return WriteFullPipelineText(path, o.str());
}

inline bool WriteFullOutputIndex(const std::filesystem::path& path, const FullProductionAssetResult& r) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"full_production_asset_output_index\",\n";
    o << "  \"gltf\": \"" << r.finalAsset.complete.asset.gltfPath.generic_string() << "\",\n";
    o << "  \"obj\": \"" << r.finalAsset.complete.asset.objPath.generic_string() << "\",\n";
    o << "  \"collision\": \"" << r.finalAsset.complete.asset.collisionObjPath.generic_string() << "\",\n";
    o << "  \"lod1\": \"" << r.finalAsset.complete.asset.lodObjPath.generic_string() << "\",\n";
    o << "  \"lod2\": \"" << r.finalAsset.lod2Path.generic_string() << "\",\n";
    o << "  \"deliveryManifest\": \"" << r.delivery.deliveryManifestPath.generic_string() << "\",\n";
    o << "  \"assetCatalog\": \"" << r.audit.assetCatalogPath.generic_string() << "\",\n";
    o << "  \"scenePreview\": \"" << r.scene.previewPath.generic_string() << "\",\n";
    o << "  \"qualityPackage\": \"" << r.quality.gltfPackagePath.generic_string() << "\",\n";
    o << "  \"repairedMesh\": \"" << r.geometry.repairedMeshPath.generic_string() << "\",\n";
    o << "  \"productionScore\": \"" << r.production.readinessScorePath.generic_string() << "\",\n";
    o << "  \"imageDetail\": \"" << r.detail.detailQualityPath.generic_string() << "\",\n";
    o << "  \"acceptanceReport\": \"" << r.acceptanceReportPath.generic_string() << "\"\n";
    o << "}\n";
    return WriteFullPipelineText(path, o.str());
}

inline FullProductionAssetResult BuildFullProductionAssetFromFrames(const std::vector<std::filesystem::path>& frames, const std::filesystem::path& outputDir, const FullProductionAssetOptions& options = FullProductionAssetOptions{}) {
    FullProductionAssetResult r;
    r.finalAsset = BuildFinalGameAssetFromFrames(frames, outputDir, options.finalOptions);
    if (!r.finalAsset.ok) { r.message = r.finalAsset.message; return r; }
    r.delivery = BuildDeliveryPack(r.finalAsset, outputDir);
    if (!r.delivery.ok) { r.message = "delivery pack failed"; return r; }
    r.audit = BuildAssetAuditPack(r.finalAsset, r.delivery, outputDir);
    if (!r.audit.ok) { r.message = "asset audit pack failed"; return r; }
    r.scene = BuildScenePack(r.finalAsset, r.audit, outputDir);
    if (!r.scene.ok) { r.message = "scene pack failed"; return r; }
    r.quality = BuildQualityPack(r.finalAsset, frames, outputDir, options.textureSize);
    if (!r.quality.ok) { r.message = "quality pack failed"; return r; }
    r.geometry = BuildGeometryRefinementPack(r.finalAsset, r.quality, frames, outputDir, options.textureSize);
    if (!r.geometry.ok) { r.message = "geometry refinement pack failed"; return r; }
    r.production = BuildProductionQualityPack(r.finalAsset, r.quality, r.geometry, outputDir);
    if (!r.production.ok) { r.message = "production quality pack failed"; return r; }
    r.detail = BuildImageDetailPack(r.finalAsset, r.geometry, frames, outputDir, options.textureSize);
    if (!r.detail.ok) { r.message = "image detail pack failed"; return r; }
    r.acceptanceReportPath = outputDir / "MAKE3D_FULL_ACCEPTANCE.md";
    r.outputIndexPath = outputDir / "make3d_full_output_index.json";
    bool a = WriteFullAcceptanceReport(r.acceptanceReportPath, r);
    bool b = WriteFullOutputIndex(r.outputIndexPath, r);
    r.ok = a && b;
    r.message = r.ok ? "Full production asset pipeline finished." : "full production report writing failed";
    return r;
}

inline FullProductionAssetResult BuildFullProductionAssetFromImage(const std::filesystem::path& image, const std::filesystem::path& outputDir, const FullProductionAssetOptions& options = FullProductionAssetOptions{}) {
    return BuildFullProductionAssetFromFrames({image}, outputDir, options);
}

} // namespace make3d
