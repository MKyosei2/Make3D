#include "FBXExporter.h"
#include <fbxsdk.h>

bool ExportToFBX(const std::string& path, const std::vector<Mesh3D>& meshes) {
    FbxManager* manager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);

    FbxScene* scene = FbxScene::Create(manager, "Scene");

    for (const auto& mesh : meshes) {
        FbxMesh* fbxMesh = FbxMesh::Create(scene, "Mesh");

        int vertCount = (int)mesh.vertices.size();
        fbxMesh->InitControlPoints(vertCount);
        for (int i = 0; i < vertCount; ++i) {
            const Vec3& v = mesh.vertices[i];
            fbxMesh->SetControlPointAt(FbxVector4(v.x, v.y, v.z), i);
        }

        int polyCount = (int)mesh.triangles.size();
        for (int i = 0; i < polyCount; i += 3) {
            fbxMesh->BeginPolygon();
            fbxMesh->AddPolygon(mesh.triangles[i]);
            fbxMesh->AddPolygon(mesh.triangles[i + 1]);
            fbxMesh->AddPolygon(mesh.triangles[i + 2]);
            fbxMesh->EndPolygon();
        }

        FbxNode* node = FbxNode::Create(scene, "Node");
        node->SetNodeAttribute(fbxMesh);
        scene->GetRootNode()->AddChild(node);
    }

    FbxExporter* exporter = FbxExporter::Create(manager, "");
    bool ok = exporter->Initialize(path.c_str(), -1, manager->GetIOSettings());
    if (!ok) return false;
    exporter->Export(scene);

    exporter->Destroy();
    scene->Destroy();
    ios->Destroy();
    manager->Destroy();
    return true;
}