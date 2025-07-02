#define NOMINMAX
#include "FBXExporter.h"
#include "MeshUtils.h"
#include <fbxsdk.h>
#include <windows.h>
#include <algorithm> 
#include <cfloat> 
#include <fbxsdk/utils/fbxgeometryconverter.h>
#pragma comment(lib, "libfbxsdk.lib")

bool exportMeshToFBX(const Mesh& mesh, const std::wstring& filename) {
    FbxManager* sdkManager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(sdkManager, IOSROOT);
    sdkManager->SetIOSettings(ios);

    FbxScene* scene = FbxScene::Create(sdkManager, "MyScene");
    FbxNode* rootNode = scene->GetRootNode();

    FbxMesh* meshNode = FbxMesh::Create(scene, "GeneratedMesh");

    int vertexCount = static_cast<int>(mesh.vertices.size());
    meshNode->InitControlPoints(vertexCount);

    // 重心を原点に移動
    double cx = 0, cy = 0, cz = 0;
    for (const auto& v : mesh.vertices) {
        cx += v.x;
        cy += v.y;
        cz += v.z;
    }
    cx /= vertexCount;
    cy /= vertexCount;
    cz /= vertexCount;

    // AABB算出
    float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

    for (int i = 0; i < vertexCount; ++i) {
        const Vertex& v = mesh.vertices[i];
        float px = (float)(v.x - cx);
        float py = (float)(v.y - cy);
        float pz = (float)(v.z - cz);
        meshNode->SetControlPointAt(FbxVector4(px, py, pz), i);

        minX = std::min(minX, px); maxX = std::max(maxX, px);
        minY = std::min(minY, py); maxY = std::max(maxY, py);
        minZ = std::min(minZ, pz); maxZ = std::max(maxZ, pz);
    }

    // AABBメッセージ表示
    wchar_t buf[512];
    swprintf_s(buf, 512,
        L"メッシュAABB\nX: %.2f ~ %.2f\nY: %.2f ~ %.2f\nZ: %.2f ~ %.2f\n頂点数: %d",
        minX, maxX, minY, maxY, minZ, maxZ, vertexCount);
    MessageBoxW(nullptr, buf, L"座標範囲", MB_OK);

    // 法線追加（すべてZ+）
    FbxGeometryElementNormal* normals = meshNode->CreateElementNormal();
    normals->SetMappingMode(FbxGeometryElement::eByControlPoint);
    normals->SetReferenceMode(FbxGeometryElement::eDirect);
    for (int i = 0; i < vertexCount; ++i)
        normals->GetDirectArray().Add(FbxVector4(0, 0, 1));

    // UV追加（仮の三角展開）
    FbxGeometryElementUV* uvElement = meshNode->CreateElementUV("UVChannel_1");
    uvElement->SetMappingMode(FbxGeometryElement::eByPolygonVertex);
    uvElement->SetReferenceMode(FbxGeometryElement::eDirect);

    for (const auto& tri : mesh.triangles) {
        meshNode->BeginPolygon();
        for (int i = 0; i < 3; ++i) {
            int vi = (i == 0) ? tri.v0 : (i == 1) ? tri.v1 : tri.v2;
            meshNode->AddPolygon(vi);

            FbxVector2 uv = (i == 0) ? FbxVector2(0, 0)
                : (i == 1) ? FbxVector2(1, 0)
                : FbxVector2(0.5, 1);
            uvElement->GetDirectArray().Add(uv);
        }
        meshNode->EndPolygon();
    }

    // ★ BuildMesh 呼び出し：Maya表示に必須
    FbxGeometryConverter converter(sdkManager);
    converter.Triangulate(scene, true);

    // ノードを作成してジオメトリを割り当て
    FbxNode* meshContainer = FbxNode::Create(scene, "MeshNode");
    meshContainer->SetNodeAttribute(meshNode);
    meshContainer->SetShadingMode(FbxNode::eTextureShading);  // Maya表示対策
    rootNode->AddChild(meshContainer);

    // ファイル名を multibyte に変換
    std::string filenameA;
    {
        int size_needed = WideCharToMultiByte(CP_ACP, 0, filename.c_str(), -1, NULL, 0, NULL, NULL);
        filenameA.resize(size_needed);
        WideCharToMultiByte(CP_ACP, 0, filename.c_str(), -1, &filenameA[0], size_needed, NULL, NULL);
    }

    // FBX Exporter 設定（ASCII形式でより透明性を）
    FbxExporter* exporter = FbxExporter::Create(sdkManager, "");
    int asciiID = sdkManager->GetIOPluginRegistry()->FindWriterIDByDescription("FBX ascii");
    if (!exporter->Initialize(filenameA.c_str(), asciiID, sdkManager->GetIOSettings())) {
        MessageBoxW(nullptr, L"FBXエクスポート初期化に失敗", L"エラー", MB_ICONERROR);
        return false;
    }

    exporter->Export(scene);
    exporter->Destroy();
    sdkManager->Destroy();

    return true;
}

void ExportMeshToFBX(const std::vector<Triangle>& triangles, const std::string& filename) {
    FbxManager* manager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);
    FbxScene* scene = FbxScene::Create(manager, "Scene");

    FbxMesh* mesh = FbxMesh::Create(scene, "Mesh");
    mesh->InitControlPoints(triangles.size() * 3);

    int idx = 0;
    for (const auto& tri : triangles) {
        for (int i = 0; i < 3; ++i) {
            mesh->SetControlPointAt(FbxVector4(
                tri.vertices[i].x,
                tri.vertices[i].y,
                tri.vertices[i].z), idx++);
        }
    }

    for (int i = 0; i < triangles.size(); ++i) {
        mesh->BeginPolygon();
        mesh->AddPolygon(i * 3 + 0);
        mesh->AddPolygon(i * 3 + 1);
        mesh->AddPolygon(i * 3 + 2);
        mesh->EndPolygon();
    }

    FbxNode* node = FbxNode::Create(scene, "MeshNode");
    node->SetNodeAttribute(mesh);
    scene->GetRootNode()->AddChild(node);

    FbxExporter* exporter = FbxExporter::Create(manager, "");
    if (exporter->Initialize(filename.c_str(), -1, manager->GetIOSettings())) {
        exporter->Export(scene);
    }
    exporter->Destroy();
    manager->Destroy();
}