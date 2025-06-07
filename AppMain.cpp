#include <windows.h>
#include "AppState.h"
#include "BuildVolumeFromImages.h"
#include <commctrl.h>

#pragma comment(lib, "Comctl32.lib")

AppState g_state;
HWND g_hPolygonInput = nullptr;
HWND g_hResolutionInput = nullptr;

void CreateControlUI(HWND hWnd) {
    CreateWindowW(L"STATIC", L"Polygon Count:",
        WS_VISIBLE | WS_CHILD,
        10, 10, 100, 20,
        hWnd, nullptr, nullptr, nullptr);

    g_hPolygonInput = CreateWindowW(L"EDIT", L"10000",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        120, 10, 80, 20,
        hWnd, nullptr, nullptr, nullptr);

    CreateWindowW(L"STATIC", L"Resolution:",
        WS_VISIBLE | WS_CHILD,
        10, 40, 100, 20,
        hWnd, nullptr, nullptr, nullptr);

    g_hResolutionInput = CreateWindowW(L"EDIT", L"128",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        120, 40, 80, 20,
        hWnd, nullptr, nullptr, nullptr);
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
        UpdateAppStateFromUI();
        return 0;

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
