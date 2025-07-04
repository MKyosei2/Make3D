#include "MainApp.h"
#include "VolumeBuilder.h"
#include "MeshGenerator.h"
#include "FBXExporter.h"
#include "PreviewRenderer.h"
#include <shobjidl.h>

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
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
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
        260, 30, 80, 20, hwnd, nullptr, hInst, nullptr);

    CreateWindow(L"STATIC", L"Voxel Resolution:", WS_VISIBLE | WS_CHILD, 150, 60, 100, 20, hwnd, nullptr, hInst, nullptr);
    hVoxelResolutionInput = CreateWindow(L"EDIT", L"64", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        260, 60, 80, 20, hwnd, nullptr, hInst, nullptr);

    CreateWindow(L"STATIC", L"Silhouette Threshold:", WS_VISIBLE | WS_CHILD, 150, 90, 120, 20, hwnd, nullptr, hInst, nullptr);
    hThresholdInput = CreateWindow(L"EDIT", L"128", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        280, 90, 60, 20, hwnd, nullptr, hInst, nullptr);

    hFolderButton = CreateWindow(L"BUTTON", L"Load Folder", WS_VISIBLE | WS_CHILD,
        150, 130, 100, 25, hwnd, (HMENU)201, hInst, nullptr);

    hSaveButton = CreateWindow(L"BUTTON", L"Save As", WS_VISIBLE | WS_CHILD,
        260, 130, 100, 25, hwnd, (HMENU)203, hInst, nullptr);

    hGenerateButton = CreateWindow(L"BUTTON", L"Generate Model", WS_VISIBLE | WS_CHILD,
        150, 170, 150, 30, hwnd, (HMENU)202, hInst, nullptr);

    hPreviewStatic = CreateWindow(L"STATIC", nullptr, WS_VISIBLE | WS_CHILD | SS_BLACKRECT,
        500, 30, 300, 300, hwnd, nullptr, hInst, nullptr);
}

void MainApp::updateAppStateFromUI() {
    wchar_t buffer[16];
    GetWindowText(hPolygonCountInput, buffer, 16);
    state.polygonCount = _wtoi(buffer);

    GetWindowText(hVoxelResolutionInput, buffer, 16);
    state.voxelResolution = _wtoi(buffer);

    GetWindowText(hThresholdInput, buffer, 16);
    state.silhouetteThreshold = (BYTE)_wtoi(buffer);
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
    IFileDialog* pFileDialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileDialog)))) {
        DWORD dwOptions;
        pFileDialog->GetOptions(&dwOptions);
        pFileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS);

        if (SUCCEEDED(pFileDialog->Show(hWnd))) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileDialog->GetResult(&pItem))) {
                PWSTR pszFolderPath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFolderPath))) {
                    const wchar_t* viewNames[6] = { L"front.png", L"back.png", L"left.png", L"right.png", L"top.png", L"bottom.png" };
                    for (int i = 0; i < 6; ++i) {
                        std::wstring path = std::wstring(pszFolderPath) + L"\\" + viewNames[i];
                        state.loadImageForView((ViewDirection)i, path);
                    }
                    CoTaskMemFree(pszFolderPath);
                }
                pItem->Release();
            }
        }
        pFileDialog->Release();
    }
}

void MainApp::selectSavePath() {
    IFileDialog* pDialog;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pDialog)))) {
        if (SUCCEEDED(pDialog->Show(hWnd))) {
            IShellItem* pItem;
            if (SUCCEEDED(pDialog->GetResult(&pItem))) {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                    state.outputFilePath = pszPath;
                    CoTaskMemFree(pszPath);
                }
                pItem->Release();
            }
        }
        pDialog->Release();
    }
}

void MainApp::onGenerateModel() {
    updateAppStateFromUI();
    VolumeData volume;
    if (!generateVolumeFromImages(state, volume)) return;

    MeshGenerator generator;
    generator.setTargetPolygonCount(state.polygonCount);
    Mesh mesh = generator.generate(volume);
    exportMeshToFBX(mesh, state.outputFilePath);
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
            else if (id == 203)
                appInstance->selectSavePath();
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}