#define NOMINMAX
#include "FBXExporter.h"
#include "MeshUtils.h"
#include <fbxsdk.h>
#include <windows.h>
#include <algorithm> 
#pragma comment(lib, "libfbxsdk.lib")

bool exportMeshToFBX(const std::wstring& filenameW, const Mesh& mesh) {
    if (mesh.vertices.empty() || mesh.triangles.empty()) {
        MessageBoxW(nullptr, L"メッシュが空です。FBX出力をスキップしました。", L"警告", MB_ICONWARNING);
        return false;
    }

    if (filenameW.size() < 4 || filenameW.substr(filenameW.size() - 4) != L".fbx") {
        MessageBoxW(nullptr, L".fbx 拡張子が必要です。", L"警告", MB_ICONWARNING);
        return false;
    }

    // 中心補正のためにバウンディングボックスを計算
    double minX = 1e10, maxX = -1e10;
    double minY = 1e10, maxY = -1e10;
    double minZ = 1e10, maxZ = -1e10;

    for (const auto& v : mesh.vertices) {
        minX = std::min(minX, (double)v.x);
        maxX = std::max(maxX, (double)v.x);
        minY = std::min(minY, (double)v.y);
        maxY = std::max(maxY, (double)v.y);
        minZ = std::min(minZ, (double)v.z);
        maxZ = std::max(maxZ, (double)v.z);
    }

    double offsetX = -(minX + maxX) * 0.5;
    double offsetY = -(minY + maxY) * 0.5;
    double offsetZ = -(minZ + maxZ) * 0.5;

    FbxManager* manager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);

    FbxScene* scene = FbxScene::Create(manager, "MeshScene");
    FbxNode* root = scene->GetRootNode();

    FbxMesh* fbxMesh = FbxMesh::Create(scene, "Mesh");
    fbxMesh->InitControlPoints((int)mesh.vertices.size());

    for (int i = 0; i < (int)mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        fbxMesh->SetControlPointAt(FbxVector4(
            (double)v.x + offsetX,
            (double)v.y + offsetY,
            (double)v.z + offsetZ), i);
    }

    for (const auto& tri : mesh.triangles) {
        if (tri.v0 < mesh.vertices.size() && tri.v1 < mesh.vertices.size() && tri.v2 < mesh.vertices.size()) {
            fbxMesh->BeginPolygon();
            fbxMesh->AddPolygon(tri.v0);
            fbxMesh->AddPolygon(tri.v1);
            fbxMesh->AddPolygon(tri.v2);
            fbxMesh->EndPolygon();
        }
    }

    FbxNode* meshNode = FbxNode::Create(scene, "MeshNode");
    meshNode->SetNodeAttribute(fbxMesh);

    // 🔧 全体のスケーリング（Maya表示調整用）
    meshNode->LclScaling.Set(FbxDouble3(0.01, 0.01, 0.01));

    root->AddChild(meshNode);

    FbxExporter* exporter = FbxExporter::Create(manager, "");
    std::string filenameA(filenameW.begin(), filenameW.end());

    if (!exporter->Initialize(filenameA.c_str(), -1, manager->GetIOSettings())) {
        MessageBoxW(nullptr, L"FBXエクスポーターの初期化に失敗しました。", L"エラー", MB_ICONERROR);
        exporter->Destroy();
        manager->Destroy();
        return false;
    }

    bool result = exporter->Export(scene);
    exporter->Destroy();
    manager->Destroy();

    if (!result) {
        MessageBoxW(nullptr, L"FBXファイルの出力に失敗しました。", L"エラー", MB_ICONERROR);
        return false;
    }

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