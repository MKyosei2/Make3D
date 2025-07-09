#include "MainApp.h"
#include <windows.h>
#include <commdlg.h>
#include <gdiplus.h>
#include "VolumeBuilder.h"
#include "MeshGenerator.h"
#include "FBXExporter.h"
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    MainApp app(hInstance);
    if (!app.initialize()) return 0;
    int result = app.run();

    GdiplusShutdown(gdiplusToken);
    return result;
}

MainApp::MainApp(HINSTANCE hInst) : hInstance(hInst), hwnd(nullptr), hSilhouette(nullptr), hStatusText(nullptr)
{
    ZeroMemory(buttons, sizeof(buttons));
    ZeroMemory(images, sizeof(images));
}

bool MainApp::initialize()
{
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

    CreateWindow(L"STATIC", L"Silhouette:", WS_VISIBLE | WS_CHILD,
        120, 80, 80, 20, hwnd, nullptr, hInstance, nullptr);

    hSilhouette = CreateWindow(L"EDIT", L"64", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
        200, 80, 60, 20, hwnd, nullptr, hInstance, nullptr);

    CreateWindow(L"BUTTON", L"Generate Model", WS_VISIBLE | WS_CHILD,
        120, 120, 140, 30, hwnd, (HMENU)202, hInstance, nullptr);

    hStatusText = CreateWindow(L"STATIC", L"Status: Ready", WS_VISIBLE | WS_CHILD,
        120, 160, 400, 20, hwnd, nullptr, hInstance, nullptr);

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
            ofn.lpstrFilter = L"Image Files (*.png;*.bmp)\0*.png;*.bmp\0All Files\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
                app->silhouetteThreshold = GetDlgItemInt(hwnd, GetDlgCtrlID(app->hSilhouette), nullptr, FALSE);
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
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool MainApp::loadImageForView(ViewDirection dir, const std::wstring& imagePath)
{
    Bitmap* bmp = Bitmap::FromFile(imagePath.c_str());

    if (!bmp) {
        MessageBox(hwnd, (L"Bitmapオブジェクトがnullです:\n" + imagePath).c_str(), L"エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    Status stat = bmp->GetLastStatus();
    if (stat != Ok) {
        std::wstring msg = L"Bitmap読み込み失敗。Statusコード: " + std::to_wstring(stat) + L"\n" + imagePath;
        MessageBox(hwnd, msg.c_str(), L"画像エラー", MB_OK | MB_ICONERROR);
        delete bmp;
        return false;
    }

    HBITMAP hBmp;
    Color bg(255, 255, 255, 0); // 透明背景
    if (bmp->GetHBITMAP(bg, &hBmp) != Ok) {
        MessageBox(hwnd, L"HBITMAP 変換に失敗しました。", L"画像エラー", MB_OK | MB_ICONERROR);
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
    VolumeBuilder builder;
    if (!builder.buildVolume(this->images, this->silhouetteThreshold)) return false;

    MeshGenerator generator;
    generator.setTargetPolygonCount(1000);
    Mesh mesh = generator.generate(builder.getVolume());

    if (!exportMeshToFBX(mesh, L"output.fbx")) {
        MessageBox(hwnd, L"FBX出力に失敗しました。", L"エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    MessageBox(hwnd, L"モデル生成および出力に成功しました。", L"成功", MB_OK);
    return true;
}
