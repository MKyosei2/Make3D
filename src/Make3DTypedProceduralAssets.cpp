#include "Make3DTypedProceduralAssets.h"
#include "Make3DGltfMaterialExporter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace make3d {
namespace {
constexpr float Pi = 3.14159265358979323846f;
static int VC(const MeshData& m) { return static_cast<int>(m.positions.size() / 3); }
static int TC(const MeshData& m) { return static_cast<int>(m.indices.size() / 3); }
static void V(MeshData& m, float x, float y, float z, float u, float v) {
    m.positions.push_back(x); m.positions.push_back(y); m.positions.push_back(z);
    m.normals.push_back(0.0f); m.normals.push_back(1.0f); m.normals.push_back(0.0f);
    m.uvs.push_back(u); m.uvs.push_back(v);
}
static void T(MeshData& m, int a, int b, int c) {
    if (a < 0 || b < 0 || c < 0 || a == b || b == c || c == a) return;
    m.indices.push_back(static_cast<std::uint32_t>(a));
    m.indices.push_back(static_cast<std::uint32_t>(b));
    m.indices.push_back(static_cast<std::uint32_t>(c));
}
static void Q(MeshData& m, int a, int b, int c, int d) { T(m,a,b,c); T(m,a,c,d); }
static void Box(MeshData& m, float cx, float cy, float cz, float sx, float sy, float sz) {
    int b = VC(m); float x0=cx-sx*.5f,x1=cx+sx*.5f,y0=cy-sy*.5f,y1=cy+sy*.5f,z0=cz-sz*.5f,z1=cz+sz*.5f;
    V(m,x0,y0,z0,0.0f,0.0f); V(m,x1,y0,z0,1.0f,0.0f); V(m,x1,y1,z0,1.0f,1.0f); V(m,x0,y1,z0,0.0f,1.0f);
    V(m,x0,y0,z1,0.0f,0.0f); V(m,x1,y0,z1,1.0f,0.0f); V(m,x1,y1,z1,1.0f,1.0f); V(m,x0,y1,z1,0.0f,1.0f);
    Q(m,b+0,b+1,b+2,b+3); Q(m,b+5,b+4,b+7,b+6); Q(m,b+4,b+0,b+3,b+7); Q(m,b+1,b+5,b+6,b+2); Q(m,b+3,b+2,b+6,b+7); Q(m,b+4,b+5,b+1,b+0);
}
static void Cylinder(MeshData& m, float cx, float y0, float y1, float cz, float rx, float rz, int seg) {
    seg = std::max(8, seg); int b = VC(m);
    for (int ring=0; ring<2; ++ring) { float y = ring ? y1 : y0; for (int s=0; s<seg; ++s) { float a = 2.0f*Pi*static_cast<float>(s)/static_cast<float>(seg); V(m,cx+std::cos(a)*rx,y,cz+std::sin(a)*rz,static_cast<float>(s)/static_cast<float>(seg),static_cast<float>(ring)); } }
    for (int s=0; s<seg; ++s) { int n=(s+1)%seg; Q(m,b+s,b+n,b+seg+n,b+seg+s); }
    int cb=VC(m); V(m,cx,y0,cz,0.5f,0.5f); int ct=VC(m); V(m,cx,y1,cz,0.5f,0.5f);
    for (int s=0; s<seg; ++s) { int n=(s+1)%seg; T(m,cb,b+n,b+s); T(m,ct,b+seg+s,b+seg+n); }
}
static void Finish(MeshData& m, float h) { RecomputeNormals(m); NormalizeMesh(m, h); }
static std::string Esc(const std::string& s) { std::ostringstream o; for(char c:s){ if(c=='\\')o<<"\\\\"; else if(c=='\"')o<<"\\\""; else o<<c; } return o.str(); }
}

MeshData BuildFurnitureProceduralAsset(const TypedProceduralAssetOptions& o) {
    MeshData m;
    Box(m, 0.0f, 0.78f, 0.0f, 1.25f, 0.16f, 0.72f);
    Box(m, -0.48f, 0.38f, -0.25f, 0.12f, 0.72f, 0.12f);
    Box(m,  0.48f, 0.38f, -0.25f, 0.12f, 0.72f, 0.12f);
    Box(m, -0.48f, 0.38f,  0.25f, 0.12f, 0.72f, 0.12f);
    Box(m,  0.48f, 0.38f,  0.25f, 0.12f, 0.72f, 0.12f);
    if (o.addDetails) Box(m, 0.0f, 1.15f, 0.30f, 1.30f, 0.75f, 0.12f);
    Finish(m, o.targetHeight); return m;
}

MeshData BuildMachineProceduralAsset(const TypedProceduralAssetOptions& o) {
    MeshData m;
    Box(m, 0.0f, .7f, 0.0f, 1.15f, 1.15f, .82f);
    Box(m, 0.0f, 1.34f, 0.0f, 1.0f, .18f, .74f);
    Cylinder(m, -.42f, .12f, .32f, .48f, .15f, .15f, o.radialSegments);
    Cylinder(m,  .42f, .12f, .32f, .48f, .15f, .15f, o.radialSegments);
    if (o.addDetails) { Box(m, 0.0f, .82f, .43f, .65f, .25f, .05f); Box(m, -.38f, .48f, .44f, .18f, .18f, .05f); Box(m, .38f, .48f, .44f, .18f, .18f, .05f); }
    Finish(m, o.targetHeight); return m;
}

MeshData BuildToolProceduralAsset(const TypedProceduralAssetOptions& o) {
    MeshData m;
    Cylinder(m, 0.0f, 0.0f, 1.45f, 0.0f, .08f, .08f, o.radialSegments);
    Box(m, 0.0f, 1.56f, 0.0f, .58f, .18f, .16f);
    if (o.addDetails) { Box(m, -.34f, 1.56f, 0.0f, .16f, .34f, .12f); Box(m, .34f, 1.56f, 0.0f, .16f, .34f, .12f); }
    Finish(m, o.targetHeight); return m;
}

MeshData BuildTerrainPieceProceduralAsset(const TypedProceduralAssetOptions& o) {
    MeshData m;
    const int n = 5;
    int base = VC(m);
    for (int z=0; z<n; ++z) for (int x=0; x<n; ++x) {
        float fx = (static_cast<float>(x)/static_cast<float>(n-1)-.5f)*1.8f;
        float fz = (static_cast<float>(z)/static_cast<float>(n-1)-.5f)*1.8f;
        float y = 0.10f + 0.16f*std::sin(static_cast<float>(x)*.9f) + 0.10f*std::cos(static_cast<float>(z)*1.2f);
        V(m, fx, y, fz, static_cast<float>(x)/static_cast<float>(n-1), static_cast<float>(z)/static_cast<float>(n-1));
    }
    for (int z=0; z<n-1; ++z) for (int x=0; x<n-1; ++x) { int a=base+z*n+x,b=a+1,c=a+n+1,d=a+n; Q(m,a,b,c,d); }
    Box(m, 0.0f, -0.08f, 0.0f, 1.9f, .18f, 1.9f);
    Finish(m, o.targetHeight); return m;
}

TypedProceduralAssetResult ExportTypedProceduralAsset(GameAssetType type, const std::filesystem::path& outputDir, const TypedProceduralAssetOptions& options) {
    TypedProceduralAssetResult r; r.assetType = type;
    switch (type) {
        case GameAssetType::Furniture: r.mesh = BuildFurnitureProceduralAsset(options); break;
        case GameAssetType::Machine: r.mesh = BuildMachineProceduralAsset(options); break;
        case GameAssetType::ToolOrWeapon: r.mesh = BuildToolProceduralAsset(options); break;
        case GameAssetType::TerrainPiece: r.mesh = BuildTerrainPieceProceduralAsset(options); break;
        default: r.message = "Unsupported typed procedural asset."; return r;
    }
    r.validation = ValidateGameAssetMesh(r.mesh);
    if (!r.validation.ok) { r.message = "Typed procedural asset validation failed."; return r; }
    std::filesystem::create_directories(outputDir);
    r.objPath = outputDir / "make3d_typed_asset.obj";
    r.gltfPath = outputDir / "make3d_typed_asset.gltf";
    r.reportPath = outputDir / "make3d_typed_asset_report.md";
    r.manifestPath = outputDir / "make3d_typed_asset_manifest.json";
    std::string error;
    if (!ExportOBJ(r.mesh, r.objPath, "", &error)) { r.message = error; return r; }
    GltfMaterialOptions mat; mat.materialName = "Make3DTypedAssetMaterial"; mat.baseColorFactor = {0.70f,0.72f,0.76f,1.0f};
    if (!ExportGLTFWithMaterial(r.mesh, r.gltfPath, mat, &error)) { r.message = error; return r; }
    std::ofstream report(r.reportPath, std::ios::binary);
    report << "# Make3D Typed Procedural Asset\n\n";
    report << "Asset type: " << ToString(type) << "\n\n" << r.validation.ToMarkdown();
    r.ok = true; r.message = "Typed procedural asset generated.";
    std::ofstream manifest(r.manifestPath, std::ios::binary);
    manifest << "{\n  \"ok\": true,\n  \"assetType\": \"" << Esc(ToString(type)) << "\",\n  \"obj\": \"" << Esc(r.objPath.generic_string()) << "\",\n  \"gltf\": \"" << Esc(r.gltfPath.generic_string()) << "\",\n  \"vertices\": " << VC(r.mesh) << ",\n  \"triangles\": " << TC(r.mesh) << "\n}\n";
    return r;
}

} // namespace make3d
