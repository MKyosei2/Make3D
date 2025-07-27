#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <fbxsdk.h>
#include <vector>
#include <string>
#include <ctime>
#include <Windows.h>
#pragma comment(lib, "libfbxsdk.lib")

std::string OpenPngFileDialog(const char* title) {
    char filePath[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "PNGファイル (*.png)\0*.png\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = title;
    if (GetOpenFileNameA(&ofn)) return filePath;
    return "";
}

// アルファ値0部分を除外して3Dメッシュ生成
void CreateDepthMeshWithAlpha(
    const unsigned char* rgba, int imgW, int imgH, int imgComp,
    const unsigned char* depth, int dW, int dH, int dComp,
    float depthScale, float zOffset,
    std::vector<float>& outVertices, std::vector<int>& outIndices
) {
    if (imgW != dW || imgH != dH) return; // サイズ一致必須
    int w = imgW, h = imgH;
    auto getDepth = [&](int idx) -> float {
        if (dComp == 1) return depth[idx] / 255.0f;
        else return depth[idx * dComp] / 255.0f;
        };
    // 頂点リスト・マッピング
    std::vector<int> vertMap(w * h, -1);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        int idx = y * w + x;
        int a = (imgComp >= 4) ? rgba[idx * imgComp + 3] : 255;
        if (a == 0) continue; // α=0スキップ
        float xx = ((float)x / (w - 1) - 0.5f);
        float yy = ((float)(h - 1 - y) / (h - 1) - 0.5f);
        float z = getDepth(idx) * depthScale + zOffset;
        vertMap[idx] = (int)(outVertices.size() / 3);
        outVertices.push_back(xx);
        outVertices.push_back(yy);
        outVertices.push_back(z);
    }
    // 面生成：4頂点ともα>0だけ三角形生成
    for (int y = 0; y < h - 1; ++y) for (int x = 0; x < w - 1; ++x) {
        int i00 = y * w + x;
        int i10 = y * w + (x + 1);
        int i01 = (y + 1) * w + x;
        int i11 = (y + 1) * w + (x + 1);
        if (vertMap[i00] < 0 || vertMap[i10] < 0 || vertMap[i11] < 0) goto skip1;
        outIndices.insert(outIndices.end(), { vertMap[i00], vertMap[i10], vertMap[i11] });
    skip1:
        if (vertMap[i00] < 0 || vertMap[i11] < 0 || vertMap[i01] < 0) continue;
        outIndices.insert(outIndices.end(), { vertMap[i00], vertMap[i11], vertMap[i01] });
    }
}

void ExportToFBX(const std::vector<float>& vertices, const std::vector<int>& indices, const std::string& filename) {
    FbxManager* lSdkManager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(lSdkManager, IOSROOT);
    lSdkManager->SetIOSettings(ios);
    FbxScene* lScene = FbxScene::Create(lSdkManager, "Scene");
    FbxNode* lRootNode = lScene->GetRootNode();
    FbxMesh* lMesh = FbxMesh::Create(lScene, "DepthMesh");
    int nVerts = vertices.size() / 3;
    lMesh->InitControlPoints(nVerts);
    for (int i = 0; i < nVerts; ++i)
        lMesh->SetControlPointAt(FbxVector4(vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]), i);
    int nTris = indices.size() / 3;
    for (int i = 0; i < nTris; ++i) {
        lMesh->BeginPolygon();
        lMesh->AddPolygon(indices[i * 3 + 0]);
        lMesh->AddPolygon(indices[i * 3 + 1]);
        lMesh->AddPolygon(indices[i * 3 + 2]);
        lMesh->EndPolygon();
    }
    FbxNode* lMeshNode = FbxNode::Create(lScene, "MeshNode");
    lMeshNode->SetNodeAttribute(lMesh);
    lRootNode->AddChild(lMeshNode);
    FbxExporter* lExporter = FbxExporter::Create(lSdkManager, "");
    if (!lExporter->Initialize(filename.c_str(), -1, lSdkManager->GetIOSettings())) {
        std::string err = "FBXExporter Initialize failed: ";
        err += lExporter->GetStatus().GetErrorString();
        MessageBoxA(nullptr, err.c_str(), "FBX出力エラー", 0);
    }
    else {
        if (!lExporter->Export(lScene)) {
            std::string err = "FBXExporter Export failed: ";
            err += lExporter->GetStatus().GetErrorString();
            MessageBoxA(nullptr, err.c_str(), "FBX出力エラー", 0);
        }
        else {
            MessageBoxA(nullptr, ("FBX Export Success!\n" + filename).c_str(), "Export", 0);
        }
    }
    lExporter->Destroy();
    lSdkManager->Destroy();
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // カラー画像（input.png RGBA必須）
    std::string imgfile = OpenPngFileDialog("カラー画像（アルファ付きPNG）を選択");
    if (imgfile.empty()) { MessageBoxA(0, "画像が選択されませんでした", "Error", 0); return 1; }
    int w, h, comp;
    unsigned char* rgbaImg = stbi_load(imgfile.c_str(), &w, &h, &comp, 4);
    if (!rgbaImg) { MessageBoxA(0, "画像が読めません", "Error", 0); return 1; }

    // 深度マップ画像
    std::string depthfile = OpenPngFileDialog("深度マップ(PNG)を選択");
    if (depthfile.empty()) { stbi_image_free(rgbaImg); MessageBoxA(0, "深度マップが選択されませんでした", "Error", 0); return 1; }
    int dw, dh, dcomp;
    unsigned char* depthImg = stbi_load(depthfile.c_str(), &dw, &dh, &dcomp, 0);
    if (!depthImg) { stbi_image_free(rgbaImg); MessageBoxA(0, "深度マップが読めません", "Error", 0); return 1; }

    if (w != dw || h != dh) {
        stbi_image_free(rgbaImg); stbi_image_free(depthImg);
        MessageBoxA(0, "画像と深度マップのサイズが異なります", "Error", 0);
        return 1;
    }

    // メッシュ生成
    float depthScale = 0.5f;
    float zOffset = 0.0f;
    std::vector<float> vertices;
    std::vector<int> indices;
    CreateDepthMeshWithAlpha(rgbaImg, w, h, 4, depthImg, dw, dh, dcomp, depthScale, zOffset, vertices, indices);

    stbi_image_free(rgbaImg); stbi_image_free(depthImg);

    char filename[256];
    std::time_t t = std::time(nullptr);
    sprintf(filename, "depthmesh_%lld.fbx", (long long)t);
    ExportToFBX(vertices, indices, filename);

    MessageBoxA(0, "完了", "OK", 0);
    return 0;
}
