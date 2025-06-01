#include "FBXExporter.h"
#include <fbxsdk.h>

void ExportToFBX(const Mesh3D& mesh, const char* filename) {
    FbxManager* manager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);

    FbxScene* scene = FbxScene::Create(manager, "Scene");
    FbxNode* root = scene->GetRootNode();

    FbxMesh* fbxMesh = FbxMesh::Create(scene, "Mesh");
    fbxMesh->InitControlPoints(mesh.vertices.size());

    for (int i = 0; i < mesh.vertices.size(); ++i) {
        auto v = mesh.vertices[i];
        fbxMesh->SetControlPointAt(FbxVector4(v.x, v.y, v.z), i);
    }

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        fbxMesh->BeginPolygon();
        fbxMesh->AddPolygon(mesh.indices[i]);
        fbxMesh->AddPolygon(mesh.indices[i + 1]);
        fbxMesh->AddPolygon(mesh.indices[i + 2]);
        fbxMesh->EndPolygon();
    }

    FbxNode* meshNode = FbxNode::Create(scene, "MeshNode");
    meshNode->SetNodeAttribute(fbxMesh);
    root->AddChild(meshNode);

    FbxExporter* exporter = FbxExporter::Create(manager, "");
    exporter->Initialize(filename, -1, manager->GetIOSettings());
    exporter->Export(scene);
    exporter->Destroy();
    manager->Destroy();
}
