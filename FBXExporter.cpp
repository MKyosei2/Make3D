#include "FBXExporter.h"
#include "MeshUtils.h"
#include <fbxsdk.h>
#include <windows.h>
#pragma comment(lib, "libfbxsdk.lib")

bool exportMeshToFBX(const std::wstring& filenameW, const Mesh& mesh) {
    // ✅ 空メッシュを防止
    if (mesh.vertices.empty() || mesh.triangles.empty()) {
        MessageBoxW(nullptr, L"メッシュが空です。FBX出力をスキップしました。", L"警告", MB_ICONWARNING);
        return false;
    }

    // ✅ ファイル名拡張子チェック
    if (filenameW.size() < 4 || filenameW.substr(filenameW.size() - 4) != L".fbx") {
        MessageBoxW(nullptr, L".fbx 拡張子が必要です。", L"警告", MB_ICONWARNING);
        return false;
    }

    FbxManager* manager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);

    FbxScene* scene = FbxScene::Create(manager, "MeshScene");
    FbxNode* root = scene->GetRootNode();

    FbxMesh* fbxMesh = FbxMesh::Create(scene, "Mesh");
    fbxMesh->InitControlPoints((int)mesh.vertices.size());

    // 頂点の設定
    for (int i = 0; i < (int)mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        fbxMesh->SetControlPointAt(FbxVector4(v.x, v.y, v.z), i);
    }

    // ポリゴンの設定
    for (const auto& tri : mesh.triangles) {
        if (tri.v0 < mesh.vertices.size() && tri.v1 < mesh.vertices.size() && tri.v2 < mesh.vertices.size()) {
            fbxMesh->BeginPolygon();
            fbxMesh->AddPolygon(tri.v0);
            fbxMesh->AddPolygon(tri.v1);
            fbxMesh->AddPolygon(tri.v2);
            fbxMesh->EndPolygon();
        }
    }

    // ノードにメッシュを登録
    FbxNode* meshNode = FbxNode::Create(scene, "MeshNode");
    meshNode->SetNodeAttribute(fbxMesh);
    root->AddChild(meshNode);

    // 出力の準備
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