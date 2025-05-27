#include <windows.h>
#include <commdlg.h>
#include <string>
#include <sstream>
#include <shlwapi.h>

#include "PNGLoader.h"
#include "VolumeBuilder.h"
#include "MeshBuilder.h"
#include "FBXExporter.h"
#include "PartVolumeBuilder.h"
#pragma comment(lib, "Shlwapi.lib")

// グローバル
char g_pngPath[MAX_PATH] = "";
int g_polyCount = 5000;

// FBX出力関数
bool GenerateFBX(const char* pngPath, const char* fbxPath, int polyCount) {
    try {
        Image2D image = LoadPNG(pngPath);

        // 仮：領域を手動定義（GUI未実装なのでここで定義）
        std::vector<PartRegion> regions = {
            { image.width / 4, image.height / 8, image.width / 2, image.height / 4 }, // Head
            { image.width / 8, image.height / 4, image.width / 4, image.height / 2 }, // Left Arm
            { image.width * 5 / 8, image.height / 4, image.width / 4, image.height / 2 }, // Right Arm
            { image.width / 4, image.height / 4, image.width / 2, image.height / 2 }, // Body
            { image.width / 3, image.height * 3 / 4, image.width / 6, image.height / 4 } // Leg
        };

        auto parts = BuildPartsFromImage(image, regions, 64);

        // FBXマネージャ・シーン初期化
        FbxManager* manager = FbxManager::Create();
        FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
        manager->SetIOSettings(ios);
        FbxScene* scene = FbxScene::Create(manager, "Scene");
        FbxNode* root = scene->GetRootNode();

        int partIndex = 0;
        for (const auto& part : parts) {
            Mesh3D mesh = GenerateMesh(part.volume, polyCount);

            // メッシュノード作成
            FbxMesh* fbxMesh = FbxMesh::Create(scene, ("Part" + std::to_string(partIndex)).c_str());
            fbxMesh->InitControlPoints(mesh.vertices.size());
            for (size_t i = 0; i < mesh.vertices.size(); ++i) {
                auto& v = mesh.vertices[i];
                fbxMesh->SetControlPointAt(FbxVector4(v.x, v.y, v.z), i);
            }
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                fbxMesh->BeginPolygon();
                fbxMesh->AddPolygon(mesh.indices[i]);
                fbxMesh->AddPolygon(mesh.indices[i + 1]);
                fbxMesh->AddPolygon(mesh.indices[i + 2]);
                fbxMesh->EndPolygon();
            }
            FbxNode* meshNode = FbxNode::Create(scene, ("MeshPart" + std::to_string(partIndex)).c_str());
            meshNode->SetNodeAttribute(fbxMesh);
            root->AddChild(meshNode);
            ++partIndex;
        }

        FbxExporter* exporter = FbxExporter::Create(manager, "");
        exporter->Initialize(fbxPath, -1, manager->GetIOSettings());
        exporter->Export(scene);
        exporter->Destroy();
        manager->Destroy();

        return true;
    }
    catch (const std::exception& e) {
        MessageBoxA(NULL, e.what(), "Error", MB_ICONERROR);
        return false;
    }
}

// ファイル選択ダイアログ
void BrowseForPNG(HWND hwnd) {
    OPENFILENAMEA ofn = { sizeof(ofn) };
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "PNG Files\0*.png\0All Files\0*.*\0";
    ofn.lpstrFile = g_pngPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

// メインウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateWindowA("button", "選択", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 20, 60, 25, hwnd, (HMENU)1, NULL, NULL);
        CreateWindowA("edit", g_pngPath, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            90, 20, 300, 25, hwnd, (HMENU)2, NULL, NULL);
        CreateWindowA("static", "ポリゴン数:", WS_CHILD | WS_VISIBLE,
            20, 60, 80, 25, hwnd, NULL, NULL, NULL);
        CreateWindowA("edit", "5000", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
            110, 60, 100, 25, hwnd, (HMENU)3, NULL, NULL);
        CreateWindowA("button", "生成", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            230, 60, 80, 25, hwnd, (HMENU)4, NULL, NULL);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) { // ファイル選択
            BrowseForPNG(hwnd);
        }
        if (LOWORD(wParam) == 4) { // 実行
            GetDlgItemTextA(hwnd, 2, g_pngPath, MAX_PATH);
            char polyStr[16] = {};
            GetDlgItemTextA(hwnd, 3, polyStr, 15);
            g_polyCount = atoi(polyStr);
            if (!PathFileExistsA(g_pngPath)) {
                MessageBoxA(hwnd, "PNGファイルが見つかりません。", "エラー", MB_ICONERROR);
                return 0;
            }

            std::string fbxPath = g_pngPath;
            fbxPath = fbxPath.substr(0, fbxPath.find_last_of('.')) + ".fbx";

            if (GenerateFBX(g_pngPath, fbxPath.c_str(), g_polyCount)) {
                MessageBoxA(hwnd, "FBXを出力しました。", "完了", MB_OK);
            }
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// WinMain
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "ImageTo3DWindow";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("ImageTo3DWindow", "ImageTo3D Converter",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 150,
        NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
