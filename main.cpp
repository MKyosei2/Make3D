#include <windows.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <string>
#include "PNGLoader.h"
#include "VolumeBuilder.h"
#include "MeshBuilder.h"
#include "FBXExporter.h"

#pragma comment(lib, "Shlwapi.lib")

char g_pngPath[MAX_PATH] = "";
int g_polyCount = 5000;

bool GenerateFBX(const char* pngPath, const char* fbxPath, int polyCount) {
    try {
        Image2D image = LoadPNG(pngPath);
        Volume volume = BuildVolumeFromSilhouette(image, 64);
        Mesh3D mesh = GenerateMesh(volume, polyCount);
        ExportToFBX(mesh, fbxPath);
        return true;
    }
    catch (const std::exception& e) {
        MessageBoxA(NULL, e.what(), "Error", MB_ICONERROR);
        return false;
    }
}

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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateWindowA("button", "選択", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 20, 60, 25, hwnd, (HMENU)1, NULL, NULL);
        CreateWindowA("edit", g_pngPath, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 90, 20, 300, 25, hwnd, (HMENU)2, NULL, NULL);
        CreateWindowA("static", "ポリゴン数:", WS_CHILD | WS_VISIBLE, 20, 60, 80, 25, hwnd, NULL, NULL, NULL);
        CreateWindowA("edit", "5000", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 110, 60, 100, 25, hwnd, (HMENU)3, NULL, NULL);
        CreateWindowA("button", "生成", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 230, 60, 80, 25, hwnd, (HMENU)4, NULL, NULL);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) BrowseForPNG(hwnd);
        if (LOWORD(wParam) == 4) {
            GetDlgItemTextA(hwnd, 2, g_pngPath, MAX_PATH);
            char polyStr[16] = {};
            GetDlgItemTextA(hwnd, 3, polyStr, 15);
            g_polyCount = atoi(polyStr);
            if (!PathFileExistsA(g_pngPath)) {
                MessageBoxA(hwnd, "PNGファイルが見つかりません。", "エラー", MB_ICONERROR);
                return 0;
            }
            char fbxPath[MAX_PATH];
            strcpy_s(fbxPath, MAX_PATH, g_pngPath);
            char* dot = strrchr(fbxPath, '.');
            if (dot) strcpy_s(dot, MAX_PATH - (dot - fbxPath), ".fbx");

            if (GenerateFBX(g_pngPath, fbxPath, g_polyCount)) {
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

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "ImageTo3DWindow";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("ImageTo3DWindow", "ImageTo3D Converter", WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 450, 150, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}