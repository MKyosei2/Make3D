#include <windows.h>
#include "GUIState.h"
#include "PartVolumeBuilder.h"
#include "MeshBuilder.h"
#include "MeshReducer.h"
#include "FBXExporter.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static GUIState state;
    switch (msg) {
    case WM_CREATE:
        CreateWindow("button", "èoóÕ",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 20, 100, 30, hwnd, (HMENU)1, NULL, NULL);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            auto images = state.imagePaths[state.selectedPart];
            Volume3D vol = BuildVolumeFromImages(images);
            Mesh3D mesh = BuildMeshFromVolume(vol);
            Mesh3D reduced = ReduceMesh(mesh, state.polygonCount);
            ExportMeshToFBX(reduced, "output.fbx", state.scaleUnit);
            MessageBox(hwnd, L"FBXèoóÕäÆóπ", L"ê¨å˜", MB_OK);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"Make3DApp";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(wc.lpszClassName, L"Make3D",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
