#include "FBXExporter.h"
#include <fbxsdk.h>
#include <string>

std::string convertToAnsi(const std::wstring& wide) {
    int len = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(len, 0);
    WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

bool exportMeshToFBX(const Mesh& mesh, const std::wstring& filename) {
    std::string ansiFilename = convertToAnsi(filename);
    ExportMeshToFBX(mesh.triangles, mesh.vertices, ansiFilename);
    return true;
}

void ExportMeshToFBX(const std::vector<Triangle>& triangles, const std::vector<Vertex>& vertices, const std::string& filename) {
    FbxManager* manager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);

    FbxScene* scene = FbxScene::Create(manager, "MeshScene");
    FbxNode* rootNode = scene->GetRootNode();

    FbxMesh* fbxMesh = FbxMesh::Create(scene, "Mesh");

    int vertexCount = static_cast<int>(vertices.size());
    fbxMesh->InitControlPoints(vertexCount);
    for (int i = 0; i < vertexCount; ++i) {
        const Vertex& v = vertices[i];
        fbxMesh->SetControlPointAt(FbxVector4(v.x, v.y, v.z), i);
    }

    for (const Triangle& t : triangles) {
        fbxMesh->BeginPolygon();
        fbxMesh->AddPolygon(t.v0);
        fbxMesh->AddPolygon(t.v1);
        fbxMesh->AddPolygon(t.v2);
        fbxMesh->EndPolygon();
    }

    FbxNode* meshNode = FbxNode::Create(scene, "MeshNode");
    meshNode->SetNodeAttribute(fbxMesh);
    rootNode->AddChild(meshNode);

    FbxExporter* exporter = FbxExporter::Create(manager, "");
    exporter->Initialize(filename.c_str(), -1, manager->GetIOSettings());
    exporter->Export(scene);

    exporter->Destroy();
    manager->Destroy();
}
