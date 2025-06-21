#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <gdiplus.h>
#include "AppState.h"
#include "MeshUtils.h"
#include "MeshGenerator.h"
#include "FBXExporter.h"
#include "BuildVolumeFromImages.h"
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shell32.lib")

using namespace Gdiplus;

AppState g_state;
HWND g_hPolygonInput = nullptr;
HWND g_hResolutionInput = nullptr;
HWND g_hImageButtons[(int)ViewDirection::Count];
HWND g_hImagePreviews[(int)ViewDirection::Count];
ULONG_PTR gdiplusToken;

void CreateControlUI(HWND hWnd) {
    CreateWindowW(L"STATIC", L"Polygon Count", WS_VISIBLE | WS_CHILD,
        10, 10, 100, 20, hWnd, nullptr, nullptr, nullptr);
    g_hPolygonInput = CreateWindowW(L"EDIT", L"10000", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        120, 10, 100, 20, hWnd, nullptr, nullptr, nullptr);

    CreateWindowW(L"STATIC", L"Voxel Res", WS_VISIBLE | WS_CHILD,
        10, 35, 100, 20, hWnd, nullptr, nullptr, nullptr);
    g_hResolutionInput = CreateWindowW(L"EDIT", L"128", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        120, 35, 100, 20, hWnd, nullptr, nullptr, nullptr);

    CreateWindowW(L"BUTTON", L"Create 3D Model", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        220, 420, 190, 30, hWnd, (HMENU)300, nullptr, nullptr);

    const wchar_t* labels[] = { L"Front", L"Back", L"Left", L"Right", L"Top", L"Bottom" };
    for (int i = 0; i < (int)ViewDirection::Count; ++i) {
        CreateWindowW(L"STATIC", labels[i], WS_VISIBLE | WS_CHILD,
            10, 70 + i * 60, 100, 20, hWnd, nullptr, nullptr, nullptr);
        g_hImageButtons[i] = CreateWindowW(L"BUTTON", L"Browse", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            120, 70 + i * 60, 80, 25, hWnd, (HMENU)(100 + i), nullptr, nullptr);
        g_hImagePreviews[i] = CreateWindowW(L"STATIC", nullptr, WS_VISIBLE | WS_CHILD | SS_BITMAP,
            220, 70 + i * 60, 100, 100, hWnd, nullptr, nullptr, nullptr);
    }

    CreateWindowW(L"BUTTON", L"Load All Images", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        10, 420, 190, 30, hWnd, (HMENU)200, nullptr, nullptr);
}

void UpdateAppStateFromUI() {
    wchar_t buf[32] = {};

    if (g_hPolygonInput) {
        GetWindowTextW(g_hPolygonInput, buf, 31);
        int poly = _wtoi(buf);
        if (poly >= 100 && poly <= 1000000)
            g_state.polygonCount = poly;
    }

    if (g_hResolutionInput) {
        GetWindowTextW(g_hResolutionInput, buf, 31);
        int res = _wtoi(buf);
        if (res >= 16 && res <= 512)
            g_state.voxelResolution = res;
    }
}

void ShowImagePreview(int viewIndex, HBITMAP hBitmap) {
    if (g_hImagePreviews[viewIndex]) {
        HDC hdc = GetDC(g_hImagePreviews[viewIndex]);
        HDC memDC = CreateCompatibleDC(hdc);
        SelectObject(memDC, hBitmap);

        BITMAP bmp;
        GetObject(hBitmap, sizeof(BITMAP), &bmp);
        StretchBlt(hdc, 0, 0, 100, 100, memDC, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY);

        DeleteDC(memDC);
        ReleaseDC(g_hImagePreviews[viewIndex], hdc);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
        InitCommonControls();
        CreateControlUI(hWnd);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) >= 100 && LOWORD(wParam) < 100 + (int)ViewDirection::Count) {
            int viewIndex = LOWORD(wParam) - 100;
            OPENFILENAME ofn = { 0 };
            wchar_t filePath[MAX_PATH] = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"Image Files (*.png;*.bmp;*.jpg)\0*.png;*.bmp;*.jpg\0";
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
                if (!g_state.loadImageForView((ViewDirection)viewIndex, filePath)) {
                    MessageBoxW(hWnd, L"画像の読み込みに失敗しました。", L"エラー", MB_ICONERROR);
                }
                else {
                    HBITMAP hBitmap = g_state.images[viewIndex];
                    ShowImagePreview(viewIndex, hBitmap);
                }
            }
        }
        else if (LOWORD(wParam) == 200) {
            BROWSEINFO bi = { 0 };
            bi.lpszTitle = L"画像フォルダを選択してください";
            LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
            if (pidl) {
                wchar_t path[MAX_PATH];
                if (SHGetPathFromIDList(pidl, path)) {
                    if (!g_state.loadImages(path)) {
                        MessageBoxW(hWnd, L"画像の一括読み込みに失敗しました。", L"エラー", MB_ICONERROR);
                    }
                    else {
                        for (int i = 0; i < (int)ViewDirection::Count; ++i) {
                            HBITMAP hBitmap = g_state.getImageBitmap((ViewDirection)i);
                            if (hBitmap) {
                                ShowImagePreview(i, hBitmap);
                            }
                        }
                        MessageBoxW(hWnd, L"画像を読み込みました。FBX出力処理をここに追加してください。", L"成功", MB_OK);
                    }
                }
            }
        }
        else if (LOWORD(wParam) == 300) {
            UpdateAppStateFromUI();
            VolumeData volume(g_state.voxelResolution,
                g_state.voxelResolution,
                g_state.voxelResolution);

            MeshGenerator generator;
            generator.setTargetPolygonCount(g_state.polygonCount);
            Mesh mesh = generator.generate(volume);
            if (generateVolumeFromImages(g_state, volume)) {
                MeshGenerator generator;
                generator.setTargetPolygonCount(g_state.polygonCount);
                Mesh mesh = generator.generate(volume);

                // ★ オプション：出力直前に確認用の三角形・頂点数表示
                /*
                wchar_t msg[256];
                swprintf_s(msg, L"頂点数: %d\n三角形数: %d", (int)mesh.vertices.size(), (int)mesh.triangles.size());
                MessageBoxW(hWnd, msg, L"確認", MB_OK);
                */

                exportMeshToFBX(L"output.fbx", mesh);
                MessageBoxW(hWnd, L"FBXファイルを出力しました。", L"成功", MB_OK);
            }
            else {
                MessageBoxW(hWnd, L"3Dモデルの生成に失敗しました。", L"エラー", MB_ICONERROR);
            }
        }
        break;

    case WM_SIZE: {
        for (int i = 0; i < (int)ViewDirection::Count; ++i) {
            MoveWindow(g_hImageButtons[i], 120, 70 + i * 60, 80, 25, TRUE);
            MoveWindow(g_hImagePreviews[i], 220, 70 + i * 60, 100, 100, TRUE);
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"My3DApp";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    HWND hWnd = CreateWindowW(L"My3DApp", L"3D Builder",
        WS_OVERLAPPEDWINDOW,
        100, 100, 640, 480,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    GdiplusShutdown(gdiplusToken);
    return 0;
}
