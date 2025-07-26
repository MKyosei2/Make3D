// MainApp.cpp : WinAPI + Marching Cubes + FBX Export GUI Tool
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <fbxsdk.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "MarchingCubesTable.h"

struct Vec3 { float x, y, z; };
struct Triangle { int a, b, c; };

std::string g_inputImagePath = "";
const float ISO_LEVEL = 0.5f;
const int VOXEL_RES = 32;

// PNG読み込み
bool LoadGrayscaleImage(const std::string& path, int& width, int& height, std::vector<unsigned char>& image) {
    int comp;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &comp, 1);
    if (!data) return false;
    image.assign(data, data + width * height);
    stbi_image_free(data);
    return true;
}

// 頂点補間（linear）
Vec3 VertexInterp(const Vec3& p1, const Vec3& p2, float valp1, float valp2) {
    float mu = (ISO_LEVEL - valp1) / (valp2 - valp1 + 1e-6f);
    return {
        p1.x + mu * (p2.x - p1.x),
        p1.y + mu * (p2.y - p1.y),
        p1.z + mu * (p2.z - p1.z)
    };
}

// Marching Cubes 実行
void GenerateMarchingCubesMesh(const std::vector<unsigned char>& volume, int resX, int resY, int resZ,
    std::vector<Vec3>& vertices, std::vector<Triangle>& triangles) {
    const int edgeVertexMap[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7}
    };
    const Vec3 cubeVertexOffset[8] = {
        {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
        {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
    };

    auto at = [&](int x, int y, int z) {
        return volume[x + y * resX + z * resX * resY] / 255.0f;
        };

    for (int z = 0; z < resZ - 1; ++z)
        for (int y = 0; y < resY - 1; ++y)
            for (int x = 0; x < resX - 1; ++x) {
                int cubeIndex = 0;
                float val[8];
                Vec3 pos[8];

                for (int i = 0; i < 8; ++i) {
                    int xi = x + (int)cubeVertexOffset[i].x;
                    int yi = y + (int)cubeVertexOffset[i].y;
                    int zi = z + (int)cubeVertexOffset[i].z;
                    val[i] = at(xi, yi, zi);
                    pos[i] = { (float)xi, (float)yi, (float)zi };
                    if (val[i] < ISO_LEVEL) cubeIndex |= (1 << i);
                }

                int edges = edgeTable[cubeIndex];
                if (edges == 0) continue;

                Vec3 vertList[12];
                for (int i = 0; i < 12; ++i) {
                    if (edges & (1 << i)) {
                        int a = edgeVertexMap[i][0];
                        int b = edgeVertexMap[i][1];
                        vertList[i] = VertexInterp(pos[a], pos[b], val[a], val[b]);
                    }
                }

                for (int i = 0; triTable[cubeIndex][i] != -1; i += 3) {
                    int idx0 = (int)vertices.size();
                    vertices.push_back(vertList[triTable[cubeIndex][i]]);
                    vertices.push_back(vertList[triTable[cubeIndex][i + 1]]);
                    vertices.push_back(vertList[triTable[cubeIndex][i + 2]]);
                    triangles.push_back({ idx0, idx0 + 1, idx0 + 2 });
                }
            }
}

// FBX出力
void ExportToFBX(const std::vector<Vec3>& vertices, const std::vector<Triangle>& triangles, const std::string& filename) {
    FbxManager* manager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);

    FbxScene* scene = FbxScene::Create(manager, "Scene");
    FbxMesh* mesh = FbxMesh::Create(scene, "Mesh");

    mesh->InitControlPoints((int)vertices.size());
    for (int i = 0; i < vertices.size(); ++i) {
        const Vec3& v = vertices[i];
        mesh->SetControlPointAt(FbxVector4(v.x, v.y, v.z), i);
    }

    for (const auto& tri : triangles) {
        mesh->BeginPolygon();
        mesh->AddPolygon(tri.a);
        mesh->AddPolygon(tri.b);
        mesh->AddPolygon(tri.c);
        mesh->EndPolygon();
    }

    FbxNode* node = FbxNode::Create(scene, "Node");
    node->SetNodeAttribute(mesh);
    scene->GetRootNode()->AddChild(node);

    FbxExporter* exporter = FbxExporter::Create(manager, "");
    exporter->Initialize(filename.c_str(), -1, manager->GetIOSettings());
    exporter->Export(scene);
    exporter->Destroy();
    scene->Destroy();
    manager->Destroy();
}

// PNG読み込み → MarchingCubes → FBX出力処理
void RunProcess(HWND hwnd) {
    if (g_inputImagePath.empty()) {
        MessageBox(hwnd, L"No image selected!", L"Error", MB_OK);
        return;
    }

    int w, h;
    std::vector<unsigned char> img;
    if (!LoadGrayscaleImage(g_inputImagePath, w, h, img)) {
        MessageBox(hwnd, L"Image load failed!", L"Error", MB_OK);
        return;
    }

    std::vector<unsigned char> volume(VOXEL_RES * VOXEL_RES * VOXEL_RES);
    for (int z = 0; z < VOXEL_RES; ++z)
        for (int y = 0; y < VOXEL_RES; ++y)
            for (int x = 0; x < VOXEL_RES; ++x) {
                int ix = x * w / VOXEL_RES;
                int iy = y * h / VOXEL_RES;
                volume[x + y * VOXEL_RES + z * VOXEL_RES * VOXEL_RES] = img[ix + iy * w];
            }

    std::vector<Vec3> verts;
    std::vector<Triangle> tris;
    GenerateMarchingCubesMesh(volume, VOXEL_RES, VOXEL_RES, VOXEL_RES, verts, tris);
    ExportToFBX(verts, tris, "output.fbx");

    MessageBox(hwnd, L"Exported to output.fbx", L"Success", MB_OK);
}

// ファイルダイアログ
std::wstring OpenImageDialog(HWND hwnd) {
    wchar_t path[MAX_PATH] = {};
    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"PNG Files\0*.png\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) return path;
    return L"";
}

// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case 1: {
            std::wstring path = OpenImageDialog(hwnd);
            if (!path.empty()) {
                char mbpath[MAX_PATH];
                WideCharToMultiByte(CP_ACP, 0, path.c_str(), -1, mbpath, MAX_PATH, NULL, NULL);
                g_inputImagePath = mbpath;
                MessageBox(hwnd, path.c_str(), L"Image Selected", MB_OK);
            }
            break;
        }
        case 2:
            RunProcess(hwnd);
            break;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0); break;
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

// エントリーポイント（WinMain）
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    const wchar_t CLASS_NAME[] = L"MainWinClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"3D Model Generator", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        NULL, NULL, hInst, NULL);

    CreateWindow(L"BUTTON", L"Select PNG", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 40, 150, 30, hwnd, (HMENU)1, hInst, NULL);
    CreateWindow(L"BUTTON", L"Generate FBX", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        200, 40, 150, 30, hwnd, (HMENU)2, hInst, NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
