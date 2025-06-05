#include "AppState.h"
#include "GUIState.h"
#include "PreviewRenderer.h"

#include <windows.h>

AppState appState;
GUIState guiState;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static bool mouseDown = false;

    switch (msg) {
    case WM_LBUTTONDOWN:
        guiState.drawingRegion = true;
        guiState.dragStart.x = LOWORD(lParam);
        guiState.dragStart.y = HIWORD(lParam);
        guiState.currentDragRect.left = guiState.dragStart.x;
        guiState.currentDragRect.top = guiState.dragStart.y;
        return 0;

    case WM_MOUSEMOVE:
        if (guiState.drawingRegion) {
            guiState.currentDragRect.right = LOWORD(lParam);
            guiState.currentDragRect.bottom = HIWORD(lParam);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (guiState.drawingRegion) {
            guiState.drawingRegion = false;
            RECT r = guiState.currentDragRect;
            SelectionRegion region;
            region.x = min(r.left, r.right);
            region.y = min(r.top, r.bottom);
            region.width = abs(r.right - r.left);
            region.height = abs(r.bottom - r.top);
            guiState.partSelectionRegions.push_back(region);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        renderPreview(hdc, ps.rcPaint);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"3DModelerWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"3D Modeler",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
