#include <windows.h>
#include <commdlg.h>
#include <string>
#include "PNGLoader.h"
#include "MeshGenerator.h"
#include "FBXExporter.h"

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const char* className = "ModelGenWindowClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(0, className, "3D Generator (No ImGui)", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    std::string pngPath;
    int polygonCount = 1000;

    while (true) {
        MSG msg;
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        int result = MessageBoxA(hWnd, "PNGを選択してFBX出力しますか？", "確認", MB_YESNOCANCEL);
        if (result == IDCANCEL) break;
        else if (result == IDYES) {
            char filename[MAX_PATH] = {};
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "PNG Files\0*.png\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST;
            if (GetOpenFileNameA(&ofn)) {
                pngPath = filename;
            }

            if (!pngPath.empty()) {
                Image2D img = LoadPNG(pngPath.c_str());
                Mesh3D mesh = GenerateMeshFromImage(img, polygonCount);
                std::string out = pngPath + std::string(".fbx");
                ExportToFBX(mesh, out.c_str());
                MessageBoxA(hWnd, "FBX出力が完了しました", "成功", MB_OK);
            }
        }
        else {
            polygonCount = (polygonCount >= 10000 ? 1000 : polygonCount + 1000);
        }
    }

    return 0;
}
