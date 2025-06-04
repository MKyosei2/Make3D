#include "FBXExporter.h"
#include <fbxsdk.h>

FBXExporter::FBXExporter() {
    manager = fbxsdk::FbxManager::Create();
    ios = fbxsdk::FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);
}

FBXExporter::~FBXExporter() {
    if (manager) manager->Destroy();
}

void FBXExporter::exportMeshToFBX(const MeshData& mesh, const std::string& filename) {
    fbxsdk::FbxScene* scene = fbxsdk::FbxScene::Create(manager, "Scene");
    fbxsdk::FbxNode* rootNode = scene->GetRootNode();
    fbxsdk::FbxMesh* fbxMesh = fbxsdk::FbxMesh::Create(scene, "Mesh");

    fbxMesh->InitControlPoints((int)mesh.vertices.size());
    for (int i = 0; i < (int)mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        fbxMesh->SetControlPointAt(fbxsdk::FbxVector4(v.x, v.y, v.z), i);
    }

    for (const auto& tri : mesh.triangles) {
        fbxMesh->BeginPolygon();
        fbxMesh->AddPolygon(tri.v0);
        fbxMesh->AddPolygon(tri.v1);
        fbxMesh->AddPolygon(tri.v2);
        fbxMesh->EndPolygon();
    }

    fbxsdk::FbxNode* meshNode = fbxsdk::FbxNode::Create(scene, "MeshNode");
    meshNode->SetNodeAttribute(fbxMesh);
    rootNode->AddChild(meshNode);

    fbxsdk::FbxExporter* exporter = fbxsdk::FbxExporter::Create(manager, "");
    if (exporter->Initialize(filename.c_str(), -1, manager->GetIOSettings())) {
        exporter->Export(scene);
    }
    exporter->Destroy();
}
