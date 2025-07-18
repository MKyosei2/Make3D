#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION

#include <windows.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <fstream>
#include <fbxsdk.h>
#include "stb_image.h" // 同じフォルダに配置すること

const int WIDTH = 256;
const int HEIGHT = 256;
float depthMap[WIDTH][HEIGHT];
bool voxel[WIDTH][HEIGHT][WIDTH];
std::wstring loadedImagePath = L"";
int polygonResolution = 32;

HWND hWndMain, hPolygonInput, hOutputButton, hLoadButton;

// --------- RAW読込（簡易） ----------
bool LoadGrayscaleRAW(const std::wstring& path, unsigned char* buffer, int& width, int& height) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) return false;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    std::vector<unsigned char> raw(size);
    fread(raw.data(), 1, size, file);
    fclose(file);
    memcpy(buffer, raw.data(), WIDTH * HEIGHT);
    width = WIDTH; height = HEIGHT;
    return true;
}

// --------- PNG読込（stb_image） ----------
bool LoadGrayscalePNG(const std::wstring& path, unsigned char* buffer, int& width, int& height) {
    std::string mbPath(path.begin(), path.end()); // UTF-16 → UTF-8仮定
    int channels;
    unsigned char* data = stbi_load(mbPath.c_str(), &width, &height, &channels, 1); // 強制グレースケール
    if (!data) return false;
    memcpy(buffer, data, width * height);
    stbi_image_free(data);
    return true;
}

void GenerateDepthMap(unsigned char* image) {
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
            depthMap[x][y] = 1.0f - (image[y * WIDTH + x] / 255.0f);
}

void BuildVoxel() {
    memset(voxel, 0, sizeof(voxel));
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            int depthZ = static_cast<int>(depthMap[x][y] * (WIDTH - 1));
            for (int z = 0; z <= depthZ; ++z) voxel[x][y][z] = true;
        }
    }
}

void GenerateMesh(std::vector<FbxVector4>& vertices, std::vector<int>& indices) {
    for (int x = 1; x < WIDTH - 1; ++x)
        for (int y = 1; y < HEIGHT - 1; ++y)
            for (int z = 1; z < WIDTH - 1; ++z)
                if (voxel[x][y][z] && !voxel[x + 1][y][z]) {
                    vertices.emplace_back(x, y, z);
                    vertices.emplace_back(x + 1, y, z);
                    vertices.emplace_back(x, y + 1, z);
                    indices.push_back(vertices.size() - 3);
                    indices.push_back(vertices.size() - 2);
                    indices.push_back(vertices.size() - 1);
                }
}

void ExportToFBX(const char* filename, const std::vector<FbxVector4>& vertices, const std::vector<int>& indices) {
    FbxManager* manager = FbxManager::Create();
    FbxScene* scene = FbxScene::Create(manager, "Scene");
    FbxMesh* mesh = FbxMesh::Create(scene, "Mesh");
    mesh->InitControlPoints(vertices.size());
    for (int i = 0; i < vertices.size(); ++i) mesh->SetControlPointAt(vertices[i], i);

    for (int i = 0; i < indices.size(); i += 3) {
        mesh->BeginPolygon();
        mesh->AddPolygon(indices[i]);
        mesh->AddPolygon(indices[i + 1]);
        mesh->AddPolygon(indices[i + 2]);
        mesh->EndPolygon();
    }

    FbxNode* node = FbxNode::Create(scene, "Node");
    node->SetNodeAttribute(mesh);
    scene->GetRootNode()->AddChild(node);

    FbxExporter* exporter = FbxExporter::Create(manager, "");
    exporter->Initialize(filename, -1, manager->GetIOSettings());
    exporter->Export(scene);
    exporter->Destroy();
    manager->Destroy();
}

void LoadImage() {
    OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
    wchar_t filePath[MAX_PATH] = L"";
    ofn.lpstrFilter = L"Image Files (*.png;*.raw)\0*.png;*.raw\0All Files\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        loadedImagePath = filePath;
    }
}

void OutputFBX() {
    unsigned char buffer[WIDTH * HEIGHT] = { 0 };
    int w = 0, h = 0;
    bool isPng = loadedImagePath.size() > 4 &&
        (loadedImagePath.substr(loadedImagePath.size() - 4) == L".png" ||
            loadedImagePath.substr(loadedImagePath.size() - 4) == L".PNG");

    bool success = false;
    if (isPng) success = LoadGrayscalePNG(loadedImagePath, buffer, w, h);
    else       success = LoadGrayscaleRAW(loadedImagePath, buffer, w, h);
    if (!success) return;

    GenerateDepthMap(buffer);
    BuildVoxel();

    std::vector<FbxVector4> vertices;
    std::vector<int> indices;
    GenerateMesh(vertices, indices);
    ExportToFBX("output.fbx", vertices, indices);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        hLoadButton = CreateWindowW(L"BUTTON", L"画像読込", WS_CHILD | WS_VISIBLE, 10, 10, 100, 30, hWnd, (HMENU)1, NULL, NULL);
        hPolygonInput = CreateWindowW(L"EDIT", L"32", WS_CHILD | WS_VISIBLE | WS_BORDER, 120, 10, 60, 30, hWnd, NULL, NULL, NULL);
        hOutputButton = CreateWindowW(L"BUTTON", L"FBX出力", WS_CHILD | WS_VISIBLE, 200, 10, 100, 30, hWnd, (HMENU)2, NULL, NULL);
        break;
    case WM_COMMAND:
        if (LOWORD(wp) == 1) LoadImage();
        if (LOWORD(wp) == 2) OutputFBX();
        break;
    case WM_DESTROY:
        PostQuitMessage(0); break;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"TK230348Win";
    RegisterClassW(&wc);
    hWndMain = CreateWindowW(L"TK230348Win", L"3D生成ツール", WS_OVERLAPPEDWINDOW, 100, 100, 350, 120, NULL, NULL, hInst, NULL);
    ShowWindow(hWndMain, SW_SHOW);

    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
