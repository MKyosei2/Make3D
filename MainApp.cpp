#include "MainApp.h"
#include <windows.h>
#include <commdlg.h>
#include <gdiplus.h>
#include "VolumeBuilder.h"
#include "MeshGenerator.h"
#include "PreviewRenderer.h"

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

MainApp::MainApp(HINSTANCE hInst) : hInstance(hInst), hwnd(nullptr), hSilhouette(nullptr), hStatusText(nullptr)
{
    ZeroMemory(buttons, sizeof(buttons));
    ZeroMemory(images, sizeof(images));
    silhouetteThreshold = 64;
}

bool MainApp::initialize()
{
    // GDI+ 初期化
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    WNDCLASS wc = {};
    wc.lpfnWndProc = MainApp::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"3DModelGeneratorClass";
    RegisterClass(&wc);

    hwnd = CreateWindowEx(0, L"3DModelGeneratorClass", L"3D Model Generator",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, this);

    if (!hwnd) return false;
    ShowWindow(hwnd, SW_SHOW);

    const wchar_t* labels[] = { L"Front", L"Back", L"Left", L"Right", L"Top", L"Bottom" };
    for (int i = 0; i < 6; ++i)
    {
        buttons[i] = CreateWindow(L"BUTTON", labels[i], WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            20, 20 + i * 30, 80, 25, hwnd, (HMENU)(100 + i), hInstance, nullptr);
    }

    CreateWindow(L"STATIC", L"Silhouette Threshold:", WS_VISIBLE | WS_CHILD,
        120, 80, 140, 20, hwnd, nullptr, hInstance, nullptr);

    hSilhouette = CreateWindow(L"EDIT", L"64", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        280, 80, 60, 20, hwnd, nullptr, hInstance, nullptr);

    CreateWindow(L"BUTTON", L"Generate Model", WS_VISIBLE | WS_CHILD,
        120, 120, 140, 30, hwnd, (HMENU)202, hInstance, nullptr);

    hStatusText = CreateWindow(L"STATIC", L"Status: Ready", WS_VISIBLE | WS_CHILD,
        120, 160, 400, 20, hwnd, nullptr, hInstance, nullptr);

    if (!preview.Initialize(hwnd)) {
        MessageBox(hwnd, L"PreviewRenderer 初期化失敗", L"エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

int MainApp::run()
{
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK MainApp::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MainApp* app = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        app = (MainApp*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)app);
    }
    else {
        app = (MainApp*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (!app) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id >= 100 && id < 106) {
            OPENFILENAME ofn = {};
            wchar_t filename[MAX_PATH] = L"";
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"Image Files (*.png;*.bmp)\0*.png;*.bmp\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
                ViewDirection dir = (ViewDirection)(id - 100);
                if (!app->loadImageForView(dir, filename)) {
                    MessageBox(hwnd, L"画像の読み込みに失敗しました。", L"エラー", MB_OK | MB_ICONERROR);
                }
                else {
                    SetWindowText(app->hStatusText, L"Status: Image Loaded");
                }
            }
        }
        else if (id == 202) {
            app->silhouetteThreshold = GetDlgItemInt(hwnd, GetDlgCtrlID(app->hSilhouette), nullptr, FALSE);
            SetWindowText(app->hStatusText, L"Status: Generating...");
            if (!app->generateModel()) {
                SetWindowText(app->hStatusText, L"Status: Error");
            }
            else {
                SetWindowText(app->hStatusText, L"Status: Model Generated");
            }
        }
        break;
    }

    case WM_DESTROY:
        for (int i = 0; i < 6; ++i) {
            if (app->images[i]) {
                DeleteObject(app->images[i]);
                app->images[i] = nullptr;
            }
        }
        app->preview.Shutdown();
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool MainApp::loadImageForView(ViewDirection dir, const std::wstring& imagePath)
{
    Bitmap* bmp = Bitmap::FromFile(imagePath.c_str());
    if (!bmp) return false;

    Color bg(255, 255, 255, 0);
    HBITMAP hBmp;
    if (bmp->GetHBITMAP(bg, &hBmp) != Ok) {
        delete bmp;
        return false;
    }

    if (images[dir]) DeleteObject(images[dir]);
    images[dir] = hBmp;
    delete bmp;
    return true;
}

bool MainApp::generateModel()
{
    std::wstring imagePath = L"Charactor.png"; // 固定画像名（変更可）

    VolumeBuilder builder;
    if (!builder.BuildFromImage(imagePath, 256, 256, 64)) {
        MessageBox(hwnd, L"VolumeBuilder: ボリューム構築に失敗", L"エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    const auto& volume = builder.GetVolumeData();

    MeshGenerator generator;
    std::vector<MeshGenerator::Vertex> vertices;
    std::vector<unsigned int> indices;
    int cubeCount = 0;

    if (!generator.GenerateMesh(volume, 256, 256, 64, vertices, indices, cubeCount)) {
        MessageBox(hwnd, L"メッシュ生成に失敗", L"エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    std::vector<PreviewRenderer::Vertex> previewVerts;
    for (const auto& v : vertices) {
        previewVerts.push_back({ {v.x, v.y, v.z}, {0, 1, 0} }); // 仮法線
    }

    preview.Render(previewVerts, indices);

    std::wstring msg = L"頂点数: " + std::to_wstring(vertices.size()) +
        L"\nポリゴン数: " + std::to_wstring(indices.size() / 3);
    MessageBox(hwnd, msg.c_str(), L"メッシュ情報", MB_OK);

    return true;
}
