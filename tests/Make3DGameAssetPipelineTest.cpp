#include "Make3DImageDetailPack.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int Fail(const std::string& message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

static bool SaveTga(const fs::path& path, int w, int h, const std::vector<unsigned char>& rgba) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file || rgba.size() != static_cast<size_t>(w) * h * 4) return false;
    unsigned char header[18] = {};
    header[2] = 2;
    header[12] = static_cast<unsigned char>(w & 0xFF);
    header[13] = static_cast<unsigned char>((w >> 8) & 0xFF);
    header[14] = static_cast<unsigned char>(h & 0xFF);
    header[15] = static_cast<unsigned char>((h >> 8) & 0xFF);
    header[16] = 32;
    header[17] = 8 | 0x20;
    file.write(reinterpret_cast<const char*>(header), sizeof(header));
    for (int i = 0; i < w * h; ++i) {
        size_t p = static_cast<size_t>(i) * 4;
        unsigned char bgra[4] = {rgba[p + 2], rgba[p + 1], rgba[p + 0], rgba[p + 3]};
        file.write(reinterpret_cast<const char*>(bgra), 4);
    }
    return true;
}

static fs::path MakeBoxInput(const fs::path& dir, const char* name, int dx) {
    constexpr int W = 128;
    constexpr int H = 128;
    std::vector<unsigned char> pixels(static_cast<size_t>(W) * H * 4, 0);
    auto set = [&](int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
        x += dx;
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        size_t p = (static_cast<size_t>(y) * W + x) * 4;
        pixels[p + 0] = r;
        pixels[p + 1] = g;
        pixels[p + 2] = b;
        pixels[p + 3] = a;
    };
    for (int y = 22; y < 116; ++y) for (int x = 30; x < 98; ++x) set(x, y, 170, 170, 180, 255);
    for (int y = 14; y < 28; ++y) for (int x = 24; x < 104; ++x) set(x, y, 120, 80, 70, 255);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 3; ++col) {
            int x0 = 40 + col * 18;
            int y0 = 36 + row * 16;
            for (int y = y0; y < y0 + 7; ++y) for (int x = x0; x < x0 + 8; ++x) set(x, y, 80, 130, 190, 255);
        }
    }
    for (int y = 96; y < 116; ++y) for (int x = 58; x < 70; ++x) set(x, y, 90, 60, 40, 255);
    fs::path path = dir / name;
    SaveTga(path, W, H, pixels);
    return path;
}

static bool Contains(const fs::path& path, const std::string& needle) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text.find(needle) != std::string::npos;
}

int main() {
    fs::path out = fs::current_path() / "game_asset_pipeline_test";
    fs::remove_all(out);
    fs::create_directories(out);

    make3d::FinalGameAssetOptions options;
    options.complete.generation.maskRefine.keepLargestComponentOnly = true;
    options.complete.generation.triangleBudget = 20000;
    options.complete.enginePreset = make3d::GameEnginePreset::Unity;
    options.complete.textureSize = 32;
    options.textureSize = 32;

    std::vector<fs::path> frames;
    frames.push_back(MakeBoxInput(out, "frame0.tga", 0));
    frames.push_back(MakeBoxInput(out, "frame1.tga", 1));
    make3d::FinalGameAssetResult finalAsset = make3d::BuildFinalGameAssetFromFrames(frames, out / "output", options);
    if (!finalAsset.ok) return Fail(finalAsset.message);
    make3d::DeliveryPackResult delivery = make3d::BuildDeliveryPack(finalAsset, out / "output");
    if (!delivery.ok) return Fail("delivery pack failed");
    make3d::AssetAuditPackResult audit = make3d::BuildAssetAuditPack(finalAsset, delivery, out / "output");
    if (!audit.ok) return Fail("asset audit pack failed");
    make3d::ScenePackResult scene = make3d::BuildScenePack(finalAsset, audit, out / "output");
    if (!scene.ok) return Fail("scene pack failed");
    make3d::QualityPackResult quality = make3d::BuildQualityPack(finalAsset, frames, out / "output", 32);
    if (!quality.ok) return Fail("quality pack failed");
    make3d::GeometryRefinementPackResult geometry = make3d::BuildGeometryRefinementPack(finalAsset, quality, frames, out / "output", 32);
    if (!geometry.ok) return Fail("geometry refinement pack failed");
    make3d::ProductionQualityPackResult production = make3d::BuildProductionQualityPack(finalAsset, quality, geometry, out / "output");
    if (!production.ok) return Fail("production quality pack failed");
    make3d::ImageDetailPackResult detail = make3d::BuildImageDetailPack(finalAsset, geometry, frames, out / "output", 32);
    if (!detail.ok) return Fail("image detail pack failed");

    const make3d::CompletedGameAssetResult& complete = finalAsset.complete;
    const make3d::GameAssetResult& result = complete.asset;
    if (result.assetType != make3d::GameAssetType::Building) return Fail("expected building asset type");
    if (result.parts.size() < 4) return Fail("editable building parts missing");
    if (!result.validation.hasCollisionMesh) return Fail("collision mesh missing");
    if (!result.validation.hasLodMesh) return Fail("lod mesh missing");
    if (!result.validation.hasUvs) return Fail("uv projection missing");
    if (!result.validation.gameReadyCandidate) return Fail("not game-ready candidate");
    if (!fs::exists(result.objPath)) return Fail("OBJ missing");
    if (!fs::exists(result.gltfPath)) return Fail("glTF missing");
    if (!fs::exists(result.collisionObjPath)) return Fail("collision OBJ missing");
    if (!fs::exists(result.lodObjPath)) return Fail("LOD OBJ missing");
    if (!fs::exists(result.reportPath)) return Fail("report missing");
    if (!fs::exists(complete.manifestPath)) return Fail("manifest missing");
    if (!fs::exists(complete.albedoTexturePath)) return Fail("albedo placeholder missing");
    if (!fs::exists(complete.normalTexturePath)) return Fail("normal placeholder missing");
    if (!fs::exists(complete.roughnessTexturePath)) return Fail("roughness placeholder missing");
    if (!fs::exists(complete.rigMetadataPath)) return Fail("rig metadata missing");
    if (!fs::exists(complete.enginePresetPath)) return Fail("engine preset missing");
    if (!fs::exists(finalAsset.projectedTexturePath)) return Fail("projected texture missing");
    if (!fs::exists(finalAsset.retopoProxyPath)) return Fail("retopo proxy missing");
    if (!fs::exists(finalAsset.bindingMetadataPath)) return Fail("binding metadata missing");
    if (!fs::exists(finalAsset.animationPreviewPath)) return Fail("animation preview missing");
    if (!fs::exists(finalAsset.meshCheckPath)) return Fail("mesh check missing");
    if (!fs::exists(finalAsset.frameReportPath)) return Fail("frame report missing");
    if (!fs::exists(finalAsset.lod2Path)) return Fail("LOD2 missing");
    if (!fs::exists(finalAsset.vertexInfluencesPath)) return Fail("vertex influences missing");
    if (!fs::exists(finalAsset.frameConsistencyPath)) return Fail("frame consistency missing");
    if (!fs::exists(finalAsset.textureAtlasPath)) return Fail("texture atlas missing");
    if (!fs::exists(finalAsset.runtimeChecklistPath)) return Fail("runtime checklist missing");
    if (!fs::exists(delivery.turntablePlanPath)) return Fail("turntable plan missing");
    if (!fs::exists(delivery.deliveryManifestPath)) return Fail("delivery manifest missing");
    if (!fs::exists(audit.qualityGatePath)) return Fail("quality gate missing");
    if (!fs::exists(audit.engineProfilesPath)) return Fail("engine profiles missing");
    if (!fs::exists(audit.colliderManifestPath)) return Fail("collider manifest missing");
    if (!fs::exists(audit.lodManifestPath)) return Fail("LOD manifest missing");
    if (!fs::exists(audit.assetCatalogPath)) return Fail("asset catalog missing");
    if (!fs::exists(scene.previewPath)) return Fail("scene preview missing");
    if (!fs::exists(scene.materialsPath)) return Fail("scene materials missing");
    if (!fs::exists(scene.scalePath)) return Fail("scene scale report missing");
    if (!fs::exists(scene.placementPath)) return Fail("scene placement missing");
    if (!fs::exists(quality.uvLayoutPath)) return Fail("UV layout missing");
    if (!fs::exists(quality.projectedAlbedoPath)) return Fail("projected albedo missing");
    if (!fs::exists(quality.retopoPlanPath)) return Fail("retopo plan missing");
    if (!fs::exists(quality.skinWeightsPath)) return Fail("skin weights v2 missing");
    if (!fs::exists(quality.temporalFusionPath)) return Fail("temporal fusion missing");
    if (!fs::exists(quality.facadeFeaturesPath)) return Fail("facade features missing");
    if (!fs::exists(quality.gltfPackagePath)) return Fail("glTF package manifest missing");
    if (!fs::exists(geometry.repairedMeshPath)) return Fail("repaired mesh missing");
    if (!fs::exists(geometry.normalMapPath)) return Fail("normal map missing");
    if (!fs::exists(geometry.occlusionMapPath)) return Fail("occlusion map missing");
    if (!fs::exists(geometry.meshRepairReportPath)) return Fail("mesh repair report missing");
    if (!fs::exists(geometry.uvSeamReportPath)) return Fail("UV seam report missing");
    if (!fs::exists(geometry.projectionQualityPath)) return Fail("projection quality report missing");
    if (!fs::exists(geometry.partRetopoGuidePath)) return Fail("part retopo guide missing");
    if (!fs::exists(production.tangentFramePath)) return Fail("tangent frame missing");
    if (!fs::exists(production.pbrMaterialSetPath)) return Fail("PBR material set missing");
    if (!fs::exists(production.skeletonBindingReportPath)) return Fail("skeleton binding report missing");
    if (!fs::exists(production.engineImportValidationPath)) return Fail("engine import validation missing");
    if (!fs::exists(production.readinessScorePath)) return Fail("readiness score missing");
    if (!fs::exists(production.textureBakeManifestPath)) return Fail("texture bake manifest missing");
    if (!fs::exists(detail.edgeMapPath)) return Fail("edge map missing");
    if (!fs::exists(detail.heightMapPath)) return Fail("height map missing");
    if (!fs::exists(detail.detailNormalPath)) return Fail("detail normal missing");
    if (!fs::exists(detail.displacedMeshPath)) return Fail("displaced preview mesh missing");
    if (!fs::exists(detail.featureLinesPath)) return Fail("feature lines missing");
    if (!fs::exists(detail.detailQualityPath)) return Fail("detail quality report missing");
    if (!finalAsset.meshCheck.usable) return Fail("mesh check not usable");
    if (finalAsset.retopoProxy.positions.empty()) return Fail("retopo proxy mesh empty");
    if (finalAsset.lod2Mesh.positions.empty()) return Fail("LOD2 mesh empty");
    if (geometry.repairedMesh.positions.empty()) return Fail("repaired mesh empty");
    if (detail.displacedMesh.positions.empty()) return Fail("displaced mesh empty");
    if (complete.bounds.sizeX <= 0.0f || complete.bounds.sizeY <= 0.0f || complete.bounds.sizeZ <= 0.0f) return Fail("invalid bounds");
    if (complete.joints.empty()) return Fail("pivot metadata missing");
    if (!Contains(result.reportPath, "Game-ready candidate")) return Fail("report metric missing");
    if (!Contains(complete.enginePresetPath, "Unity")) return Fail("engine preset content missing");
    if (!Contains(complete.rigMetadataPath, "pivot_center")) return Fail("pivot center missing");
    if (!Contains(finalAsset.bindingMetadataPath, "procedural binding")) return Fail("binding metadata content missing");
    if (!Contains(finalAsset.animationPreviewPath, "turntable_preview")) return Fail("animation preview content missing");
    if (!Contains(finalAsset.frameReportPath, "frameCount")) return Fail("frame report content missing");
    if (!Contains(finalAsset.vertexInfluencesPath, "influences")) return Fail("vertex influences content missing");
    if (!Contains(finalAsset.frameConsistencyPath, "consistent")) return Fail("frame consistency content missing");
    if (!Contains(finalAsset.runtimeChecklistPath, "Runtime Import Checklist")) return Fail("runtime checklist content missing");
    if (!Contains(delivery.turntablePlanPath, "turntable_preview_plan")) return Fail("turntable plan content missing");
    if (!Contains(delivery.deliveryManifestPath, "Make3DDeliveryPack")) return Fail("delivery manifest content missing");
    if (!Contains(audit.qualityGatePath, "Quality Gate")) return Fail("quality gate content missing");
    if (!Contains(audit.engineProfilesPath, "Unity")) return Fail("engine profiles content missing");
    if (!Contains(audit.colliderManifestPath, "collider_manifest")) return Fail("collider manifest content missing");
    if (!Contains(audit.lodManifestPath, "lod_manifest")) return Fail("LOD manifest content missing");
    if (!Contains(audit.assetCatalogPath, "Make3DAssetCatalog")) return Fail("asset catalog content missing");
    if (!Contains(scene.previewPath, "scene_preview_config")) return Fail("scene preview content missing");
    if (!Contains(scene.materialsPath, "scene_materials")) return Fail("scene materials content missing");
    if (!Contains(scene.scalePath, "Scale Report")) return Fail("scene scale content missing");
    if (!Contains(scene.placementPath, "scene_placement")) return Fail("scene placement content missing");
    if (!Contains(quality.uvLayoutPath, "svg")) return Fail("UV layout content missing");
    if (!Contains(quality.retopoPlanPath, "retopology_plan")) return Fail("retopo plan content missing");
    if (!Contains(quality.skinWeightsPath, "skin_weights_v2")) return Fail("skin weights content missing");
    if (!Contains(quality.temporalFusionPath, "temporal_fusion_report")) return Fail("temporal fusion content missing");
    if (!Contains(quality.facadeFeaturesPath, "facade_feature_map")) return Fail("facade features content missing");
    if (!Contains(quality.gltfPackagePath, "gltf_package_manifest")) return Fail("glTF package content missing");
    if (!Contains(geometry.meshRepairReportPath, "mesh_repair_report")) return Fail("mesh repair report content missing");
    if (!Contains(geometry.uvSeamReportPath, "uv_seam_report")) return Fail("UV seam report content missing");
    if (!Contains(geometry.projectionQualityPath, "projection_quality_report")) return Fail("projection quality content missing");
    if (!Contains(geometry.partRetopoGuidePath, "part_retopology_guide")) return Fail("part retopo guide content missing");
    if (!Contains(production.tangentFramePath, "tangent_frame_set")) return Fail("tangent frame content missing");
    if (!Contains(production.pbrMaterialSetPath, "pbr_material_set")) return Fail("PBR material content missing");
    if (!Contains(production.skeletonBindingReportPath, "skeleton_binding_report")) return Fail("skeleton binding content missing");
    if (!Contains(production.engineImportValidationPath, "engine_import_validation")) return Fail("engine import validation content missing");
    if (!Contains(production.readinessScorePath, "production_readiness_score")) return Fail("readiness score content missing");
    if (!Contains(production.textureBakeManifestPath, "texture_bake_manifest")) return Fail("texture bake content missing");
    if (!Contains(detail.featureLinesPath, "image_feature_lines")) return Fail("feature lines content missing");
    if (!Contains(detail.detailQualityPath, "image_detail_quality_report")) return Fail("detail quality content missing");

    std::cout << "[PASS] Make3D final game asset pipeline regression test\n";
    std::cout << "Review target: " << result.gltfPath.u8string() << "\n";
    std::cout << "Manifest: " << complete.manifestPath.u8string() << "\n";
    std::cout << "Retopo proxy: " << finalAsset.retopoProxyPath.u8string() << "\n";
    std::cout << "LOD2: " << finalAsset.lod2Path.u8string() << "\n";
    std::cout << "Delivery manifest: " << delivery.deliveryManifestPath.u8string() << "\n";
    std::cout << "Asset catalog: " << audit.assetCatalogPath.u8string() << "\n";
    std::cout << "Scene preview: " << scene.previewPath.u8string() << "\n";
    std::cout << "Quality pack: " << quality.gltfPackagePath.u8string() << "\n";
    std::cout << "Repaired mesh: " << geometry.repairedMeshPath.u8string() << "\n";
    std::cout << "Production score: " << production.readinessScorePath.u8string() << "\n";
    std::cout << "Image detail: " << detail.detailQualityPath.u8string() << "\n";
    return 0;
}
