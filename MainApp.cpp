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

std::string g_frontPath = "", g_sidePath = "", g_topPath = "";
const float ISO_LEVEL = 0.5f;
const int VOXEL_RES = 128;

bool LoadBinaryMask(const std::string& path, int& w, int& h, std::vector<unsigned char>& mask) {
    int comp;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 1);
    if (!data) return false;
    mask.assign(data, data + w * h);
    stbi_image_free(data);
    return true;
}

void GenerateIntersectedVolume(std::vector<unsigned char>& volume) {
    int fw, fh, sw, sh, tw, th;
    std::vector<unsigned char> front, side, top;
    if (!LoadBinaryMask(g_frontPath, fw, fh, front)) return;
    if (!LoadBinaryMask(g_sidePath, sw, sh, side)) return;
    if (!LoadBinaryMask(g_topPath, tw, th, top))  return;

    volume.resize(VOXEL_RES * VOXEL_RES * VOXEL_RES);
    for (int z = 0; z < VOXEL_RES; ++z)
        for (int y = 0; y < VOXEL_RES; ++y)
            for (int x = 0; x < VOXEL_RES; ++x) {
                int fx = x * fw / VOXEL_RES, fy = y * fh / VOXEL_RES;
                int sx = z * sw / VOXEL_RES, sy = y * sh / VOXEL_RES;
                int tx = x * tw / VOXEL_RES, tz = z * th / VOXEL_RES;

                bool f = front[fx + fy * fw] > 128;
                bool s = side[sx + sy * sw] > 128;
                bool t = top[tx + tz * tw] > 128;

                volume[x + y * VOXEL_RES + z * VOXEL_RES * VOXEL_RES] = (f && s && t) ? 255 : 0;
            }
}

Vec3 VertexInterp(const Vec3& p1, const Vec3& p2, float valp1, float valp2) {
    float mu = (ISO_LEVEL - valp1) / (valp2 - valp1 + 1e-6f);
    return {
        p1.x + mu * (p2.x - p1.x),
        p1.y + mu * (p2.y - p1.y),
        p1.z + mu * (p2.z - p1.z)
    };
}

void GenerateMarchingCubesMesh(const std::vector<unsigned char>& volume, int resX, int resY, int resZ,
    std::vector<Vec3>& vertices, std::vector<Triangle>& triangles) {
    const int edgeVertexMap[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    const Vec3 offset[8] = {
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},
        {0,0,1},{1,0,1},{1,1,1},{0,1,1}
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
                    int xi = x + (int)offset[i].x;
                    int yi = y + (int)offset[i].y;
                    int zi = z + (int)offset[i].z;
                    val[i] = at(xi, yi, zi);
                    pos[i] = { (float)xi, (float)yi, (float)zi };
                    if (val[i] < ISO_LEVEL) cubeIndex |= (1 << i);
                }
                int edges = edgeTable[cubeIndex];
                if (edges == 0) continue;

                Vec3 vertList[12];
                for (int i = 0; i < 12; ++i)
                    if (edges & (1 << i)) {
                        int a = edgeVertexMap[i][0];
                        int b = edgeVertexMap[i][1];
                        vertList[i] = VertexInterp(pos[a], pos[b], val[a], val[b]);
                    }

                for (int i = 0; triTable[cubeIndex][i] != -1; i += 3) {
                    int idx = (int)vertices.size();
                    vertices.push_back(vertList[triTable[cubeIndex][i]]);
                    vertices.push_back(vertList[triTable[cubeIndex][i + 1]]);
                    vertices.push_back(vertList[triTable[cubeIndex][i + 2]]);
                    triangles.push_back({ idx, idx + 1, idx + 2 });
                }
            }
}

void ExportToFBX(const std::vector<Vec3>& vertices, const std::vector<Triangle>& triangles, const std::string& filename) {
    FbxManager* manager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);

    FbxScene* scene = FbxScene::Create(manager, "Scene");
    FbxMesh* mesh = FbxMesh::Create(scene, "Mesh");

    mesh->InitControlPoints((int)vertices.size());
    for (int i = 0; i < vertices.size(); ++i)
        mesh->SetControlPointAt(FbxVector4(vertices[i].x, vertices[i].y, vertices[i].z), i);

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

std::wstring OpenFileDialog(HWND hwnd) {
    wchar_t path[MAX_PATH] = {};
    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"PNG Files\0*.png\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    return GetOpenFileName(&ofn) ? std::wstring(path) : L"";
}

void RunExport(HWND hwnd) {
    std::vector<unsigned char> volume;
    GenerateIntersectedVolume(volume);

    std::vector<Vec3> verts;
    std::vector<Triangle> tris;
    GenerateMarchingCubesMesh(volume, VOXEL_RES, VOXEL_RES, VOXEL_RES, verts, tris);
    ExportToFBX(verts, tris, "output.fbx");
    MessageBox(hwnd, L"FBX Exported: output.fbx", L"Done", MB_OK);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case 1: {
            auto w = OpenFileDialog(hwnd);
            if (!w.empty()) {
                char buf[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, buf, MAX_PATH, 0, 0);
                g_frontPath = buf;
            }
            break;
        }
        case 2: {
            auto w = OpenFileDialog(hwnd);
            if (!w.empty()) {
                char buf[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, buf, MAX_PATH, 0, 0);
                g_sidePath = buf;
            }
            break;
        }
        case 3: {
            auto w = OpenFileDialog(hwnd);
            if (!w.empty()) {
                char buf[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, buf, MAX_PATH, 0, 0);
                g_topPath = buf;
            }
            break;
        }
        case 4:
            RunExport(hwnd); break;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0); break;
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"VoxelWin";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(L"VoxelWin", L"3面シルエット → 3D再構成",
        WS_OVERLAPPEDWINDOW, 100, 100, 400, 250, 0, 0, hInst, 0);

    CreateWindow(L"BUTTON", L"Front", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 30, 30, 100, 30, hwnd, (HMENU)1, hInst, 0);
    CreateWindow(L"BUTTON", L"Side", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 150, 30, 100, 30, hwnd, (HMENU)2, hInst, 0);
    CreateWindow(L"BUTTON", L"Top", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 270, 30, 100, 30, hwnd, (HMENU)3, hInst, 0);
    CreateWindow(L"BUTTON", L"Export FBX", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 130, 100, 120, 40, hwnd, (HMENU)4, hInst, 0);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        TranslateMessage(&msg); DispatchMessage(&msg);
    }
    return 0;
}
