#include "MainApp.h"
#include "VolumeBuilder.h"
#include "MeshGenerator.h"
#include "FBXExporter.h"
#include "PreviewRenderer.h"
#include <shobjidl.h>

MainApp* MainApp::appInstance = nullptr;

void AppState::clearImages() {
    for (int i = 0; i < ViewDirection::Count; ++i) {
        if (images[i]) {
            DeleteObject(images[i]);
            images[i] = nullptr;
        }
    }
}

bool AppState::loadImageForView(ViewDirection dir, const std::wstring& imagePath) {
    HBITMAP bmp = (HBITMAP)LoadImage(nullptr, imagePath.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (!bmp) return false;
    if (images[dir]) DeleteObject(images[dir]);
    images[dir] = bmp;
    return true;
}

HBITMAP AppState::getImageBitmap(ViewDirection dir) const {
    return images[dir];
}

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
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"3DGenAppClass";
    RegisterClass(&wc);

    hWnd = CreateWindowEx(0, L"3DGenAppClass", L"3D Model Generator",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
        nullptr, nullptr, hInst, nullptr);

    createControlUI(hWnd);
    ShowWindow(hWnd, SW_SHOW);

    preview = new PreviewRenderer(hPreviewStatic);
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

    hSingleImageCheck = CreateWindow(L"BUTTON", L"Single Image Mode", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        150, 120, 200, 20, hwnd, (HMENU)301, hInst, nullptr);

    hFolderButton = CreateWindow(L"BUTTON", L"Load Folder", WS_VISIBLE | WS_CHILD,
        150, 150, 100, 25, hwnd, (HMENU)201, hInst, nullptr);

    hSaveButton = CreateWindow(L"BUTTON", L"Save As", WS_VISIBLE | WS_CHILD,
        260, 150, 100, 25, hwnd, (HMENU)203, hInst, nullptr);

    hClearButton = CreateWindow(L"BUTTON", L"Clear Images", WS_VISIBLE | WS_CHILD,
        370, 150, 100, 25, hwnd, (HMENU)204, hInst, nullptr);

    hGenerateButton = CreateWindow(L"BUTTON", L"Generate Model", WS_VISIBLE | WS_CHILD,
        150, 190, 150, 30, hwnd, (HMENU)202, hInst, nullptr);

    hStatusText = CreateWindow(L"STATIC", L"Status: Ready", WS_VISIBLE | WS_CHILD,
        150, 230, 300, 20, hwnd, nullptr, hInst, nullptr);

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

    state.isSingleImage = (SendMessage(hSingleImageCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
}

void MainApp::showImagePreview(ViewDirection dir) {
    if (preview) preview->render();
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
    SetWindowText(hStatusText, L"Status: Images Loaded");
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

void MainApp::onClearImages() {
    state.clearImages();
    SetWindowText(hStatusText, L"Status: Images Cleared");
}

void MainApp::onGenerateModel() {
    SetWindowText(hStatusText, L"Status: Generating...");
    updateAppStateFromUI();

    VolumeData volume;
    if (!generateVolumeFromImages(state, volume)) {
        SetWindowText(hStatusText, L"Error: Failed to generate volume");
        return;
    }

    MeshGenerator generator;
    generator.setTargetPolygonCount(state.polygonCount);
    Mesh mesh = generator.generate(volume);

    if (!exportMeshToFBX(mesh, state.outputFilePath)) {
        SetWindowText(hStatusText, L"Error: Failed to export FBX");
        return;
    }

    if (preview) {
        preview->setMesh(mesh);
        preview->render();
    }
    SetWindowText(hStatusText, L"Status: Model Generated and Saved");
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
            else if (id == 204)
                appInstance->onClearImages();
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}