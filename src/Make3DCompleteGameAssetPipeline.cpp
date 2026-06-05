#include "Make3DCompleteGameAssetPipeline.h"

#include <fstream>
#include <sstream>

namespace make3d {

namespace {

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

static bool ExportTexturedObj(const MeshData& mesh, const std::filesystem::path& objPath, const std::string& textureName, std::string* error) {
    std::filesystem::create_directories(objPath.parent_path());
    std::ofstream obj(objPath, std::ios::binary);
    if (!obj) { if (error) *error = "Failed to open textured OBJ."; return false; }
    auto mtlPath = objPath; mtlPath.replace_extension(".mtl");
    obj << "mtllib " << mtlPath.filename().generic_string() << "\n";
    obj << "o Make3DCompleteGameAsset\n";
    for (size_t i=0;i+2<mesh.positions.size();i+=3) obj << "v " << mesh.positions[i] << " " << mesh.positions[i+1] << " " << mesh.positions[i+2] << "\n";
    for (size_t i=0;i+1<mesh.uvs.size();i+=2) obj << "vt " << mesh.uvs[i] << " " << mesh.uvs[i+1] << "\n";
    for (size_t i=0;i+2<mesh.normals.size();i+=3) obj << "vn " << mesh.normals[i] << " " << mesh.normals[i+1] << " " << mesh.normals[i+2] << "\n";
    obj << "usemtl Make3DProjectedTexture\n";
    for (size_t i=0;i+2<mesh.indices.size();i+=3) {
        auto a=mesh.indices[i]+1, b=mesh.indices[i+1]+1, c=mesh.indices[i+2]+1;
        obj << "f " << a << "/" << a << "/" << a << " " << b << "/" << b << "/" << b << " " << c << "/" << c << "/" << c << "\n";
    }
    std::ofstream mtl(mtlPath, std::ios::binary);
    if (!mtl) { if (error) *error = "Failed to open textured MTL."; return false; }
    mtl << "newmtl Make3DProjectedTexture\nKa 1 1 1\nKd 1 1 1\nKs 0 0 0\nNs 1\nmap_Kd " << textureName << "\n";
    return true;
}

static void WriteGuide(const CompleteGameAssetResult& r) {
    std::ofstream g(r.importGuidePath, std::ios::binary);
    g << "# Make3D Import Guide\n\n";
    g << "Open first: " << r.texturedObjPath.generic_string() << "\n\n";
    g << "Fallback glTF: " << r.base.gltfPath.generic_string() << "\n";
    g << "Collider proxy: " << r.base.colliderObjPath.generic_string() << "\n";
    g << "LOD proxy: " << r.base.lodObjPath.generic_string() << "\n\n";
    g << "Use meter scale, bottom pivot, collider OBJ for simple collision, and LOD proxy for distance view.\n";
}

static void WriteUnityMetadata(const CompleteGameAssetResult& r) {
    std::ofstream u(r.unityMetadataPath, std::ios::binary);
    u << "{\n";
    u << "  \"assetName\": \"" << EscapeJson(r.base.metadata.assetName) << "\",\n";
    u << "  \"assetType\": \"" << EscapeJson(ToString(r.base.metadata.assetType)) << "\",\n";
    u << "  \"unitScaleMeters\": " << r.base.metadata.unitScaleMeters << ",\n";
    u << "  \"visualMesh\": \"" << EscapeJson(r.texturedObjPath.filename().generic_string()) << "\",\n";
    u << "  \"colliderMesh\": \"" << EscapeJson(r.base.colliderObjPath.filename().generic_string()) << "\",\n";
    u << "  \"lodProxyMesh\": \"" << EscapeJson(r.base.lodObjPath.filename().generic_string()) << "\"\n";
    u << "}\n";
}

}

CompleteGameAssetResult BuildCompleteGameAsset(const ImageRGBA& color, const DepthImage& depth, const std::vector<std::uint8_t>& mask, const std::filesystem::path& outputDir, const CompleteGameAssetOptions& options) {
    CompleteGameAssetResult r;
    std::filesystem::create_directories(outputDir);
    r.base = BuildGenericGameAsset(color, depth, mask, outputDir / "base", options.generator);
    if (!r.base.ok) { r.message = r.base.message; return r; }
    r.repairedMesh = r.base.mesh;
    if (options.repairMesh) r.repairReport = RepairGameAssetMesh(r.repairedMesh, options.regeneratePlanarUv);
    r.finalValidation = ValidateGameAssetMesh(r.repairedMesh);
    if (!r.finalValidation.ok) { r.message = "Complete game asset final validation failed."; return r; }
    std::string error;
    r.texturePath = outputDir / "projected_texture.ppm";
    if (options.writeProjectedTexture && !WriteProjectedTexturePPM(color, mask, r.texturePath, options.textureSize, &error)) { r.message = error; return r; }
    r.texturedObjPath = outputDir / "make3d_complete_textured.obj";
    if (options.writeTexturedObj && !ExportTexturedObj(r.repairedMesh, r.texturedObjPath, r.texturePath.filename().generic_string(), &error)) { r.message = error; return r; }
    r.completeReportPath = outputDir / "complete_game_asset_report.md";
    r.completeManifestPath = outputDir / "complete_game_asset_manifest.json";
    r.importGuidePath = outputDir / "IMPORT_GUIDE.md";
    r.unityMetadataPath = outputDir / "unity_import_metadata.json";
    std::ofstream report(r.completeReportPath, std::ios::binary);
    report << "# Make3D Complete Game Asset Report\n\n" << r.base.classification.ToMarkdown() << "\n" << r.repairReport.ToMarkdown() << "\n" << r.finalValidation.ToMarkdown() << "\n";
    r.ok = true;
    r.message = "Complete game asset pipeline finished.";
    std::ofstream manifest(r.completeManifestPath, std::ios::binary);
    manifest << "{\n";
    manifest << "  \"ok\": true,\n";
    manifest << "  \"assetType\": \"" << EscapeJson(ToString(r.base.classification.assetType)) << "\",\n";
    manifest << "  \"texturedObj\": \"" << EscapeJson(r.texturedObjPath.generic_string()) << "\",\n";
    manifest << "  \"projectedTexture\": \"" << EscapeJson(r.texturePath.generic_string()) << "\",\n";
    manifest << "  \"baseManifest\": \"" << EscapeJson(r.base.manifestPath.generic_string()) << "\"\n";
    manifest << "}\n";
    if (options.writeImportGuide) WriteGuide(r);
    if (options.writeUnityMetadata) WriteUnityMetadata(r);
    return r;
}

} // namespace make3d
