#pragma once
#include <windows.h>
#include <string>
#include "common.h"
#include "ImageProcessor.h"
#include "PreviewRenderer.h"

struct AppState {
    HBITMAP images[ViewDirection::Count] = { nullptr };
    int polygonCount = 1000;
    int voxelResolution = 64;
    bool isSingleImage = false;
    std::wstring outputFilePath = L"output.fbx";
    BYTE silhouetteThreshold = 128;

    void clearImages();
    bool loadImageForView(ViewDirection dir, const std::wstring& imagePath);
    HBITMAP getImageBitmap(ViewDirection dir) const;
};

class MainApp {
public:
    MainApp(HINSTANCE hInstance);
    int run();

private:
    HINSTANCE hInst;
    HWND hWnd;
    AppState state;
    PreviewRenderer* preview = nullptr;

    HWND hImageButtons[ViewDirection::Count];
    HWND hPolygonCountInput;
    HWND hVoxelResolutionInput;
    HWND hThresholdInput;
    HWND hGenerateButton;
    HWND hFolderButton;
    HWND hSaveButton;
    HWND hPreviewStatic;
    HWND hSingleImageCheck;
    HWND hStatusText;
    HWND hClearButton;

    void createMainWindow();
    void createControlUI(HWND hwnd);
    void updateAppStateFromUI();
    void showImagePreview(ViewDirection dir);
    void loadImagesFromFolder();
    void selectSavePath();
    void onGenerateModel();
    void onClearImages();
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static MainApp* appInstance;
};
