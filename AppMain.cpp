#include <windows.h>
#include <commctrl.h>
#include <shlobj.h> // フォルダ選択用
#include "AppState.h"
#include "BuildVolumeFromImages.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shell32.lib")

AppState g_state;
HWND g_hPolygonInput = nullptr;
HWND g_hResolutionInput = nullptr;
HWND g_hImageButtons[(int)ViewDirection::Count];

void CreateControlUI(HWND hWnd) {
    CreateWindowW(L"STATIC", L"Polygon Count", WS_VISIBLE | WS_CHILD,
        10, 10, 100, 20, hWnd, nullptr, nullptr, nullptr);
    g_hPolygonInput = CreateWindowW(L"EDIT", L"10000", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        120, 10, 100, 20, hWnd, nullptr, nullptr, nullptr);

    const wchar_t* labels[] = { L"Front", L"Back", L"Left", L"Right", L"Top", L"Bottom" };
    for (int i = 0; i < (int)ViewDirection::Count; ++i) {
        CreateWindowW(L"STATIC", labels[i], WS_VISIBLE | WS_CHILD,
            10, 40 + i * 30, 100, 20, hWnd, nullptr, nullptr, nullptr);
        g_hImageButtons[i] = CreateWindowW(L"BUTTON", L"Browse", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            120, 40 + i * 30, 80, 25, hWnd, (HMENU)(100 + i), nullptr, nullptr);
    }

    CreateWindowW(L"BUTTON", L"Load All Images", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        10, 230, 190, 30, hWnd, (HMENU)200, nullptr, nullptr);
}

void UpdateAppStateFromUI() {
    wchar_t buf[32] = {};

    if (g_hPolygonInput) {
        GetWindowTextW(g_hPolygonInput, buf, 31);
        g_state.polygonCount = _wtoi(buf);
    }
    if (g_hResolutionInput) {
        GetWindowTextW(g_hResolutionInput, buf, 31);
        int res = _wtoi(buf);
        if (res >= 16 && res <= 1024)
            g_state.voxelResolution = res;
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        InitCommonControls();
        CreateControlUI(hWnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) >= 100 && LOWORD(wParam) < 100 + (int)ViewDirection::Count) {
            int viewIndex = LOWORD(wParam) - 100;
            OPENFILENAME ofn = { 0 };
            wchar_t filePath[MAX_PATH] = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"画像ファイル (*.png;*.bmp;*.jpg)\0*.png;*.bmp;*.jpg\0";
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
                if (!g_state.loadImageForView((ViewDirection)viewIndex, filePath)) {
                    MessageBoxW(hWnd, L"画像の読み込みに失敗しました。", L"エラー", MB_ICONERROR);
                }
                else {
                    MessageBoxW(hWnd, L"画像を読み込みました。", L"成功", MB_OK);
                }
            }
        }
        else if (LOWORD(wParam) == 200) {
            // ここでFBX出力処理を呼び出す（仮）
            MessageBoxW(hWnd, L"画像を読み込みました。FBX出力処理をここに追加してください。", L"成功", MB_OK);
        }
        break;

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
    return 0;
}
