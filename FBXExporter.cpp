#include "GUIState.h"
#include <fbxsdk.h>

void ExportToFBX(const Mesh3D& mesh, const char* filename) {
    FbxManager* mgr = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(mgr, IOSROOT);
    mgr->SetIOSettings(ios);
    FbxScene* scene = FbxScene::Create(mgr, "Scene");
    FbxNode* root = scene->GetRootNode();

    FbxMesh* fbxMesh = FbxMesh::Create(scene, "Mesh");
    fbxMesh->InitControlPoints((int)mesh.vertices.size());
    for (int i = 0; i < mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        fbxMesh->SetControlPointAt(FbxVector4(v.x, v.y, v.z), i);
    }

    for (int i = 0; i + 2 < mesh.indices.size(); i += 3) {
        fbxMesh->BeginPolygon();
        fbxMesh->AddPolygon(mesh.indices[i]);
        fbxMesh->AddPolygon(mesh.indices[i + 1]);
        fbxMesh->AddPolygon(mesh.indices[i + 2]);
        fbxMesh->EndPolygon();
    }

    FbxNode* node = FbxNode::Create(scene, "Node");
    node->SetNodeAttribute(fbxMesh);
    root->AddChild(node);

    FbxExporter* exporter = FbxExporter::Create(mgr, "");
    exporter->Initialize(filename, -1, mgr->GetIOSettings());
    exporter->Export(scene);
    exporter->Destroy();
    mgr->Destroy();
}
