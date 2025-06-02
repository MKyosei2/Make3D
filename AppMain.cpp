#include <windows.h>
#include "AppState.h"
#include "VolumeUtils.h"
#include "ImageUtils.h"
#include "MeshUtils.h"
#include "PartProcessor.h"
#include "FBXExporter.h"

// --- ウィンドウプロシージャ（最小限） ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"Make3DClass";
    RegisterClass(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, L"Make3D", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);

    // --- 初期化と仮GUI処理（本格UIはWin32で後日実装） ---
    GUIState state;
    state.polygonCount = 1000;
    state.exportCombined = true;
    state.scaleUnit = ExportScaleUnit::Unity;

    // 仮ロード（正面画像だけ）
    Image2D frontImg;
    LoadPNG("front.png", frontImg);
    state.partViewImages[PartType::Body][ViewType::Front] = "front.png";

    // --- 3D構築処理 ---
    auto volumes = BuildPartVolumes({
        { PartType::Body, {{ ViewType::Front, frontImg }} }
        });

    std::vector<Mesh3D> meshes;
    for (const auto& [part, vol] : volumes) {
        Mesh3D mesh = BuildMeshFromVolume(vol);
        mesh = ReduceMesh(mesh, state.polygonCount);
        meshes.push_back(mesh);
    }

    // --- FBX出力 ---
    ExportToFBX("output.fbx", meshes);

    // --- メッセージループ ---
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}