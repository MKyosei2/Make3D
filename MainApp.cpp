#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <fbxsdk.h>
#include <vector>
#include <string>
#include <ctime>
#include <Windows.h>
#pragma comment(lib, "libfbxsdk.lib")

std::string OpenPngFileDialog() {
    char filePath[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "PNGファイル (*.png)\0*.png\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "PNGファイルを選択";
    if (GetOpenFileNameA(&ofn)) return filePath;
    return "";
}

// 画像縮小（最近傍補間）最大96x96
void resize_nearest(const unsigned char* src, int sw, int sh, int sc, unsigned char* dst, int dw, int dh, int dc) {
    for (int y = 0; y < dh; ++y) for (int x = 0; x < dw; ++x) {
        int sx = x * sw / dw, sy = y * sh / dh;
        for (int c = 0; c < dc; ++c) dst[(y * dw + x) * dc + c] = src[(sy * sw + sx) * sc + c];
    }
}

// αまたは明度しきい値
std::vector<bool> createMask(const unsigned char* img, int w, int h, int comp, unsigned char threshold = 32) {
    std::vector<bool> mask(w * h, false);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        int idx = y * w + x;
        if (comp == 4) mask[idx] = (img[idx * 4 + 3] > threshold);
        else {
            int gray = (img[idx * comp + 0] + img[idx * comp + 1] + img[idx * comp + 2]) / 3;
            mask[idx] = (gray > threshold);
        }
    }
    return mask;
}

// 画像のシルエット部分を全て押し出し（“板押し出し”立体）
void CreateExtrudeMesh(const std::vector<bool>& mask, int w, int h, int depth, float scaleXY, float scaleZ,
    std::vector<float>& outVertices, std::vector<int>& outIndices)
{
    auto idx = [=](int x, int y, int z) { return z * w * h + y * w + x; };
    // 頂点生成
    for (int z = 0; z <= depth; ++z) {
        float zz = ((float)z / depth - 0.5f) * scaleZ;
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
            float xx = ((float)x / (w - 1) - 0.5f) * scaleXY;
            float yy = ((float)(h - 1 - y) / (h - 1) - 0.5f) * scaleXY;
            outVertices.push_back(xx);
            outVertices.push_back(yy);
            outVertices.push_back(zz);
        }
    }
    // 面生成（シルエット部分のみ立方体ボクセルを並べる）
    for (int z = 0; z < depth; ++z)
        for (int y = 0; y < h - 1; ++y)
            for (int x = 0; x < w - 1; ++x) {
                if (mask[y * w + x]) {
                    // 押し出しキューブ6面を三角形で作る
                    int v0 = idx(x, y, z);
                    int v1 = idx(x + 1, y, z);
                    int v2 = idx(x + 1, y + 1, z);
                    int v3 = idx(x, y + 1, z);
                    int v4 = idx(x, y, z + 1);
                    int v5 = idx(x + 1, y, z + 1);
                    int v6 = idx(x + 1, y + 1, z + 1);
                    int v7 = idx(x, y + 1, z + 1);
                    // 12三角面
                    // 前
                    outIndices.insert(outIndices.end(), { v0,v1,v2, v0,v2,v3 });
                    // 後
                    outIndices.insert(outIndices.end(), { v4,v7,v6, v4,v6,v5 });
                    // 左
                    outIndices.insert(outIndices.end(), { v0,v3,v7, v0,v7,v4 });
                    // 右
                    outIndices.insert(outIndices.end(), { v1,v5,v6, v1,v6,v2 });
                    // 上
                    outIndices.insert(outIndices.end(), { v3,v2,v6, v3,v6,v7 });
                    // 下
                    outIndices.insert(outIndices.end(), { v0,v4,v5, v0,v5,v1 });
                }
            }
}

// FBX出力
void ExportToFBX(const std::vector<float>& vertices, const std::vector<int>& indices, const std::string& filename) {
    FbxManager* lSdkManager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(lSdkManager, IOSROOT);
    lSdkManager->SetIOSettings(ios);
    FbxScene* lScene = FbxScene::Create(lSdkManager, "Scene");
    FbxNode* lRootNode = lScene->GetRootNode();
    FbxMesh* lMesh = FbxMesh::Create(lScene, "ExtrudedMesh");
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
    std::string pngfile = OpenPngFileDialog();
    if (pngfile.empty()) {
        MessageBoxA(0, "PNGファイルが選択されませんでした", "Error", 0);
        return 1;
    }
    int w, h, comp;
    unsigned char* img0 = stbi_load(pngfile.c_str(), &w, &h, &comp, 0);
    if (!img0) {
        MessageBoxA(0, "PNGファイルが読めません", "Error", 0);
        return 1;
    }
    // 最大96x96へ縮小
    int tw = w, th = h;
    if (w > 96 || h > 96) {
        float aspect = (float)w / h;
        if (w > h) { tw = 96; th = (int)(96 / aspect); }
        else { th = 96; tw = (int)(96 * aspect); }
    }
    std::vector<unsigned char> img(tw * th * comp);
    resize_nearest(img0, w, h, comp, img.data(), tw, th, comp);
    stbi_image_free(img0);

    std::vector<bool> mask = createMask(img.data(), tw, th, comp);
    int depth = 8;      // 3D厚み
    float scaleXY = 1.0f;
    float scaleZ = 0.2f;

    std::vector<float> vertices;
    std::vector<int> indices;
    CreateExtrudeMesh(mask, tw, th, depth, scaleXY, scaleZ, vertices, indices);

    char filename[256];
    std::time_t t = std::time(nullptr);
    sprintf(filename, "output_%lld.fbx", (long long)t);
    ExportToFBX(vertices, indices, filename);

    MessageBoxA(0, "完了", "OK", 0);
    return 0;
}
