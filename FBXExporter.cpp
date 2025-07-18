#include "FBXExporter.h"
#include <fbxsdk.h>

#pragma comment(lib, "libfbxsdk.lib")

bool FBXExporter::Export(const std::wstring& filename,
    const std::vector<float>& vertices,
    const std::vector<unsigned int>& indices)
{
    FbxManager* manager = FbxManager::Create();
    if (!manager) return false;

    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);

    FbxScene* scene = FbxScene::Create(manager, "Scene");

    FbxNode* rootNode = scene->GetRootNode();
    FbxMesh* mesh = FbxMesh::Create(scene, "Mesh");

    int vertexCount = static_cast<int>(vertices.size()) / 3;
    mesh->InitControlPoints(vertexCount);

    for (int i = 0; i < vertexCount; ++i)
    {
        float x = vertices[i * 3 + 0];
        float y = vertices[i * 3 + 1];
        float z = vertices[i * 3 + 2];
        mesh->SetControlPointAt(FbxVector4(x, y, z), i);
    }

    int triangleCount = static_cast<int>(indices.size()) / 3;
    for (int i = 0; i < triangleCount; ++i)
    {
        mesh->BeginPolygon();
        mesh->AddPolygon(indices[i * 3 + 0]);
        mesh->AddPolygon(indices[i * 3 + 1]);
        mesh->AddPolygon(indices[i * 3 + 2]);
        mesh->EndPolygon();
    }

    FbxNode* meshNode = FbxNode::Create(scene, "MeshNode");
    meshNode->SetNodeAttribute(mesh);
    rootNode->AddChild(meshNode);

    FbxExporter* exporter = FbxExporter::Create(manager, "");
    std::string path(filename.begin(), filename.end());

    if (!exporter->Initialize(path.c_str(), -1, manager->GetIOSettings())) {
        exporter->Destroy();
        manager->Destroy();
        return false;
    }

    exporter->Export(scene);
    exporter->Destroy();
    manager->Destroy();
    return true;
}
