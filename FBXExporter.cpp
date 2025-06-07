#include "FBXExporter.h"
#include <fbxsdk.h>

#pragma comment(lib, "libfbxsdk.lib")

bool exportMeshToFBX(const std::wstring& filenameW, const Mesh& mesh) {
    FbxManager* manager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);

    FbxScene* scene = FbxScene::Create(manager, "MeshScene");

    FbxNode* root = scene->GetRootNode();
    FbxMesh* fbxMesh = FbxMesh::Create(scene, "Mesh");

    int vertCount = (int)mesh.vertices.size();
    fbxMesh->InitControlPoints(vertCount);

    for (int i = 0; i < vertCount; ++i) {
        const auto& v = mesh.vertices[i];
        fbxMesh->SetControlPointAt(FbxVector4(v.x, v.y, v.z), i);
    }

    for (const auto& tri : mesh.triangles) {
        fbxMesh->BeginPolygon();
        fbxMesh->AddPolygon(tri.v0);
        fbxMesh->AddPolygon(tri.v1);
        fbxMesh->AddPolygon(tri.v2);
        fbxMesh->EndPolygon();
    }

    FbxNode* meshNode = FbxNode::Create(scene, "MeshNode");
    meshNode->SetNodeAttribute(fbxMesh);
    root->AddChild(meshNode);

    FbxExporter* exporter = FbxExporter::Create(manager, "");
    std::string filenameA(filenameW.begin(), filenameW.end());
    if (!exporter->Initialize(filenameA.c_str(), -1, manager->GetIOSettings())) {
        manager->Destroy();
        return false;
    }

    bool result = exporter->Export(scene);
    exporter->Destroy();
    manager->Destroy();
    return result;
}
