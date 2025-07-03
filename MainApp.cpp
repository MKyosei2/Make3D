#include "MainApp.h"
#include "VolumeBuilder.h"
#include "MeshGenerator.h"
#include "FBXExporter.h"

MainApp* MainApp::appInstance = nullptr;

MainApp::MainApp(HINSTANCE hInstance) : hInst(hInstance) {}

int MainApp::run() {
    appInstance = this;
    createMainWindow();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

void MainApp::createMainWindow() {
    WNDCLASS wc = { };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"3DGenAppClass";
    RegisterClass(&wc);

    hWnd = CreateWindowEx(0, L"3DGenAppClass", L"3D Model Generator",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInst, nullptr);

    createControlUI(hWnd);
    ShowWindow(hWnd, SW_SHOW);
}

void MainApp::createControlUI(HWND hwnd) {
    const wchar_t* labels[6] = { L"Front", L"Back", L"Left", L"Right", L"Top", L"Bottom" };
    for (int i = 0; i < 6; ++i) {
        hImageButtons[i] = CreateWindow(L"BUTTON", labels[i], WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            20, 30 + i * 30, 80, 25, hwnd, (HMENU)(100 + i), hInst, nullptr);
    }

    CreateWindow(L"STATIC", L"Polygon Count:", WS_VISIBLE | WS_CHILD, 150, 30, 100, 20, hwnd, nullptr, hInst, nullptr);
    hPolygonCountInput = CreateWindow(L"EDIT", L"1000", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        250, 30, 80, 20, hwnd, nullptr, hInst, nullptr);

    CreateWindow(L"STATIC", L"Voxel Resolution:", WS_VISIBLE | WS_CHILD, 150, 60, 100, 20, hwnd, nullptr, hInst, nullptr);
    hVoxelResolutionInput = CreateWindow(L"EDIT", L"64", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        250, 60, 80, 20, hwnd, nullptr, hInst, nullptr);

    hFolderButton = CreateWindow(L"BUTTON", L"Load Folder", WS_VISIBLE | WS_CHILD,
        150, 100, 100, 25, hwnd, (HMENU)201, hInst, nullptr);

    hGenerateButton = CreateWindow(L"BUTTON", L"Generate Model", WS_VISIBLE | WS_CHILD,
        150, 140, 150, 30, hwnd, (HMENU)202, hInst, nullptr);

    hPreviewStatic = CreateWindow(L"STATIC", nullptr, WS_VISIBLE | WS_CHILD | SS_BLACKRECT,
        400, 30, 128, 128, hwnd, nullptr, hInst, nullptr);
}

void MainApp::updateAppStateFromUI() {
    wchar_t buffer[16];
    GetWindowText(hPolygonCountInput, buffer, 16);
    state.polygonCount = _wtoi(buffer);

    GetWindowText(hVoxelResolutionInput, buffer, 16);
    state.voxelResolution = _wtoi(buffer);
}

void MainApp::showImagePreview(ViewDirection dir) {
    HBITMAP bmp = state.getImageBitmap(dir);
    if (!bmp) return;

    HDC hdc = GetDC(hPreviewStatic);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);
    BitBlt(hdc, 0, 0, 128, 128, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
    ReleaseDC(hPreviewStatic, hdc);
}

void MainApp::loadImagesFromFolder() {
    // フォルダ選択ダイアログ処理（省略）
}

void MainApp::onGenerateModel() {
    updateAppStateFromUI();
    VolumeData volume;
    if (!generateVolumeFromImages(state, volume)) return;

    MeshGenerator generator;
    generator.setTargetPolygonCount(state.polygonCount);
    Mesh mesh = generator.generate(volume);
    exportMeshToFBX(mesh, L"output.fbx");
}

LRESULT CALLBACK MainApp::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        if (appInstance) {
            int id = LOWORD(wParam);
            if (id >= 100 && id < 106)
                appInstance->showImagePreview((ViewDirection)(id - 100));
            else if (id == 201)
                appInstance->loadImagesFromFolder();
            else if (id == 202)
                appInstance->onGenerateModel();
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}